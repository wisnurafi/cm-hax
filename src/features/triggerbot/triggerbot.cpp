// aimbot/triggerbot.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "triggerbot.h"
#include "../../core/globals.h"
#include "../../utils/input.h"

namespace Trigger {

// Click state machine.
//   IDLE   -> nothing in flight. May transition to PRESSING when the gate
//             is satisfied, the crosshair sits on a target, and the
//             configured refire delay has elapsed since the last release.
//   PRESSING -> we sent LEFTDOWN. Waits until `holdMs` has elapsed in
//             real time, then sends LEFTUP and goes back to IDLE.
//
// Real-time gating (no Sleep) lets the renderer drive this at frame
// cadence without ever blocking. We also force a LEFTUP on ResetState so
// the button can't get stuck if the user toggles off mid-click or the
// DLL unloads while pressed.
enum State { ST_IDLE = 0, ST_PRESSING = 1 };

static State s_state            = ST_IDLE;
static DWORD s_pressUntilTick   = 0;  // when to send LEFTUP
static DWORD s_lastReleaseTick  = 0;  // for refire delay

static void SendDown() { mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0); }
static void SendUp()   { mouse_event(MOUSEEVENTF_LEFTUP,   0, 0, 0, 0); }

void ResetState() {
    if (s_state == ST_PRESSING) {
        // Don't leave the LMB stuck down on the user's mouse.
        SendUp();
    }
    s_state           = ST_IDLE;
    s_pressUntilTick  = 0;
    s_lastReleaseTick = 0;
}

bool IsActivationActive() {
    if (g_state.triggerActivationMode == ACT_ALWAYS) return true;
    if (g_state.triggerActivationKey == 0)           return false;
    return Input::IsKeyDown(g_state.triggerActivationKey);
}

// True only when CombatMaster.exe is foregrounded. Prevents Always-mode
// from spamming LMB into other windows when the user alt-tabs.
static bool IsGameForeground() {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

void OnFrame(bool crosshairOnTarget) {
    DWORD now = GetTickCount();

    // Always advance an in-flight click, even if the user just toggled the
    // feature off, so we still send the matching LEFTUP.
    if (s_state == ST_PRESSING) {
        if ((LONG)(now - s_pressUntilTick) >= 0) {
            SendUp();
            s_state           = ST_IDLE;
            s_lastReleaseTick = now;
        }
        return;
    }

    if (!g_state.triggerEnabled)  { ResetState(); return; }
    if (g_state.showMenu)         { return; }
    if (!IsGameForeground())      { return; }
    if (!IsActivationActive())    { return; }
    if (!crosshairOnTarget)       { return; }

    // Refire delay between clicks. Lower bound so we never busy-spam if
    // the user sets the slider to zero.
    int refireMs = g_state.triggerRefireMs;
    if (refireMs < 10) refireMs = 10;
    if (s_lastReleaseTick != 0 && (LONG)(now - s_lastReleaseTick) < refireMs) {
        return;
    }

    int holdMs = g_state.triggerHoldMs;
    if (holdMs < 5)   holdMs = 5;
    if (holdMs > 200) holdMs = 200;

    SendDown();
    s_state          = ST_PRESSING;
    s_pressUntilTick = now + (DWORD)holdMs;
}

bool DebugIsClicking() { return s_state == ST_PRESSING; }

} // namespace Trigger
