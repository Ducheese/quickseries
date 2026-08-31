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

    cvarHeEnable = CreateConVar(
        "sm_botqg_he_enable", "1",
        "是否启用 Bot 高爆手雷 (HE) 投掷 AI（1：启用；0：禁用）",
        FCVAR_NOTIFY, true, 0.0, true, 1.0
    );

    cvarSmokeEnable = CreateConVar(
        "sm_botqg_smoke_enable", "1",
        "是否启用 Bot 战术烟雾弹 (Smoke) 投掷 AI（1：启用；0：禁用）",
        FCVAR_NOTIFY, true, 0.0, true, 1.0
    );

    cvarFlashEnable = CreateConVar(
        "sm_botqg_flash_enable", "1",
        "是否启用 Bot 战术闪光弹 (Flashbang) 投掷 AI（1：启用；0：禁用）",
        FCVAR_NOTIFY, true, 0.0, true, 1.0
    );

    cvarSmokeHp = CreateConVar(
        "sm_botqg_smoke_hp", "35",
        "残血脚底自保烟触发血量阈值（HP <= 设定值时触发）",
        FCVAR_NOTIFY, true, 1.0, true, 100.0
    );

    cvarRequireCover = CreateConVar(
        "sm_botqg_require_cover", "1",
        "战术限制：仅在被掩体阻挡（无直连枪线）时才丢雷，防止面对面对枪时丢雷：1=启用，0=禁用直接丢",
        FCVAR_NOTIFY, true, 0.0, true, 1.0
    );

    cvarFriendlyFire = CreateConVar(
        "sm_botqg_check_friendly", "1",
        "安全检查：防止误伤或闪白队友（1=启用友军安全检查，0=禁用直接投掷）",
        FCVAR_NOTIFY, true, 0.0, true, 1.0
    );

    cvarGiveGrenades = CreateConVar(
        "sm_botqg_give_grenades", "1",
        "是否在 Bot 出生时自动补给全套手雷（1 HE + 1 Smoke + 1 Flash）（1=启用，0=禁用）",
        FCVAR_NOTIFY, true, 0.0, true, 1.0
    );

    cvarBlockGrenade = CreateConVar(
        "sm_botqg_block_native_grenade", "1",
        "是否阻止 Bot 原生 AI 主动将手雷掏在手上（1=启用拦截，0=允许原生掏雷）：防止 Bot 捏雷裸奔",
        FCVAR_NOTIFY, true, 0.0, true, 1.0
    );

    cvarPlayerPreview = CreateConVar(
        "sm_botqg_preview", "1",
        "是否在真人玩家捏雷瞄准时实时显示预测弹道轨迹（1=开启，0=关闭）",
        FCVAR_NOTIFY, true, 0.0, true, 1.0
    );

    cvarDebug = CreateConVar(
        "sm_botqg_debug", "1",
        "调试可视化与信息：1=开启聊天栏感知提示+弹道圆圈，0=关闭",
        FCVAR_NOTIFY, true, 0.0, true, 1.0
    );

    cvarGravity = FindConVar("sv_gravity");

    AutoExecConfig(true, "plugin.botquickgrenade");
}
