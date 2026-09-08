[中文](README.md) | English

# QuickSeries

A fast-paced combat action suite for CS:S, containing **QuickMelee (Quick Melee)** and **QuickGrenade (Quick Grenade)**.

All plugins use a modular design (split into sub `.inc` files) and provide full native APIs with **cross-plugin mutual exclusion and collision handling**.

---

## Plugin List and Core Features

### 1. QuickMelee (Quick Melee Swing)
- **One-key instant melee**: Skips the knife Deploy animation, forces a light/heavy slash with damage, and automatically and smoothly switches back to the primary/secondary weapon after the attack.
- **Scope & ironsight compatible**: Automatically restores FOV when slashing while scoped; compatible with ironsight weapons via `EF_NODRAW` viewmodel restoration.
- **Anti-interruption protection**: Blocks auto-switch from picking up weapons on the ground during the swing; safely blocked while dead, in freezetime, or defusing.

### 2. QuickGrenade (Quick Grenade Pull & Throw)
- **Hold to cook & aim**: Supports `+sm_quickfrag` / `+sm_quicksmoke` / `+sm_quickflash` hold-to-cook, the first-person viewmodel keeps the pull-pin pose, you can move, jump and aim freely while cooking.
- **Release to throw**: Triggers the throw animation (`ACT_VM_THROW` + 3D throw) instantly on release, and seamlessly switches back to the previous weapon after the entity spawns.
- **Tap protection**: Minimum pull time protection (`min_pull_time`) completely prevents arm jitter on very short taps.

---

## Cross-Plugin Mutual Exclusion

The two plugins are optional dependencies of each other and handle conflicts natively:

1. **Cooking grenade + press melee**: When the player is holding `+sm_quickfrag` to cook and suddenly presses `sm_quickmelee` due to a close encounter, the plugin will automatically and safely call `QuickGrenade_Cancel` to cancel the throw and **immediately and seamlessly switch to the quick melee swing**.
2. **Melee in progress + press grenade**: When the player is in the melee swing (~0.3s), pressing the grenade key will **automatically block the grenade request** to avoid animation and logic conflicts between the melee recovery and grenade pull.
3. **Grenade in flight**: During the phase where the grenade has been released and is flying (0.1s–0.2s), melee will be safely blocked to ensure the grenade entity is always spawned successfully.

---

## Directory Structure

```
F:\Git\cs-source-dev\Ducheese\quickseries\
├── .gitignore
├── LICENSE.txt
├── README.md
└── addons\sourcemod\scripting\
    ├── QuickMelee.sp                    QuickMelee plugin entry
    ├── QuickGrenade.sp                  QuickGrenade plugin entry
    ├── include\                         Public API headers
    │   ├── quickmelee.inc               QuickMelee developer header
    │   └── quickgrenade.inc             QuickGrenade developer header
    ├── QuickMelee\                      QuickMelee sub-modules
    │   ├── global.inc                   Global definitions and variables
    │   ├── helper.inc                   Weapon switch and utility functions
    │   ├── core.inc                     Core swing and conflict handling
    │   ├── commands.inc                 Command registration
    │   └── hooks.inc                    Button and event listeners
    └── QuickGrenade\                    QuickGrenade sub-modules
        ├── global.inc                   Global definitions and state variables
        ├── helper.inc                   Inventory search and weapon switch utilities
        ├── core.inc                     Core pull, throw and conflict handling
        ├── commands.inc                 Command registration (+cmd / -cmd)
        └── hooks.inc                    Button injection and animation lock hooks
```

---

## Console Commands and Binds

### QuickMelee
| Function | Command | Suggested bind |
| :--- | :--- | :--- |
| **Quick melee** | `sm_quickmelee` | `bind mouse3 sm_quickmelee` |

### QuickGrenade
| Grenade type | Weapon | Suggested hold bind (hold to cook, release to throw) | Single tap command |
| :--- | :--- | :--- | :--- |
| **HE Grenade** | `weapon_hegrenade` | `bind v +sm_quickfrag` / `+sm_quickhe` | `sm_quickfrag` / `sm_quickhe` |
| **Smoke Grenade** | `weapon_smokegrenade` | `bind v +sm_quicksmoke` | `sm_quicksmoke` |
| **Flashbang** | `weapon_flashbang` | `bind v +sm_quickflash` | `sm_quickflash` |
| **Tactical priority** | Drained in `sm_quickgrenade_priority` order | `bind g +sm_quicktac` | `sm_quicktac` |

---

## ConVar Configuration

### `cfg/sourcemod/plugin.quickmelee.cfg`
| ConVar | Default | Description |
| :--- | :---: | :--- |
| `sm_quickmelee_enable` | `1` | Enable plugin (1: enabled, 0: disabled) |
| `sm_quickmelee_hit_type` | `1` | Attack type (1: primary light slash, 0: secondary heavy slash) |
| `sm_quickmelee_back_type` | `2` | Weapon to switch back after swing (2: previous weapon, 1: primary then secondary, 0: keep knife) |
| `sm_quickmelee_switch_weapon_time` | `0.4` | Delay from trigger to weapon switch-back (seconds) |
| `sm_quickmelee_forbidden` | `"weapon_minigun"` | Blacklist of weapons that block quick melee |
| `sm_quickmelee_fix_viewmodel` | `1` | Fix dual viewmodels on melee (0: leave alone; 1: hide 1 show 0; 2: stock knife hides 1/shows 0, custom knives hide 0/show 1) |

### `cfg/sourcemod/plugin.quickgrenade.cfg`
| ConVar | Default | Description |
| :--- | :---: | :--- |
| `sm_quickgrenade_enable` | `1` | Enable plugin (1: enabled, 0: disabled) |
| `sm_quickgrenade_back_type` | `2` | Weapon to switch back after throw (2: previous weapon, 1: primary then secondary, 0: keep grenade) |
| `sm_quickgrenade_min_pull_time` | `0.2` | Minimum pull animation time (seconds, protects tap) |
| `sm_quickgrenade_throw_delay_time` | `0.6` | Delay after release before switching back (seconds, must be >0.15s) |
| `sm_quickgrenade_auto_throw_time` | `0.25` | Auto throw delay for single-tap commands (seconds) |
| `sm_quickgrenade_forbidden` | `"weapon_minigun"` | Blacklist of weapons that block quick grenade |
| `sm_quickgrenade_priority` | `"FSH"` | Tactical grenade priority code (F=flash S=smoke H=HE, drained in order) |

---

## Developer API

### `include <quickmelee>`
```sourcepawn
// Check whether the player is currently in a quick melee swing
native bool QuickMelee_IsCombat(int client);

// Force a quick melee for the player
native bool QuickMelee_Trigger(int client);

// Cancel quick melee and switch back to the previous weapon
native void QuickMelee_Cancel(int client);
```

### `include <quickgrenade>`
```sourcepawn
// Check whether the player is currently cooking or throwing
native bool QuickGrenade_IsCombat(int client);

// Get current grenade state (STATE_IDLE, STATE_HOLDING, STATE_THROWING, STATE_SWITCHING)
native QuickGrenadeState QuickGrenade_GetState(int client);

// Force a quick grenade pull/throw (exact classname match, unchanged, no recompile needed)
native bool QuickGrenade_Trigger(int client, const char[] grenadeClassname, bool isHold = false);

// Force a quick grenade pull/throw by tactical role (GRENADE_ROLE_HE/SMOKE/FLASH; custom grenades reusing stock ammo types are picked up automatically)
native bool QuickGrenade_TriggerByRole(int client, int role, bool isHold = false);

// Cancel cooking and reset
native void QuickGrenade_Cancel(int client);
```
