#if defined _botquickgrenade_perception_included
  #endinput
#endif
#define _botquickgrenade_perception_included

//========================================================================================
// PERCEPTION SYSTEM & SHORT-TERM MEMORY
// 负责视觉、听觉（脚步/枪声/消音器识别/散布）、受击感知与记忆更新
//========================================================================================

/**
 * 射线过滤器：忽略自身
 */
public bool TraceFilter_DontHitSelf(int entity, int mask, any data)
{
    return (entity != data);
}

/**
 * 获取玩家实体的世界空间 AABB 包围盒中心坐标
 * 自动适应站立 (72高度, 中心36) 与下蹲 (54高度, 中心27) 状态
 */
stock void GetClientAABBCenter(int client, float center[3])
{
    float origin[3], mins[3], maxs[3];
    GetClientAbsOrigin(client, origin);
    GetClientMins(client, mins);
    GetClientMaxs(client, maxs);

    center[0] = origin[0] + (mins[0] + maxs[0]) * 0.5;
    center[1] = origin[1] + (mins[1] + maxs[1]) * 0.5;
    center[2] = origin[2] + (mins[2] + maxs[2]) * 0.5;
}

/**
 * 视觉扫描：更新 Bot 当前视野内直接可见的存活敌人
 */
stock void UpdateBotVision(int bot)
{
    int botTeam = GetClientTeam(bot);

    float botEye[3], eyeAng[3], fwd[3];
    GetClientEyePosition(bot, botEye);
    GetClientEyeAngles(bot, eyeAng);
    GetAngleVectors(eyeAng, fwd, NULL_VECTOR, NULL_VECTOR);
    NormalizeVector(fwd, fwd);

    float now = GetGameTime();

    for (int t = 1; t <= MaxClients; t++)
    {
        if (!IsClientInGame(t) || !IsPlayerAlive(t)) continue;
        if (GetClientTeam(t) == botTeam)             continue;

        // 获取敌人当前 AABB 盒子的空间物理中心
        float targetPos[3];
        GetClientAABBCenter(t, targetPos);

        float toTarget[3];
        SubtractVectors(targetPos, botEye, toTarget);
        float dist = GetVectorLength(toTarget);
        if (dist < 1.0 || dist > VISION_MAX_RANGE) continue;

        NormalizeVector(toTarget, toTarget);

        // 视锥角度判定 (夹角 <= BOT_FOV_HALF)
        float dot = GetVectorDotProduct(fwd, toTarget);
        if (dot > 1.0) dot = 1.0;
        else if (dot < -1.0) dot = -1.0;
        float angle = RadToDeg(ArcCosine(dot));

        if (angle <= BOT_FOV_HALF)
        {
            // 物理视线遮挡检测 (Line of Sight)
            TR_TraceRayFilter(botEye, targetPos, MASK_VISIBLE, RayType_EndPoint, TraceFilter_DontHitSelf, bot);
            bool visible = (TR_GetFraction() == 1.0 || TR_GetEntityIndex() == t);

            if (visible)
            {
                float origin[3];
                GetClientAbsOrigin(t, origin);

                // 首次发现或记忆过期重新看见时输出提示
                if (now - g_fLastPerceivedTime[bot][t] > 1.5)
                {
                    LogDebug(bot, "\x04[BotQG - 视觉]\x01 Bot \x03%N\x01 视线发现了敌人 \x05%N\x01 (距离: %.0f, 夹角: %.1f°)", bot, t, dist, angle);
                }

                g_fLastPerceivedTime[bot][t] = now;
                g_vecLastKnownPos[bot][t]    = origin;
            }
        }
    }
}

/**
 * 听觉处理：敌人开火事件感知（含消音器识别与坐标散布）
 */
