#if defined _botquickgrenade_trajectory_included
  #endinput
#endif
#define _botquickgrenade_trajectory_included

//========================================================================================
// TRAJECTORY SIMULATION & DETONATION SOLVER
// 负责 CS:S 原生手雷反弹物理积分（含法线反射与弹性衰减）与两段式仰角搜索
//========================================================================================

/**
 * 模拟给定角度下的手雷完整飞行与反弹轨迹，输出最终爆炸点（Detonation Point）
 *
 * @param bot          Bot 实体索引
 * @param angles       指定的投掷欧拉角 (pitch, yaw, roll)
 * @param detonatePos  输出引信耗尽或滚停时的最终爆炸坐标
 * @param visualize    是否向所有玩家客户端渲染橙色弹道 Beam（含反弹轨迹）
 */
stock void SimulateGrenadeTrajectoryWithAngles(int bot, float angles[3], float detonatePos[3], bool visualize)
{
    float pitch = angles[0];
    float throwSpeed = (90.0 - pitch) * 6.0;
    if (throwSpeed > 750.0) throwSpeed = 750.0;

    // 俯仰角修正公式（与 CS:S 引擎及 grenade_trajectory 一致）
    float correctedAngles[3];
    correctedAngles[0] = -10.0 + pitch + FloatAbs(pitch) * 10.0 / 90.0;
    correctedAngles[1] = angles[1];
    correctedAngles[2] = angles[2];

    float fwd[3], right[3], up[3];
    GetAngleVectors(correctedAngles, fwd, right, up);
    NormalizeVector(fwd, fwd);

    // 起点：Bot 眼位沿投掷朝向前向推进 16 单位
    float pos[3];
    GetClientEyePosition(bot, pos);
    pos[0] += fwd[0] * 16.0;
    pos[1] += fwd[1] * 16.0;
    pos[2] += fwd[2] * 16.0;

    // 初始速度向量 = 视线朝向 * 投掷速度 + 玩家自身移动速度惯性
    float playerVel[3], vel[3];
    GetEntPropVector(bot, Prop_Data, "m_vecAbsVelocity", playerVel);
    vel[0] = fwd[0] * throwSpeed + playerVel[0];
    vel[1] = fwd[1] * throwSpeed + playerVel[1];
    vel[2] = fwd[2] * throwSpeed + playerVel[2];

    float gravity = float(cvarGravity.IntValue);
    float gForce = 0.4 * gravity; // CS:S 手雷重力系数为 0.4 * sv_gravity
    float dt = SIM_TIMESTEP;

    int bounceCount = 0;
    const int MAX_BOUNCES = 5;

    for (float t = 0.0; t <= GRENADE_FLIGHT_TIME; t += dt)
    {
        float nextPos[3];
        nextPos[0] = pos[0] + vel[0] * dt;
        nextPos[1] = pos[1] + vel[1] * dt;

        // 竖直方向梯形数值积分
        float newVz = vel[2] - gForce * dt;
        nextPos[2] = pos[2] + (vel[2] + newVz) * 0.5 * dt;
        vel[2] = newVz;

        Handle ray = TR_TraceHullEx(pos, nextPos, SIM_MINS, SIM_MAXS, MASK_SHOT_HULL);
        float frac = TR_GetFraction(ray);

        if (frac != 1.0)
        {
            // 第一帧若碰撞自身则跳过
            if (t == 0.0 && TR_GetEntityIndex(ray) == bot)
            {
                CloseHandle(ray);
                pos = nextPos;
                continue;
            }

            // 获取碰撞接触点
            TR_GetEndPosition(nextPos, ray);

            // 绘制撞击前的这段激光弹道
            if (visualize)
            {
                DrawBeamSegment(pos, nextPos, COLOR_TRAJ, BEAM_LIFE);
            }

            // 1. 计算法线反弹向量 (Impulse = 2.0 * dot(N, V))
            float normal[3];
            TR_GetPlaneNormal(ray, normal);
            float impulse = 2.0 * GetVectorDotProduct(normal, vel);
            for (int i = 0; i < 3; i++)
            {
                vel[i] -= impulse * normal[i];
                if (FloatAbs(vel[i]) < 0.1)
                {
                    vel[i] = 0.0;
                }
            }

            // 2. 表面弹性与能量衰减
            int hitEntity = TR_GetEntityIndex(ray);
            float surfaceElasticity = 1.0;
            if (hitEntity > 0 && IsValidEntity(hitEntity) && HasEntProp(hitEntity, Prop_Send, "m_flElasticity"))
            {
                float el = GetEntPropFloat(hitEntity, Prop_Send, "m_flElasticity");
                if (el > 0.0) surfaceElasticity = el;
            }
            float elasticity = 0.45 * surfaceElasticity;
            ScaleVector(vel, elasticity);

            CloseHandle(ray);
            pos = nextPos;
            bounceCount++;

            // 3. 性能优化截断：若速度衰减至滚停状态 (speed < 15) 或达到反弹上限，判定已静止落地
            float speedSq = vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2];
            if (speedSq < 225.0 || bounceCount >= MAX_BOUNCES)
            {
                detonatePos = pos;
                return;
            }

            continue;
        }

        if (visualize)
        {
            DrawBeamSegment(pos, nextPos, COLOR_TRAJ, BEAM_LIFE);
        }

        CloseHandle(ray);
        pos = nextPos;
    }

    // 引信时间到期（t >= GRENADE_FLIGHT_TIME），在空中或地面引爆
    detonatePos = pos;
}

