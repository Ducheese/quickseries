#if defined _quickmelee_included
 #endinput
#endif
#define _quickmelee_included

/**
 * 获取玩家当前是否正处于快速近战挥刀状态
 *
 * @param client        玩家 Client Index
 * @return              true 为处于快速近战中，false 为闲置
 */
native bool QuickMelee_IsCombat(int client);

/**
 * 手动为玩家触发快速近战流程
 *
 * @param client        玩家 Client Index
 * @return              true 触发成功，false 失败（冷却/持禁武器/死亡等）
 */
native bool QuickMelee_Trigger(int client);

/**
 * 手动取消玩家当前的快速近战流程并安全切回原武器
 *
 * @param client        玩家 Client Index
 */
native void QuickMelee_Cancel(int client);

public SharedPlugin __pl_quickmelee = 
{
    name = "quickmelee",
    file = "QuickMelee.smx",
#if defined REQUIRE_PLUGIN
    required = 1,
#else
    required = 0,
#endif
};

#if !defined REQUIRE_PLUGIN
public void __pl_quickmelee_SetNTVOptional()
{
    MarkNativeAsOptional("QuickMelee_IsCombat");
    MarkNativeAsOptional("QuickMelee_Trigger");
    MarkNativeAsOptional("QuickMelee_Cancel");
}
#endif