stock void ProcessGunfire(int shooter)
{
    if (shooter < 1 || shooter > MaxClients || !IsClientInGame(shooter) || !IsPlayerAlive(shooter))
        return;

    int shooterTeam = GetClientTeam(shooter);
    float shooterPos[3];
    GetClientAbsOrigin(shooter, shooterPos);

    // 检查武器是否为消音状态
    bool isSilenced = false;
    int activeWep = GetEntPropEnt(shooter, Prop_Data, "m_hActiveWeapon");
    if (activeWep > 0 && IsValidEntity(activeWep))
    {
        char wepClass[32];
        GetEntityClassname(activeWep, wepClass, sizeof(wepClass));

        if (StrEqual(wepClass, "weapon_m4a1", false) || StrEqual(wepClass, "weapon_usp", false))
        {
            if (GetEntProp(activeWep, Prop_Send, "m_bSilencerOn") == 1)
            {
                isSilenced = true;
            }
        }
        else if (StrEqual(wepClass, "weapon_tmp", false))
        {
            isSilenced = true; // TMP 默认内置消音
        }
    }

    float maxHearDist = isSilenced ? GUNFIRE_SILENCED_RANGE : GUNFIRE_HEARING_RANGE;
    float now = GetGameTime();

    for (int bot = 1; bot <= MaxClients; bot++)
    {
        if (!IsClientInGame(bot) || !IsFakeClient(bot) || !IsPlayerAlive(bot))
            continue;
        if (GetClientTeam(bot) == shooterTeam)
            continue;

        float botPos[3];
        GetClientAbsOrigin(bot, botPos);

        float dist = GetVectorDistance(botPos, shooterPos);
        if (dist <= maxHearDist)
        {
            // 为枪声位置施加随机坐标散布，模拟听声辨位误差
            float perceivedPos[3];
            perceivedPos[0] = shooterPos[0] + GetRandomFloat(-SOUND_DISPERSION_GUNFIRE, SOUND_DISPERSION_GUNFIRE);
            perceivedPos[1] = shooterPos[1] + GetRandomFloat(-SOUND_DISPERSION_GUNFIRE, SOUND_DISPERSION_GUNFIRE);
            perceivedPos[2] = shooterPos[2];

            g_fLastPerceivedTime[bot][shooter] = now;
            g_vecLastKnownPos[bot][shooter]    = perceivedPos;

            LogDebug(bot, "\x04[BotQG - 听觉]\x01 Bot \x03%N\x01 听到敌人 \x05%N\x01 的枪声%s (距离: %.0f, 误差散布: %.0f)",
                bot, shooter, isSilenced ? "(消音)" : "", dist, SOUND_DISPERSION_GUNFIRE);
        }
    }
}

/**
 * 听觉处理：敌人脚步声感知（NormalSoundHook 回调处理）
 */
stock void ProcessFootstep(int walker)
{
    if (walker < 1 || walker > MaxClients || !IsClientInGame(walker) || !IsPlayerAlive(walker))
        return;

    int walkerTeam = GetClientTeam(walker);
    float walkerPos[3];
    GetClientAbsOrigin(walker, walkerPos);
    float now = GetGameTime();

    for (int bot = 1; bot <= MaxClients; bot++)
    {
        if (!IsClientInGame(bot) || !IsFakeClient(bot) || !IsPlayerAlive(bot))
            continue;
        if (GetClientTeam(bot) == walkerTeam)
            continue;

        float botPos[3];
        GetClientAbsOrigin(bot, botPos);

        float dist = GetVectorDistance(botPos, walkerPos);
        if (dist <= FOOTSTEP_HEARING_RANGE)
        {
            // 为脚步声施加轻微散布
            float perceivedPos[3];
            perceivedPos[0] = walkerPos[0] + GetRandomFloat(-SOUND_DISPERSION_FOOTSTEP, SOUND_DISPERSION_FOOTSTEP);
            perceivedPos[1] = walkerPos[1] + GetRandomFloat(-SOUND_DISPERSION_FOOTSTEP, SOUND_DISPERSION_FOOTSTEP);
            perceivedPos[2] = walkerPos[2];

            // 若之前已有更新的视觉/受击记忆，不覆盖为粗糙的脚步声；否则更新
            if (now - g_fLastPerceivedTime[bot][walker] > 0.5)
            {
                g_fLastPerceivedTime[bot][walker] = now;
                g_vecLastKnownPos[bot][walker]    = perceivedPos;

                LogDebug(bot, "\x04[BotQG - 脚步]\x01 Bot \x03%N\x01 听到了敌人 \x05%N\x01 的脚步声 (距离: %.0f)", bot, walker, dist);
            }
        }
    }
}

