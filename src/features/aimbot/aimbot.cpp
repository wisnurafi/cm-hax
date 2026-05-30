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

// Target persistence state
static int   s_lockedTargetIdx = -1;
static DWORD s_lastSwitchTime  = 0;
static const DWORD SWITCH_COOLDOWN_MS = 350; // Human mode: min time between target switches

void ResetState() {
    s_lockActive   = false;
    s_targetActive = false;
    s_residualX = 0.0f;
    s_residualY = 0.0f;
    s_lockedTargetIdx = -1;
}

void SetLockedTargetIndex(int idx) { s_lockedTargetIdx = idx; }
int  GetLockedTargetIndex()        { return s_lockedTargetIdx; }

bool CanSwitchTarget() {
    if (g_state.aimTargetMethod == TGT_STICKY) return false; // Sticky never switches
    DWORD now = GetTickCount();
    return (now - s_lastSwitchTime) >= SWITCH_COOLDOWN_MS;
}

void NotifyTargetSwitch() {
    s_lastSwitchTime = GetTickCount();
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

    // Apply ADS multiplier when aim key is held (ADS active)
    if (g_state.aimActivationMode == ACT_HOLD && IsActivationActive()) {
        smooth *= ClampFloat(g_state.adsMultiplier, 1.0f, 5.0f);
        smooth = ClampFloat(smooth, 1.0f, 30.0f);
    }

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
            // Adaptive alpha: lerp faster when far, slower when close to target.
            // This prevents jitter when nearly on-target while still snapping
            // quickly from far away (rage-friendly).
            float baseAlpha = ClampFloat(0.70f / smooth, 0.16f, 0.70f);
            float proximityFactor = ClampFloat(jumpDist / 80.0f, 0.15f, 1.0f);
            float targetAlpha = baseAlpha * proximityFactor;
            s_stableX += jumpX * targetAlpha;
            s_stableY += jumpY * targetAlpha;
        }
    }

    float dx = s_stableX - centerX;
    float dy = s_stableY - centerY;
    float distance = sqrtf(dx * dx + dy * dy);

    // Adaptive dead zone: larger when close to prevent micro-jitter
    float deadZone = ClampFloat(1.5f - distance * 0.01f, 0.4f, 1.5f);
    if (distance < deadZone) {
        s_residualX = 0.0f;
        s_residualY = 0.0f;
        return;
    }

    // Distance-based gain curve: aggressive when far, gentle when close.
    // This makes rage (big FOV, low smooth) feel snappy without shaking.
    float baseGain = ClampFloat(0.34f / smooth, 0.045f, 0.34f);
    float distanceCurve = ClampFloat(distance / 120.0f, 0.25f, 1.0f);
    float gain = baseGain * distanceCurve;

    // Max step scales with distance but is capped more tightly at close range
    float maxStep = ClampFloat(8.0f + distance * 0.18f, 10.0f, 55.0f);

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
