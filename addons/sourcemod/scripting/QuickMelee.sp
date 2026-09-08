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

    CreateConVar("sm_quickmelee_version", PLUGIN_VERSION, "插件版本", FCVAR_PROTECTED);
    cvarEnable = CreateConVar("sm_quickmelee_enable", "1", "是否启用快速近战插件（1：启用；0：禁用）", FCVAR_NOTIFY, true, 0.0, true, 1.0);
    cvarHitType = CreateConVar("sm_quickmelee_hit_type", "1", "快速轻击或重击（1：左键轻击；0：右键重击）", FCVAR_NOTIFY, true, 0.0, true, 1.0);
    cvarBackType = CreateConVar("sm_quickmelee_back_type", "2", "挥完刀之后切到什么武器（2：切回原来武器；1：优先切到主武器，其次副武器；0：保持持刀）", FCVAR_NOTIFY, true, 0.0, true, 2.0);
    cvarSwitchWeaponTime = CreateConVar("sm_quickmelee_switch_weapon_time", "0.4", "从触发快速近战到切回武器的时间间隔（单位：秒）", FCVAR_NOTIFY, true, 0.1);
    cvarForbiddenList = CreateConVar("sm_quickmelee_forbidden", "weapon_minigun", "需要禁用触发快速近战的武器名单", FCVAR_NOTIFY);
    cvarFixViewModel = CreateConVar("sm_quickmelee_fix_viewmodel", "1", "挥刀时是否修复双视图模型（0：不干预；1：藏1号亮0号；2：原版刀藏1亮0、加枪刀藏0亮1）", FCVAR_NOTIFY, true, 0.0, true, 2.0);

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