/**
 * 受击处理：Bot 被击中后立即感知攻击者精确位置
 */
stock void ProcessHurt(int victim, int attacker)
{
    if (victim < 1 || victim > MaxClients || !IsFakeClient(victim) || !IsPlayerAlive(victim))
        return;
    if (attacker < 1 || attacker > MaxClients || !IsClientInGame(attacker) || !IsPlayerAlive(attacker))
        return;
    if (GetClientTeam(victim) == GetClientTeam(attacker))
        return;

    float attackerPos[3];
    GetClientAbsOrigin(attacker, attackerPos);

    g_fLastPerceivedTime[victim][attacker] = GetGameTime();
    g_vecLastKnownPos[victim][attacker]    = attackerPos;

    LogDebug(victim, "\x04[BotQG - 受击]\x01 Bot \x03%N\x01 被敌人 \x05%N\x01 击中，立刻感知其精确位置!", victim, attacker);
}

/**
 * 检查 Bot 当前是否处于严重致盲状态（用于烟雾弹脱困自保）
 * 要求处于致盲期内且剩余致盲时长达到阈值（排除余光轻微晃眼或即将复明的场景）
 */
stock bool IsBotFlashed(int bot, float minRemainingDuration = SMOKE_BLIND_MIN_DURATION)
{
    float now = GetGameTime();
    if (g_fFlashEndTime[bot] <= now)
        return false;

    return ((g_fFlashEndTime[bot] - now) >= minRemainingDuration);
}

/**
 * 玩家死亡或断开清空记忆
 */
stock void ClearPlayerMemory(int client)
{
    g_fFlashEndTime[client] = 0.0;

    for (int b = 1; b <= MaxClients; b++)
    {
        g_fLastPerceivedTime[b][client] = 0.0;  // 别人对我的记忆（列）
        g_fLastPerceivedTime[client][b] = 0.0;  // 我对别人的记忆（行）
        g_vecLastKnownPos[b][client]    = NULL_VECTOR;
        g_vecLastKnownPos[client][b]    = NULL_VECTOR;
    }
}

/**
 * 清空场上所有 Bot 的感知记忆（回合重置时调用）
 */
stock void ClearAllMemory()
{
    for (int i = 1; i <= MaxClients; i++)
    {
        g_fFlashEndTime[i] = 0.0;

        for (int j = 1; j <= MaxClients; j++)
        {
            g_fLastPerceivedTime[i][j] = 0.0;
            g_vecLastKnownPos[i][j]    = NULL_VECTOR;
        }
    }
}

/**
 * 重置 C4 战术全局状态（回合开始或结束时调用）
 */
stock void ResetC4State()
{
    g_bIsC4Planted    = false;
    g_vecPlantedC4Pos = NULL_VECTOR;
    g_bIsC4Dropped    = false;
    g_vecDroppedC4Pos = NULL_VECTOR;
    g_fLastTeamC4SmokeTime[CS_TEAM_T]  = 0.0;
    g_fLastTeamC4SmokeTime[CS_TEAM_CT] = 0.0;
}

/**
 * C4 安放成功
 */
stock void ProcessBombPlanted(const float pos[3])
{
    g_bIsC4Planted    = true;
    g_vecPlantedC4Pos = pos;
    g_bIsC4Dropped    = false;
    g_vecDroppedC4Pos = NULL_VECTOR;
}

/**
 * C4 掉落在地上
 */
stock void ProcessBombDropped(const float pos[3])
{
    g_bIsC4Dropped    = true;
    g_vecDroppedC4Pos = pos;
}

/**
 * C4 被拾起
 */
stock void ProcessBombPickup()
{
    g_bIsC4Dropped    = false;
    g_vecDroppedC4Pos = NULL_VECTOR;
}
