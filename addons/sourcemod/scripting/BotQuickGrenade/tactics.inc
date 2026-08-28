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
    Handle ray = TR_TraceRayFilterEx(botEye, targetCenter, MASK_SHOT, RayType_EndPoint, TraceFilter_DontHitSelf, bot);

    bool canShoot = false;
    // TR_GetFraction(ray) == 1.0 指射线走完了全程，也就是射线没有被遮挡的意思
    if (TR_GetFraction(ray) == 1.0 || TR_GetEntityIndex(ray) == target)
    {
        canShoot = true;
    }
    CloseHandle(ray);

    // 有遮挡，敌人躲在掩体后
    if (!canShoot)
    {
        // 检查头部射线
        ray = TR_TraceRayFilterEx(botEye, targetHead, MASK_SHOT, RayType_EndPoint, TraceFilter_DontHitSelf, bot);
        if (TR_GetFraction(ray) == 1.0 || TR_GetEntityIndex(ray) == target)
        {
            canShoot = true;     // 躲掩体但是露头，还是优先用枪
        }
        CloseHandle(ray);
    }

    return canShoot;
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

        // 1. 感知记忆时效性检查（拒绝全图透视）
        if (now - g_fLastPerceivedTime[bot][t] > MEMORY_DURATION)
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