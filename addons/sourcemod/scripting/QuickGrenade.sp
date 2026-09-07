//========================================================================================
// QuickGrenade - 快速手雷插件 (QuickSeries)
// 模块化设计，支持与 QuickMelee 跨插件动作互斥与原生 API 调用
//========================================================================================

#pragma semicolon 1
#pragma newdecls required

#include <sourcemod>
#include <sdktools>
#include <sdkhooks>
#include <cstrike>

// 引入跨插件联动支持（可选依赖）
#undef REQUIRE_PLUGIN
#include <quickmelee>
#define REQUIRE_PLUGIN

#include <quickgrenade>

// 武器画像与 cfg 配置（role 发现 cfg 优先；本体换图加载一份自有拷贝，见 OnMapStart）
#include <quickgrenade_profile>

#include "QuickGrenade/global"     // 全局定义与变量
#include "QuickGrenade/helper"     // 工具与辅助函数
#include "QuickGrenade/core"       // 核心引雷与投掷逻辑
#include "QuickGrenade/commands"   // 控制台指令注册与回调
#include "QuickGrenade/hooks"      // 实体/按键/游戏事件钩子

//========================================================================================
// PLUGIN INFO
//========================================================================================

public Plugin myinfo =
{
    name = "QuickGrenade",
    author = "Ducheese",
    description = "Quick Grenade for CS:S (Part of QuickSeries)",
    version = PLUGIN_VERSION,
    url = "https://space.bilibili.com/1889622121"
};

public APLRes AskPluginLoad2(Handle myself, bool late, char[] error, int err_max)
{
    CreateNative("QuickGrenade_IsCombat", Native_IsCombat);
    CreateNative("QuickGrenade_GetState", Native_GetState);
    CreateNative("QuickGrenade_Trigger", Native_Trigger);
    CreateNative("QuickGrenade_TriggerByRole", Native_TriggerByRole);
    CreateNative("QuickGrenade_Cancel", Native_Cancel);
    RegPluginLibrary("quickgrenade");
    return APLRes_Success;
}

public void OnPluginStart()
{
    AutoExecConfig(true, "plugin.quickgrenade");

    CreateConVar("sm_quickgrenade_version", PLUGIN_VERSION, "插件版本", FCVAR_PROTECTED);
    cvarEnable = CreateConVar("sm_quickgrenade_enable", "1", "是否启用快速手雷插件（1：启用；0：禁用）", FCVAR_NOTIFY, true, 0.0, true, 1.0);
    cvarBackType = CreateConVar("sm_quickgrenade_back_type", "2", "丢出手雷之后切到什么武器（2：切回原来武器；1：优先切到主武器，其次副武器；0：保持持雷）", FCVAR_NOTIFY, true, 0.0, true, 2.0);
    cvarMinPullTime = CreateConVar("sm_quickgrenade_min_pull_time", "0.2", "最短引雷动画持续时间（单位：秒，点按时保证引雷动作完整展现）", FCVAR_NOTIFY, true, 0.05, true, 2.0);
    cvarThrowDelayTime = CreateConVar("sm_quickgrenade_throw_delay_time", "0.6", "松开引雷后，等待投掷动作完成及实体发射再切回武器的时间（单位：秒，必须大于0.15秒）", FCVAR_NOTIFY, true, 0.2, true, 3.0);
    cvarAutoThrowTime = CreateConVar("sm_quickgrenade_auto_throw_time", "0.25", "通过单次触发命令（非 +cmd 长按）时，自动引雷多久后松手投掷（单位：秒）", FCVAR_NOTIFY, true, 0.1, true, 2.0);
    cvarForbiddenList = CreateConVar("sm_quickgrenade_forbidden", "weapon_minigun", "需要禁用触发快速手雷的武器名单", FCVAR_NOTIFY);
    cvarFixViewModel = CreateConVar("sm_quickgrenade_fix_viewmodel", "0", "引雷时是否修复双视图模型显示（1：强制显示0号v模并隐藏1号；0：不干预，1号保持原样）", FCVAR_NOTIFY, true, 0.0, true, 1.0);
    cvarPriority = CreateConVar("sm_quickgrenade_priority", "FSH", "战术手雷优先级编码（F=闪光 S=烟雾 H=高爆，按顺序耗尽）", FCVAR_NOTIFY);

    RegisterQuickGrenadeCommands();
    RegisterQuickGrenadeHooks();
}

public void OnMapStart()
{
    g_bfreezetime = false;

    // 加载武器画像配置（本插件自有拷贝；cfg 修改后下张图生效）
    QG_LoadWeaponProfilesConfig();

    for (int i = 1; i <= MaxClients; i++)
    {
        ResetClientQuickGrenade(i);
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
    ResetClientQuickGrenade(client);
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

public int Native_GetState(Handle plugin, int numParams)
{
    int client = GetNativeCell(1);
    if (!IsValidClient(client, false))
        return view_as<int>(STATE_IDLE);

    return view_as<int>(g_PlayerState[client]);
}

public int Native_Trigger(Handle plugin, int numParams)
{
    int client = GetNativeCell(1);
    if (!IsValidClient(client, true))
        return false;

    char grenadeClass[32];
    GetNativeString(2, grenadeClass, sizeof(grenadeClass));
    bool isHold = GetNativeCell(3);

    return TriggerQuickGrenadeByClass(client, grenadeClass, isHold);
}

public int Native_TriggerByRole(Handle plugin, int numParams)
{
    int client = GetNativeCell(1);
    if (!IsValidClient(client, true))
        return false;

    int role = GetNativeCell(2);
    bool isHold = GetNativeCell(3);

    return TriggerQuickGrenadeByRole(client, role, isHold);
}

public int Native_Cancel(Handle plugin, int numParams)
{
    int client = GetNativeCell(1);
    if (!IsValidClient(client, false))
        return 0;

    CancelQuickGrenade(client);
    return 0;
}
