#if defined _botquickgrenade_tactics_included
  #endinput
#endif
#define _botquickgrenade_tactics_included

//========================================================================================
// TACTICAL EVALUATION & DECISION ENGINE
// 负责直连枪线检测、掩体战术限制、队友防误伤安全检查以及最佳目标决策
//========================================================================================

/**
 * 直连枪线检测：检查 Bot 当前位置是否能直接开枪打中敌人（无掩体阻挡）
 *
 * @param bot     Bot 实体索引
 * @param target  目标实体索引
 * @return        若胸口或头部射线直通无阻挡返回 true，否则 false
 */
stock bool CanShootTargetDirectly(int bot, int target)
{
    if (!IsClientInGame(target) || !IsPlayerAlive(target))
        return false;

    float botEye[3], targetCenter[3], targetHead[3];
    GetClientEyePosition(bot, botEye);
    GetClientEyePosition(target, targetHead);
    GetClientAABBCenter(target, targetCenter); // 目标 AABB 物理中心

    // 检查身体中心射线
    TR_TraceRayFilter(botEye, targetCenter, MASK_SHOT, RayType_EndPoint, TraceFilter_DontHitSelf, bot);
    if (TR_GetFraction() == 1.0 || TR_GetEntityIndex() == target)
    {
        return true;
    }

    // 检查头部射线
    TR_TraceRayFilter(botEye, targetHead, MASK_SHOT, RayType_EndPoint, TraceFilter_DontHitSelf, bot);
    if (TR_GetFraction() == 1.0 || TR_GetEntityIndex() == target)
    {
        return true;
    }

    return false;
}

/**
 * 队友防误伤检测：检查手雷落点附近是否存在存活队友
 *
 * @param bot     Bot 实体索引
 * @param pos     手雷爆炸落点坐标
 * @param radius  安全保护半径
 * @return        若落点附近有队友返回 true，否则 false
 */
stock bool IsTeammateNearPosition(int bot, const float pos[3], float radius)
{
    int botTeam = GetClientTeam(bot);

    for (int mate = 1; mate <= MaxClients; mate++)
    {
        if (mate == bot) continue; // 自身距离由 DIST_MIN 保证
        if (!IsClientInGame(mate) || !IsPlayerAlive(mate)) continue;
        if (GetClientTeam(mate) != botTeam) continue;

        float matePos[3];
        GetClientAbsOrigin(mate, matePos);

        if (GetVectorDistance(matePos, pos) <= radius)
        {
            return true; // 发现队友处于落点危险区
        }
    }
    return false;
}

/**
 * 核心决策函数：遍历记忆库，寻找符合掩体战术与安全条件的最佳投掷目标并计算投掷角度
 *
 * @param bot          Bot 实体索引
 * @param throwAngles  输出计算出的最佳投掷欧拉角 (pitch, yaw, 0)
 * @param landPos      输出最佳落点坐标
 * @param visualize    是否绘制可视化弹道与判定圆
 * @return             有效目标实体索引，若无可行目标返回 -1
 */
