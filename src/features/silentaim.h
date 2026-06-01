// features/silentaim.h
// Silent aim: modifies camera transform + command input angles during
// UseShootingWeapon. Restores on next frame via PollRestore().
#pragma once

namespace SilentAim {
    bool Init();
    void Shutdown();
    bool IsInitialized();
    // Call from ESP data thread each tick to restore camera after shot
    void PollRestore();
}
