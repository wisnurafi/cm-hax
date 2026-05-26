// features/combat.h
// Per-frame patches that zero out recoil/spread on the local player's weapon.
// Called from inside the ESP data thread because that thread is already
// attached to the IL2CPP domain and walks PlayerRoot anyway.
#pragma once

namespace Combat {
    // myPlayer is the local player's PlayerRoot (validated by caller).
    void ApplyWeaponPatches(void* myPlayer);
}
