//========================================================================================
// BotQuickGrenade_Demo
// Bot 自动投掷快速手雷 —— 演示版（主动仰角计算版）
//
// 核心设计：
//   不再依赖 bot 当前朝向，而是主动计算"要打中敌人需要的仰角"：
//   1. 找到候选目标（距离 DIST_MIN ~ DIST_MAX 的敌人）
//   2. 计算 bot 到目标的水平偏航角（yaw）
//   3. 对 pitch 范围做数值搜索（粗搜 + 细搜），找出落点最接近目标的仰角
//   4. 若最优落点在 HE_HIT_RADIUS 范围内，覆盖 bot 的朝向，触发投雷
//
// 可视化（sm_botqg_debug 1 时）：
//   - 橙色分段 Beam：计算出的最优弹道曲线
//   - 绿圈（命中）/ 红圈（最优落点仍打不到人）：落点判定圆
//
// 测试辅助：
//   - sm_botqg_bullet_scale：所有子弹伤害倍率（默认 0.1）
//   - sm_botqg_bot_grenades：bot 出生手雷备弹（默认 10）
//
// 依赖：QuickGrenade（quickgrenade.smx）
//========================================================================================

#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <sdktools>
#include <sdkhooks>
#include <cstrike>

#undef REQUIRE_PLUGIN
#include <quickgrenade>
#define REQUIRE_PLUGIN

//----------------------------------------------------------------------------------------
// 参数常量
//----------------------------------------------------------------------------------------

#define HE_HIT_RADIUS       160.0   // HE 落点命中判定半径（单位）
#define DIST_MIN            220.0   // bot 与目标最近有效投雷距离
#define DIST_MAX            900.0   // bot 与目标最远有效投雷距离
#define GRENADE_FLIGHT_TIME 2.5     // HE 弹道最长模拟时间（秒）
#define SIM_TIMESTEP        0.05    // 弹道模拟时间步长（秒）
#define BOT_THINK_INTERVAL  1.0     // bot AI 检查间隔（秒）
#define GRENADE_COOLDOWN    12.0    // 单 bot 投雷冷却（秒）

// pitch 搜索范围（Source 引擎角度：负=仰角 正=俯角）
#define PITCH_MIN          -80.0    // 最大仰角（往高处投）
#define PITCH_MAX           60.0    // 最大俯角（往低处投，太大就投脚边）
#define PITCH_STEP_COARSE    7.0    // 粗搜步长（°）
#define PITCH_STEP_FINE      1.0    // 细搜步长（°），在粗搜最优结果附近细化

// 朝向覆盖持续时间（秒）：需覆盖整个引雷 + 投掷过程
// QuickGrenade autoThrowTime(~0.25) + throwDelay(~0.6) + 余量
#define ANGLE_OVERRIDE_DUR  1.5

// 落点判定圆的分段数
#define CIRCLE_SEGS         24

// Beam 宽度 / 显示时长（略大于 BOT_THINK_INTERVAL，避免闪烁）
#define BEAM_WIDTH          1.5
#define BEAM_LIFE           1.5

// 弹道 HULL 半尺寸（与引擎 CBaseCSGrenade 一致）
static const float SIM_MINS[3] = {-2.0, -2.0, -2.0};
static const float SIM_MAXS[3] = { 2.0,  2.0,  2.0};

// Beam 颜色（RGBA）
static const int COLOR_TRAJ[4] = {255, 160,   0, 220};  // 橙色弹道
static const int COLOR_HIT[4]  = {  0, 220,  60, 220};  // 绿色命中圆
static const int COLOR_MISS[4] = {255,  40,  40, 220};  // 红色未命中圆

// 测试辅助
#define BOT_GRENADE_AMMO    10  // bot 出生手雷备弹

//----------------------------------------------------------------------------------------
// 全局变量
//----------------------------------------------------------------------------------------

ConVar g_cvGravity;
ConVar g_cvDebug;
ConVar g_cvBulletScale;
ConVar g_cvBotGrenades;

int   g_iBeamSprite;
float g_fLastThrowTime[MAXPLAYERS + 1];

