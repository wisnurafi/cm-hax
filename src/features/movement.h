// features/movement.h
// Movement enhancements: bunny-hop, infinite sprint, auto slide-cancel.
// Called from the ESP data thread (same pattern as Combat::ApplyWeaponPatches).
#pragma once

namespace Movement {
    // myPlayer is the local player's PlayerRoot (validated by caller).
    void ApplyMovementPatches(void* myPlayer);
}
