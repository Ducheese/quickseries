#if defined _botquickgrenade_visuals_included
  #endinput
#endif
#define _botquickgrenade_visuals_included

//========================================================================================
// VISUAL RENDERING & LOGGING HELPERS
// 负责弹道激光绘制、落点圆圈渲染以及带颜色的聊天栏调试日志
//========================================================================================

/**
 * 向所有客户端绘制一段激光线段
 */
stock void DrawBeamSegment(const float pos[3], const float endPos[3], const int color[4], float life)
{
    if (g_iBeamSprite <= 0) return;

    TE_SetupBeamPoints(
        pos, endPos,
        g_iBeamSprite, 0,
        0, 0,
        life,
        BEAM_WIDTH, BEAM_WIDTH,
        0,
        0.0,
        color,
        0
    );
    TE_SendToAll(0.0);
}

/**
 * 以落点 center 为圆心、radius 为半径在地面水平渲染一个圆圈
 */
stock void DrawLandingCircle(const float center[3], float radius, const int color[4], float life)
{
    if (g_iBeamSprite <= 0) return;

    float step = (2.0 * 3.1415926535) / float(CIRCLE_SEGS);

    float prevPt[3], currPt[3];
    prevPt[0] = center[0] + radius;
    prevPt[1] = center[1];
    prevPt[2] = center[2] + 2.0; // 稍微抬高 2 单位防止与地面重叠裁剪

    for (int i = 1; i <= CIRCLE_SEGS; i++)
    {
        float angle = float(i) * step;
        currPt[0] = center[0] + radius * Cosine(angle);
        currPt[1] = center[1] + radius * Sine(angle);
        currPt[2] = center[2] + 2.0;

        DrawBeamSegment(prevPt, currPt, color, life);
        prevPt = currPt;
    }
}

/**
 * 格式化输出调试日志到所有玩家聊天栏
 */
stock void LogDebug(const char[] format, any ...)
{
    if (!GetConVarBool(cvarDebug)) return;

    char buffer[256];
    VFormat(buffer, sizeof(buffer), format, 2);
    PrintToChatAll("%s", buffer);
}
