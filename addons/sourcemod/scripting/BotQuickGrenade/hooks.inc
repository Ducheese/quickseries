#if defined _botquickgrenade_hooks_included
  #endinput
#endif
#define _botquickgrenade_hooks_included

//========================================================================================
// HOOKS & TIMERS
// 负责事件监听、声音钩子、按键指令覆写与主循环定时器
//========================================================================================

stock void RegisterBotQuickGrenadeHooks()
{
    HookEvent("player_spawn", Event_PlayerSpawn);
    HookEvent("player_death", Event_PlayerDeath);
    HookEvent("player_hurt", Event_PlayerHurt);
    HookEvent("weapon_fire", Event_WeaponFire);

    HookEvent("round_start", Event_RoundStart, EventHookMode_PostNoCopy);
    HookEvent("round_freeze_end", Event_RoundStart, EventHookMode_PostNoCopy);
    HookEvent("bomb_planted", Event_BombPlanted);
    HookEvent("bomb_defused", Event_BombDefused, EventHookMode_PostNoCopy);
    HookEvent("bomb_exploded", Event_BombDefused, EventHookMode_PostNoCopy);
    HookEvent("bomb_dropped", Event_BombDropped);
    HookEvent("bomb_pickup", Event_BombPickup);

    AddNormalSoundHook(Hook_NormalSound);

    CreateTimer(BOT_THINK_INTERVAL, Timer_BotThink, _, TIMER_REPEAT | TIMER_FLAG_NO_MAPCHANGE);
}

stock void UnregisterBotQuickGrenadeHooks()
{
    RemoveNormalSoundHook(Hook_NormalSound);
}

/**
 * 检查当前是否有任意一种手雷 AI 处于启用状态（用于感知输入层门禁）
 */
stock bool IsAnyGrenadeEnabled()
{
    return (GetConVarBool(cvarHeEnable) || GetConVarBool(cvarSmokeEnable));
}

//========================================================================================
// 声音钩子：检测敌人脚步声
//========================================================================================

public Action Hook_NormalSound(int clients[64], int &numClients, char sample[PLATFORM_MAX_PATH],
    int &entity, int &channel, float &volume, int &level, int &pitch, int &flags)
{
    if (!IsAnyGrenadeEnabled()) return Plugin_Continue;

    if (entity < 1 || entity > MaxClients || !IsClientInGame(entity) || !IsPlayerAlive(entity))
        return Plugin_Continue;

    // 过滤脚步声特征关键字
    if (StrContains(sample, "footsteps", false) != -1 || StrContains(sample, "player/footstep", false) != -1)
    {
        ProcessFootstep(entity);
    }

    return Plugin_Continue;
}

//========================================================================================
// 游戏事件
//========================================================================================

public void Event_RoundStart(Handle event, const char[] name, bool dontBroadcast)
{
    ResetC4State();
    ClearAllMemory();
}

public void Event_BombPlanted(Handle event, const char[] name, bool dontBroadcast)
{
    float pos[3];
    pos[0] = GetEventFloat(event, "x");
    pos[1] = GetEventFloat(event, "y");
    pos[2] = GetEventFloat(event, "z");

    if (pos[0] == 0.0 && pos[1] == 0.0 && pos[2] == 0.0)
    {
        int bombEnt = FindEntityByClassname(-1, "planted_c4");
        if (bombEnt > 0 && IsValidEntity(bombEnt))
        {
            GetEntPropVector(bombEnt, Prop_Data, "m_vecAbsOrigin", pos);
        }
    }

    ProcessBombPlanted(pos);
    LogDebug("\x04[BotQG - C4]\x01 C4 已安放于坐标 (%.0f, %.0f, %.0f)", pos[0], pos[1], pos[2]);
}

public void Event_BombDefused(Handle event, const char[] name, bool dontBroadcast)
{
    ResetC4State();
}

public void Event_BombDropped(Handle event, const char[] name, bool dontBroadcast)
{
    int ent = GetEventInt(event, "entindex");
    float pos[3];
    if (ent > 0 && IsValidEntity(ent))
    {
        GetEntPropVector(ent, Prop_Data, "m_vecAbsOrigin", pos);
    }
    else
    {
        int planter = GetClientOfUserId(GetEventInt(event, "userid"));
        if (planter > 0 && IsClientInGame(planter))
        {
            GetClientAbsOrigin(planter, pos);
        }
    }

    ProcessBombDropped(pos);
    LogDebug("\x04[BotQG - C4]\x01 C4 已掉落在地 (%.0f, %.0f, %.0f)", pos[0], pos[1], pos[2]);
}