// 朝向覆盖（投雷期间强制 bot 保持计算好的仰角）
bool  g_bAngleOverride[MAXPLAYERS + 1]      = {false};
float g_fOverrideAngles[MAXPLAYERS + 1][3];
float g_fOverrideExpiry[MAXPLAYERS + 1]     = {0.0};

//========================================================================================
// PLUGIN INFO
//========================================================================================

public Plugin myinfo =
{
    name        = "BotQuickGrenade Demo",
    author      = "Ducheese",
    description = "Bot auto HE with active pitch search + QuickGrenade (demo)",
    version     = "1.0",
    url         = ""
};

//========================================================================================
// LIFECYCLE
//========================================================================================

public void OnPluginStart()
{
    g_cvGravity     = FindConVar("sv_gravity");
    g_cvDebug       = CreateConVar("sm_botqg_debug", "1",
        "调试可视化：1=显示弹道+落点圆圈，0=关闭", FCVAR_NOTIFY,
        true, 0.0, true, 1.0);
    g_cvBulletScale = CreateConVar("sm_botqg_bullet_scale", "0.1",
        "测试用：子弹伤害倍率（0.1=10%，1.0=正常）", FCVAR_NOTIFY,
        true, 0.0, true, 1.0);
    g_cvBotGrenades = CreateConVar("sm_botqg_bot_grenades", "10",
        "测试用：bot 出生时每种手雷的备弹数量", FCVAR_NOTIFY,
        true, 0.0, true, 50.0);

    HookEvent("player_spawn", Event_PlayerSpawn);
    CreateTimer(BOT_THINK_INTERVAL, Timer_BotThink, _, TIMER_REPEAT | TIMER_FLAG_NO_MAPCHANGE);
}

public void OnClientPutInServer(int client)
{
    SDKHook(client, SDKHook_OnTakeDamage, OnTakeDamage);
}

public void OnMapStart()
{
    g_iBeamSprite = PrecacheModel("sprites/laserbeam.spr");

    for (int i = 1; i <= MaxClients; i++)
    {
        g_fLastThrowTime[i]  = 0.0;
        g_bAngleOverride[i]  = false;
        g_fOverrideExpiry[i] = 0.0;
    }
}

public void OnClientDisconnect(int client)
{
    g_fLastThrowTime[client]  = 0.0;
    g_bAngleOverride[client]  = false;
}

//========================================================================================
// 测试辅助：子弹伤害缩放
//========================================================================================

public Action OnTakeDamage(int victim, int &attacker, int &inflictor,
    float &damage, int &damagetype)
{
    if (!(damagetype & DMG_BULLET))
        return Plugin_Continue;

    float scale = g_cvBulletScale.FloatValue;
    if (scale == 1.0)
        return Plugin_Continue;

    damage *= scale;
    return Plugin_Changed;
}

//========================================================================================
// 测试辅助：bot 出生给满手雷
//========================================================================================

public void Event_PlayerSpawn(Handle event, const char[] name, bool dontBroadcast)
{
    int client = GetClientOfUserId(GetEventInt(event, "userid"));
    if (client < 1 || client > MaxClients || !IsClientInGame(client)) return;
    if (!IsFakeClient(client)) return;

    CreateTimer(0.1, Timer_GiveBotGrenades, client);
}

public Action Timer_GiveBotGrenades(Handle timer, int client)
{
    if (!IsClientInGame(client) || !IsPlayerAlive(client))
        return Plugin_Continue;

    int ammo = g_cvBotGrenades.IntValue;

    static const char grenades[][] = {
        "weapon_hegrenade",
        "weapon_flashbang",
        "weapon_smokegrenade"
    };

    for (int i = 0; i < sizeof(grenades); i++)
    {
        GivePlayerItem(client, grenades[i]);

        for (int slot = 3; slot <= 7; slot++)
        {
            int wep = GetPlayerWeaponSlot(client, slot);
            if (wep <= 0 || !IsValidEntity(wep)) continue;

            char cls[32];
            GetEntityClassname(wep, cls, sizeof(cls));
            if (!StrEqual(cls, grenades[i], false)) continue;

            int ammoType = GetEntProp(wep, Prop_Send, "m_iPrimaryAmmoType");
            if (ammoType >= 0)
                SetEntProp(client, Prop_Send, "m_iAmmo", ammo, _, ammoType);
            break;
        }
    }

    return Plugin_Continue;
}

