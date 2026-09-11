[English](README_EN.md) | 中文

# QuickSeries（快速战斗系列）

针对 CS:S 引擎特性打造的快速战斗动作插件套件，包含 **QuickMelee（快速近战）** 与 **QuickGrenade（快速手雷）** 两个核心插件。

所有插件均采用模块化（拆分子 `.inc`）设计，并提供完善的原生 API（Natives）与**跨插件动作互斥与协同防冲突机制**。

---

## 插件列表与核心功能

### 1. QuickMelee（快速近战挥刀）
- **一键瞬发近战**：跳过拔刀 Deploy 动画，强制执行轻击/重击挥刀并触发伤害判定，攻击完成后自动平滑切回主武器/副武器。
- **开镜与机瞄兼容**：狙击开镜中挥刀自动复位 FOV，兼容机瞄武器 `EF_NODRAW` 视图模型还原。
- **防打断保护**：挥刀期间拦截地面捡枪自动切枪，死亡、冻结期与拆弹阶段安全拦截。

### 2. QuickGrenade（快速手雷引雷与投掷）
- **长按引雷与瞄准**：支持 `+sm_quickfrag` / `+sm_quicksmoke` / `+sm_quickflash` 物理长按，第一人称视图模型持续保持拔销待发姿势，可在引雷状态下自由跑位、跳跃与瞄准。
- **松手即丢**：松开按键瞬间触发丢雷动画（`ACT_VM_THROW` + 人物 3D 投掷动作），并在实体生成后无缝切回原武器。
- **点按保护**：设有最短引雷时间保护（`min_pull_time`），彻底杜绝极快轻点按键时手臂抽搐问题。

---

## 跨插件「动作互斥与防冲突」机制

两款插件作为可选依赖相互协同，原生处理动作冲突：

1. **手雷引雷中按挥刀**：当玩家正长按 `+sm_quickfrag` 引雷瞄准时，若突然遭遇近身敌人并按下 `sm_quickmelee`，插件会自动安全调用 `QuickGrenade_Cancel` 取消丢雷，并**立即无缝转入快速挥刀**。
2. **挥刀中按丢雷**：当玩家正处于挥刀动作（~0.3s）时，若按下丢雷按键，插件会**自动拦截丢雷请求**，避免挥刀后摇与手雷掏取动作产生逻辑与动画冲突。
3. **丢雷脱手阶段**：手雷已松手正在飞出判定阶段（0.1s~0.2s），挥刀会被安全拦截，确保手雷实体必定成功发射。

---

## 目录结构

```
F:\Git\cs-source-dev\Ducheese\quickseries\
├── .gitignore
├── LICENSE.txt
├── README.md
└── addons\sourcemod\scripting\
    ├── QuickMelee.sp                    QuickMelee 插件入口
    ├── QuickGrenade.sp                  QuickGrenade 插件入口
    ├── include\                         公共 API 头文件
    │   ├── quickmelee.inc               QuickMelee 开发者头文件
    │   └── quickgrenade.inc             QuickGrenade 开发者头文件
    ├── QuickMelee\                      QuickMelee 内部子模块
    │   ├── global.inc                   全局定义与变量
    │   ├── helper.inc                   切枪与工具函数
    │   ├── core.inc                     核心挥刀与防冲突调度
    │   ├── commands.inc                 指令注册
    │   └── hooks.inc                    按键与事件监听
    └── QuickGrenade\                    QuickGrenade 内部子模块
        ├── global.inc                   全局定义与状态机变量
        ├── helper.inc                   背包检索与切枪工具函数
        ├── core.inc                     核心引雷、投掷与防冲突调度
        ├── commands.inc                 指令注册（+cmd / -cmd）
        └── hooks.inc                    按键注入与动画时间锁定钩子
```

---

## 控制台指令与键位绑定

### QuickMelee
| 功能 | 指令 | 推荐绑定 |
| :--- | :--- | :--- |
| **快速近战** | `sm_quickmelee` | `bind mouse3 sm_quickmelee` |

### QuickGrenade
| 手雷类型 | 对应武器 | 推荐长按绑定（按住引雷，松手丢雷） | 单次点按指令 |
| :--- | :--- | :--- | :--- |
| **高爆手雷** | `weapon_hegrenade` | `bind v +sm_quickfrag` / `+sm_quickhe` | `sm_quickfrag` / `sm_quickhe` |
| **烟雾弹** | `weapon_smokegrenade` | `bind v +sm_quicksmoke` | `sm_quicksmoke` |
| **闪光弹** | `weapon_flashbang` | `bind v +sm_quickflash` | `sm_quickflash` |
| **战术优先级** | 按 `sm_quickgrenade_priority` 顺序耗尽 | `bind g +sm_quicktac` | `sm_quicktac` |

