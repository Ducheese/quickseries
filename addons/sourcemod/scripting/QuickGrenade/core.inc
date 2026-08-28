#if defined _quickgrenade_core_included
 #endinput
#endif
#define _quickgrenade_core_included

//========================================================================================
// CORE LOGIC
//========================================================================================

/**
 * 触发快速手雷流程
 *
 * @param client            客户端索引
 * @param grenadeClassname  目标手雷类名（如 weapon_hegrenade）
 * @param isHoldCmd         是否为 +cmd 长按模式
 * @return                  true 触发成功，false 失败
 */
bool TriggerQuickGrenade(int client, const char[] grenadeClassname, bool isHoldCmd)
{
    if (!GetConVarBool(cvarEnable)) return false;
    if (!client) client = 1;

    // 基本条件校验
    if (!IsValidClient(client, true) || g_bfreezetime || isCombat[client]) return false;
    if (g_PlayerState[client] != STATE_IDLE) return false;

    // 跨插件防冲突：若正在执行快速近战挥刀动作，阻止抢断手雷
    if (GetFeatureStatus(FeatureType_Native, "QuickMelee_IsCombat") == FeatureStatus_Available && QuickMelee_IsCombat(client))
    {
        return false;
    }

    // 检查玩家背包中是否拥有对应手雷且备弹大于0
    int grenadeEnt = FindPlayerGrenade(client, grenadeClassname);
    if (grenadeEnt <= 0 || !IsValidEntity(grenadeEnt))
    {
        return false; // 玩家没有该类手雷，静默退出不打断当前武器
    }

    int curActiveWeapon = GetEntPropEnt(client, Prop_Data, "m_hActiveWeapon");
    char curWeaponClass[32];
    if (curActiveWeapon > 0 && IsValidEntity(curActiveWeapon))
    {
        GetEntityClassname(curActiveWeapon, curWeaponClass, sizeof(curWeaponClass));
    }

    // 检查禁用黑名单
    char forbiddenList[2048];
    GetConVarString(cvarForbiddenList, forbiddenList, sizeof(forbiddenList));
    if (curWeaponClass[0] != '\0' && StrContains(forbiddenList, curWeaponClass, false) != -1)
    {
        return false;
    }

    // 记录切手雷前的原始武器
    PreWeapon[client] = curActiveWeapon;

    // 处理狙击枪开镜状态
    if (GetEntProp(client, Prop_Send, "m_iFOV") != Fov[client])
    {
        SetEntProp(client, Prop_Send, "m_iFOV", Fov[client]);
    }

    // 机瞄/特殊视图模型修复：确保手雷v模能够正常渲染
    if (IsValidViewModelEntity(client, ClientVM3[client][0]) && IsValidViewModelEntity(client, ClientVM3[client][1]))
    {
        int entEffects = GetEntProp(ClientVM3[client][0], Prop_Send, "m_fEffects");
        int entEffects2 = GetEntProp(ClientVM3[client][1], Prop_Send, "m_fEffects");

        if (entEffects == EF_NODRAW && entEffects2 == 0)
        {
            entEffects2 |= EF_NODRAW;
            SetEntProp(ClientVM3[client][1], Prop_Send, "m_fEffects", entEffects2);

            entEffects2 = GetEntProp(ClientVM3[client][0], Prop_Send, "m_fEffects");
            entEffects2 &= ~EF_NODRAW;
            SetEntProp(ClientVM3[client][0], Prop_Send, "m_fEffects", entEffects2);
        }
    }

    // 进入引雷阶段
    isCombat[client] = true;
    g_PlayerState[client] = STATE_HOLDING;
    g_iActiveGrenadeEnt[client] = grenadeEnt;
    strcopy(g_sGrenadeClass[client], sizeof(g_sGrenadeClass[]), grenadeClassname);
    g_fHoldStartTime[client] = GetGameTime();
    g_bCmdHolding[client] = isHoldCmd;

    // 清空换枪延迟，强制切至手雷（跳过切出手雷时的Deploy拉栓动作）
    SetEntPropFloat(client, Prop_Data, "m_flNextAttack", 0.0);
    SetEntPropEnt(client, Prop_Send, "m_hActiveWeapon", grenadeEnt);

    // 清空射击计数与手雷攻击冷却，使得在第 1 帧注入 IN_ATTACK 后能立即开始引雷动画 ACT_VM_PULLPIN
    SetEntProp(client, Prop_Send, "m_iShotsFired", 0);
    SetEntPropFloat(grenadeEnt, Prop_Send, "m_flNextPrimaryAttack", 0.0);
    SetEntPropFloat(grenadeEnt, Prop_Send, "m_flNextSecondaryAttack", 0.0);

    // 如果不是长按命令（如单次点击 sm_quickfrag），创建自动投掷定时器
    if (!isHoldCmd)
    {
        float autoThrowDelay = GetConVarFloat(cvarAutoThrowTime);
        g_hDelayThrowTimer[client] = CreateTimer(autoThrowDelay, Timer_AutoThrow, client);
    }

    return true;
}

/**
 * 松开按键/结束引雷
 *
 * @param client    客户端索引
 */
