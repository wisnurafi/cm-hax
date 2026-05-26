// features/combat.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "combat.h"
#include "../core/globals.h"

namespace Combat {

void ApplyWeaponPatches(void* myPlayer) {
    if (!myPlayer) return;
    if (!g_state.noRecoil && !g_state.noSpread) return;

    __try {
        void* cameraController  = *(void**)((uint8_t*)myPlayer + 0x48);          // _cameraController
        if (!cameraController) return;
        void* recoilWeaponInfo  = *(void**)((uint8_t*)cameraController + 0x188); // _recoilWeaponInfo
        if (!recoilWeaponInfo) return;

        if (g_state.noRecoil) {
            *(float*)((uint8_t*)recoilWeaponInfo + 0x60) = 0.0f; // RecoilKickPower
            *(float*)((uint8_t*)recoilWeaponInfo + 0x7C) = 0.0f; // CamRecoilPowerRange.x
            *(float*)((uint8_t*)recoilWeaponInfo + 0x80) = 0.0f; // CamRecoilPowerRange.y
            *(float*)((uint8_t*)cameraController + 0x194) = 0.0f; // _recoilPower
            *(float*)((uint8_t*)cameraController + 0x198) = 0.0f; // _recoilAnimValue
        }
        if (g_state.noSpread) {
            *(float*)((uint8_t*)recoilWeaponInfo + 0x34) = 0.0f; // SpreadVerMinMax.x
            *(float*)((uint8_t*)recoilWeaponInfo + 0x38) = 0.0f; // SpreadVerMinMax.y
            *(float*)((uint8_t*)recoilWeaponInfo + 0x3C) = 0.0f; // SpreadHorMinMax.x
            *(float*)((uint8_t*)recoilWeaponInfo + 0x40) = 0.0f; // SpreadHorMinMax.y
        }
    } __except(1) {}
}

} // namespace Combat