stock int FindTargetAndComputeAngles(int bot, float throwAngles[3], float landPos[3], bool visualize)
{
    int botTeam = GetClientTeam(bot);
    float now = GetGameTime();

    float botEye[3];
    GetClientEyePosition(bot, botEye);

    for (int t = 1; t <= MaxClients; t++)
    {
        if (!IsClientInGame(t) || !IsPlayerAlive(t)) continue;
        if (GetClientTeam(t) == botTeam) continue;

        // 1. 感知记忆时效性检查（必须真实感知过且在有效期内，拒绝全图透视与开局零值误判）
        if (g_fLastPerceivedTime[bot][t] <= 0.0 || (now - g_fLastPerceivedTime[bot][t] > MEMORY_DURATION))
            continue;

        // 2. 掩体战术限制检查：若枪线完全通畅直连，则优先用枪交火，禁止面对面当脸拔雷
        if (GetConVarBool(cvarRequireCover) && CanShootTargetDirectly(bot, t))
        {
            if (visualize)
            {
                LogDebug("\x04[BotQG - 战术放弃]\x01 Bot \x03%N\x01 与敌人 \x05%N\x01 枪线直连通畅，优先用枪，放弃丢雷", bot, t);
            }
            continue;
        }

        // 3. 距离区间粗筛（太近防自炸，太远抛不到）
        float targetPos[3];
        targetPos = g_vecLastKnownPos[bot][t];

        float dist = GetVectorDistance(botEye, targetPos);
        if (dist < DIST_MIN || dist > DIST_MAX) continue;

        // 4. 水平偏航角计算
        float dx = targetPos[0] - botEye[0];
        float dy = targetPos[1] - botEye[1];
        float yaw = RadToDeg(ArcTangent2(dy, dx));

        // 5. 仰角数值搜索（求解抛物线）
        float bestPitch;
        bool hit = FindBestPitchForTarget(bot, targetPos, yaw, bestPitch, landPos);

        if (hit)
        {
            // 6. 队友防误伤安全检查
            if (GetConVarBool(cvarFriendlyFire) && IsTeammateNearPosition(bot, landPos, TEAMMATE_SAFETY_RADIUS))
            {
                if (visualize)
                {
                    LogDebug("\x04[BotQG - 安全放弃]\x01 Bot \x03%N\x01 计算出落点附近有存活队友，放弃丢雷以防误伤", bot);
                }
                continue;
            }

            throwAngles[0] = bestPitch;
            throwAngles[1] = yaw;
            throwAngles[2] = 0.0;

            if (visualize)
            {
                float simAngles[3];
                simAngles[0] = bestPitch;
                simAngles[1] = yaw;
                simAngles[2] = 0.0;
                float visLand[3];
                SimulateGrenadeTrajectoryWithAngles(bot, simAngles, visLand, true);
                DrawLandingCircle(landPos, HE_HIT_RADIUS, COLOR_HIT, BEAM_LIFE);
            }

            return t;
        }
    }

    return -1;
}

/**
 * 战术烟雾弹目标评估与弹道求解：
 * 优先级 1：C4 核心事件（CT 拆包封烟、T 下包封烟、T 捡掉落包封烟）
 * 优先级 2：残血脚底自保烟（HP <= SMOKE_HEALTH_THRESHOLD 且处于威胁中）
 * 优先级 3：弹夹打空/换弹视线中点烟（clip1 == 0 或 换弹中，且面前有敌人）
 *
 * @param bot          Bot 实体索引
 * @param throwAngles  输出求解出的投掷欧拉角
 * @param landPos      输出烟雾弹最终落点坐标
 * @param outReason    输出触发原因描述
 * @param maxReasonLen 描述字符串缓冲区大小
 * @param debug        是否渲染弹道与圆圈
 * @return             若找到有效烟雾战术目标且解算成功返回 true，否则 false
 */
