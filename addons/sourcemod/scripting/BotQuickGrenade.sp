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

// 武器画像（弹道求解与角色发现唯一依据；本体换图加载，本插件自带一份）
#include <quickgrenade_profile>

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
    QG_LoadWeaponProfilesConfig();
    ResetAllGrenadeTimers();
    RegisterBotQuickGrenadeCommands();
    RegisterBotQuickGrenadeHooks();
}

public void OnPluginEnd()
{
    UnregisterBotQuickGrenadeHooks();
    delete g_hWeaponProfiles;
}

public void OnMapStart()
{
    QG_LoadWeaponProfilesConfig();
    g_iBeamSprite = PrecacheModel("sprites/laserbeam.spr");

    ResetAllGrenadeTimers();

    for (int i = 1; i <= MaxClients; i++)
    {
        g_bAngleOverride[i]      = false;
        g_fOverrideExpiry[i]     = 0.0;
    }

    // 全局大清空
    ClearAllMemory();

    // 启动 Bot AI 主思考循环与观战 HUD 定时器（换图自毁后在新图重新拉起）
    CreateTimer(BOT_THINK_INTERVAL, Timer_BotThink, _, TIMER_REPEAT | TIMER_FLAG_NO_MAPCHANGE);
    CreateTimer(0.5, Timer_UpdateSpectatorHUD, _, TIMER_REPEAT | TIMER_FLAG_NO_MAPCHANGE);
}

public void OnClientPutInServer(int client)
{
    if (IsFakeClient(client))
    {
        SDKHook(client, SDKHook_WeaponSwitch, Hook_BotWeaponSwitch);
    }
}

public void OnClientDisconnect(int client)
{
    ResetClientGrenadeTimers(client);

    g_bAngleOverride[client]      = false;
    g_fOverrideExpiry[client]     = 0.0;

    // 双向十字交叉清理，也就是只清理和自己相关的
    ClearPlayerMemory(client);
}
