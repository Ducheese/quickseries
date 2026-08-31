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
 * 检查 Bot 当前是否正在执行不可打断的 C4 核心动作（正在按 E 拆包 或 正在按住左键插雷管下包）
 */
stock bool IsBotBusyWithC4(int bot)
{
    if (GetEntProp(bot, Prop_Send, "m_bIsDefusing") == 1)
        return true;

    int activeWep = GetEntPropEnt(bot, Prop_Send, "m_hActiveWeapon");
    if (activeWep > 0 && IsValidEntity(activeWep))
    {
        char wepCls[32];
        GetEntityClassname(activeWep, wepCls, sizeof(wepCls));
        if (StrEqual(wepCls, "weapon_c4", false))
        {
            if (HasEntProp(activeWep, Prop_Send, "m_bStartedArming") && GetEntProp(activeWep, Prop_Send, "m_bStartedArming") == 1)
                return true;
        }
    }
    return false;
}

/**
 * 检查当前游戏内敌对阵营是否仍有存活玩家/Bot
 */
stock bool HasLivingEnemies(int botTeam)
{
    int enemyTeam = (botTeam == CS_TEAM_CT) ? CS_TEAM_T : CS_TEAM_CT;
    for (int i = 1; i <= MaxClients; i++)
    {
        if (IsClientInGame(i) && IsPlayerAlive(i) && GetClientTeam(i) == enemyTeam)
        {
            return true;
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
 * @return             有效目标实体索引，若无可行目标返回 -1
 */
stock int FindTargetAndComputeAngles(int bot, float throwAngles[3], float landPos[3])
{
    if (!IsClientInGame(bot) || !IsPlayerAlive(bot) || IsBotBusyWithC4(bot))
        return -1;

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
            continue;

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
                continue;

            throwAngles[0] = bestPitch;
            throwAngles[1] = yaw;
            throwAngles[2] = 0.0;

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
 * @param isC4Event    输出是否命中了 C4 战术事件（用于阵营团队协同冷却更新）
 * @return             若找到有效烟雾战术目标且解算成功返回 true，否则 false
 */
stock bool FindSmokeTacticalTarget(int bot, float throwAngles[3], float landPos[3], char[] outReason, int maxReasonLen, bool &isC4Event)
{
    isC4Event = false;

    if (!IsClientInGame(bot) || !IsPlayerAlive(bot) || IsBotBusyWithC4(bot))
        return false;

    int team = GetClientTeam(bot);

    // 敌方存活检查：若敌对阵营已全灭（无任何外部威胁），严禁浪费投掷战术烟雾弹
    if (!HasLivingEnemies(team))
        return false;

    float botEye[3], botPos[3], botAng[3], fwd[3];
    GetClientEyePosition(bot, botEye);
    GetClientAbsOrigin(bot, botPos);
    GetClientEyeAngles(bot, botAng);
    GetAngleVectors(botAng, fwd, NULL_VECTOR, NULL_VECTOR);
    NormalizeVector(fwd, fwd);

    float smokeTarget[3];
    bool hasTarget = false;

    //====================================================================================
    // 优先级 1：C4 核心战术事件（阵营共享烟雾冷却：若同队已有队友在 C4 封烟，浓烟散去前不扎堆重复丢）
    //====================================================================================
    bool teamC4SmokeOnCooldown = (GetGameTime() - g_fLastTeamC4SmokeTime[team] < C4_TEAM_SMOKE_COOLDOWN);

    if (!teamC4SmokeOnCooldown)
    {
        // 1.1 CT 拆包与掩护封烟 (Defuse Smoke Cover)
        if (team == CS_TEAM_CT)
        {
            if (g_bIsC4Planted)
            {
                float distToC4 = GetVectorDistance(botPos, g_vecPlantedC4Pos);
                bool isDefusing = (GetEntProp(bot, Prop_Send, "m_bIsDefusing") == 1);
                bool inBombZone = (HasEntProp(bot, Prop_Send, "m_bInBombZone") && GetEntProp(bot, Prop_Send, "m_bInBombZone") == 1);

                // 检查是否有任意 CT 队友正在拆包
                bool teammateDefusing = false;
                for (int t = 1; t <= MaxClients; t++)
                {
                    if (t != bot && IsClientInGame(t) && IsPlayerAlive(t) && GetClientTeam(t) == CS_TEAM_CT)
                    {
                        if (GetEntProp(t, Prop_Send, "m_bIsDefusing") == 1)
                        {
                            teammateDefusing = true;
                            break;
                        }
                    }
                }

                // 自己没在拆包的前提下：自己准备去拆（进包点/靠近C4），或者有队友正在拆包且在投掷范围内
                if (!isDefusing && (inBombZone || distToC4 <= C4_DEFUSE_APPROACH_DIST || (teammateDefusing && distToC4 <= DIST_MAX)))
                {
                    smokeTarget = g_vecPlantedC4Pos;
                    hasTarget = true;
                    isC4Event = true;
                    Format(outReason, maxReasonLen, teammateDefusing ? "CT 掩护队友拆包封烟" : "CT 进点拆包前置封烟");
                }
            }
        }
        // 1.2 T 阵营 C4 战术
        else if (team == CS_TEAM_T)
        {
            // 1.2.1 若 C4 已经安放成功 -> T 方执行「守包延时封烟」：像 HE 一样将烟雾弹精准抛向回防的 CT 掩体/通道
            if (g_bIsC4Planted)
            {
                float now = GetGameTime();
                for (int t = 1; t <= MaxClients; t++)
                {
                    if (!IsClientInGame(t) || !IsPlayerAlive(t) || GetClientTeam(t) == CS_TEAM_T)
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
                            isC4Event = true;
                            Format(outReason, maxReasonLen, "T 守包封锁回防 CT 掩体 (敌: %N, 距离: %.0f)", t, d);
                            break;
                        }
                    }
                }
            }
            else
            {
                // 1.2.2 C4 未安放 -> 自己持包进点 / 掩护队友下包 / 捡掉落包
                int activeWep = GetEntPropEnt(bot, Prop_Send, "m_hActiveWeapon");
                if (activeWep > 0 && IsValidEntity(activeWep))
                {
                    char wepCls[32];
                    GetEntityClassname(activeWep, wepCls, sizeof(wepCls));
                    if (StrEqual(wepCls, "weapon_c4", false))
                    {
                        bool isPlanting = (HasEntProp(activeWep, Prop_Send, "m_bStartedArming") && GetEntProp(activeWep, Prop_Send, "m_bStartedArming") == 1);
                        bool inBombZone = (HasEntProp(bot, Prop_Send, "m_bInBombZone") && GetEntProp(bot, Prop_Send, "m_bInBombZone") == 1);

                        // 自己持包：进包点且尚未开始插雷管时前置封烟（插雷管中绝不自打断）
                        if (!isPlanting && inBombZone)
                        {
                            smokeTarget[0] = botPos[0] + fwd[0] * C4_PLANT_SELF_OFFSET;
                            smokeTarget[1] = botPos[1] + fwd[1] * C4_PLANT_SELF_OFFSET;
                            smokeTarget[2] = botPos[2];
                            hasTarget = true;
                            isC4Event = true;
                            Format(outReason, maxReasonLen, "T 持包进点前置封烟");
                        }
                    }
                }

                // 自己不是下包者（没拿 C4 或不在下包），检查是否有队友正在包点下包
                if (!hasTarget)
                {
                    for (int t = 1; t <= MaxClients; t++)
                    {
                        if (t == bot || !IsClientInGame(t) || !IsPlayerAlive(t) || GetClientTeam(t) != CS_TEAM_T)
                            continue;

                        int tWep = GetEntPropEnt(t, Prop_Send, "m_hActiveWeapon");
                        if (tWep > 0 && IsValidEntity(tWep))
                        {
                            char tCls[32];
                            GetEntityClassname(tWep, tCls, sizeof(tCls));
                            if (StrEqual(tCls, "weapon_c4", false))
                            {
                                bool tIsPlanting = (HasEntProp(tWep, Prop_Send, "m_bStartedArming") && GetEntProp(tWep, Prop_Send, "m_bStartedArming") == 1);
                                bool tInBombZone = (HasEntProp(t, Prop_Send, "m_bInBombZone") && GetEntProp(t, Prop_Send, "m_bInBombZone") == 1);

                                if (tIsPlanting || tInBombZone)
                                {
                                    float tPos[3];
                                    GetClientAbsOrigin(t, tPos);
                                    float distToPlanter = GetVectorDistance(botPos, tPos);

                                    if (distToPlanter >= C4_PLANTER_COVER_MIN_DIST && distToPlanter <= DIST_MAX)
                                    {
                                        smokeTarget = tPos;
                                        hasTarget = true;
                                        isC4Event = true;
                                        Format(outReason, maxReasonLen, "T 掩护队友 %N 下包封烟 (距离: %.0f)", t, distToPlanter);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                if (!hasTarget && g_bIsC4Dropped)
                {
                    float distToDropped = GetVectorDistance(botPos, g_vecDroppedC4Pos);
                    if (distToDropped <= C4_DROPPED_SEARCH_DIST)
                    {
                        smokeTarget = g_vecDroppedC4Pos;
                        hasTarget = true;
                        isC4Event = true;
                        Format(outReason, maxReasonLen, "T 掩护拾取掉落 C4 封烟 (距离: %.0f)", distToDropped);
                    }
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
                float nearestDist = MAX_SEARCH_DIST;
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
                    Format(outReason, maxReasonLen, inReload ? "换弹视线中点封烟" : "弹夹打空视线中点封烟");
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
        throwAngles[0] = bestPitch;
        throwAngles[1] = yaw;
        throwAngles[2] = 0.0;

        return true;
    }

    return false;
}

//========================================================================================
// 战术闪光弹 (FLASHBANG) 核心门禁与决策引擎
//========================================================================================

/**
 * 门禁 1：敌人致盲质量检测
 * 爆炸点与敌人眼位之间必须视线直通（无实心掩体阻隔），且爆炸点位于敌人视线正前方视锥内 (Dot > 0.0)
 */
stock bool CanFlashBlindEnemy(const float flashPos[3], int enemy)
{
    float enemyEye[3];
    GetClientEyePosition(enemy, enemyEye);

    // 1. 视线通达性检查
    TR_TraceRayFilter(flashPos, enemyEye, MASK_VISIBLE, RayType_EndPoint, TraceFilter_WorldOnly);
    if (TR_GetFraction() != 1.0)
        return false;

    // 2. 敌人视线朝向夹角检查（必须处于敌人正面视野内）
    float enemyAng[3], enemyFwd[3], toFlash[3];
    GetClientEyeAngles(enemy, enemyAng);
    GetAngleVectors(enemyAng, enemyFwd, NULL_VECTOR, NULL_VECTOR);
    NormalizeVector(enemyFwd, enemyFwd);

    SubtractVectors(flashPos, enemyEye, toFlash);
    NormalizeVector(toFlash, toFlash);

    return (GetVectorDotProduct(enemyFwd, toFlash) > 0.0);
}

/**
 * 门禁 2：严苛防自闪检测
 * 爆炸点与 Bot 自身眼位之间必须有实心掩体阻隔（典型掩体后高抛）；若空旷无遮挡，则距离必须大于 400 且背身
 */
stock bool IsFlashSafeFromSelf(int bot, const float flashPos[3])
{
    float botEye[3];
    GetClientEyePosition(bot, botEye);

    TR_TraceRayFilter(flashPos, botEye, MASK_VISIBLE, RayType_EndPoint, TraceFilter_WorldOnly);
    if (TR_GetFraction() < 1.0)
    {
        return true; // 自身与爆炸点之间有掩体遮挡，绝对安全
    }

    // 若无掩体阻隔，要求距离远 (> FLASH_SELF_SAFE_DIST) 且背身
    float dist = GetVectorDistance(botEye, flashPos);
    if (dist <= FLASH_SELF_SAFE_DIST)
        return false;

    float botAng[3], botFwd[3], toFlash[3];
    GetClientEyeAngles(bot, botAng);
    GetAngleVectors(botAng, botFwd, NULL_VECTOR, NULL_VECTOR);
    NormalizeVector(botFwd, botFwd);

    SubtractVectors(flashPos, botEye, toFlash);
    NormalizeVector(toFlash, toFlash);

    return (GetVectorDotProduct(botFwd, toFlash) < 0.0);
}

/**
 * 门禁 3：严苛防队友检测
 * 检查爆炸点是否会在强致盲半径 (FLASH_HIT_RADIUS) 内直连并闪瞎任何正视该方向的存活队友
 */
stock bool IsTeammateFlashed(int bot, const float flashPos[3])
{
    int botTeam = GetClientTeam(bot);

    for (int mate = 1; mate <= MaxClients; mate++)
    {
        if (mate == bot || !IsClientInGame(mate) || !IsPlayerAlive(mate) || GetClientTeam(mate) != botTeam)
            continue;

        float mateEye[3];
        GetClientEyePosition(mate, mateEye);

        float dist = GetVectorDistance(mateEye, flashPos);
        if (dist <= FLASH_HIT_RADIUS)
        {
            TR_TraceRayFilter(flashPos, mateEye, MASK_VISIBLE, RayType_EndPoint, TraceFilter_WorldOnly);
            if (TR_GetFraction() == 1.0)
            {
                // 射线通畅，进一步检查队友是否面朝闪光
                float mateAng[3], mateFwd[3], toFlash[3];
                GetClientEyeAngles(mate, mateAng);
                GetAngleVectors(mateAng, mateFwd, NULL_VECTOR, NULL_VECTOR);
                NormalizeVector(mateFwd, mateFwd);

                SubtractVectors(flashPos, mateEye, toFlash);
                NormalizeVector(toFlash, toFlash);

                if (GetVectorDotProduct(mateFwd, toFlash) > 0.0)
                {
                    return true; // 会闪瞎正面直视的队友！
                }
            }
        }
    }
    return false;
}

/**
 * 战术闪光弹决策与弹道求解：
 * 遍历短期记忆库，针对掩体后存活敌人（无直连枪线时）实施高抛破点闪光压制
 *
 * @param bot          Bot 实体索引
 * @param throwAngles  输出求解出的投掷欧拉角
 * @param landPos      输出闪光弹最终落点坐标
 * @param outReason    输出触发原因描述
 * @param maxReasonLen 描述字符串缓冲区大小
 * @return             有效目标实体索引，若无可行目标返回 -1
 */
stock int FindFlashTacticalTarget(int bot, float throwAngles[3], float landPos[3], char[] outReason, int maxReasonLen)
{
    if (!IsClientInGame(bot) || !IsPlayerAlive(bot) || IsBotBusyWithC4(bot))
        return -1;

    int botTeam = GetClientTeam(bot);

    // 敌方存活检查：若敌方全灭，无任何威胁，严禁丢闪
    if (!HasLivingEnemies(botTeam))
        return -1;

    float now = GetGameTime();
    float botEye[3];
    GetClientEyePosition(bot, botEye);

    // 遍历短期记忆库中掩体后的所有存活敌人（涵盖回防、进点与常规架枪局面）
    for (int t = 1; t <= MaxClients; t++)
    {
        if (!IsClientInGame(t) || !IsPlayerAlive(t) || GetClientTeam(t) == botTeam)
            continue;

        if (g_fLastPerceivedTime[bot][t] <= 0.0 || (now - g_fLastPerceivedTime[bot][t] > MEMORY_DURATION))
            continue;

        // 掩体战术限制：必须被掩体阻挡无直连枪线，方可高抛闪光（露头直连优先拿枪对射）
        if (GetConVarBool(cvarRequireCover) && CanShootTargetDirectly(bot, t))
            continue;

        float targetPos[3];
        targetPos = g_vecLastKnownPos[bot][t];

        float dist = GetVectorDistance(botEye, targetPos);
        if (dist < DIST_MIN || dist > DIST_MAX)
            continue;

        float dx = targetPos[0] - botEye[0];
        float dy = targetPos[1] - botEye[1];
        float yaw = RadToDeg(ArcTangent2(dy, dx));

        float bestPitch;
        bool hit = FindBestPitchForTarget(bot, targetPos, yaw, bestPitch, landPos, FLASH_HIT_RADIUS);
        if (hit)
        {
            // 三大安全门禁检验：敌人正向有效致盲 + 严苛防自闪 + 友军安全检查 (受 cvarFriendlyFire 控制)
            bool mateSafe = (!GetConVarBool(cvarFriendlyFire) || !IsTeammateFlashed(bot, landPos));

            if (CanFlashBlindEnemy(landPos, t) && IsFlashSafeFromSelf(bot, landPos) && mateSafe)
            {
                throwAngles[0] = bestPitch;
                throwAngles[1] = yaw;
                throwAngles[2] = 0.0;
                Format(outReason, maxReasonLen, "掩体后破点闪光 (敌: %N, 距离: %.0f)", t, dist);
                return t;
            }
        }
    }

    return -1;
}