public void Event_BombPickup(Handle event, const char[] name, bool dontBroadcast)
{
    ProcessBombPickup();
    LogDebug("\x04[BotQG - C4]\x01 掉落的 C4 已被拾起");
}

public void Event_PlayerSpawn(Handle event, const char[] name, bool dontBroadcast)
{
    int client = GetClientOfUserId(GetEventInt(event, "userid"));

    if (client < 1 || client > MaxClients || !IsClientInGame(client)) return;

    if (IsFakeClient(client) && GetConVarBool(cvarGiveGrenades))
    {
        // 延迟 0.1 秒等武器槽初始化完成再发雷
        CreateTimer(0.1, Timer_GiveBotGrenades, client);
    }
}

public void Event_PlayerDeath(Handle event, const char[] name, bool dontBroadcast)
{
    int victim = GetClientOfUserId(GetEventInt(event, "userid"));
    if (victim >= 1 && victim <= MaxClients)
    {
        // 双向十字清理
        ClearPlayerMemory(victim);
    }
}

public void Event_PlayerHurt(Handle event, const char[] name, bool dontBroadcast)
{
    if (!IsAnyGrenadeEnabled()) return;

    int victim = GetClientOfUserId(GetEventInt(event, "userid"));
    int attacker = GetClientOfUserId(GetEventInt(event, "attacker"));

    ProcessHurt(victim, attacker);
}

public void Event_WeaponFire(Handle event, const char[] name, bool dontBroadcast)
{
    if (!IsAnyGrenadeEnabled()) return;

    int shooter = GetClientOfUserId(GetEventInt(event, "userid"));
    ProcessGunfire(shooter);
}

//========================================================================================
// 补给辅助：Bot 出生自动发雷定时器（1 HE + 1 Smoke + 1 Flash）
//========================================================================================

public Action Timer_GiveBotGrenades(Handle timer, int client)
{
    if (!IsClientInGame(client) || !IsPlayerAlive(client))
        return Plugin_Continue;

    static const char grenades[][] = {
        "weapon_hegrenade",
        "weapon_flashbang",
        "weapon_smokegrenade"
    };

    for (int i = 0; i < sizeof(grenades); i++)
    {
        int wep = GivePlayerItem(client, grenades[i]);
        if (wep <= 0 || !IsValidEntity(wep))
        {
            // 若身上已存在该类名手雷，遍历背包获取其实体
            for (int w = 0; w < 48; w++)
            {
                int myWep = GetEntPropEnt(client, Prop_Send, "m_hMyWeapons", w);
                if (myWep > 0 && IsValidEntity(myWep))
                {
                    char cls[32];
                    GetEntityClassname(myWep, cls, sizeof(cls));
                    if (StrEqual(cls, grenades[i], false))
                    {
                        wep = myWep;
                        break;
                    }
                }
            }
        }

        if (wep > 0 && IsValidEntity(wep))
        {
            int ammoType = GetEntProp(wep, Prop_Send, "m_iPrimaryAmmoType");
            if (ammoType >= 0)
            {
                SetEntProp(client, Prop_Send, "m_iAmmo", 1, _, ammoType);
            }
        }
    }

    return Plugin_Continue;
}

//========================================================================================
// 朝向覆写：在投雷期间强制 Bot 视线与物理身体锁定在计算好的仰角
//========================================================================================