//========================================================================================
// 朝向覆盖：在投雷期间强制 bot 保持计算好的仰角
// OnPlayerRunCmd 对所有客户端调用；仅对有覆盖标记的 bot 生效。
// 注意：在 Source 引擎中，对 FakeClient (Bot) 仅修改 angles 参数只影响 usercmd，
// 必须同时调用 TeleportEntity 和设置 m_angEyeAngles 才能真正扭转 Bot 的实体朝向与视线！
//========================================================================================

public Action OnPlayerRunCmd(int client, int &buttons, int &impulse,
    float vel[3], float angles[3], int &weapon,
    int &subtype, int &cmdnum, int &tickcount, int &seed, int mouse[2])
{
    if (!IsFakeClient(client))         return Plugin_Continue;
    if (!g_bAngleOverride[client])     return Plugin_Continue;

    if (GetGameTime() > g_fOverrideExpiry[client])
    {
        g_bAngleOverride[client] = false;
        return Plugin_Continue;
    }

    // 强制 pitch / yaw，roll 不动
    angles[0] = g_fOverrideAngles[client][0];
    angles[1] = g_fOverrideAngles[client][1];

    // 强制扭转 Bot 实体的物理朝向与网络同步眼位角度
    TeleportEntity(client, NULL_VECTOR, g_fOverrideAngles[client], NULL_VECTOR);
    SetEntPropFloat(client, Prop_Send, "m_angEyeAngles[0]", g_fOverrideAngles[client][0]);
    SetEntPropFloat(client, Prop_Send, "m_angEyeAngles[1]", g_fOverrideAngles[client][1]);

    return Plugin_Changed;
}

//========================================================================================
// BOT THINK TIMER
//========================================================================================

public Action Timer_BotThink(Handle timer)
{
    bool debug = g_cvDebug.BoolValue;

    for (int bot = 1; bot <= MaxClients; bot++)
    {
        if (!IsClientInGame(bot) || !IsFakeClient(bot) || !IsPlayerAlive(bot))
            continue;

        if (GetGameTime() - g_fLastThrowTime[bot] < GRENADE_COOLDOWN)
            continue;

        if (GetFeatureStatus(FeatureType_Native, "QuickGrenade_IsCombat") == FeatureStatus_Available
            && QuickGrenade_IsCombat(bot))
            continue;

        if (!BotHasGrenade(bot, "weapon_hegrenade"))
            continue;

        // 主动计算仰角，找到能打中的目标
        float throwAngles[3], landPos[3];
        int target = FindTargetAndComputeAngles(bot, throwAngles, landPos, debug);

        if (target > 0)
        {
            // 覆盖 bot 朝向角度，触发投雷
            g_bAngleOverride[bot]     = true;
            g_fOverrideAngles[bot][0] = throwAngles[0];
            g_fOverrideAngles[bot][1] = throwAngles[1];
            g_fOverrideAngles[bot][2] = 0.0;
            g_fOverrideExpiry[bot]    = GetGameTime() + ANGLE_OVERRIDE_DUR;

            // 触发瞬间立即硬切一次朝向
            TeleportEntity(bot, NULL_VECTOR, g_fOverrideAngles[bot], NULL_VECTOR);
            SetEntPropFloat(bot, Prop_Send, "m_angEyeAngles[0]", throwAngles[0]);
            SetEntPropFloat(bot, Prop_Send, "m_angEyeAngles[1]", throwAngles[1]);

            if (GetFeatureStatus(FeatureType_Native, "QuickGrenade_Trigger") == FeatureStatus_Available)
            {
                if (QuickGrenade_Trigger(bot, "weapon_hegrenade", false))
                    g_fLastThrowTime[bot] = GetGameTime();
            }
        }
    }

    return Plugin_Continue;
}

