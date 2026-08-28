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
    MarkNativeAsOptional("QuickGrenade_Cancel");
}
#endif
