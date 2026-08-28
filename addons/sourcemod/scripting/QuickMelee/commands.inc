#if defined _quickmelee_commands_included
 #endinput
#endif
#define _quickmelee_commands_included

//========================================================================================
// COMMAND REGISTRATION & CALLBACKS
//========================================================================================

void RegisterQuickMeleeCommands()
{
    RegConsoleCmd("sm_quickmelee", Cmd_QuickMelee, "触发快速近战（挥刀后自动切回主武器）");
}

public Action Cmd_QuickMelee(int client, int args)
{
    PerformQuickMelee(client);
    return Plugin_Handled;
}
