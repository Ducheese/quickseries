#if defined _botquickgrenade_commands_included
  #endinput
#endif
#define _botquickgrenade_commands_included

//========================================================================================
// CONVARS & COMMAND REGISTRATION
// 注册插件控制台变量与配置自动生成
//========================================================================================

stock void RegisterBotQuickGrenadeCommands()
{
    CreateConVar("sm_botquickgrenade_version", PLUGIN_VERSION, "Bot 快速手雷 AI 插件版本", FCVAR_NOTIFY | FCVAR_DONTRECORD);

    cvarEnable = CreateConVar(
        "sm_botqg_enable", "1",
        "是否启用 Bot 快速手雷 AI（1：启用；0：禁用）",
        FCVAR_NOTIFY, true, 0.0, true, 1.0
    );

    cvarDebug = CreateConVar(
        "sm_botqg_debug", "1",
        "调试可视化与信息：1=开启聊天栏感知提示+弹道圆圈，0=关闭",
        FCVAR_NOTIFY, true, 0.0, true, 1.0
    );

    cvarRequireCover = CreateConVar(
        "sm_botqg_require_cover", "1",
        "战术限制：仅在被掩体阻挡（无直连枪线）时才丢雷，防止面对面对枪时丢雷：1=启用，0=禁用直接丢",
        FCVAR_NOTIFY, true, 0.0, true, 1.0
    );

    cvarFriendlyFire = CreateConVar(
        "sm_botqg_check_friendly", "1",
        "安全检查：落点附近有存活队友时禁止丢雷，防止误伤队友：1=启用，0=禁用",
        FCVAR_NOTIFY, true, 0.0, true, 1.0
    );

    cvarSmokeEnable = CreateConVar(
        "sm_botqg_smoke_enable", "1",
        "是否启用 Bot 智能战术烟雾弹 AI（1：启用；0：禁用）",
        FCVAR_NOTIFY, true, 0.0, true, 1.0
    );

    cvarSmokeHp = CreateConVar(
        "sm_botqg_smoke_hp", "35",
        "残血脚底自保烟触发血量阈值（HP <= 设定值时触发）",
        FCVAR_NOTIFY, true, 1.0, true, 100.0
    );

    cvarBulletScale = CreateConVar(
        "sm_botqg_bullet_scale", "0.1",
        "测试辅助：子弹伤害倍率（0.1=10% 伤害，1.0=正常全额伤害）",
        FCVAR_NOTIFY, true, 0.0, true, 1.0
    );

    cvarBotGrenades = CreateConVar(
        "sm_botqg_bot_grenades", "10",
        "测试辅助：Bot 出生时每种手雷的备弹补给数量",
        FCVAR_NOTIFY, true, 0.0, true, 50.0
    );

    cvarGravity = FindConVar("sv_gravity");

    AutoExecConfig(true, "plugin.botquickgrenade");
}
