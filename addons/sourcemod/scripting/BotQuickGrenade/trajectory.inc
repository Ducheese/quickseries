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
 * @param detonatePos  输出引信耗尽、触碰或滚停时的最终爆炸坐标
 * @param profile      武器画像（初速/重力/引信/引爆模式）
 * @param visualize    是否向所有玩家客户端渲染弹道 Beam（含反弹轨迹）
 * @param color        弹道渲染颜色（RGBA，默认 COLOR_HE）
 * @param beamLife     激光线段残留时间（秒，默认 BEAM_LIFE）
 */
stock void SimulateGrenadeTrajectoryWithAngles(int bot, const float angles[3], float detonatePos[3], const GrenadeProfile profile, bool visualize = false, const int color[4] = COLOR_HE, float beamLife = BEAM_LIFE)
{
    float pitch = angles[0];
    float throwSpeed;

    // 初速模式分流：恒定初速（RPG 类直飞）vs 引擎仰角公式（原版抛物线）
    if (profile.speedMode == SPEED_FIXED)
    {
        throwSpeed = profile.speed;
    }
    else
    {
        throwSpeed = (90.0 - pitch) * 6.0;
        if (throwSpeed > profile.speed) throwSpeed = profile.speed;
    }

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

    float gravity = (cvarGravity != null) ? cvarGravity.FloatValue : 800.0;
    float gForce = profile.gravityScale * gravity; // 重力倍率取画像（原版 0.4，直飞类 0.0）
    float dt = SIM_TIMESTEP;
    float maxFlightTime = (profile.detonateTime > 0.0) ? profile.detonateTime : 1.5;

    int bounceCount = 0;
    const int MAX_BOUNCES = 5;

    for (float t = 0.0; t <= maxFlightTime; t += dt)
    {
        float nextPos[3];
        nextPos[0] = pos[0] + vel[0] * dt;
        nextPos[1] = pos[1] + vel[1] * dt;

        // 竖直方向梯形数值积分
        float newVz = vel[2] - gForce * dt;
        nextPos[2] = pos[2] + (vel[2] + newVz) * 0.5 * dt;
        vel[2] = newVz;

        TR_TraceHullFilter(pos, nextPos, SIM_MINS, SIM_MAXS, MASK_SHOT_HULL, TraceFilter_DontHitSelf, bot);
        float frac = TR_GetFraction();

        if (frac != 1.0)
        {
            // 获取碰撞接触点
            TR_GetEndPosition(nextPos);

            // 绘制撞击前的这段激光弹道
            if (visualize)
            {
                DrawBeamSegment(pos, nextPos, color, beamLife);
            }

            // 触碰即炸（燃烧瓶/RPG 类）：首碰即最终爆炸点，不反弹
            if (profile.detonateMode == DETONATE_TOUCH)
            {
                detonatePos = nextPos;
                return;
            }

            // 1. 计算法线反弹向量 (Impulse = 2.0 * dot(N, V))
            float normal[3];
            TR_GetPlaneNormal(null, normal);
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
            int hitEntity = TR_GetEntityIndex();
            float surfaceElasticity = 1.0;
            if (hitEntity > 0 && IsValidEntity(hitEntity) && HasEntProp(hitEntity, Prop_Data, "m_flElasticity"))
            {
                float el = GetEntPropFloat(hitEntity, Prop_Data, "m_flElasticity");
                if (el > 0.0) surfaceElasticity = el;
            }
            float elasticity = 0.45 * surfaceElasticity;
            ScaleVector(vel, elasticity);

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
            DrawBeamSegment(pos, nextPos, color, beamLife);
        }

        pos = nextPos;
    }

    // 引信时间到期，在空中或地面引爆
    detonatePos = pos;
}

public bool TraceFilter_WorldOnly(int entity, int contentsMask, any data)
{
    // 忽略所有玩家/Bot 实体，只检测世界笔刷与静态实体掩体
    return (entity == 0 || entity > MaxClients);
}

/**
 * 爆炸冲击波通达性检测：
 * 验证最终爆炸点与敌人目标之间是否有不可穿透的实心高墙阻隔
 */
