#if defined _quickgrenade_hooks_included
 #endinput
#endif
#define _quickgrenade_hooks_included

//========================================================================================
// HOOKS & EVENT LISTENERS
//========================================================================================

void RegisterQuickGrenadeHooks()
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
        if (g_PlayerState[client] == STATE_HOLDING)
        {
            // 长按引雷阶段：持续注入 IN_ATTACK 保持拔销瞄准/引雷动作
            buttons |= IN_ATTACK;

            // 关键保护：只有当 PrimaryAttack() 已经在第 1 帧成功触发且 m_bPinPulled 变为 1 之后，
            // 才将 TimeWeaponIdle 和 NextPrimaryAttack 锁定推移到未来时间。
            // 这样既确保了第 1 帧的拔销动画 (ACT_VM_PULLPIN) 顺利触发，
            // 又彻底杜绝了后续长按期间引擎自动调用 WeaponIdle() 导致的拉销动作循环重播！
            int grenade = g_iActiveGrenadeEnt[client];
            if (grenade > 0 && IsValidEntity(grenade))
            {
                if (GetEntProp(grenade, Prop_Send, "m_bPinPulled") != 0)
                {
                    SetEntPropFloat(grenade, Prop_Send, "m_flTimeWeaponIdle", GetGameTime() + 1.0);
                    SetEntPropFloat(grenade, Prop_Send, "m_flNextPrimaryAttack", GetGameTime() + 1.0);
                }
            }
        }
        else if (g_PlayerState[client] == STATE_THROWING)
        {
            // 丢雷阶段：强制剥除 IN_ATTACK，通知引擎立即丢出手雷
            buttons &= ~IN_ATTACK;
        }
    }

    return Plugin_Continue;
}

public Action OnWeaponCanSwitch(int client, int weapon) 
{
    if (isCombat[client])
    {
        return Plugin_Handled; // 快速手雷动作期间阻止触碰地面武器导致的自动切枪
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
        ResetClientQuickGrenade(client);
    }
}

public void Event_PlayerDeath(Handle event, const char[] name, bool dontBroadcast)
{
    int client = GetClientOfUserId(GetEventInt(event, "userid"));
    if (client >= 1 && client <= MaxClients)
    {
        ResetClientQuickGrenade(client);
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
        ResetClientQuickGrenade(i);
    }
}

public void EventRoundFreezeEnd(Handle event, const char[] name, bool dontBroadcast)
{
    g_bfreezetime = false;
}
