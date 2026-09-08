#if defined _botquickgrenade_visuals_included
  #endinput
#endif
#define _botquickgrenade_visuals_included

//========================================================================================
// VISUAL RENDERING & LOGGING HELPERS
// 负责弹道激光绘制、落点圆圈渲染以及带颜色的聊天栏调试日志
//========================================================================================

/**
 * 向所有客户端绘制一段激光线段
 */
stock void DrawBeamSegment(const float pos[3], const float endPos[3], const int color[4], float life)
{
    if (g_iBeamSprite <= 0) return;

    TE_SetupBeamPoints(
        pos, endPos,
        g_iBeamSprite, 0,
        0, 0,
        life,
        BEAM_WIDTH, BEAM_WIDTH,
        0,
        0.0,
        color,
        0
    );
    TE_SendToAll(0.0);
}

/**
 * 以落点 center 为圆心、radius 为半径在地面水平渲染一个圆圈
 */
stock void DrawLandingCircle(const float center[3], float radius, const int color[4], float life)
{
    if (g_iBeamSprite <= 0) return;

    float step = (2.0 * 3.1415926535) / float(CIRCLE_SEGS);

    float prevPt[3], currPt[3];
    prevPt[0] = center[0] + radius;
    prevPt[1] = center[1];
    prevPt[2] = center[2] + 2.0; // 稍微抬高 2 单位防止与地面重叠裁剪

    for (int i = 1; i <= CIRCLE_SEGS; i++)
    {
        float angle = float(i) * step;
        currPt[0] = center[0] + radius * Cosine(angle);
        currPt[1] = center[1] + radius * Sine(angle);
        currPt[2] = center[2] + 2.0;

        DrawBeamSegment(prevPt, currPt, color, life);
        prevPt = currPt;
    }
}

/**
 * 检查真人客户端 viewer 是否正在观察目标 target（第一人称或第三人称视角）
 */
stock bool IsSpectatingTarget(int viewer, int target)
{
    if (!IsClientInGame(viewer) || IsFakeClient(viewer))
        return false;

    // 若真人活在场上
    if (IsPlayerAlive(viewer))
    {
        return (viewer == target);
    }

    // 真人处于观察者阵营或已阵亡，获取其当前观察的目标实体索引
    if (HasEntProp(viewer, Prop_Send, "m_hObserverTarget"))
    {
        int obsTarget = GetEntPropEnt(viewer, Prop_Send, "m_hObserverTarget");
        return (obsTarget == target);
    }

    return false;
}

/**
 * 格式化输出调试日志：仅向正在观察该 Bot 的真人玩家打印聊天栏信息
 *
 * @param bot      产生行为的 Bot 实体索引（若 <= 0 则为全服全局事件）
 * @param format   格式化字符串
 */
stock void LogDebug(int bot, const char[] format, any ...)
{
    if (!GetConVarBool(cvarDebug)) return;

    char buffer[256];
    VFormat(buffer, sizeof(buffer), format, 3);

    if (bot <= 0)
    {
        // 全局事件（如 C4 安放/掉落）：广播给所有真人玩家
        for (int i = 1; i <= MaxClients; i++)
        {
            if (IsClientInGame(i) && !IsFakeClient(i))
            {
                PrintToChat(i, "%s", buffer);
            }
        }
        return;
    }

    // 定点事件：仅发送给当前正在观察该 Bot 的真人玩家
    for (int i = 1; i <= MaxClients; i++)
    {
        if (IsSpectatingTarget(i, bot))
        {
            PrintToChat(i, "%s", buffer);
        }
    }
}

/**
 * 获取玩家当前背包中持有的各类手雷实际备弹数量
 */
stock void GetClientGrenadeCounts(int client, int &heCount, int &smokeCount, int &flashCount)
{
    heCount = 0;
    smokeCount = 0;
    flashCount = 0;

    for (int i = 0; i < 48; i++)
    {
        int wep = GetEntPropEnt(client, Prop_Send, "m_hMyWeapons", i);
        if (wep > 0 && IsValidEntity(wep))
        {
            // 只认弹药序号：同包里不会同时出现两颗同池雷（瓶顶替雷发，二选一），无需去重
            int role = QG_GetGrenadeRole(wep);
            if (role == GRENADE_ROLE_UNKNOWN)
                continue;

            int ammoType = GetEntProp(wep, Prop_Send, "m_iPrimaryAmmoType");
            int ammo = (ammoType >= 0) ? GetEntProp(client, Prop_Send, "m_iAmmo", _, ammoType) : 1;

            if (role == GRENADE_ROLE_HE)
            {
                heCount += ammo;
            }
            else if (role == GRENADE_ROLE_SMOKE)
            {
                smokeCount += ammo;
            }
            else if (role == GRENADE_ROLE_FLASH)
            {
                flashCount += ammo;
            }
        }
    }
}

/**
 * 实时更新正在观战 Bot 的真人玩家 HintText HUD
 */
public Action Timer_UpdateSpectatorHUD(Handle timer)
{
    if (!GetConVarBool(cvarDebug)) return Plugin_Continue;

    for (int i = 1; i <= MaxClients; i++)
    {
        if (!IsClientInGame(i) || IsFakeClient(i))
            continue;

        int target = -1;

        // 若真人玩家已阵亡或处于旁观者阵营，获取其当前观察的目标实体
        if (!IsPlayerAlive(i))
        {
            if (HasEntProp(i, Prop_Send, "m_hObserverTarget"))
            {
                target = GetEntPropEnt(i, Prop_Send, "m_hObserverTarget");
            }
        }

        if (target > 0 && target <= MaxClients && IsClientInGame(target) && IsPlayerAlive(target) && IsFakeClient(target))
        {
            int he, smoke, flash;
            GetClientGrenadeCounts(target, he, smoke, flash);

            float now = GetGameTime();
            float cdHe = (now - g_fLastHeThrowTime[target] >= HE_COOLDOWN) ? 0.0 : (HE_COOLDOWN - (now - g_fLastHeThrowTime[target]));
            float cdSmoke = (now - g_fLastSmokeThrowTime[target] >= SMOKE_COOLDOWN) ? 0.0 : (SMOKE_COOLDOWN - (now - g_fLastSmokeThrowTime[target]));
            float cdFlash = (now - g_fLastFlashThrowTime[target] >= FLASH_COOLDOWN) ? 0.0 : (FLASH_COOLDOWN - (now - g_fLastFlashThrowTime[target]));

            char hudText[256];
            Format(hudText, sizeof(hudText), "【观察 Bot: %N】\n高爆手雷 (HE): %d 颗 %s\n战术烟雾 (Smoke): %d 颗 %s\n战术闪光 (Flash): %d 颗 %s",
                target,
                he, (he <= 0 ? "(无)" : (cdHe > 0.0 ? "(冷却中)" : "[就绪]")),
                smoke, (smoke <= 0 ? "(无)" : (cdSmoke > 0.0 ? "(冷却中)" : "[就绪]")),
                flash, (flash <= 0 ? "(无)" : (cdFlash > 0.0 ? "(冷却中)" : "[就绪]"))
            );

            PrintHintText(i, "%s", hudText);
            StopSound(i, SNDCHAN_STATIC, "ui/hint.wav");
        }
    }

    return Plugin_Continue;
}
