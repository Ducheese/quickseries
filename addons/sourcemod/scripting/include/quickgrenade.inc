#if defined _quickgrenade_included
 #endinput
#endif
#define _quickgrenade_included

// 玩家快速手雷状态枚举
enum QuickGrenadeState
{
    STATE_IDLE = 0,       // 闲置状态
    STATE_HOLDING,        // 拔销引雷中（长按 IN_ATTACK 保持引雷姿势）
    STATE_THROWING,       // 松开投掷中（释放 IN_ATTACK 触发 ACT_VM_THROW 与发射投掷物）
    STATE_SWITCHING       // 投掷完成，正在切回武器
};

// 手雷战术角色与引擎弹药序号（消除类名硬编码的唯一依据，Single Source of Truth）
// 序号来自引擎 AddAmmoType 调用顺序（cs_gamerules.cpp：子弹 1-10，手雷 11 起），全版本一致
#define GRENADE_ROLE_UNKNOWN  0
#define GRENADE_ROLE_HE       1
#define GRENADE_ROLE_SMOKE    2
#define GRENADE_ROLE_FLASH    3

#define AMMOIDX_HEGRENADE     11
#define AMMOIDX_FLASHBANG     12
#define AMMOIDX_SMOKEGRENADE  13

/**
 * 解析手雷实体的战术角色（读其底层弹药序号，加枪雷只要复用原版弹药序号即自动识别）
 *
 * @param wep   武器实体索引
 * @return      GRENADE_ROLE_HE / GRENADE_ROLE_SMOKE / GRENADE_ROLE_FLASH / GRENADE_ROLE_UNKNOWN
 */
stock int QG_GetGrenadeRole(int wep)
{
    if (wep <= 0 || !IsValidEntity(wep))
        return GRENADE_ROLE_UNKNOWN;

    int ammoType = GetEntProp(wep, Prop_Send, "m_iPrimaryAmmoType");
    if (ammoType == AMMOIDX_FLASHBANG)
        return GRENADE_ROLE_FLASH;
    if (ammoType == AMMOIDX_SMOKEGRENADE)
        return GRENADE_ROLE_SMOKE;
    if (ammoType == AMMOIDX_HEGRENADE)
        return GRENADE_ROLE_HE;
    return GRENADE_ROLE_UNKNOWN;
}

/**
 * 寻找玩家背包中指定战术角色的首个有弹手雷实体
 *
 * @param client    玩家 Client Index
 * @param role      手雷战术角色（GRENADE_ROLE_HE, GRENADE_ROLE_FLASH, GRENADE_ROLE_SMOKE）
 * @return          实体索引，无则 -1
 */
stock int QG_FindGrenadeByRole(int client, int role)
{
    if (role == GRENADE_ROLE_UNKNOWN)
        return -1;

    for (int i = 0; i < 48; i++)
    {
        int weapon = GetEntPropEnt(client, Prop_Send, "m_hMyWeapons", i);
        if (weapon > 0 && IsValidEntity(weapon))
        {
            if (QG_GetGrenadeRole(weapon) != role)
                continue;

            // 能走到这里的一定带有效弹药序号（角色本身就是从弹药序号认出来的），备弹无条件必查
            int ammoType = GetEntProp(weapon, Prop_Send, "m_iPrimaryAmmoType");
            int ammo = GetEntProp(client, Prop_Send, "m_iAmmo", _, ammoType);
            if (ammo > 0)
            {
                return weapon;
            }
        }
    }
    return -1;
}

/**
 * 获取玩家当前是否正处于快速引雷或投掷状态
 *
 * @param client        玩家 Client Index
 * @return              true 为处于快速手雷过程中，false 为闲置
 */
native bool QuickGrenade_IsCombat(int client);

/**
 * 获取玩家当前快速手雷的状态枚举
 *
 * @param client        玩家 Client Index
 * @return              QuickGrenadeState 枚举值
 */
native QuickGrenadeState QuickGrenade_GetState(int client);

/**
 * 手动为玩家触发指定手雷的快速投掷/引雷流程
 *
 * @param client            玩家 Client Index
 * @param grenadeClassname  手雷类名（如 weapon_hegrenade, weapon_smokegrenade, weapon_flashbang）
 * @param isHold            是否以 +cmd 长按模式触发
 * @return                  true 触发成功，false 触发失败（无雷/冷却中/死亡等）
 */
native bool QuickGrenade_Trigger(int client, const char[] grenadeClassname, bool isHold = false);

/**
 * 按战术角色触发快速手雷（加枪雷只要复用原版弹药序号即可被找到，无需硬编码类名）
 *
 * @param client        玩家 Client Index
 * @param role          手雷战术角色（GRENADE_ROLE_HE, GRENADE_ROLE_SMOKE, GRENADE_ROLE_FLASH）
 * @param isHold        是否以 +cmd 长按模式触发
 * @return              true 触发成功，false 触发失败（无雷/冷却中/死亡等）
 */
native bool QuickGrenade_TriggerByRole(int client, int role, bool isHold = false);

/**
 * 手动取消玩家当前的快速引雷流程并复位
 *
 * @param client        玩家 Client Index
 */
native void QuickGrenade_Cancel(int client);

public SharedPlugin __pl_quickgrenade = 
{
    name = "quickgrenade",
    file = "QuickGrenade.smx",
#if defined REQUIRE_PLUGIN
    required = 1,
#else
    required = 0,
#endif
};

#if !defined REQUIRE_PLUGIN
public void __pl_quickgrenade_SetNTVOptional()
{
    MarkNativeAsOptional("QuickGrenade_IsCombat");
    MarkNativeAsOptional("QuickGrenade_GetState");
    MarkNativeAsOptional("QuickGrenade_Trigger");
    MarkNativeAsOptional("QuickGrenade_TriggerByRole");
    MarkNativeAsOptional("QuickGrenade_Cancel");
}
#endif