stock bool CanExplosionReachTarget(const float detonatePos[3], const float targetPos[3])
{
    float start[3], end[3];
    start[0] = detonatePos[0];
    start[1] = detonatePos[1];
    start[2] = detonatePos[2] + 4.0; // 稍微抬高 4 单位避免贴地微小地形凸起误差

    end[0] = targetPos[0];
    end[1] = targetPos[1];
    end[2] = targetPos[2] + 20.0;    // 对准敌人下半身/膝盖高度

    TR_TraceRayFilter(start, end, MASK_VISIBLE, RayType_EndPoint, TraceFilter_WorldOnly);
    return (TR_GetFraction() == 1.0);
}

/**
 * 针对目标空间坐标搜索最优抛投仰角（以最终爆炸点为基准进行两段式搜索）
 *
 * @param bot          Bot 实体索引
 * @param targetPos    目标 3D 空间坐标
 * @param yaw          水平瞄准偏航角
 * @param bestPitch    输出求解出的最优仰角
 * @param detonatePos  输出最优最终爆炸点坐标
 * @param profile      武器画像（容差取 hitRadius）
 * @return             若存在爆炸点在容差范围内且爆炸视线通畅返回 true，否则 false
 */
stock bool FindBestPitchForTarget(int bot, const float targetPos[3], float yaw, float &bestPitch, float detonatePos[3], const GrenadeProfile profile)
{
    float maxHitRadius = profile.hitRadius;

    // 无重力直瞄类（RPG）：直线解析秒算，不跑抛物线搜索
    if (profile.gravityScale <= 0.05)
    {
        float eye[3], dir[3], directAngles[3];
        GetClientEyePosition(bot, eye);
        SubtractVectors(targetPos, eye, dir);
        GetVectorAngles(dir, directAngles);

        float testAngles[3];
        testAngles[0] = directAngles[0];
        testAngles[1] = yaw;
        testAngles[2] = 0.0;

        float tDetonate[3];
        SimulateGrenadeTrajectoryWithAngles(bot, testAngles, tDetonate, profile, false);

        float d = GetVectorDistance(tDetonate, targetPos);
        if (d <= maxHitRadius && CanExplosionReachTarget(tDetonate, targetPos))
        {
            bestPitch = directAngles[0];
            detonatePos = tDetonate;
            return true;
        }
        return false;
    }

    float bestDist = MAX_SEARCH_DIST;
    bestPitch = 0.0;

    // --- 阶段一：粗搜 (步长 PITCH_STEP_COARSE，从高抛仰角向平视逐步搜索) ---
    float coarseBest = 0.0;
    float coarseMinDist = MAX_SEARCH_DIST;

    for (float p = PITCH_MIN; p <= PITCH_MAX; p += PITCH_STEP_COARSE)
    {
        float angles[3];
        angles[0] = p;
        angles[1] = yaw;
        angles[2] = 0.0;

        float tDetonate[3];
        SimulateGrenadeTrajectoryWithAngles(bot, angles, tDetonate, profile, false);

        float d = GetVectorDistance(tDetonate, targetPos);
        // 粗搜最优基准点必须满足：视线通达可穿透，防止细搜中心锚定在被死墙阻隔的无效死角
        if (d < coarseMinDist && CanExplosionReachTarget(tDetonate, targetPos))
        {
            coarseMinDist = d;
            coarseBest = p;
        }

        // 若落点在有效杀伤/覆盖范围内且爆炸冲击波未被高墙阻隔，记录为候选
        if (d <= maxHitRadius && d < bestDist && CanExplosionReachTarget(tDetonate, targetPos))
        {
            bestDist = d;
            bestPitch = p;
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
        SimulateGrenadeTrajectoryWithAngles(bot, angles, tDetonate, profile, false);

        float d = GetVectorDistance(tDetonate, targetPos);
        // 细搜阶段必须同时满足：距离更优 + 冲击波视线畅通直达目标
        if (d <= maxHitRadius && d < bestDist && CanExplosionReachTarget(tDetonate, targetPos))
        {
            bestDist = d;
            bestPitch = p;
            detonatePos = tDetonate;
        }
    }

    return (bestDist <= maxHitRadius);
}
