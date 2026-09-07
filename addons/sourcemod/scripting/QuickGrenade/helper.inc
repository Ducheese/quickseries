#if defined _quickgrenade_helper_included
 #endinput
#endif
#define _quickgrenade_helper_included

//========================================================================================
// HELPER FUNCTIONS
//========================================================================================

/**
 * 寻找玩家背包中指定类名的手雷实体
 *
 * @param client            玩家 Client Index
 * @param targetClassname   目标手雷类名
 * @return                  手雷实体索引，未找到或弹药为空返回 -1
 */
int FindPlayerGrenade(int client, const char[] targetClassname)
{
    for (int i = 0; i < 48; i++)
    {
        int weapon = GetEntPropEnt(client, Prop_Send, "m_hMyWeapons", i);
        if (weapon > 0 && IsValidEntity(weapon))
        {
            char classname[32];
            GetEntityClassname(weapon, classname, sizeof(classname));
            if (StrEqual(classname, targetClassname, false))
            {
                // 是雷就有弹药序号：无弹药序号的不是雷，直接跳过；备弹无条件必查
                int ammoType = GetEntProp(weapon, Prop_Send, "m_iPrimaryAmmoType");
                if (ammoType < 0)
                    continue;

                int ammo = GetEntProp(client, Prop_Send, "m_iAmmo", _, ammoType);
                if (ammo > 0)
                {
                    return weapon;
                }
            }
        }
    }
    return -1;
}

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
 * 重置指定客户端的快速手雷状态与定时器
 *
 * @param client    玩家 Client Index
 */
void ResetClientQuickGrenade(int client)
{
    if (g_hDelayThrowTimer[client] != null)
    {
        KillTimer(g_hDelayThrowTimer[client]);
        g_hDelayThrowTimer[client] = null;
    }

    if (g_hSwitchWeaponTimer[client] != null)
    {
        KillTimer(g_hSwitchWeaponTimer[client]);
        g_hSwitchWeaponTimer[client] = null;
    }

    g_PlayerState[client] = STATE_IDLE;
    isCombat[client] = false;
    g_bCmdHolding[client] = false;
    PreWeapon[client] = -1;
    g_iActiveGrenadeEnt[client] = -1;
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