/**
 * 针对目标空间坐标搜索最优抛投仰角（以最终爆炸点为基准进行两段式搜索）
 *
 * @param bot         Bot 实体索引
 * @param targetPos   目标 3D 空间坐标
 * @param yaw         水平瞄准偏航角
 * @param bestPitch   输出求解出的最优仰角
 * @param detonatePos 输出最优最终爆炸点坐标
 * @return            若存在爆炸点在 HE_HIT_RADIUS 范围内的有效仰角返回 true，否则 false
 */
stock bool FindBestPitchForTarget(int bot, const float targetPos[3], float yaw, float &bestPitch, float detonatePos[3])
{
    float bestDist = 9999999.0;
    bestPitch = 0.0;

    // --- 阶段一：粗搜 (步长 PITCH_STEP_COARSE) ---
    float coarseBest = 0.0;
    for (float p = PITCH_MIN; p <= PITCH_MAX; p += PITCH_STEP_COARSE)
    {
        float angles[3];
        angles[0] = p;
        angles[1] = yaw;
        angles[2] = 0.0;

        float tDetonate[3];
        SimulateGrenadeTrajectoryWithAngles(bot, angles, tDetonate, false);

        float d = GetVectorDistance(tDetonate, targetPos);
        if (d < bestDist)
        {
            bestDist = d;
            coarseBest = p;
            detonatePos = tDetonate;
        }
    }

    // --- 阶段二：细搜 (在 coarseBest 前后细化精修) ---
    float fineMin = coarseBest - PITCH_STEP_COARSE;
    float fineMax = coarseBest + PITCH_STEP_COARSE;
    if (fineMin < PITCH_MIN) fineMin = PITCH_MIN;
    if (fineMax > PITCH_MAX) fineMax = PITCH_MAX;

    for (float p = fineMin; p <= fineMax; p += PITCH_STEP_FINE)
    {
        float angles[3];
        angles[0] = p;
        angles[1] = yaw;
        angles[2] = 0.0;

        float tDetonate[3];
        SimulateGrenadeTrajectoryWithAngles(bot, angles, tDetonate, false);

        float d = GetVectorDistance(tDetonate, targetPos);
        if (d < bestDist)
        {
            bestDist = d;
            bestPitch = p;
            detonatePos = tDetonate;
        }
    }

    return (bestDist <= HE_HIT_RADIUS);
}

/**
 * 检查玩家背包中是否拥有指定类名的手雷
 */
stock bool BotHasGrenade(int client, const char[] classname)
{
    for (int slot = 3; slot <= 7; slot++)
    {
        int wep = GetPlayerWeaponSlot(client, slot);
        if (wep <= 0 || !IsValidEntity(wep)) continue;

        char cls[32];
        GetEntityClassname(wep, cls, sizeof(cls));
        if (StrEqual(cls, classname, false))
            return true;
    }
    return false;
}