#if defined _quickgrenade_global_included
 #endinput
#endif
#define _quickgrenade_global_included

//========================================================================================
// DEFINES
//========================================================================================

#define PLUGIN_VERSION        "1.0"

#define EF_NODRAW             32

//========================================================================================
// HANDLES & VARIABLES
//========================================================================================

// 全局状态
bool g_bfreezetime = false;                                   // 冻结时间/拆弹/回合结束阶段无法触发快速手雷
bool isCombat[MAXPLAYERS+1] = {false};                        // 快速手雷期间不能再次触发（防重复与防捡枪切枪）
QuickGrenadeState g_PlayerState[MAXPLAYERS+1] = {STATE_IDLE};   // 玩家快速手雷状态
int PreWeapon[MAXPLAYERS+1] = {-1};                           // 原来武器的实体号
int g_iActiveGrenadeEnt[MAXPLAYERS+1] = {-1};                 // 正在投掷的手雷实体号（实体直驱唯一真源）

float g_fHoldStartTime[MAXPLAYERS+1] = {0.0};                 // 开始长按引雷的时间戳
bool g_bCmdHolding[MAXPLAYERS+1] = {false};                   // 是否正通过 +cmd 持续长按中
Handle g_hDelayThrowTimer[MAXPLAYERS+1] = {null};             // 最小引雷延时定时器
Handle g_hSwitchWeaponTimer[MAXPLAYERS+1] = {null};           // 投掷后切枪定时器

int ClientVM3[MAXPLAYERS+1][2];                               // 存放客户端的两个视图模型
int Fov[MAXPLAYERS+1] = {0};                                  // 记录初始FOV值

// ConVar Handles
ConVar cvarEnable;
ConVar cvarBackType;
ConVar cvarMinPullTime;
ConVar cvarThrowDelayTime;
ConVar cvarAutoThrowTime;
ConVar cvarForbiddenList;
ConVar cvarFixViewModel;
ConVar cvarPriority;
