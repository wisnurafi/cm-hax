// features/magicbullet.cpp
// Magic Bullet is now handled through silent aim's hook (UseShootingWeapon).
// When magicBulletEnabled is true, silent aim removes its FOV limit.
// This file just provides the Init/Shutdown stubs for the API.
#include "magicbullet.h"
#include "../core/logging.h"

namespace MagicBullet {

static bool g_initialized = false;

bool IsInitialized() { return g_initialized; }

bool Init() {
    // Magic bullet logic lives in silentaim.cpp — it checks g_state.magicBulletEnabled
    // and bypasses the FOV limit when active. No separate hook needed.
    g_initialized = true;
    Log("MagicBullet: ready (uses silent aim hook)");
    return true;
}

void Shutdown() {
    g_initialized = false;
}

} // namespace MagicBullet
