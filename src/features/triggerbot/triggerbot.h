// aimbot/triggerbot.h
// Non-blocking LMB click state machine. The renderer figures out whether
// the crosshair is over a valid target (it already iterates and projects
// every player) and calls Trigger::OnFrame() once per frame with that
// signal. The state machine handles activation gating, click pacing, and
// guarantees we never leave the button stuck down.
#pragma once

namespace Trigger {
    enum ActivationMode {
        ACT_ALWAYS = 0,
        ACT_HOLD   = 1,
    };

    // Reset internal state. Forces a LEFTUP if a click was in flight, so
    // it's safe to call from unload paths or when toggling the feature off.
    void ResetState();

    // Returns true when the user's activation gate is currently satisfied.
    // Always true in Always mode; in Hold mode requires the bound key.
    // Returns false in Hold mode if no key is bound, so first-install
    // toggling can't accidentally spam clicks.
    bool IsActivationActive();

    // Drive the state machine. `crosshairOnTarget` is true when the
    // renderer determined the screen-center crosshair sits over an enemy
    // (live, not team, visible-check honored, padded bbox). Skipped while
    // the menu is open or the game window isn't foregrounded.
    void OnFrame(bool crosshairOnTarget);

    // Read-only state inspection for the Debug tab.
    bool DebugIsClicking();
}
