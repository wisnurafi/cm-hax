// features/combat.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "combat.h"
#include "../core/globals.h"
#include "../utils/memory.h"

namespace Combat {

// ---- No Shake ----
// Patches PlayDamageShakeAnimation to immediately return (0xC3 = ret)
static const uintptr_t kRVA_PlayDamageShake = 0x33C6470;
static BytePatch s_shakePatch = {};
static bool      s_shakeActive = false;

bool IsNoShakeActive() { return s_shakeActive; }

void ApplyNoShake() {
    if (s_shakeActive) return;
    HMODULE hProject = GetModuleHandleA("Project.dll");
    if (!hProject) return;

    uint8_t* addr = (uint8_t*)((uintptr_t)hProject + kRVA_PlayDamageShake);
    s_shakePatch.addr = addr;
    s_shakePatch.active = false;
    DWORD oldProtect;
    if (VirtualProtect(addr, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(s_shakePatch.original, addr, 1);
        addr[0] = 0xC3; // ret
        VirtualProtect(addr, 1, oldProtect, &oldProtect);
        s_shakePatch.active = true;
    }
    s_shakeActive = true;
}

void RestoreNoShake() {
    if (!s_shakeActive) return;
    if (s_shakePatch.active && s_shakePatch.addr) {
        DWORD oldProtect;
        if (VirtualProtect(s_shakePatch.addr, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(s_shakePatch.addr, s_shakePatch.original, 1);
            VirtualProtect(s_shakePatch.addr, 1, oldProtect, &oldProtect);
            s_shakePatch.active = false;
        }
    }
    s_shakeActive = false;
}

// ---- Per-frame weapon patches ----
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
