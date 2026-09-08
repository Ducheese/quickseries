#if defined _quickgrenade_profile_included
  #endinput
#endif
#define _quickgrenade_profile_included

#include <quickgrenade>

//========================================================================================
// WEAPON PROFILE & KEYVALUES CONFIGURATION SYSTEM
// 武器画像与 KeyValues 开放配置系统（提供三级绝对安全的默认值兜底）
//========================================================================================

// 引爆方式枚举
enum GrenadeDetonateMode
{
    DETONATE_FUSE = 0,       // 定时引信（原版手雷，达到时限引爆）
    DETONATE_TOUCH = 1       // 触碰即炸（燃烧瓶 molotov、火箭弹 RPG）
};

// 初速度模式枚举
enum GrenadeSpeedMode
{
    SPEED_ENGINE = 0,        // CS:S 引擎原生仰角计算公式：(90.0 - pitch) * 6.0，上限 speed
    SPEED_FIXED = 1          // 固定初速度（如 RPG 直飞 1400.0）
};

// 手雷物理与战术画像结构体
enum struct GrenadeProfile
{
    int                 role;            // GRENADE_ROLE_HE / SMOKE / FLASH
    GrenadeDetonateMode detonateMode;    // 引爆模式
    float               detonateTime;    // 引信时长 (秒)
    float               gravityScale;    // 重力倍率 (相对于 sv_gravity，默认 0.4，直飞为 0.0)
    GrenadeSpeedMode    speedMode;       // 速度模式
    float               speed;           // 速度值
    float               hitRadius;       // 强效果界限
    float               minThrowDist;    // 防自伤距离（太近不扔）
    float               maxThrowDist;    // 理论射程上限
    int                 viewmodel;       // 雷 v 模在几号 predicted_viewmodel，缺省 0

    // 出处：重力 0.4=basecsgrenade_projectile.h:54（三雷统一，烟的 0.55 被覆盖）；
    // 初速 750+(90-p)*6=weapon_basecsgrenade.cpp:414-417（仰角修正 407/411 等价）。
    void InitDefault(int targetRole)
    {
        this.role = targetRole;
        this.detonateMode = DETONATE_FUSE;
        this.gravityScale = 0.4;
        this.speedMode = SPEED_ENGINE;
        this.speed = 750.0;
        this.maxThrowDist = 953.0;   // [推导] 静止平地引信内最远（750/1.5s/0.4g/眼64/仰角26°，积分+闭式双算）
        this.viewmodel = 0;

        switch (targetRole)
        {
            case GRENADE_ROLE_SMOKE:
            {
                // [引擎] 引信 smokegrenade_projectile.cpp:40（滚停门顺延，仿真按纯引信近似）
                this.detonateTime = 1.5;
                // [引擎] CONSTANT_UNITS_SMOKEGRENADERADIUS（cs_shareddefs.h:134）
                this.hitRadius = 155.0;
                // 烟无杀伤 → 无下限
                this.minThrowDist = 0.0;
            }
            case GRENADE_ROLE_FLASH:
            {
                // [引擎] 引信 flashbang_projectile.cpp:259
                this.detonateTime = 1.5;
                // [战术] 正对满视野 3s 强致盲界限：(4-600×4/1500)×1.25（同文件 169-174）
                this.hitRadius = 600.0;
                // 闪光防自伤与防队友由战术视线几何判定（遮挡/背身点积），无球形物理距离下限
                this.minThrowDist = 0.0;
            }
            default: // GRENADE_ROLE_HE（未知角色也落这里，由调用方保证只传有效角色）
            {
                this.role = GRENADE_ROLE_HE;
                // [引擎] 引信 hegrenade_projectile.cpp:33
                this.detonateTime = 1.5;
                // [战术] 半伤线：高斯 σ=350/3，≥50% 伤害界限 d≈137（伤害最大范围 350=100×3.5 另算，hegrenade:42-43）
                this.hitRadius = 137.0;
                // [战术] 防自伤距离 = 伤害最大范围 350，界内不扔
                this.minThrowDist = 350.0;
            }
        }
    }
}

// 全局武器配置字典（类名 -> GrenadeProfile）
StringMap g_hWeaponProfiles = null;

