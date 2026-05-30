// aimbot/aimbot.h
// Mouse-driven aimbot. Pure 2D smoothing/stickiness over screen-space targets.
// Target selection (pick the closest enemy / lock current target) lives in
// the ESP renderer because that's where W2S happens.
#pragma once

namespace Aim {
    enum ActivationMode {
        ACT_ALWAYS = 0,
        ACT_HOLD   = 1,
    };

    enum TargetMethod {
        TGT_STICKY = 0,  // Lock first target, don't switch until dead/off-screen
        TGT_HUMAN  = 1,  // Allow switching with cooldown + inertia
    };

    void ResetState();

    // True when the user's activation gate is currently satisfied. Always true
    // in Always mode; in Hold mode requires the bound key to be physically
    // held. Returns false if Hold mode is selected with no key bound.
    bool IsActivationActive();

    // Move the system cursor a frame's worth toward (targetX, targetY) using
    // (centerX, centerY) as the "origin" (typically screen center). No-ops
    // when the activation gate isn't satisfied.
    void Apply(float targetX, float targetY, float centerX, float centerY);

    // Sticky lock state read by the renderer when picking a target.
    bool  TargetActive();
    void  SetTargetActive(bool active);
    float TargetX();
    float TargetY();
    void  SetTarget(float x, float y);

    // Target persistence: tracks which player index is currently locked.
    // Used by the renderer to prevent flicking between targets.
    void  SetLockedTargetIndex(int idx);
    int   GetLockedTargetIndex();
    bool  CanSwitchTarget();  // Returns true if cooldown has elapsed (Human mode)
    void  NotifyTargetSwitch(); // Resets the switch cooldown timer
}
