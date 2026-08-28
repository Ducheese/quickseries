#if defined _quickmelee_hooks_included
 #endinput
#endif
#define _quickmelee_hooks_included

//========================================================================================
// HOOKS & EVENT LISTENERS
//========================================================================================

void RegisterQuickMeleeHooks()
{
    HookEvent("round_end", EventRoundEnd);
    HookEvent("round_freeze_end", EventRoundFreezeEnd);
    HookEvent("bomb_begindefuse", Bomb_Event_Begin);
    HookEvent("bomb_abortdefuse", Bomb_Event_End);
    HookEvent("player_spawn", Event_PlayerSpawn);
    HookEvent("player_death", Event_PlayerDeath);
}

public Action OnPlayerRunCmd(int client, int &buttons, int &impulse, float vel[3], float angles[3], int &weapon, int &subtype, int &cmdnum, int &tickcount, int &seed, int mouse[2])
{
    if (IsValidClient(client, true))
    {
        if (IsInWhatClick[client] == 1)
        {
            buttons |= IN_ATTACK;
            IsInWhatClick[client] = 0;
        }
        else if (IsInWhatClick[client] == 2)
        {
            buttons |= IN_ATTACK2;
            IsInWhatClick[client] = 0;
        }
    }

    return Plugin_Continue;
}

public Action OnWeaponCanSwitch(int client, int weapon) 
{
    if (isCombat[client])
    {
        return Plugin_Handled; // 快速近战动作期间阻止触碰地面武器导致的自动切枪
    }
    return Plugin_Continue;
}

public void OnEntityCreated(int entity, const char[] classname)
{ 
    if (!GetConVarBool(cvarEnable)) return;
    
    if (StrEqual(classname, "predicted_viewmodel", false))
    {
        SDKHook(entity, SDKHook_Spawn, OnEntitySpawned2);
    }
}

public void OnEntitySpawned2(int entity)
{
    int owner = GetEntPropEnt(entity, Prop_Send, "m_hOwner");

    if (owner > 0 && owner <= MaxClients)
    {
        if (GetEntProp(entity, Prop_Send, "m_nViewModelIndex") == 0)
        {
            ClientVM3[owner][0] = entity;
        }
        else if (GetEntProp(entity, Prop_Send, "m_nViewModelIndex") == 1)
        {
            ClientVM3[owner][1] = entity;
        }
    }
}

public void Event_PlayerSpawn(Handle event, const char[] name, bool dontBroadcast)
{
    int client = GetClientOfUserId(GetEventInt(event, "userid"));
    if (client >= 1 && client <= MaxClients)
    {
        Fov[client] = GetEntProp(client, Prop_Send, "m_iFOV");
        ResetClientQuickMelee(client);
    }
}

public void Event_PlayerDeath(Handle event, const char[] name, bool dontBroadcast)
{
    int client = GetClientOfUserId(GetEventInt(event, "userid"));
    if (client >= 1 && client <= MaxClients)
    {
        ResetClientQuickMelee(client);
    }
}

public void Bomb_Event_Begin(Handle event, const char[] name, bool dontBroadcast)
{
    g_bfreezetime = true;
}

public void Bomb_Event_End(Handle event, const char[] name, bool dontBroadcast)
{
    g_bfreezetime = false;
}

public void EventRoundEnd(Handle event, const char[] name, bool dontBroadcast)
{
    g_bfreezetime = true;
    for (int i = 1; i <= MaxClients; i++)
    {
        ResetClientQuickMelee(i);
    }
}

public void EventRoundFreezeEnd(Handle event, const char[] name, bool dontBroadcast)
{
    g_bfreezetime = false;
}