// 全局视图修复模式（cfg Settings 段 viewmodel_fix_mode）
// 0=不干预（原版 / HAN 武器框架）；
// 1=统一藏 1 号亮 0 号（老加枪+老机瞄）；
// 2=按配置分流，viewmodel=1 的走藏 0 显 1，其余走藏 1 显 0（新加枪+新机瞄）
int g_iQGViewModelFixMode = 0;

/**
 * 视图修复模式（本体修槽块唯一入口；非法值一律按 0 处理）
 *
 * @return  0 / 1 / 2
 */
stock int QG_ViewModelFixMode()
{
    if (g_iQGViewModelFixMode == 1 || g_iQGViewModelFixMode == 2)
        return g_iQGViewModelFixMode;
    return 0;
}

/**
 * 创建生成默认配置文件（包含原版与三大魔改手雷范例及详尽注释）
 */
stock void QG_WriteDefaultConfigFile(const char[] path)
{
    Handle file = OpenFile(path, "w");
    if (file == null)
        return;

    WriteFileLine(file, "// ==========================================================================");
    WriteFileLine(file, "// QuickGrenade & BotQuickGrenade 武器物理与战术配置文件");
    WriteFileLine(file, "//");
    WriteFileLine(file, "// 全局 Settings 段（兼容加枪武器）：");
    WriteFileLine(file, "//   viewmodel_fix_mode : 视图修复模式");
    WriteFileLine(file, "//                        0=不干预（原版 / HAN 武器框架）；");
    WriteFileLine(file, "//                        1=统一藏 1 号亮 0 号（老加枪+老机瞄）；");
    WriteFileLine(file, "//                        2=按配置分流，viewmodel=1 的走藏 0 显 1，其余走藏 1 显 0（新加枪+新机瞄）");
    WriteFileLine(file, "//");
    WriteFileLine(file, "// 大多数情况下无需配置，系统会自动根据底层弹药类型给出完善的默认值兜底。");
    WriteFileLine(file, "// 需要配置的两种情形：");
    WriteFileLine(file, "// 1. 改了手雷物理（引信/碰撞/重力/速度，如燃烧瓶、RPG）；");
    WriteFileLine(file, "// 2. 只改战术定位不改物理（如抗战整合包闪光弹当高爆用，配一个 role 即可）。");
    WriteFileLine(file, "// 在下方为对应武器类名添加个性化参数。");
    WriteFileLine(file, "//");
    WriteFileLine(file, "// 可选字段说明（未配置的字段自动继承对应角色的原版默认值；数值填 0 等同未配，唯 gravity 可填 0 关重力）：");
    WriteFileLine(file, "//   role          : 战术角色，可选 he (高爆) / flash (闪光) / smoke (烟雾)");
    WriteFileLine(file, "//   detonate      : 引爆机制，可选 fuse (延时引信) / touch (触碰即炸)");
    WriteFileLine(file, "//   detonate_time : 引信时长（秒），原版手雷均为 1.5；触碰模式无引信，该值仅作仿真飞行上界");
    WriteFileLine(file, "//   gravity       : 重力倍率（相对于 sv_gravity，原版手雷为 0.4，直飞设为 0.0）");
    WriteFileLine(file, "//   speed_mode    : 速度模式，可选 engine (原版仰角公式) / fixed (固定初速度；重力≤0.05 且配了 speed 而未写本项时自动按 fixed 处理）");
    WriteFileLine(file, "//   speed         : 初速度上限或固定初速度（默认 750.0）");
    WriteFileLine(file, "//   radius        : 效果外圈半径（小于理论伤害范围即可，可以适当缩小以预测伤害效果，见范例）");
    WriteFileLine(file, "//   safe_dist     : 安全距离，低于该距离不扔（防自伤；HE 默认 350，flash/smoke 默认 0.0，flash的“安全距离”由 BotQuickGrenade 全局变量决定，无法在此配置）");
    WriteFileLine(file, "//   range         : 理论射程，超过该距离 Bot 直接放弃求解（默认 953，长程武器如 RPG 可放宽）");
    WriteFileLine(file, "//   viewmodel     : 雷 v 模在几号 predicted_viewmodel（仅在 viewmodel_fix_mode 取 2 时有效，缺省 0，需要“藏 0 显 1”的加枪武器填 1）");
    WriteFileLine(file, "//");
    WriteFileLine(file, "// 配置重载：修改后换图生效");
    WriteFileLine(file, "// ==========================================================================");
    WriteFileLine(file, "\"QuickGrenadeWeapons\"");
    WriteFileLine(file, "{");
    WriteFileLine(file, "    // 全局 Settings 段");
    WriteFileLine(file, "    \"Settings\"");
    WriteFileLine(file, "    {");
    WriteFileLine(file, "        \"viewmodel_fix_mode\"  \"0\"         // 视图修复模式");
    WriteFileLine(file, "    }");
    WriteFileLine(file, "");
    WriteFileLine(file, "    // 范例 1：燃烧瓶 (触碰即炸：StartTouch 触发，中心+四角±30 共 5 团 env_fire×firesize 50)");
    WriteFileLine(file, "    \"weapon_molotov\"");
    WriteFileLine(file, "    {");
    WriteFileLine(file, "        \"role\"              \"he\"");
    WriteFileLine(file, "        \"detonate\"          \"touch\"       // 触地即炸");
    WriteFileLine(file, "        \"detonate_time\"     \"5.0\"         // 触碰无引信，此值仅作仿真上界（44°满抛飞约3.4s，dev 修后可删）");
    WriteFileLine(file, "        \"radius\"            \"92.0\"        // 火场覆盖（四角 42.4＋单团 50）");
    WriteFileLine(file, "        \"safe_dist\"         \"92.0\"        // 与效果圈同值：火不分敌我，进圈就烧");
    WriteFileLine(file, "        \"range\"             \"1843.0\"      // 无引信，第一落点即炸（仰角≈44°，平地）");
    WriteFileLine(file, "        \"viewmodel\"         \"1\"           // 新加枪+新机瞄，定制雷 v 模在 1 号槽");
    WriteFileLine(file, "    }");
    WriteFileLine(file, "");
    WriteFileLine(file, "    // 范例 2：火箭筒 (homingmissiles.sp，按 missile_* 默认值推导：damage 300→被引擎截到 250，");
    WriteFileLine(file, "    //         radius 800，speed 1400；改 convar 必重算。type 0 无制导直线精确，追踪/准星制导下直线仿真为保守下界)");
    WriteFileLine(file, "    \"weapon_rpg\"");
    WriteFileLine(file, "    {");
    WriteFileLine(file, "        \"role\"              \"he\"");
    WriteFileLine(file, "        \"detonate\"          \"touch\"       // OnStartTouch 即炸");
    WriteFileLine(file, "        \"detonate_time\"     \"3.0\"         // 触碰无引信，此值仅作仿真上界（3s×1400=4200，dev 修后可删）");
    WriteFileLine(file, "        \"gravity\"           \"0.0\"         // MOVETYPE_FLY 无重力");
    WriteFileLine(file, "        \"speed_mode\"        \"fixed\"       // 每 think 重置初速，恒定（跟随 missile_speed）");
    WriteFileLine(file, "        \"speed\"             \"1400.0\"      // 跟随 missile_speed（默认 1400，可配 300~3000）");
    WriteFileLine(file, "        \"radius\"            \"478.0\"       // 50血线（250威力/800半径高斯下）");
    WriteFileLine(file, "        \"safe_dist\"         \"800.0\"       // 物理边缘，界外零自伤（纯远程武器）");
    WriteFileLine(file, "        \"range\"             \"4200.0\"      // 无物理上限（飞到碰墙为止），按地图尺度取，酌情");
    WriteFileLine(file, "        \"viewmodel\"         \"1\"           // 新加枪+新机瞄，定制雷 v 模在 1 号槽");
    WriteFileLine(file, "    }");
    WriteFileLine(file, "");
    WriteFileLine(file, "    // 范例 3：双插件叠加的原版高爆雷（引信 2.5s × 威力 200 × 半径 400，");
    WriteFileLine(file, "    //         如 Rocket HE 类改引信 × snlite 改威力半径；以下数值全由此三数推导，改输入必重算）");
    WriteFileLine(file, "    \"weapon_hegrenade\"");
    WriteFileLine(file, "    {");
    WriteFileLine(file, "        \"role\"              \"he\"");
    WriteFileLine(file, "        \"detonate\"          \"fuse\"");
    WriteFileLine(file, "        \"detonate_time\"     \"2.5\"         // 延长引信");
    WriteFileLine(file, "        \"radius\"            \"222.0\"       // 50血线（200威力/400半径下）");
    WriteFileLine(file, "        \"safe_dist\"         \"400.0\"       // 物理边缘 400，界外零自伤");
    WriteFileLine(file, "        \"range\"             \"1570.0\"      // 引信 2.5s 对应理论射程（仰角≈31°，不填会沿用 953 默认误杀远目标）");
    WriteFileLine(file, "    }");
    WriteFileLine(file, "}");

    delete file;
}

