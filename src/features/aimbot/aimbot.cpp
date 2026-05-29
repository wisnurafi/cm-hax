// aimbot/aimbot.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>

#include "aimbot.h"
#include "../../core/globals.h"
#include "../../utils/math.h"
#include "../../utils/input.h"

namespace Aim {

static bool  s_lockActive   = false;
static bool  s_targetActive = false;
static float s_stableX = 0.0f, s_stableY = 0.0f;
static float s_targetX = 0.0f, s_targetY = 0.0f;
static float s_residualX = 0.0f, s_residualY = 0.0f;

void ResetState() {
    s_lockActive   = false;
    s_targetActive = false;
    s_residualX = 0.0f;
    s_residualY = 0.0f;
}

bool IsActivationActive() {
    if (g_state.aimActivationMode == ACT_ALWAYS) return true;
    if (g_state.aimActivationKey == 0) return false; // Hold mode without a key = inactive
    return Input::IsKeyDown(g_state.aimActivationKey);
}

bool TargetActive()              { return s_targetActive; }
void SetTargetActive(bool a)     { s_targetActive = a; }
float TargetX()                  { return s_targetX; }
float TargetY()                  { return s_targetY; }
void  SetTarget(float x, float y){ s_targetX = x; s_targetY = y; }

static LONG RoundMouseDelta(float value) {
    if (value > 0.0f) return (LONG)(value + 0.5f);
    if (value < 0.0f) return (LONG)(value - 0.5f);
    return 0;
}

void Apply(float targetX, float targetY, float centerX, float centerY) {
    if (!IsActivationActive()) {
        ResetState();
        return;
    }

    float smooth = ClampFloat(g_state.aimbotSmooth, 1.0f, 15.0f);

    if (!s_lockActive) {
        s_stableX = targetX;
        s_stableY = targetY;
        s_lockActive = true;
    } else {
        float jumpX = targetX - s_stableX;
        float jumpY = targetY - s_stableY;
        float jumpDist = sqrtf(jumpX * jumpX + jumpY * jumpY);
        float resetDistance = ClampFloat(g_state.aimbotFov * 0.45f, 55.0f, 180.0f);

        if (jumpDist > resetDistance) {
            s_stableX = targetX;
            s_stableY = targetY;
            s_residualX = 0.0f;
            s_residualY = 0.0f;
        } else {
            float targetAlpha = ClampFloat(0.70f / smooth, 0.16f, 0.70f);
            s_stableX += jumpX * targetAlpha;
            s_stableY += jumpY * targetAlpha;
        }
    }

    float dx = s_stableX - centerX;
    float dy = s_stableY - centerY;

    if (fabsf(dx) < 0.75f && fabsf(dy) < 0.75f) {
        s_residualX = 0.0f;
        s_residualY = 0.0f;
        return;
    }

    float distance = sqrtf(dx * dx + dy * dy);
    float gain = ClampFloat(0.34f / smooth, 0.045f, 0.34f);
    float maxStep = ClampFloat(14.0f + distance * 0.12f, 24.0f, 72.0f);

    float stepX = dx * gain;
    float stepY = dy * gain;

    stepX = ClampFloat(stepX, -maxStep, maxStep);
    stepY = ClampFloat(stepY, -maxStep, maxStep);

    if (fabsf(stepX) > fabsf(dx)) stepX = dx;
    if (fabsf(stepY) > fabsf(dy)) stepY = dy;

    s_residualX += stepX;
    s_residualY += stepY;

    LONG moveX = RoundMouseDelta(s_residualX);
    LONG moveY = RoundMouseDelta(s_residualY);

    if ((dx > 0.0f && moveX < 0) || (dx < 0.0f && moveX > 0) || fabsf((float)moveX) > fabsf(dx)) moveX = 0;
    if ((dy > 0.0f && moveY < 0) || (dy < 0.0f && moveY > 0) || fabsf((float)moveY) > fabsf(dy)) moveY = 0;

    if (moveX == 0 && moveY == 0) return;

    s_residualX -= (float)moveX;
    s_residualY -= (float)moveY;
    mouse_event(MOUSEEVENTF_MOVE, (DWORD)moveX, (DWORD)moveY, 0, 0);
}

} // namespace Aim