public Action OnPlayerRunCmd(int client, int &buttons, int &impulse,
    float vel[3], float angles[3], int &weapon,
    int &subtype, int &cmdnum, int &tickcount, int &seed, int mouse[2])
{
    // 真人玩家捏雷（按住左键/右键拔栓瞄准）时，实时预览物理弹道
    if (!IsFakeClient(client) && IsPlayerAlive(client))
    {
        if (cvarPlayerPreview != null && GetConVarBool(cvarPlayerPreview))
        {
            if ((buttons & IN_ATTACK) || (buttons & IN_ATTACK2))
            {
                int activeWep = GetEntPropEnt(client, Prop_Send, "m_hActiveWeapon");
                if (activeWep > 0 && IsValidEntity(activeWep))
                {
                    char wepCls[32];
                    GetEntityClassname(activeWep, wepCls, sizeof(wepCls));

                    int color[4];
                    float hitRadius = 0.0;
                    bool isGrenade = false;

                    if (StrEqual(wepCls, "weapon_hegrenade", false))
                    {
                        color = COLOR_HE;
                        hitRadius = HE_HIT_RADIUS;
                        isGrenade = true;
                    }
                    else if (StrEqual(wepCls, "weapon_smokegrenade", false))
                    {
                        color = COLOR_SMOKE;
                        hitRadius = SMOKE_HIT_RADIUS;
                        isGrenade = true;
                    }
                    else if (StrEqual(wepCls, "weapon_flashbang", false))
                    {
                        color = COLOR_FLASH;
                        hitRadius = FLASH_HIT_RADIUS;
                        isGrenade = true;
                    }

                    if (isGrenade)
                    {
                        float now = GetGameTime();
                        static float s_fLastShowTime[MAXPLAYERS + 1];

                        if (now - s_fLastShowTime[client] >= 0.05)
                        {
                            s_fLastShowTime[client] = now;

                            float eyeAng[3], landPos[3];
                            GetClientEyeAngles(client, eyeAng);

                            // 实时渲染 0.1 秒动态弹道与落点圆，随准心平滑移动
                            SimulateGrenadeTrajectoryWithAngles(client, eyeAng, landPos, true, color, 0.1);
                            DrawLandingCircle(landPos, hitRadius, color, 0.1);
                        }
                    }
                }
            }
        }
        return Plugin_Continue;
    }

    if (!IsFakeClient(client))         return Plugin_Continue;
    if (!g_bAngleOverride[client])     return Plugin_Continue;

    if (GetGameTime() > g_fOverrideExpiry[client])
    {
        g_bAngleOverride[client] = false;
        return Plugin_Continue;
    }

    // 压制 Bot 原生 AI 视线争夺
    angles[0] = g_fOverrideAngles[client][0];
    angles[1] = g_fOverrideAngles[client][1];

    // 强行扭转 Bot 实体物理朝向与网络同步眼位角度（三者缺一不可）
    TeleportEntity(client, NULL_VECTOR, g_fOverrideAngles[client], NULL_VECTOR);
    SetEntPropFloat(client, Prop_Send, "m_angEyeAngles[0]", g_fOverrideAngles[client][0]);
    SetEntPropFloat(client, Prop_Send, "m_angEyeAngles[1]", g_fOverrideAngles[client][1]);

    return Plugin_Changed;
}

/**
 * 统一执行 Bot 投雷动作：物理身体转向、视线锁定与快速投雷触发
 */
stock bool ExecuteBotThrow(int bot, const char[] weaponClass, const float angles[3], float now, float &lastThrowTime)
{
    g_bAngleOverride[bot]     = true;
    g_fOverrideAngles[bot][0] = angles[0];
    g_fOverrideAngles[bot][1] = angles[1];
    g_fOverrideAngles[bot][2] = 0.0;
    g_fOverrideExpiry[bot]    = now + ANGLE_OVERRIDE_DUR;

    TeleportEntity(bot, NULL_VECTOR, g_fOverrideAngles[bot], NULL_VECTOR);
    SetEntPropFloat(bot, Prop_Send, "m_angEyeAngles[0]", angles[0]);
    SetEntPropFloat(bot, Prop_Send, "m_angEyeAngles[1]", angles[1]);

    if (GetFeatureStatus(FeatureType_Native, "QuickGrenade_Trigger") == FeatureStatus_Available)
    {
        if (QuickGrenade_Trigger(bot, weaponClass, false))
        {
            lastThrowTime = now;
            return true;
        }
    }

    return false;
}

//========================================================================================
// 主思考循环：高爆手雷 (HE) 与 智能战术烟雾弹 (Smoke)
//========================================================================================

