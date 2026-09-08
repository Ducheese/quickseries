#if defined _botquickgrenade_global_included
  #endinput
#endif
#define _botquickgrenade_global_included

//========================================================================================
// DEFINES & CONSTANTS
// 所有距离、时间、物理步长等参数集中统一管理
//========================================================================================

#define PLUGIN_VERSION              "1.0"

// --- 投雷公共判定与物理模拟参数 ---
#define MAX_SEARCH_DIST             999999.0 // 搜索初值极大值（距离无穷大哨兵值）
#define SIM_TIMESTEP                0.05    // 弹道模拟积分步长（关系到弹道的平滑精度）
#define ANGLE_OVERRIDE_DUR          1.5     // 投雷动作期间强制覆盖视角锁定的持续时间（秒，锁自家动作时间线，与引信无关）

// --- 仰角搜索范围与步频 ---
#define PITCH_MIN                  -45.0    // 最大高抛仰角（向天空高抛，负数为仰角，收敛至 -45° 防止触碰天顶）
#define PITCH_MAX                   20.0    // 最大俯角（向低处投掷，限制在 20° 防止向下砸地自炸）
#define PITCH_STEP_COARSE           6.0     // 粗搜步长（度）
#define PITCH_STEP_FINE             1.0     // 细搜步长（度，在粗搜最优结果附近精化）

// --- 感知与记忆系统参数 ---
#define BOT_THINK_INTERVAL          0.2     // Bot AI 主思考循环间隔（0.2s 搭配 5 帧错峰轮询，单 Bot 等效 1.0s）
#define MEMORY_DURATION             5.0     // 敌人最后暴露坐标（Last Known Position）记忆有效期（秒）
#define BOT_FOV_HALF                60.0    // Bot 视野半角（度，总视锥 120°）
#define VISION_MAX_RANGE            1600.0  // 视觉直接感知最大有效视距（可以比画像 range 大）
#define GUNFIRE_HEARING_RANGE       1350.0  // 未消音武器开火的听觉感知范围（可以比画像 range 大）
#define GUNFIRE_SILENCED_RANGE      600.0   // 消音武器开火的听觉感知范围
#define FOOTSTEP_HEARING_RANGE      450.0   // 敌人跑步脚步声的听觉感知范围（8到10米内的脚步声）
#define SOUND_DISPERSION_GUNFIRE    60.0    // 听觉枪声位置的随机模糊散布半径（模仿听声辨位不确定度）
#define SOUND_DISPERSION_FOOTSTEP   40.0    // 听觉脚步位置的随机模糊散布半径

// --- 高爆手雷 (HE Grenade) 专属参数 ---
#define HE_COOLDOWN                 12.0    // HE 投掷冷却时间（秒）
stock const int COLOR_HE[4]       = {255,  50,  50, 220};  // 红色：HE 弹道与落点

// --- 战术烟雾弹 (Smoke Grenade) 与 C4 战术专属参数 ---
#define SMOKE_COOLDOWN              15.0    // Smoke 投掷冷却时间（秒）
#define SMOKE_HEALTH_THRESHOLD      35      // 残血自保脚底烟触发血量阈值 (HP <= 35)
#define SMOKE_INTERPOLATION_RATIO   0.45    // 弹夹打空/换弹时视线中点烟的插值比例（45% 偏向己方一侧）
#define SMOKE_SELF_OFFSET           80.0    // 残血/致盲脚底自保烟沿视线前推距离
#define SMOKE_SELF_MIN_THREAT_DIST  350.0   // 残血脚底自保烟允许投掷的最小威胁距离（HU，小于该距离且手中有弹未换弹时禁止拔烟送死）
#define SMOKE_BLIND_MIN_DURATION    1.5     // 触发被闪致盲脱困脚底烟的最小剩余致盲时长（秒）
#define SMOKE_BLIND_MIN_ALPHA       200.0   // 触发被闪致盲脱困脚底烟的最小致盲不透明度（满白 255，排除余光轻微晃眼）
#define C4_DEFUSE_APPROACH_DIST     400.0   // CT 靠近 C4 准备封烟拆包的最大搜索距离
#define C4_PLANT_SELF_OFFSET        50.0    // T 持包进点身前铺烟推移距离
#define C4_PLANTER_COVER_MIN_DIST   100.0   // T 掩护队友下包最小安全投掷距离
#define C4_DROPPED_SEARCH_DIST      600.0   // T 掩护拾取掉落 C4 的最大感知搜索距离
#define C4_TEAM_SMOKE_COOLDOWN      12.0    // 阵营 C4 战术封烟共享冷却时间（秒，防止多队友扎堆重复封烟）
stock const int COLOR_SMOKE[4]    = { 50, 220,  50, 220};  // 绿色：Smoke 弹道与落点

