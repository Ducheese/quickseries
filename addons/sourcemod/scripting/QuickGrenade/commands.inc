#if defined _quickgrenade_commands_included
 #endinput
#endif
#define _quickgrenade_commands_included

//========================================================================================
// COMMAND REGISTRATION & CALLBACKS
//========================================================================================

void RegisterQuickGrenadeCommands()
{
    // 注册高爆手雷命令 (HE Grenade)
    RegConsoleCmd("sm_quickfrag", Cmd_QuickFrag, "触发快速高爆手雷（点按自动投掷）");
    RegConsoleCmd("+sm_quickfrag", Cmd_QuickFrag_Plus, "长按快速高爆手雷（按住拔销引雷，松开投掷）");
    RegConsoleCmd("-sm_quickfrag", Cmd_QuickFrag_Minus, "松开快速高爆手雷（投掷）");
    RegConsoleCmd("sm_quickhe", Cmd_QuickFrag, "触发快速高爆手雷别名");
    RegConsoleCmd("+sm_quickhe", Cmd_QuickFrag_Plus, "长按快速高爆手雷别名");
    RegConsoleCmd("-sm_quickhe", Cmd_QuickFrag_Minus, "松开快速高爆手雷别名");

    // 注册烟雾弹命令 (Smoke Grenade)
    RegConsoleCmd("sm_quicksmoke", Cmd_QuickSmoke, "触发快速烟雾弹（点按自动投掷）");
    RegConsoleCmd("+sm_quicksmoke", Cmd_QuickSmoke_Plus, "长按快速烟雾弹（按住拔销引雷，松开投掷）");
    RegConsoleCmd("-sm_quicksmoke", Cmd_QuickSmoke_Minus, "松开快速烟雾弹（投掷）");

    // 注册闪光弹命令 (Flashbang)
    RegConsoleCmd("sm_quickflash", Cmd_QuickFlash, "触发快速闪光弹（点按自动投掷）");
    RegConsoleCmd("+sm_quickflash", Cmd_QuickFlash_Plus, "长按快速闪光弹（按住拔销引雷，松开投掷）");
    RegConsoleCmd("-sm_quickflash", Cmd_QuickFlash_Minus, "松开快速闪光弹（投掷）");
}

// --- 高爆手雷 (HE Grenade) ---
public Action Cmd_QuickFrag(int client, int args)
{
    TriggerQuickGrenade(client, "weapon_hegrenade", false);
    return Plugin_Handled;
}

public Action Cmd_QuickFrag_Plus(int client, int args)
{
    TriggerQuickGrenade(client, "weapon_hegrenade", true);
    return Plugin_Handled;
}

public Action Cmd_QuickFrag_Minus(int client, int args)
{
    ReleaseQuickGrenade(client);
    return Plugin_Handled;
}

// --- 烟雾弹 (Smoke Grenade) ---
public Action Cmd_QuickSmoke(int client, int args)
{
    TriggerQuickGrenade(client, "weapon_smokegrenade", false);
    return Plugin_Handled;
}

public Action Cmd_QuickSmoke_Plus(int client, int args)
{
    TriggerQuickGrenade(client, "weapon_smokegrenade", true);
    return Plugin_Handled;
}

public Action Cmd_QuickSmoke_Minus(int client, int args)
{
    ReleaseQuickGrenade(client);
    return Plugin_Handled;
}

// --- 闪光弹 (Flashbang) ---
public Action Cmd_QuickFlash(int client, int args)
{
    TriggerQuickGrenade(client, "weapon_flashbang", false);
    return Plugin_Handled;
}

public Action Cmd_QuickFlash_Plus(int client, int args)
{
    TriggerQuickGrenade(client, "weapon_flashbang", true);
    return Plugin_Handled;
}

public Action Cmd_QuickFlash_Minus(int client, int args)
{
    ReleaseQuickGrenade(client);
    return Plugin_Handled;
}
