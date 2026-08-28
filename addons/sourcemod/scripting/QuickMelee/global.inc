#if defined _quickmelee_global_included
 #endinput
#endif
#define _quickmelee_global_included

//========================================================================================
// DEFINES
//========================================================================================

#define PLUGIN_VERSION        "1.4"

#define EF_NODRAW             32

//========================================================================================
// HANDLES & VARIABLES
//========================================================================================

// 全局变量
bool g_bfreezetime = false;                   // 冻结/拆弹/回合结束阶段无法触发快速近战
bool isCombat[MAXPLAYERS+1] = {false};        // 快速近战期间不能再次触发（防重复与防捡枪切枪）
int PreWeapon[MAXPLAYERS+1] = {-1};           // 原来武器的实体号

int ClientVM3[MAXPLAYERS+1][2];               // 存放客户端的两个视图模型
int Fov[MAXPLAYERS+1] = {0};                  // 记录初始FOV值

int IsInWhatClick[MAXPLAYERS+1] = {0};        // 攻击注入标志（1=IN_ATTACK左键轻击，2=IN_ATTACK2右键重击）
Handle g_hSwitchTimer[MAXPLAYERS+1] = {null}; // 切武器延时定时器

// ConVar Handles
ConVar cvarEnable;
ConVar cvarHitType;
ConVar cvarBackType;
ConVar cvarSwitchWeaponTime;
ConVar cvarForbiddenList;