/**
 * 初始化并加载武器配置文件（configs/quickgrenade_weapons.cfg）
 */
stock void QG_LoadWeaponProfilesConfig()
{
    if (g_hWeaponProfiles == null)
    {
        g_hWeaponProfiles = new StringMap();
    }
    else
    {
        g_hWeaponProfiles.Clear();
    }

    // 全局开关缺省关闭（cfg 无 Settings 段即原版行为）
    g_iQGViewModelFixMode = 0;

    char cfgPath[PLATFORM_MAX_PATH];
    BuildPath(Path_SM, cfgPath, sizeof(cfgPath), "configs/quickgrenade_weapons.cfg");

    if (!FileExists(cfgPath))
    {
        QG_WriteDefaultConfigFile(cfgPath);
    }

    KeyValues kv = new KeyValues("QuickGrenadeWeapons");
    if (!kv.ImportFromFile(cfgPath))
    {
        delete kv;
        return;
    }

    if (kv.GotoFirstSubKey())
    {
        do
        {
            char classname[64];
            kv.GetSectionName(classname, sizeof(classname));
            TrimString(classname);
            if (classname[0] == '\0')
                continue;

            // 全局 Settings 段不是武器：消费全局配置后跳过，不进画像字典
            if (StrEqual(classname, "Settings", false))
            {
                int mode = kv.GetNum("viewmodel_fix_mode", 0);
                g_iQGViewModelFixMode = (mode == 1 || mode == 2) ? mode : 0;
                continue;
            }

            char roleStr[16];
            kv.GetString("role", roleStr, sizeof(roleStr), "he");
            int role = GRENADE_ROLE_HE;
            if (StrEqual(roleStr, "flash", false))
                role = GRENADE_ROLE_FLASH;
            else if (StrEqual(roleStr, "smoke", false))
                role = GRENADE_ROLE_SMOKE;

            // 先赋上该角色的标准原版默认值（未配置字段 100% 自动优雅兜底）
            GrenadeProfile profile;
            profile.InitDefault(role);

            // 引爆方式
            char detonateStr[16];
            kv.GetString("detonate", detonateStr, sizeof(detonateStr), "");
            if (StrEqual(detonateStr, "touch", false))
            {
                profile.detonateMode = DETONATE_TOUCH;
            }
            else if (StrEqual(detonateStr, "fuse", false))
            {
                profile.detonateMode = DETONATE_FUSE;
            }

            // 引信时间
            float detTime = kv.GetFloat("detonate_time", 0.0);
            if (detTime > 0.0)
            {
                profile.detonateTime = detTime;
            }

            // 重力倍率（缺省 0.4；显式 0 关重力；非法值按缺省处理，不静默变直飞）
            float gravScale = kv.GetFloat("gravity", -1.0);
            if (gravScale >= 0.0)
            {
                profile.gravityScale = gravScale;
            }

            // 速度模式与初速度
            char speedModeStr[16];
            kv.GetString("speed_mode", speedModeStr, sizeof(speedModeStr), "");
            if (StrEqual(speedModeStr, "fixed", false))
            {
                profile.speedMode = SPEED_FIXED;
            }
            else if (StrEqual(speedModeStr, "engine", false))
            {
                profile.speedMode = SPEED_ENGINE;
            }

            float spd = kv.GetFloat("speed", 0.0);
            if (spd > 0.0)
            {
                profile.speed = spd;
                if (profile.gravityScale <= 0.05 && speedModeStr[0] == '\0')
                {
                    profile.speedMode = SPEED_FIXED;
                }
            }

            // 半径与自伤防范距离
            float radius = kv.GetFloat("radius", 0.0);
            if (radius > 0.0)
            {
                profile.hitRadius = radius;
            }

            float minDist = kv.GetFloat("safe_dist", 0.0);
            bool hasMinDist = (minDist > 0.0);
            if (hasMinDist)
            {
                profile.minThrowDist = minDist;
            }

            float maxDist = kv.GetFloat("range", 0.0);
            if (maxDist > 0.0)
            {
                profile.maxThrowDist = maxDist;
            }

            // 雷 v 模槽位（缺省 0；新加枪定制雷填 1，详见 cfg 注释）
            int vslot = kv.GetNum("viewmodel", 0);
            profile.viewmodel = (vslot == 1) ? 1 : 0;

            g_hWeaponProfiles.SetArray(classname, profile, sizeof(profile));

        } while (kv.GotoNextKey());
    }

    delete kv;
}

