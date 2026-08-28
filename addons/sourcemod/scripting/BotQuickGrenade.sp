//========================================================================================
// BotQuickGrenade - Bot 快速手雷战术 AI 插件 (QuickSeries 配套扩展)
//
// 模块化架构：
//   - BotQuickGrenade/global.inc      : 全局常量、宏定义、Handles 与状态数组
//   - BotQuickGrenade/visuals.inc     : 弹道光束、落点判定圆与带颜色调试日志
//   - BotQuickGrenade/trajectory.inc  : 物理弹道模拟、积分计算与两段式仰角搜索
//   - BotQuickGrenade/perception.inc  : 视锥角度、视线检测、脚步声/枪声/消音感知与记忆衰减
//   - BotQuickGrenade/tactics.inc     : 直连枪线检测、掩体战术限制、队友防误伤安全检查
//   - BotQuickGrenade/commands.inc    : ConVar 注册与配置文件自动生成
//   - BotQuickGrenade/hooks.inc       : 声音钩子、事件监听、按键朝向覆写与主循环
//========================================================================================

#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <sdktools>
#include <sdkhooks>
#include <cstrike>

// 跨插件调用 QuickGrenade（必须依赖）
#undef REQUIRE_PLUGIN
#include <quickgrenade>
#define REQUIRE_PLUGIN

#include "BotQuickGrenade/global"
#include "BotQuickGrenade/visuals"
#include "BotQuickGrenade/trajectory"
#include "BotQuickGrenade/perception"
#include "BotQuickGrenade/tactics"
#include "BotQuickGrenade/commands"
#include "BotQuickGrenade/hooks"

//========================================================================================
// PLUGIN INFO
//========================================================================================

public Plugin myinfo =
{
    name        = "BotQuickGrenade",
    author      = "Ducheese",
    description = "Tactical HE Grenade AI for Bots with perception and ballistic solver",
    version     = PLUGIN_VERSION,
    url         = "https://space.bilibili.com/1889622121"
};

//========================================================================================
// LIFECYCLE
//========================================================================================

public void OnPluginStart()
{
    RegisterBotQuickGrenadeCommands();
    RegisterBotQuickGrenadeHooks();
}

public void OnPluginEnd()
{
    UnregisterBotQuickGrenadeHooks();
}

public void OnMapStart()
{
    g_iBeamSprite = PrecacheModel("sprites/laserbeam.spr");

    for (int i = 1; i <= MaxClients; i++)
    {
        g_fLastThrowTime[i]  = 0.0;
        g_bAngleOverride[i]  = false;
        g_fOverrideExpiry[i] = 0.0;

        for (int j = 1; j <= MaxClients; j++)
        {
            g_fLastPerceivedTime[i][j] = 0.0;
            g_vecLastKnownPos[i][j]    = NULL_VECTOR;
        }
    }
}

public void OnClientPutInServer(int client)
{
    SDKHook(client, SDKHook_OnTakeDamage, OnTakeDamage);
}

public void OnClientDisconnect(int client)
{
    g_fLastThrowTime[client]  = 0.0;
    g_bAngleOverride[client]  = false;
    g_fOverrideExpiry[client] = 0.0;

    ClearPlayerMemory(client);
}