void ReleaseQuickGrenade(int client)
{
    if (!IsValidClient(client, true)) return;

    g_bCmdHolding[client] = false;

    if (g_PlayerState[client] != STATE_HOLDING) return;

    float heldTime = GetGameTime() - g_fHoldStartTime[client];
    float minPullTime = GetConVarFloat(cvarMinPullTime);

    // 若按住时间已达到最短引雷时间，直接执行投掷
    if (heldTime >= minPullTime)
    {
        ExecuteThrow(client);
    }
    else
    {
        // 否则等待剩余引雷时间走完，保证引雷拔销动作平滑展示
        float remaining = minPullTime - heldTime;
        if (g_hDelayThrowTimer[client] != null)
        {
            KillTimer(g_hDelayThrowTimer[client]);
            g_hDelayThrowTimer[client] = null;
        }
        g_hDelayThrowTimer[client] = CreateTimer(remaining, Timer_AutoThrow, client);
    }
}

/**
 * 自动引雷结束回调（单次点击或最短引雷时间保护）
 */
public Action Timer_AutoThrow(Handle timer, int client)
{
    g_hDelayThrowTimer[client] = null;

    if (IsValidClient(client, true) && g_PlayerState[client] == STATE_HOLDING)
    {
        // 若不是正在通过 +cmd 物理长按，则执行丢雷
        if (!g_bCmdHolding[client])
        {
            ExecuteThrow(client);
        }
    }
    return Plugin_Continue;
}

/**
 * 执行丢雷动作：由引雷状态转入投掷状态
 *
 * @param client    客户端索引
 */
void ExecuteThrow(int client)
{
    if (g_PlayerState[client] != STATE_HOLDING) return;

    // 状态切换为投掷中：OnPlayerRunCmd 中将不再注入 IN_ATTACK 并且强行清空该键位
    // 引擎的 CBaseCSGrenade::ItemPostFrame 捕捉到 m_bPinPulled 为 true 且松开攻击键后，
    // 会自动播放 ACT_VM_THROW（丢雷v模动作）与 PLAYERANIMEVENT_THROW_GRENADE（人物3D丢雷动作），
    // 并在 0.1 秒后生成飞行手雷实体（ThrowGrenade）
    g_PlayerState[client] = STATE_THROWING;

    int grenade = g_iActiveGrenadeEnt[client];
    if (grenade > 0 && IsValidEntity(grenade))
    {
        SetEntPropFloat(grenade, Prop_Send, "m_flNextPrimaryAttack", 0.0);
        SetEntPropFloat(grenade, Prop_Send, "m_flTimeWeaponIdle", 0.0);
    }

    if (g_hSwitchWeaponTimer[client] != null)
    {
        KillTimer(g_hSwitchWeaponTimer[client]);
        g_hSwitchWeaponTimer[client] = null;
    }

    // 投掷动作及投掷物生成需要一定时间，等待投掷完成后切回武器
    float throwDelay = GetConVarFloat(cvarThrowDelayTime);
    if (throwDelay < 0.2) throwDelay = 0.2; // 严防过早切枪导致 Holster() 抹去投掷物生成
    g_hSwitchWeaponTimer[client] = CreateTimer(throwDelay, Timer_SwitchWeapon, client);
}

/**
 * 取消玩家当前的快速引雷流程并安全复位
 *
 * @param client    客户端索引
 */
void CancelQuickGrenade(int client)
{
    if (!IsValidClient(client, false)) return;

    if (g_PlayerState[client] != STATE_IDLE)
    {
        int prev = PreWeapon[client];
        ResetClientQuickGrenade(client);

        if (prev > 0 && IsValidEntity(prev))
        {
            SwitchWeapon(client, prev);
        }
        else
        {
            SwitchToBestWeapon(client);
        }
    }
}

/**
 * 投掷完成后切回原始武器定时器
 */
public Action Timer_SwitchWeapon(Handle timer, int client)
{
    g_hSwitchWeaponTimer[client] = null;

    // 恢复状态
    g_PlayerState[client] = STATE_IDLE;
    isCombat[client] = false;
    g_bCmdHolding[client] = false;

    if (!IsValidClient(client, true))
    {
        return Plugin_Continue;
    }

    int backType = GetConVarInt(cvarBackType);
    if (backType == 2)
    {
        // 切回原来武器
        int prev = PreWeapon[client];
        bool bPrevValid = false;

        if (prev > 0 && IsValidEntity(prev))
        {
            if (prev != g_iActiveGrenadeEnt[client])
            {
                bPrevValid = true; // 原武器是枪械/刀具，依然有效
            }
            else
            {
                // 原武器就是该手雷，检查扔完后背包是否还有该雷的剩余备弹（例如原先有2颗闪光弹）
                int ammoType = GetEntProp(prev, Prop_Send, "m_iPrimaryAmmoType");
                if (ammoType >= 0 && GetEntProp(client, Prop_Send, "m_iAmmo", _, ammoType) > 0)
                {
                    bPrevValid = true;
                }
            }
        }

        if (bPrevValid)
        {
            SwitchWeapon(client, prev);
        }
        else
        {
            // 原武器已失效/用光（如手上仅存的手雷已丢出），自动降级切最佳武器（主武器 -> 副武器 -> 刀）
            SwitchToBestWeapon(client);
        }
    }
    else if (backType == 1)
    {
        // 优先主武器，其次副武器，最后刀
        SwitchToBestWeapon(client);
    }
    else if (backType == 0)
    {
        // 保持持雷/当前状态
    }

    return Plugin_Continue;
}