/**
 * 查询类名在配置文件中的显式战术角色（cfg 优先发现的核心）
 *
 * @param classname   武器类名（如 weapon_molotov）
 * @return            配置的角色；未配置返回 GRENADE_ROLE_UNKNOWN
 */
stock int QG_GetConfiguredRole(const char[] classname)
{
    if (classname[0] == '\0' || g_hWeaponProfiles == null)
        return GRENADE_ROLE_UNKNOWN;

    GrenadeProfile customProfile;
    if (g_hWeaponProfiles.GetArray(classname, customProfile, sizeof(customProfile)))
        return customProfile.role;

    return GRENADE_ROLE_UNKNOWN;
}

/**
 * 查询类名的雷 v 模槽位（mode 2 分流判据；用法见 cfg 注释）
 *
 * @param classname   武器类名（如 weapon_molotov）
 * @return            1=雷 v 模在 1 号槽，0=在 0 号槽（缺省）
 */
stock int QG_GetGrenadeViewSlot(const char[] classname)
{
    if (classname[0] == '\0' || g_hWeaponProfiles == null)
        return 0;

    GrenadeProfile customProfile;
    if (g_hWeaponProfiles.GetArray(classname, customProfile, sizeof(customProfile))
        && customProfile.viewmodel == 1)
        return 1;

    return 0;
}