public Action Timer_BotThink(Handle timer)
{
    if (!IsAnyGrenadeEnabled()) return Plugin_Continue;

    bool heEnabled    = GetConVarBool(cvarHeEnable);
    bool smokeEnabled = GetConVarBool(cvarSmokeEnable);
    bool visualize    = GetConVarBool(cvarDebug);
    float now         = GetGameTime();

    static int s_iThinkTick = 0;
    s_iThinkTick++;

    for (int bot = 1; bot <= MaxClients; bot++)
    {
        if (!IsClientInGame(bot) || !IsFakeClient(bot) || !IsPlayerAlive(bot))
            continue;

        // 错峰分片轮询：将全服 Bot 平摊至 5 帧周期内执行，平滑 CPU 射线计算负载
        if ((bot % 5) != (s_iThinkTick % 5))
            continue;

        // 1. 刷新视觉感知
        UpdateBotVision(bot);

        // 2. 跨插件防冲突：若正在执行快速手雷/近战动作，不重复触发
        if (GetFeatureStatus(FeatureType_Native, "QuickGrenade_IsCombat") == FeatureStatus_Available
            && QuickGrenade_IsCombat(bot))
            continue;

        // 3. 核心动作防自打断：若正在按 E 拆包或正在插雷管下包，严禁触发任何投雷动作
        if (IsBotBusyWithC4(bot))
            continue;

        // 动作互斥锁：防止同 Tick 连续抛出多种手雷
        bool threwGrenade = false;

        //================================================================================
        // 支线 A：智能战术烟雾弹 (Smoke Grenade) —— 三级战术掩护（优先级最高）
        //================================================================================
        if (!threwGrenade && smokeEnabled
            && (now - g_fLastSmokeThrowTime[bot] >= SMOKE_COOLDOWN)
            && BotHasGrenade(bot, "weapon_smokegrenade"))
        {
            float smokeAngles[3], smokeLandPos[3];
            char smokeReason[128];
            bool isC4Event = false;

            if (FindSmokeTacticalTarget(bot, smokeAngles, smokeLandPos, smokeReason, sizeof(smokeReason), isC4Event)
                && ExecuteBotThrow(bot, "weapon_smokegrenade", smokeAngles, now, g_fLastSmokeThrowTime[bot]))
            {
                threwGrenade = true;

                // 若本次封烟属于 C4 战术事件，刷新本阵营共享封烟时间戳（同队其他队友在冷却期内不重复封烟）
                if (isC4Event)
                {
                    int botTeam = GetClientTeam(bot);
                    g_fLastTeamC4SmokeTime[botTeam] = now;
                }

                if (visualize)
                {
                    SimulateGrenadeTrajectoryWithAngles(bot, smokeAngles, smokeLandPos, true, COLOR_SMOKE);
                    DrawLandingCircle(smokeLandPos, SMOKE_HIT_RADIUS, COLOR_SMOKE, BEAM_LIFE);
                }

                LogDebug("\x04[BotQG - 烟雾]\x01 Bot \x03%N\x01 触发战术封烟! (原因: \x05%s\x01, Pitch: \x04%.1f°\x01, Yaw: \x04%.1f°\x01)",
                    bot, smokeReason, smokeAngles[0], smokeAngles[1]);
            }
        }

        //================================================================================
        // 支线 B：高爆手雷 (HE Grenade) —— 掩体高抛
        //================================================================================
        if (!threwGrenade && heEnabled
            && (now - g_fLastHeThrowTime[bot] >= HE_COOLDOWN)
            && BotHasGrenade(bot, "weapon_hegrenade"))
        {
            float throwAngles[3], landPos[3];
            int target = FindTargetAndComputeAngles(bot, throwAngles, landPos);

            if (target > 0 && ExecuteBotThrow(bot, "weapon_hegrenade", throwAngles, now, g_fLastHeThrowTime[bot]))
            {
                threwGrenade = true;

                if (visualize)
                {
                    SimulateGrenadeTrajectoryWithAngles(bot, throwAngles, landPos, true, COLOR_HE);
                    DrawLandingCircle(landPos, HE_HIT_RADIUS, COLOR_HE, BEAM_LIFE);
                }

                LogDebug("\x04[BotQG - 投掷]\x01 Bot \x03%N\x01 朝掩体后 \x05%N\x01 触发高抛砸雷! (Pitch: \x04%.1f°\x01, Yaw: \x04%.1f°\x01)",
                    bot, target, throwAngles[0], throwAngles[1]);
            }
        }
    }

    return Plugin_Continue;
}