---

## ConVar 参数配置

### `cfg/sourcemod/plugin.quickmelee.cfg`
| ConVar | 默认值 | 说明 |
| :--- | :---: | :--- |
| `sm_quickmelee_enable` | `1` | 插件开关（1：启用；0：禁用） |
| `sm_quickmelee_hit_type` | `1` | 攻击类型（1：左键轻击；0：右键重击） |
| `sm_quickmelee_back_type` | `2` | 挥刀后切枪（2：切回原武器；1：优先主武器其次副武器；0：保持持刀） |
| `sm_quickmelee_switch_weapon_time` | `0.4` | 从触发到切回武器的时间间隔（秒） |
| `sm_quickmelee_forbidden` | `"weapon_minigun"` | 禁用快速近战的武器黑名单 |
| `sm_quickmelee_fix_viewmodel` | `0` | 挥刀时是否修复双视图模型（0：不干预；1：统统藏1号亮0号；2：仅原版刀藏1亮0，其他放行） |

### `cfg/sourcemod/plugin.quickgrenade.cfg`
| ConVar | 默认值 | 说明 |
| :--- | :---: | :--- |
| `sm_quickgrenade_enable` | `1` | 插件开关（1：启用；0：禁用） |
| `sm_quickgrenade_back_type` | `2` | 投掷后切枪（2：切回原武器；1：优先主武器其次副武器；0：保持持雷） |
| `sm_quickgrenade_min_pull_time` | `0.2` | 最短引雷动画时间（秒，保护点按动作完整） |
| `sm_quickgrenade_throw_delay_time` | `0.6` | 松开引雷后等待投掷完成再切武器的时间（秒，必须 > 0.15s） |
| `sm_quickgrenade_auto_throw_time` | `0.25` | 单次触发指令时自动引雷多久后投掷（秒） |
| `sm_quickgrenade_forbidden` | `"weapon_minigun"` | 禁用快速手雷的武器黑名单 |
| `sm_quickgrenade_priority` | `"FSH"` | 战术手雷优先级编码（F=闪光 S=烟雾 H=高爆，按顺序耗尽） |

### `configs/quickgrenade_weapons.cfg`（武器画像，改后换图生效）

| 配置项 | 默认值 | 说明 |
| :--- | :---: | :--- |
| `Settings.viewmodel_fix_mode` | `0` | 视图修复：0=不干预（原版/HAN）；1=统统藏1亮0（老加枪/老机瞄）；2=仅原版三雷藏1亮0，加枪等其他类名直接放行（新加枪/新机瞄） |
| 条目 `role` | `he` | 战术角色：flash 弹药当 HE 使时配 `he`（唯一用途） |
| 条目 `detonate` | `fuse` | `touch`=触碰即炸（燃烧瓶/RPG 类） |
| 条目 `safe_dist` / `range` | 缺省 | 安全距离（低于不扔）/理论射程（超出 Bot 放弃）；填 0 等同未配 |

BotQuickGrenade 已全量接入画像（HE/烟/闪求解、预筛、HUD 计数），原版服缺省行为见上表。

---

## 开发者 API

### `include <quickmelee>`
```sourcepawn
// 检查玩家当前是否正处于快速近战挥刀状态
native bool QuickMelee_IsCombat(int client);

// 手动为玩家触发快速近战流程
native bool QuickMelee_Trigger(int client);

// 取消快速近战并切回原武器
native void QuickMelee_Cancel(int client);
```

### `include <quickgrenade>`
```sourcepawn
// 检查玩家当前是否正处于快速手雷过程中
native bool QuickGrenade_IsCombat(int client);

// 获取玩家当前手雷状态枚举 (STATE_IDLE, STATE_HOLDING, STATE_THROWING, STATE_SWITCHING)
native QuickGrenadeState QuickGrenade_GetState(int client);

// 手动为玩家触发快速手雷（类名精确匹配，行为不变，老插件无需重编）
native bool QuickGrenade_Trigger(int client, const char[] grenadeClassname, bool isHold = false);

// 按战术角色触发快速手雷（GRENADE_ROLE_HE/SMOKE/FLASH，加枪雷复用原版弹药序号即自动命中）
native bool QuickGrenade_TriggerByRole(int client, int role, bool isHold = false);

// 取消快速手雷引雷并切回原武器
native void QuickGrenade_Cancel(int client);
```