/**
 * 寻找玩家背包中指定战术角色的首个有弹手雷实体（cfg 优先，弹药序号回退）
 *
 * 与 quickgrenade.inc 的 QG_FindGrenadeByRole 的唯一区别：先查 cfg 的 classname→role
 * 映射，再回退到底层弹药序号。复用原版弹药序号的加枪雷两者等价；使用全新弹药序号的
 * 自定义雷只有这个函数能发现。需要 include <quickgrenade_profile> 且加载过配置。
 * 备弹无条件必查（是雷就有弹药序号，见 quickgrenade.inc 同名函数注释）。
 *
 * @param client    玩家 Client Index
 * @param role      手雷战术角色（GRENADE_ROLE_HE, GRENADE_ROLE_FLASH, GRENADE_ROLE_SMOKE）
 * @return          实体索引，无则 -1
 */
stock int QG_FindGrenadeByRoleEx(int client, int role)
{
    if (role == GRENADE_ROLE_UNKNOWN)
        return -1;

    for (int i = 0; i < 48; i++)
    {
        int weapon = GetEntPropEnt(client, Prop_Send, "m_hMyWeapons", i);
        if (weapon > 0 && IsValidEntity(weapon))
        {
            char cls[64];
            GetEntityClassname(weapon, cls, sizeof(cls));

            int wrole = QG_GetConfiguredRole(cls);
            if (wrole == GRENADE_ROLE_UNKNOWN)
                wrole = QG_GetGrenadeRole(weapon);

            if (wrole != role)
                continue;

            // 是雷就有弹药序号，备弹无条件必查
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
    return -1;
}