// --- 战术闪光弹 (Flashbang) 专属参数 ---
#define FLASH_COOLDOWN              10.0    // Flash 投掷冷却时间（秒）
#define FLASH_BLIND_DOT_MIN         0.6     // 敌人有效强致盲最小朝向 Dot 阈值（对应 CS:S 源码强致盲第一档：dot >= 0.6，约 53° 正对锥）
#define FLASH_SELF_BACK_DOT_MAX    -0.2     // 自身背身防自闪 Dot 上限（对应 CS:S 源码背身最弱档阈值：dot < -0.2）
#define FLASH_SELF_SAFE_DIST_BACK   1000.0  // 无掩体背身防自闪安全距离（HU，源码背身档 fadeHold < 0.5s 对应 dist > 1000 HU，约等于原版画像 range（953））
#define FLASH_SELF_SAFE_DIST_FRONT  2800.0  // 无掩体非背身防自闪安全距离（HU，源码直视强致盲档 fadeTime < 0.5s 需 dist > 2800 HU，超出投雷上限，等效绝不丢）
#define FLASH_TEAM_MATE_DOT_MIN     0.0     // 队友受闪危险视线 Dot 阈值（dot > 0.0 即面朝闪光半球视作有误伤致盲风险）
stock const int COLOR_FLASH[4]    = {  0, 200, 255, 220};  // 亮蓝色：Flash 弹道与落点

// --- 可视化渲染参数 ---
#define CIRCLE_SEGS                 24      // 落点判定圆分段数
#define BEAM_WIDTH                  1.5     // 弹道光束宽度
#define BEAM_LIFE                   1.5     // 弹道与圆圈残留时间（秒）

// 弹道 HULL 碰撞体半尺寸（用于手雷撞墙反弹的物理结算）
stock const float SIM_MINS[3]     = {-2.0, -2.0, -2.0};
stock const float SIM_MAXS[3]     = { 2.0,  2.0,  2.0};

//========================================================================================
// HANDLES & GLOBAL VARIABLES
//========================================================================================

// 手雷功能独立开关 ConVar Handles
ConVar cvarHeEnable;
ConVar cvarSmokeEnable;
ConVar cvarFlashEnable;

// 战术配置与通用 ConVar Handles
ConVar cvarSmokeHp;
ConVar cvarRequireCover;
ConVar cvarFriendlyFire;
ConVar cvarGiveGrenades;
ConVar cvarBlockGrenade;
ConVar cvarPlayerPreview;
ConVar cvarDebug;
ConVar cvarGravity;

// 资源索引
int g_iBeamSprite = -1;

// 各手雷投掷冷却时间戳
float g_fLastHeThrowTime[MAXPLAYERS + 1]    = {0.0};         // 上次投掷 HE 时间戳
float g_fLastSmokeThrowTime[MAXPLAYERS + 1] = {0.0};         // 上次投掷 Smoke 时间戳
float g_fLastFlashThrowTime[MAXPLAYERS + 1] = {0.0};         // 上次投掷 Flash 时间戳
float g_fFlashEndTime[MAXPLAYERS + 1]       = {0.0};         // 玩家当前闪光致盲结束的时间戳

// C4 战术全局状态
bool  g_bIsC4Planted                        = false;         // 场上是否存在已安放的 C4
float g_vecPlantedC4Pos[3];                                  // 已安放 C4 的 3D 坐标
bool  g_bIsC4Dropped                        = false;         // 场上是否存在掉落的 C4
float g_vecDroppedC4Pos[3];                                  // 掉落 C4 的 3D 坐标
float g_fLastTeamC4SmokeTime[4]             = {0.0};         // 阵营上次 C4 战术封烟时间戳（CT=3, T=2）

// 视线与朝向覆写
bool  g_bAngleOverride[MAXPLAYERS + 1]      = {false};       // 是否处于朝向锁定中
float g_fOverrideExpiry[MAXPLAYERS + 1]     = {0.0};         // 朝向锁定到期时间戳
// 用前赋值
float g_fOverrideAngles[MAXPLAYERS + 1][3];                  // 锁定的目标欧拉角 (pitch, yaw, roll)

// Bot 感知与短期记忆库: [bot_index][target_index]
// 数据本身是瞬时快照，而非动态追踪
float g_fLastPerceivedTime[MAXPLAYERS + 1][MAXPLAYERS + 1];  // 目标最后一次被感知的绝对时间戳
float g_vecLastKnownPos[MAXPLAYERS + 1][MAXPLAYERS + 1][3];  // 目标最后一次被感知的 3D 空间坐标