stock bool FindSmokeTacticalTarget(int bot, float throwAngles[3], float landPos[3], char[] outReason, int maxReasonLen, bool visualize)
{
    if (!IsClientInGame(bot) || !IsPlayerAlive(bot))
        return false;

    int team = GetClientTeam(bot);
    float botEye[3], botPos[3], botAng[3], fwd[3];
    GetClientEyePosition(bot, botEye);
    GetClientAbsOrigin(bot, botPos);
    GetClientEyeAngles(bot, botAng);
    GetAngleVectors(botAng, fwd, NULL_VECTOR, NULL_VECTOR);
    NormalizeVector(fwd, fwd);

    float smokeTarget[3];
    bool hasTarget = false;

    //====================================================================================
    // 优先级 1：C4 核心战术事件
    //====================================================================================

    // 1.1 CT 拆包封烟 (Defuse Smoke Cover)

    if (team == 3) // 3 = CT
    {
        if (g_bIsC4Planted)
        {
            float distToC4 = GetVectorDistance(botPos, g_vecPlantedC4Pos);
            bool isDefusing = (GetEntProp(bot, Prop_Send, "m_bIsDefusing") == 1);
            if (isDefusing || distToC4 <= C4_DEFUSE_APPROACH_DIST)
            {
                smokeTarget = g_vecPlantedC4Pos;
                hasTarget = true;
                Format(outReason, maxReasonLen, "CT 掩护拆包封烟 (距离 C4: %.0f)", distToC4);
            }
        }
    }
    // 1.2 T 阵营 C4 战术
    else if (team == 2) // 2 = T
    {
        // 1.2.1 若 C4 已经安放成功 -> T 方执行「守包延时封烟」：像 HE 一样将烟雾弹精准抛向回防的 CT 掩体/通道
        if (g_bIsC4Planted)
        {
            float now = GetGameTime();
            for (int t = 1; t <= MaxClients; t++)
            {
                if (!IsClientInGame(t) || !IsPlayerAlive(t) || GetClientTeam(t) == 2)
                    continue;

                if (g_fLastPerceivedTime[bot][t] > 0.0 && (now - g_fLastPerceivedTime[bot][t] <= MEMORY_DURATION))
                {
                    float enemyPos[3];
                    enemyPos = g_vecLastKnownPos[bot][t];
                    float d = GetVectorDistance(botPos, enemyPos);

                    if (d >= DIST_MIN && d <= DIST_MAX)
                    {
                        smokeTarget = enemyPos;
                        hasTarget = true;
                        Format(outReason, maxReasonLen, "T 守包封锁回防 CT 掩体 (敌: %N, 距离: %.0f)", t, d);
                        break;
                    }
                }
            }
        }
        else
        {
            // 1.2.2 C4 未安放 -> 正在下包掩护 或 捡掉落包封烟
            int activeWep = GetEntPropEnt(bot, Prop_Send, "m_hActiveWeapon");
            if (activeWep > 0 && IsValidEntity(activeWep))
            {
                char wepCls[32];
                GetEntityClassname(activeWep, wepCls, sizeof(wepCls));
                if (StrEqual(wepCls, "weapon_c4", false))
                {
                    // 必须是真正按住左键正在下包（m_bStartedArming == 1），杜绝开局手持 C4 跑路时误在脚底封烟
                    bool isPlanting = (HasEntProp(activeWep, Prop_Send, "m_bStartedArming") && GetEntProp(activeWep, Prop_Send, "m_bStartedArming") == 1);
                    if (isPlanting)
                    {
                        smokeTarget[0] = botPos[0] + fwd[0] * 50.0;
                        smokeTarget[1] = botPos[1] + fwd[1] * 50.0;
                        smokeTarget[2] = botPos[2];
                        hasTarget = true;
                        Format(outReason, maxReasonLen, "T 掩护下包封烟");
                    }
                }
            }

            if (!hasTarget && g_bIsC4Dropped)
            {
                float distToDropped = GetVectorDistance(botPos, g_vecDroppedC4Pos);
                if (distToDropped <= 600.0)
                {
                    smokeTarget = g_vecDroppedC4Pos;
                    hasTarget = true;
                    Format(outReason, maxReasonLen, "T 掩护拾取掉落 C4 封烟 (距离: %.0f)", distToDropped);
                }
            }
        }
    }

    //====================================================================================
    // 优先级 2：残血脚底自保烟 (HP <= 设定值)
    //====================================================================================
    if (!hasTarget)
    {
        int hp = GetClientHealth(bot);
        int hpThreshold = (cvarSmokeHp != null) ? cvarSmokeHp.IntValue : SMOKE_HEALTH_THRESHOLD;

        if (hp <= hpThreshold)
        {
            bool hasThreat = false;
            float now = GetGameTime();
            for (int t = 1; t <= MaxClients; t++)
            {
                if (!IsClientInGame(t) || !IsPlayerAlive(t) || GetClientTeam(t) == team)
                    continue;

                if (g_fLastPerceivedTime[bot][t] > 0.0 && (now - g_fLastPerceivedTime[bot][t] <= MEMORY_DURATION))
                {
                    hasThreat = true;
                    break;
                }
            }

            if (hasThreat)
            {
                smokeTarget[0] = botPos[0] + fwd[0] * SMOKE_SELF_OFFSET;
                smokeTarget[1] = botPos[1] + fwd[1] * SMOKE_SELF_OFFSET;
                smokeTarget[2] = botPos[2];
                hasTarget = true;
                Format(outReason, maxReasonLen, "残血自保脚底烟 (HP: %d)", hp);
            }
        }
    }

    //====================================================================================
    // 优先级 3：弹夹打空 / 换弹视线中点烟 (Clip == 0)
    //====================================================================================
    if (!hasTarget)
    {
        int activeWep = GetEntPropEnt(bot, Prop_Send, "m_hActiveWeapon");
        if (activeWep > 0 && IsValidEntity(activeWep))
        {
            int clip1 = GetEntProp(activeWep, Prop_Send, "m_iClip1");
            bool inReload = (HasEntProp(activeWep, Prop_Data, "m_bInReload") && GetEntProp(activeWep, Prop_Data, "m_bInReload") == 1);

            if (clip1 == 0 || inReload)
            {
                float nearestDist = 999999.0;
                int nearestEnemy = -1;
                float now = GetGameTime();

                for (int t = 1; t <= MaxClients; t++)
                {
                    if (!IsClientInGame(t) || !IsPlayerAlive(t) || GetClientTeam(t) == team)
                        continue;

                    if (g_fLastPerceivedTime[bot][t] > 0.0 && (now - g_fLastPerceivedTime[bot][t] <= MEMORY_DURATION))
                    {
                        float d = GetVectorDistance(botPos, g_vecLastKnownPos[bot][t]);
                        if (d < nearestDist && d <= DIST_MAX)
                        {
                            nearestDist = d;
                            nearestEnemy = t;
                        }
                    }
                }

                if (nearestEnemy > 0)
                {
                    float enemyPos[3];
                    enemyPos = g_vecLastKnownPos[bot][nearestEnemy];

                    smokeTarget[0] = botPos[0] + (enemyPos[0] - botPos[0]) * SMOKE_INTERPOLATION_RATIO;
                    smokeTarget[1] = botPos[1] + (enemyPos[1] - botPos[1]) * SMOKE_INTERPOLATION_RATIO;
                    smokeTarget[2] = botPos[2] + (enemyPos[2] - botPos[2]) * SMOKE_INTERPOLATION_RATIO;

                    hasTarget = true;
                    Format(outReason, maxReasonLen, "弹夹耗尽换弹中点烟 (敌: %N, 距离: %.0f)", nearestEnemy, nearestDist);
                }
            }
        }
    }

    if (!hasTarget)
        return false;

    //====================================================================================
    // 弹道求解：计算水平偏航角与最优仰角
    //====================================================================================
    float dx = smokeTarget[0] - botEye[0];
    float dy = smokeTarget[1] - botEye[1];
    float yaw = RadToDeg(ArcTangent2(dy, dx));

    float bestPitch;
    bool hit = FindBestPitchForTarget(bot, smokeTarget, yaw, bestPitch, landPos, SMOKE_HIT_RADIUS);

    if (hit)
    {
        if (GetConVarBool(cvarFriendlyFire) && IsTeammateNearPosition(bot, landPos, TEAMMATE_SAFETY_RADIUS))
        {
            return false;
        }

        throwAngles[0] = bestPitch;
        throwAngles[1] = yaw;
        throwAngles[2] = 0.0;

        if (visualize)
        {
            float simAngles[3];
            simAngles[0] = bestPitch;
            simAngles[1] = yaw;
            simAngles[2] = 0.0;
            float visLand[3];
            SimulateGrenadeTrajectoryWithAngles(bot, simAngles, visLand, true);
            DrawLandingCircle(landPos, SMOKE_HIT_RADIUS, COLOR_SMOKE, BEAM_LIFE);
        }

        return true;
    }

    return false;
}