//========================================================================================
// FindTargetAndComputeAngles
//
// 遍历候选敌人，对每个目标调用 FindBestPitchForTarget 求最优仰角。
// 找到能落在 HE_HIT_RADIUS 范围内的目标后立即返回。
//
// throwAngles[3]：输出最优投掷角（pitch, yaw, 0）
// landPos[3]    ：输出最优落点（用于可视化）
// 返回目标 client index；-1 表示无可行目标
//========================================================================================

int FindTargetAndComputeAngles(int bot, float throwAngles[3], float landPos[3], bool visualize)
{
    int botTeam = GetClientTeam(bot);

    float botEye[3];
    GetClientEyePosition(bot, botEye);

    for (int t = 1; t <= MaxClients; t++)
    {
        if (!IsClientInGame(t) || !IsPlayerAlive(t)) continue;
        if (GetClientTeam(t) == botTeam)             continue;

        float tp[3];
        GetClientAbsOrigin(t, tp);

        float dist = GetVectorDistance(botEye, tp);
        if (dist < DIST_MIN || dist > DIST_MAX) continue;

        // 计算水平偏航角（bot → 目标）
        float dx  = tp[0] - botEye[0];
        float dy  = tp[1] - botEye[1];
        float yaw = RadToDeg(ArcTangent2(dy, dx));

        // 数值搜索最优仰角
        float bestPitch;
        bool  hit = FindBestPitchForTarget(bot, tp, yaw, bestPitch, landPos);

        if (hit)
        {
            throwAngles[0] = bestPitch;
            throwAngles[1] = yaw;
            throwAngles[2] = 0.0;

            // 可视化：画出计算好的最优弹道 + 绿圈
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

//========================================================================================
// FindBestPitchForTarget
//
// 对给定目标位置和水平偏航，在 [PITCH_MIN, PITCH_MAX] 范围内搜索最优仰角。
// 先粗搜（步长 PITCH_STEP_COARSE），再在最优结果附近细搜（步长 PITCH_STEP_FINE）。
// 若最优落点在 HE_HIT_RADIUS 范围内，返回 true 并填写 bestPitch 和 landPos。
//========================================================================================

bool FindBestPitchForTarget(int bot, const float targetPos[3], float yaw,
    float &bestPitch, float landPos[3])
{
    float bestDist = 9999999.0;
    bestPitch      = 0.0;

    // --- 粗搜 ---
    float coarseBest = 0.0;
    for (float p = PITCH_MIN; p <= PITCH_MAX; p += PITCH_STEP_COARSE)
    {
        float angles[3];
        angles[0] = p;
        angles[1] = yaw;
        angles[2] = 0.0;

        float tLand[3];
        SimulateGrenadeTrajectoryWithAngles(bot, angles, tLand, false);

        float d = GetVectorDistance(tLand, targetPos);
        if (d < bestDist)
        {
            bestDist  = d;
            coarseBest = p;
            landPos   = tLand;
        }
    }

    // --- 细搜：在粗搜最优结果的 ±PITCH_STEP_COARSE 范围内精化 ---
    float fineMin = coarseBest - PITCH_STEP_COARSE;
    float fineMax = coarseBest + PITCH_STEP_COARSE;
    if (fineMin < PITCH_MIN) fineMin = PITCH_MIN;
    if (fineMax > PITCH_MAX) fineMax = PITCH_MAX;

    for (float p = fineMin; p <= fineMax; p += PITCH_STEP_FINE)
    {
        float angles[3];
        angles[0] = p;
        angles[1] = yaw;
        angles[2] = 0.0;

        float tLand[3];
        SimulateGrenadeTrajectoryWithAngles(bot, angles, tLand, false);

        float d = GetVectorDistance(tLand, targetPos);
        if (d < bestDist)
        {
            bestDist = d;
            bestPitch = p;
            landPos  = tLand;
        }
    }

    return (bestDist <= HE_HIT_RADIUS);
}

//========================================================================================
// SimulateGrenadeTrajectoryWithAngles
//
// 与原版弹道模拟相同的物理，但接受显式角度参数而不是读取 bot 当前朝向。
// 这样可以用任意 (pitch, yaw) 预测弹道，而无需改动 bot 的实际视角。
//
// 物理来源：grenade_trajectory.sp（Psycheat） + CS:S 引擎源码
//   初速大小：(90° - pitch) × 6.0，上限 750
//   引力系数：0.4 × sv_gravity
//   竖直积分：梯形法
//   碰撞检测：HULL ±2，首次碰撞即停（不模拟反弹）
//========================================================================================

void SimulateGrenadeTrajectoryWithAngles(int bot, float angles[3],
    float landPos[3], bool visualize)
{
    float pitch      = angles[0];
    float throwSpeed = (90.0 - pitch) * 6.0;
    if (throwSpeed > 750.0) throwSpeed = 750.0;

    // grenade_trajectory.sp 的俯仰角修正
    float correctedAngles[3];
    correctedAngles[0] = -10.0 + pitch + FloatAbs(pitch) * 10.0 / 90.0;
    correctedAngles[1] = angles[1];
    correctedAngles[2] = angles[2];

    float fwd[3], right[3], up[3];
    GetAngleVectors(correctedAngles, fwd, right, up);
    NormalizeVector(fwd, fwd);

    // 起点：眼位前方 16 单位
    float pos[3];
    GetClientEyePosition(bot, pos);
    pos[0] += fwd[0] * 16.0;
    pos[1] += fwd[1] * 16.0;
    pos[2] += fwd[2] * 16.0;

    // 初速 = 方向 × 速度 + 玩家惯性
    float playerVel[3], vel[3];
    GetEntPropVector(bot, Prop_Data, "m_vecAbsVelocity", playerVel);
    vel[0] = fwd[0] * throwSpeed + playerVel[0];
    vel[1] = fwd[1] * throwSpeed + playerVel[1];
    vel[2] = fwd[2] * throwSpeed + playerVel[2];

    float gForce = 0.4 * float(g_cvGravity.IntValue);
    float dt     = SIM_TIMESTEP;

    for (float t = 0.0; t <= GRENADE_FLIGHT_TIME; t += dt)
    {
        float nextPos[3];
        nextPos[0] = pos[0] + vel[0] * dt;
        nextPos[1] = pos[1] + vel[1] * dt;

        float newVz = vel[2] - gForce * dt;
        nextPos[2]  = pos[2] + (vel[2] + newVz) * 0.5 * dt;
        vel[2]      = newVz;

        Handle ray  = TR_TraceHullEx(pos, nextPos, SIM_MINS, SIM_MAXS, MASK_SHOT_HULL);
        float  frac = TR_GetFraction(ray);

        if (frac != 1.0)
        {
            if (t == 0.0 && TR_GetEntityIndex(ray) == bot)
            {
                CloseHandle(ray);
                pos = nextPos;
                continue;
            }

            TR_GetEndPosition(landPos, ray);

            if (visualize)
                DrawBeamSegment(pos, landPos, COLOR_TRAJ, BEAM_LIFE);

            CloseHandle(ray);
            return;
        }

        if (visualize)
            DrawBeamSegment(pos, nextPos, COLOR_TRAJ, BEAM_LIFE);

        CloseHandle(ray);
        pos = nextPos;
    }

    landPos = pos;
}

//========================================================================================
// DrawBeamSegment
//========================================================================================

void DrawBeamSegment(const float pos[3], const float endPos[3], const int color[4], float life)
{
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

//========================================================================================
// DrawLandingCircle
// 以落点为圆心、radius 为半径绘制水平圆圈（抬高 +2 单位避免地面裁剪）
//========================================================================================

void DrawLandingCircle(const float center[3], float radius, const int color[4], float life)
{
    float step = (2.0 * 3.14159265) / float(CIRCLE_SEGS);

    float prevPt[3], currPt[3];
    prevPt[0] = center[0] + radius;
    prevPt[1] = center[1];
    prevPt[2] = center[2] + 2.0;

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

//========================================================================================
// BotHasGrenade
//========================================================================================

bool BotHasGrenade(int client, const char[] classname)
{
    for (int slot = 3; slot <= 7; slot++)
    {
        int wep = GetPlayerWeaponSlot(client, slot);
        if (wep <= 0 || !IsValidEntity(wep)) continue;

        char cls[32];
        GetEntityClassname(wep, cls, sizeof(cls));
        if (StrEqual(cls, classname, false))
            return true;
    }
    return false;
}