//========================================================================================
// QuickMelee - 快速近战插件 (QuickSeries)
// 模块化设计，支持与 QuickGrenade 跨插件动作互斥与原生 API 调用
//========================================================================================

#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <sdktools>
#include <sdkhooks>
#include <cstrike>

// 引入跨插件联动支持（可选依赖）
#undef REQUIRE_PLUGIN
#include <quickgrenade>
#define REQUIRE_PLUGIN

#include <quickmelee>

#include "QuickMelee/global"     // 全局定义与变量
#include "QuickMelee/helper"     // 工具与辅助函数
#include "QuickMelee/core"       // 核心挥刀与切枪逻辑
#include "QuickMelee/commands"   // 控制台指令注册与回调
#include "QuickMelee/hooks"      // 实体/按键/游戏事件钩子

//========================================================================================
// PLUGIN INFO
//========================================================================================

public Plugin myinfo =
{
    name = "QuickMelee",
    author = "Ducheese",
    description = "Quick Melee for CS:S (Part of QuickSeries)",
    version = PLUGIN_VERSION,
    url = "https://space.bilibili.com/1889622121"
};

public APLRes AskPluginLoad2(Handle myself, bool late, char[] error, int err_max)
{
    CreateNative("QuickMelee_IsCombat", Native_IsCombat);
    CreateNative("QuickMelee_Trigger", Native_Trigger);
    CreateNative("QuickMelee_Cancel", Native_Cancel);
    RegPluginLibrary("quickmelee");
    return APLRes_Success;
}

public void OnPluginStart()
{
    AutoExecConfig(true, "plugin.quickmelee");

    CreateConVar("sm_quickmelee_version", PLUGIN_VERSION, "Plugin version", FCVAR_PROTECTED);
    cvarEnable = CreateConVar("sm_quickmelee_enable", "1", "Whether to enable QuickMelee (1: enabled, 0: disabled)", FCVAR_NOTIFY, true, 0.0, true, 1.0);
    cvarHitType = CreateConVar("sm_quickmelee_hit_type", "1", "Quick melee attack type (1: primary light slash, 0: secondary heavy slash)", FCVAR_NOTIFY, true, 0.0, true, 1.0);
    cvarBackType = CreateConVar("sm_quickmelee_back_type", "2", "Weapon to switch back after melee (2: previous weapon, 1: primary > secondary, 0: keep knife)", FCVAR_NOTIFY, true, 0.0, true, 2.0);
    cvarSwitchWeaponTime = CreateConVar("sm_quickmelee_switch_weapon_time", "0.4", "Delay from trigger to weapon switch-back (seconds)", FCVAR_NOTIFY, true, 0.1);
    cvarForbiddenList = CreateConVar("sm_quickmelee_forbidden", "weapon_minigun", "Blacklist of weapons that block quick melee", FCVAR_NOTIFY);
    cvarFixViewModel = CreateConVar("sm_quickmelee_fix_viewmodel", "0", "Fix dual viewmodels on melee (0: leave alone; 1: hide 1 show 0 unconditionally; 2: stock knife hides 1 shows 0, others pass through)", FCVAR_NOTIFY, true, 0.0, true, 2.0);

    RegisterQuickMeleeCommands();
    RegisterQuickMeleeHooks();
}

public void OnMapStart()
{
    g_bfreezetime = false;

    for (int i = 1; i <= MaxClients; i++)
    {
        ResetClientQuickMelee(i);
        ClientVM3[i][0] = -1;
        ClientVM3[i][1] = -1;
    }
}

public void OnClientPutInServer(int client)
{
    SDKHook(client, SDKHook_WeaponCanSwitchTo, OnWeaponCanSwitch);
}

public void OnClientDisconnect(int client)
{
    ResetClientQuickMelee(client);
    ClientVM3[client][0] = -1;
    ClientVM3[client][1] = -1;
}

//========================================================================================
// NATIVES
//========================================================================================

public int Native_IsCombat(Handle plugin, int numParams)
{
    int client = GetNativeCell(1);
    if (!IsValidClient(client, false))
        return false;

    return isCombat[client];
}

public int Native_Trigger(Handle plugin, int numParams)
{
    int client = GetNativeCell(1);
    if (!IsValidClient(client, true))
        return false;

    return PerformQuickMelee(client);
}

public int Native_Cancel(Handle plugin, int numParams)
{
    int client = GetNativeCell(1);
    if (!IsValidClient(client, false))
        return 0;

    CancelQuickMelee(client);
    return 0;
}
