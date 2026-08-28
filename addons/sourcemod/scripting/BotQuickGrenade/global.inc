#if defined _botquickgrenade_global_included
  #endinput
#endif
#define _botquickgrenade_global_included

//========================================================================================
// DEFINES & CONSTANTS
// 所有距离、时间、物理步长等参数集中统一管理
//========================================================================================

#define PLUGIN_VERSION              "1.0"

// --- 投雷判定与物理参数 ---
#define DIST_MIN                    350.0   // Bot 与目标最近有效投雷距离（HE 默认的伤害范围，350.0 以外才算完全无伤害）
#define DIST_MAX                    1050.0  // Bot 与目标最远有效投雷距离（默认 1.5 秒引信时间，抛物线最远就是20米左右）

#define HE_HIT_RADIUS               160.0   // HE 落点命中有效判定半径（在这个范围内伤害比较高）

#define GRENADE_FLIGHT_TIME         1.5     // 弹道模拟最长飞行时间上限（取引信时间）
#define SIM_TIMESTEP                0.05    // 弹道模拟积分步长（关系到弹道的平滑精度）

#define GRENADE_COOLDOWN            12.0    // 单个 Bot 丢雷后的独立冷却时间（秒）

// --- 仰角搜索范围与步频 ---
#define PITCH_MIN                  -50.0    // 最大高抛仰角（向天空高抛，负数为仰角，收敛至 -50° 防止触碰天顶空气墙）
#define PITCH_MAX                   50.0    // 最大俯角（向低处投掷，限制在 20° 防止向下砸地自炸）
#define PITCH_STEP_COARSE           6.0     // 粗搜步长（度）
#define PITCH_STEP_FINE             1.0     // 细搜步长（度，在粗搜最优结果附近精化）

// --- 感知与记忆系统参数 ---
#define BOT_THINK_INTERVAL          0.2     // Bot AI 主思考与扫描循环间隔（0.2s 搭配 5 帧错峰轮询，单 Bot 等效 1.0s 周期，彻底消除同 Tick 物理射线尖峰）
#define MEMORY_DURATION             10.0    // 敌人最后暴露坐标（Last Known Position）记忆有效期（算是在假设这段时间内，敌人不会转点特别远）

#define BOT_FOV_HALF                60.0    // Bot 视野半角（度，总视锥 120°）
#define VISION_MAX_RANGE            1600.0  // 视觉直接感知最大有效视距（可以比 DIST_MAX 大）

#define GUNFIRE_HEARING_RANGE       1350.0  // 未消音武器开火的听觉感知范围（可以比 DIST_MAX 大）
#define GUNFIRE_SILENCED_RANGE      600.0   // 消音武器开火的听觉感知范围
#define FOOTSTEP_HEARING_RANGE      450.0   // 敌人跑步脚步声的听觉感知范围（8到10米内的脚步声）

#define SOUND_DISPERSION_GUNFIRE    60.0    // 听觉枪声位置的随机模糊散布半径（模仿听声辨位不确定度）
#define SOUND_DISPERSION_FOOTSTEP   40.0    // 听觉脚步位置的随机模糊散布半径

// --- 战术与安全检查参数 ---
#define TEAMMATE_SAFETY_RADIUS      220.0   // 队友防误伤安全半径（若爆炸点该范围内有存活队友则终止投雷）
#define ANGLE_OVERRIDE_DUR          1.5     // 投雷动作期间强制覆盖视角锁定的持续时间（秒）

// --- 烟雾弹战术参数 ---
#define SMOKE_HEALTH_THRESHOLD      35      // 残血自保脚底烟触发血量阈值 (HP <= 35)
#define SMOKE_COOLDOWN              15.0    // 单个 Bot 投掷烟雾弹的独立冷却时间（秒）
#define SMOKE_INTERPOLATION_RATIO   0.45    // 弹夹打空/换弹时视线中点烟的插值比例（45% 偏向己方一侧）
#define SMOKE_SELF_OFFSET           80.0    // 残血脚底自保烟沿视线前推距离（单位：游戏距离）
#define C4_DEFUSE_APPROACH_DIST     400.0   // CT 靠近 C4 准备封烟拆包的最大搜索距离
#define SMOKE_HIT_RADIUS            220.0   // 烟雾弹有效落点判定半径（烟雾扩散范围大，放宽至 220）

// --- 可视化渲染参数 ---
#define CIRCLE_SEGS                 24      // 落点判定圆分段数
#define BEAM_WIDTH                  1.5     // 弹道光束宽度
#define BEAM_LIFE                   1.5     // 弹道与圆圈残留时间（秒）

// --- 测试辅助参数 ---
#define TEST_BOT_GRENADE_AMMO       10      // 测试模式下 Bot 出生补给的手雷数量

// 弹道 HULL 碰撞体半尺寸与碰撞掩码（采用 CS:S 引擎手雷原生碰撞规则，不被玩家空气墙阻挡）
stock const float SIM_MINS[3] = {-2.0, -2.0, -2.0};    // 用于手雷撞墙反弹的物理结算
stock const float SIM_MAXS[3] = { 2.0,  2.0,  2.0};
#define MASK_GRENADE_SIM            MASK_SHOT_HULL

// 可视化颜色常量（RGBA）
stock const int COLOR_TRAJ[4] = {255, 160,   0, 220};  // 橙色：弹道抛物线
stock const int COLOR_HIT[4]  = {  0, 220,  60, 220};  // 绿色：命中判定圆
stock const int COLOR_MISS[4] = {255,  40,  40, 220};  // 红色：未命中/不合格落点
stock const int COLOR_SMOKE[4]= {180, 180, 200, 220};  // 灰白色：烟雾弹落点判定圆

//========================================================================================
// HANDLES & GLOBAL VARIABLES
//========================================================================================

// ConVar Handles
ConVar cvarEnable;
ConVar cvarDebug;
ConVar cvarRequireCover;
ConVar cvarFriendlyFire;
ConVar cvarSmokeEnable;
ConVar cvarSmokeHp;
ConVar cvarBulletScale;
ConVar cvarBotGrenades;
ConVar cvarGravity;

// 资源索引
int g_iBeamSprite = -1;

// 各 Bot 独立状态
float g_fLastThrowTime[MAXPLAYERS + 1]      = {0.0};         // 上次丢 HE 手雷时间戳
float g_fLastSmokeThrowTime[MAXPLAYERS + 1] = {0.0};         // 上次丢烟雾弹时间戳

// C4 战术全局状态
bool  g_bIsC4Planted                        = false;         // 场上是否存在已安放的 C4
float g_vecPlantedC4Pos[3];                                  // 已安放 C4 的 3D 坐标
bool  g_bIsC4Dropped                        = false;         // 场上是否存在掉落的 C4
float g_vecDroppedC4Pos[3];                                  // 掉落 C4 的 3D 坐标

// 视线与朝向覆写
bool  g_bAngleOverride[MAXPLAYERS + 1]      = {false};       // 是否处于朝向锁定中
float g_fOverrideAngles[MAXPLAYERS + 1][3];                  // 锁定的目标欧拉角 (pitch, yaw, roll)
float g_fOverrideExpiry[MAXPLAYERS + 1]     = {0.0};         // 朝向锁定到期时间戳

// Bot 感知与短期记忆库: [bot_index][target_index]
float g_fLastPerceivedTime[MAXPLAYERS + 1][MAXPLAYERS + 1];  // 目标最后一次被感知的绝对时间戳
float g_vecLastKnownPos[MAXPLAYERS + 1][MAXPLAYERS + 1][3];  // 目标最后一次被感知的 3D 空间坐标
