#if defined _quickmelee_core_included
 #endinput
#endif
#define _quickmelee_core_included

//========================================================================================
// CORE LOGIC
//========================================================================================

/**
 * 执行快速近战动作
 *
 * @param client    玩家 Client Index
 * @return          true 触发成功，false 失败
 */
bool PerformQuickMelee(int client)
{
    if (!GetConVarBool(cvarEnable)) return false;
    if (!client) client = 1;

    // 基本条件校验
    if (!IsValidClient(client, true) || g_bfreezetime || isCombat[client]) return false;

    // 跨插件防冲突：若正在执行快速手雷动作，进行协同处理
    if (GetFeatureStatus(FeatureType_Native, "QuickGrenade_IsCombat") == FeatureStatus_Available && QuickGrenade_IsCombat(client))
    {
        if (GetFeatureStatus(FeatureType_Native, "QuickGrenade_GetState") == FeatureStatus_Available && QuickGrenade_GetState(client) == STATE_HOLDING)
        {
            // 若玩家正处于手雷长按引雷瞄准中，主动取消引雷流程，无缝转入快速挥刀
            QuickGrenade_Cancel(client);
        }
        else
        {
            // 若手雷已脱手正在投掷动画中，则等待其完成，不发生冲突
            return false;
        }
    }

    int melee = GetPlayerWeaponSlot(client, 2);
    if (melee <= 0 || !IsValidEntity(melee))
    {
        return false; // 无近战武器
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

    // 记录原始武器
    PreWeapon[client] = curActiveWeapon;

    // 处理狙击枪开镜状态
    if (GetEntProp(client, Prop_Send, "m_iFOV") != Fov[client])
    {
        SetEntProp(client, Prop_Send, "m_iFOV", Fov[client]);
    }

    // 机瞄/特殊视图模型修复（sm_quickmelee_fix_viewmodel：0=不干预；1=统统藏 1 号亮 0 号；2=仅原版刀藏 1 亮 0，其他放行）
    int fixMode = GetConVarInt(cvarFixViewModel);
    if (fixMode != 0 && IsValidViewModelEntity(client, ClientVM3[client][0]) && IsValidViewModelEntity(client, ClientVM3[client][1]))
    {
        bool shouldFix = (fixMode == 1);
        if (fixMode == 2)
        {
            char meleeCls[32];
            GetEntityClassname(melee, meleeCls, sizeof(meleeCls));
            shouldFix = StrEqual(meleeCls, "weapon_knife", false);
        }

        if (shouldFix)
        {
            int fx0 = GetEntProp(ClientVM3[client][0], Prop_Send, "m_fEffects");
            int fx1 = GetEntProp(ClientVM3[client][1], Prop_Send, "m_fEffects");

            if ((fx0 & EF_NODRAW) != 0 && (fx1 & EF_NODRAW) == 0)
            {
                SetEntProp(ClientVM3[client][1], Prop_Send, "m_fEffects", fx1 | EF_NODRAW);
                SetEntProp(ClientVM3[client][0], Prop_Send, "m_fEffects", fx0 & ~EF_NODRAW);
            }
        }
    }

    // 进入快速近战战斗状态
    isCombat[client] = true;

    // 清空换枪延迟，强制切刀（跳过切刀 Deploy 动画）
    SetEntPropFloat(client, Prop_Data, "m_flNextAttack", 0.0);
    SetEntPropEnt(client, Prop_Send, "m_hActiveWeapon", melee);

    // w 模可见性修复：Deploy 被跳过导致 EF_NODRAW 残留，第三方全程看不到刀（m_iState 交由 UpdateClientData 下一帧自动翻转，不手动置以避免切枪时 idle 闪现）
    if (curActiveWeapon > 0 && IsValidEntity(curActiveWeapon) && curActiveWeapon != melee)
    {
        int fxOld = GetEntProp(curActiveWeapon, Prop_Send, "m_fEffects");
        if ((fxOld & EF_NODRAW) == 0)
            SetEntProp(curActiveWeapon, Prop_Send, "m_fEffects", fxOld | EF_NODRAW);
    }
    int fxNew = GetEntProp(melee, Prop_Send, "m_fEffects");
    if (fxNew & EF_NODRAW)
        SetEntProp(melee, Prop_Send, "m_fEffects", fxNew & ~EF_NODRAW);

    // 清空射击计数与刀的攻击冷却，使得在 OnPlayerRunCmd 中注入攻击键后能立即挥刀
    SetEntProp(client, Prop_Send, "m_iShotsFired", 0);
    SetEntPropFloat(melee, Prop_Send, "m_flNextPrimaryAttack", 0.0);
    SetEntPropFloat(melee, Prop_Send, "m_flNextSecondaryAttack", 0.0);

    // 设定攻击注入类型
    if (GetConVarBool(cvarHitType))
    {
        IsInWhatClick[client] = 1; // 左键轻击
    }
    else
    {
        IsInWhatClick[client] = 2; // 右键重击
    }

    if (g_hSwitchTimer[client] != null)
    {
        KillTimer(g_hSwitchTimer[client]);
        g_hSwitchTimer[client] = null;
    }

    float switchTime = GetConVarFloat(cvarSwitchWeaponTime);
    g_hSwitchTimer[client] = CreateTimer(switchTime, Timer_SwitchWeapon, client);

    return true;
}

/**
 * 取消玩家当前的快速近战流程并复位
 *
 * @param client    玩家 Client Index
 */
void CancelQuickMelee(int client)
{
    if (!IsValidClient(client, false)) return;

    if (isCombat[client])
    {
        int prev = PreWeapon[client];
        ResetClientQuickMelee(client);

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
 * 挥刀完成后切回原始武器定时器
 */
public Action Timer_SwitchWeapon(Handle timer, int client)
{
    g_hSwitchTimer[client] = null;
    isCombat[client] = false;

    if (!IsValidClient(client, true))
    {
        return Plugin_Continue;
    }

    int backType = GetConVarInt(cvarBackType);
    if (backType == 2)
    {
        // 切回原来武器
        int prev = PreWeapon[client];
        if (prev > 0 && IsValidEntity(prev) && prev != GetPlayerWeaponSlot(client, 2))
        {
            SwitchWeapon(client, prev);
        }
        else
        {
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
        // 保持持刀
    }

    return Plugin_Continue;
}
