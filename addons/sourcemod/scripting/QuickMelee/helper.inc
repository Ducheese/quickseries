#if defined _quickmelee_helper_included
 #endinput
#endif
#define _quickmelee_helper_included

//========================================================================================
// HELPER FUNCTIONS
//========================================================================================

/**
 * 客户端切枪执行函数（带掏枪动画）
 *
 * @param client    玩家 Client Index
 * @param weapon    目标武器实体索引
 */
void SwitchWeapon(int client, int weapon)
{
    if (IsValidEntity(weapon))
    {
        char classname[32];
        GetEntityClassname(weapon, classname, sizeof(classname));
        FakeClientCommand(client, "use %s", classname);
    }
}

/**
 * 优先切回主武器，其次副武器，最后刀
 *
 * @param client    玩家 Client Index
 */
void SwitchToBestWeapon(int client)
{
    int primary = GetPlayerWeaponSlot(client, 0);
    if (primary > 0 && IsValidEntity(primary))
    {
        SwitchWeapon(client, primary);
        return;
    }

    int secondary = GetPlayerWeaponSlot(client, 1);
    if (secondary > 0 && IsValidEntity(secondary))
    {
        SwitchWeapon(client, secondary);
        return;
    }

    int melee = GetPlayerWeaponSlot(client, 2);
    if (melee > 0 && IsValidEntity(melee))
    {
        SwitchWeapon(client, melee);
        return;
    }
}

/**
 * 重置指定客户端的快速近战状态与定时器
 *
 * @param client    玩家 Client Index
 */
void ResetClientQuickMelee(int client)
{
    if (g_hSwitchTimer[client] != null)
    {
        KillTimer(g_hSwitchTimer[client]);
        g_hSwitchTimer[client] = null;
    }

    isCombat[client] = false;
    IsInWhatClick[client] = 0;
    PreWeapon[client] = -1;
}

/**
 * 校验视图模型实体合法性
 *
 * @param client    玩家 Client Index
 * @param entity    视图模型实体索引
 * @return          true 为合法 predicted_viewmodel，false 否则
 */
bool IsValidViewModelEntity(int client, int entity)
{
    if (!IsValidEntity(entity))
        return false;

    char classname[32];
    GetEntityClassname(entity, classname, sizeof(classname));

    return StrEqual(classname, "predicted_viewmodel", false)
        && GetEntPropEnt(entity, Prop_Send, "m_hOwner") == client
        && HasEntProp(entity, Prop_Send, "m_fEffects");
}

/**
 * 校验客户端合法性
 *
 * @param client    玩家 Client Index
 * @param bAlive    是否要求玩家存活
 * @return          true 为合法玩家，false 否则
 */
stock bool IsValidClient(int client, bool bAlive = false)
{
    return (client >= 1 && client <= MaxClients && IsClientInGame(client) && !IsClientSourceTV(client) && (!bAlive || IsPlayerAlive(client)));
}
