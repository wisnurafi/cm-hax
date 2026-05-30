// features/cosmetics.cpp
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "cosmetics.h"
#include "../core/globals.h"
#include "../core/logging.h"
#include "../utils/memory.h"

namespace Cosmetics {

char  g_statusMsg[128] = "";
unsigned long g_statusTime = 0;

// Is*Available RVAs (patched to always return true for unlock-all).
static const uintptr_t kUnlockRVAs[] = {
    0x351E860,  // CharacterStore::IsEmblemAvailable
    0x351E740,  // CharacterStore::IsCallingCardAvailable
    0x351ED10,  // CharacterStore::IsOperatorAvailable
    0x351E9A0,  // CharacterStore::IsOperatorAccessoryAvailable
    0x351F0E0,  // CharacterStore::IsWristbandAvailable
    0x351EE30,  // CharacterStore::IsVehicleAvailable
    0x35172A0,  // ArsenalStore::IsBlueprintAvailable
    0x3518260,  // ArsenalStore::IsCharmAvailable
    0x3517500,  // ArsenalStore::IsCamoAvailable
    0x35183D0,  // ArsenalStore::IsDetailColorAvailable
    0x3518CA0,  // ArsenalStore::IsReticleAvailable
    0x3518F40,  // ArsenalStore::IsWeaponAvailable
    0x35170F0,  // ArsenalStore::IsBattlePerkAvailable
};
static const int kUnlockCount = (int)(sizeof(kUnlockRVAs) / sizeof(kUnlockRVAs[0]));

static BytePatch s_patches[kUnlockCount] = {};
static bool      s_unlockActive = false;

bool IsUnlockActive() { return s_unlockActive; }

void ApplyUnlockAll() {
    if (s_unlockActive) return;
    HMODULE hProject = GetModuleHandleA("Project.dll");
    if (!hProject) return;

    uintptr_t base = (uintptr_t)hProject;
    uint8_t patch[] = { 0xB0, 0x01, 0xC3 }; // mov al, 1; ret
    int patched = 0;
    for (int i = 0; i < kUnlockCount; i++) {
        uint8_t* addr = (uint8_t*)(base + kUnlockRVAs[i]);
        s_patches[i].addr = addr;
        s_patches[i].active = false;
        DWORD oldProtect;
        if (VirtualProtect(addr, 3, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(s_patches[i].original, addr, 3);
            memcpy(addr, patch, 3);
            VirtualProtect(addr, 3, oldProtect, &oldProtect);
            s_patches[i].active = true;
            patched++;
        }
    }
    s_unlockActive = true;
    Log("UnlockAll: patched %d/%d Is*Available functions", patched, kUnlockCount);
}

void RestoreUnlockAll() {
    if (!s_unlockActive) return;
    int restored = 0;
    for (int i = 0; i < kUnlockCount; i++) {
        if (!s_patches[i].active || !s_patches[i].addr) continue;
        DWORD oldProtect;
        if (VirtualProtect(s_patches[i].addr, 3, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(s_patches[i].addr, s_patches[i].original, 3);
            VirtualProtect(s_patches[i].addr, 3, oldProtect, &oldProtect);
            s_patches[i].active = false;
            restored++;
        }
    }
    s_unlockActive = false;
    Log("UnlockAll: restored %d functions", restored);
}

// ---- Max Level Weapons ----
// Patches:
//   get_IsMaxLevel() -> always return true (mov al, 1; ret)
//   get_Level()      -> jump to get_MaxLevel() so it returns max value
//   IsAttachmentAvailable() + all weapon-level-gated availability checks
static const uintptr_t kRVA_IsMaxLevel          = 0x3530D40;
static const uintptr_t kRVA_GetLevel            = 0x3530DA0;
static const uintptr_t kRVA_GetMaxLevel         = 0x3530E90;

static const uintptr_t kMaxLevelAvailRVAs[] = {
    0x3516AB0,  // ArsenalStore::IsAttachmentAvailable(EntityType, EntityType)
    0x3517500,  // ArsenalStore::IsCamoAvailable(EntityType, EntityType)
    0x3517610,  // ArsenalStore::IsCamoAvailable(WeaponStoreData&, EntityType)
    0x3517C50,  // ArsenalStore::IsCamoCategoryAvailable(EntityType, ECamoType)
    0x3517D60,  // ArsenalStore::IsCamoCategoryAvailable(WeaponStoreData&, ECamoType)
    0x35183D0,  // ArsenalStore::IsDetailColorAvailable(EntityType, EntityType)
    0x3518660,  // ArsenalStore::IsDetailColorCategoryAvailable(EntityType, EDetailColorType)
    0x3518770,  // ArsenalStore::IsDetailColorCategoryAvailable(WeaponStoreData&, EDetailColorType)
};
static const int kMaxLevelAvailCount = (int)(sizeof(kMaxLevelAvailRVAs) / sizeof(kMaxLevelAvailRVAs[0]));

static BytePatch s_isMaxLevelPatch = {};
static BytePatch s_maxLevelAvailPatches[8] = {};
static uint8_t   s_getLevelOriginal[5] = {};
static uint8_t*  s_getLevelAddr = nullptr;
static bool      s_maxLevelActive = false;

bool IsMaxLevelActive() { return s_maxLevelActive; }

void ApplyMaxLevel() {
    if (s_maxLevelActive) return;
    HMODULE hProject = GetModuleHandleA("Project.dll");
    if (!hProject) return;

    uintptr_t base = (uintptr_t)hProject;
    int patched = 0;
    uint8_t retTrue[] = { 0xB0, 0x01, 0xC3 }; // mov al, 1; ret

    // Patch get_IsMaxLevel -> mov al, 1; ret
    {
        uint8_t* addr = (uint8_t*)(base + kRVA_IsMaxLevel);
        s_isMaxLevelPatch.addr = addr;
        s_isMaxLevelPatch.active = false;
        DWORD oldProtect;
        if (VirtualProtect(addr, 3, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(s_isMaxLevelPatch.original, addr, 3);
            memcpy(addr, retTrue, 3);
            VirtualProtect(addr, 3, oldProtect, &oldProtect);
            s_isMaxLevelPatch.active = true;
            patched++;
        }
    }

    // Patch get_Level -> jmp get_MaxLevel (relative E9 jump)
    {
        s_getLevelAddr = (uint8_t*)(base + kRVA_GetLevel);
        uint8_t* targetAddr = (uint8_t*)(base + kRVA_GetMaxLevel);
        int32_t relOffset = (int32_t)(targetAddr - (s_getLevelAddr + 5));
        DWORD oldProtect;
        if (VirtualProtect(s_getLevelAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(s_getLevelOriginal, s_getLevelAddr, 5);
            s_getLevelAddr[0] = 0xE9; // jmp rel32
            memcpy(s_getLevelAddr + 1, &relOffset, 4);
            VirtualProtect(s_getLevelAddr, 5, oldProtect, &oldProtect);
            patched++;
        }
    }

    // Patch all weapon-level-gated availability checks -> mov al, 1; ret
    for (int i = 0; i < kMaxLevelAvailCount; i++) {
        uint8_t* addr = (uint8_t*)(base + kMaxLevelAvailRVAs[i]);
        s_maxLevelAvailPatches[i].addr = addr;
        s_maxLevelAvailPatches[i].active = false;
        DWORD oldProtect;
        if (VirtualProtect(addr, 3, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(s_maxLevelAvailPatches[i].original, addr, 3);
            memcpy(addr, retTrue, 3);
            VirtualProtect(addr, 3, oldProtect, &oldProtect);
            s_maxLevelAvailPatches[i].active = true;
            patched++;
        }
    }

    s_maxLevelActive = true;
    Log("MaxLevel: patched %d/%d functions", patched, 2 + kMaxLevelAvailCount);
}

void RestoreMaxLevel() {
    if (!s_maxLevelActive) return;
    int restored = 0;

    // Restore get_IsMaxLevel
    if (s_isMaxLevelPatch.active && s_isMaxLevelPatch.addr) {
        DWORD oldProtect;
        if (VirtualProtect(s_isMaxLevelPatch.addr, 3, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(s_isMaxLevelPatch.addr, s_isMaxLevelPatch.original, 3);
            VirtualProtect(s_isMaxLevelPatch.addr, 3, oldProtect, &oldProtect);
            s_isMaxLevelPatch.active = false;
            restored++;
        }
    }

    // Restore get_Level
    if (s_getLevelAddr) {
        DWORD oldProtect;
        if (VirtualProtect(s_getLevelAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(s_getLevelAddr, s_getLevelOriginal, 5);
            VirtualProtect(s_getLevelAddr, 5, oldProtect, &oldProtect);
            s_getLevelAddr = nullptr;
            restored++;
        }
    }

    // Restore all availability patches
    for (int i = 0; i < kMaxLevelAvailCount; i++) {
        if (!s_maxLevelAvailPatches[i].active || !s_maxLevelAvailPatches[i].addr) continue;
        DWORD oldProtect;
        if (VirtualProtect(s_maxLevelAvailPatches[i].addr, 3, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(s_maxLevelAvailPatches[i].addr, s_maxLevelAvailPatches[i].original, 3);
            VirtualProtect(s_maxLevelAvailPatches[i].addr, 3, oldProtect, &oldProtect);
            s_maxLevelAvailPatches[i].active = false;
            restored++;
        }
    }

    s_maxLevelActive = false;
    Log("MaxLevel: restored %d functions", restored);
}

void SaveEquippedToCloud() {
    if (!g_state.gameClientStaticFields) {
        snprintf(g_statusMsg, sizeof(g_statusMsg), "ERROR: GameClient not resolved");
        g_statusTime = GetTickCount();
        return;
    }

    __try {
        uintptr_t gameBase = (uintptr_t)GetModuleHandleA("Project.dll");
        void* gameClientRef = *(void**)((uint8_t*)g_state.gameClientStaticFields + 0x0);
        if (!IsPlausiblePtr(gameClientRef)) {
            snprintf(g_statusMsg, sizeof(g_statusMsg), "ERROR: GameClient._ref is null");
        } else {
            void* profile = *(void**)((uint8_t*)gameClientRef + 0x60);
            if (!IsPlausiblePtr(profile)) {
                snprintf(g_statusMsg, sizeof(g_statusMsg), "ERROR: ProfileCache is null");
            } else {
                uint8_t* charStore = (uint8_t*)profile + 0x7D8;
                typedef void (*t_buy)(void*, int32_t, void*);
                struct { int offset; uintptr_t buyRVA; const char* name; } items[] = {
                    { 0x60, 0x351DFC0, "Operator" },
                    { 0x30, 0x351DE20, "Emblem" },
                    { 0x40, 0x351DD50, "CallingCard" },
                    { 0x70, 0x351E160, "Wristband" },
                };
                int bought = 0;
                for (int i = 0; i < 4; i++) {
                    int32_t id = *(int32_t*)(charStore + items[i].offset);
                    if (id > 0) {
                        t_buy fn = (t_buy)(gameBase + items[i].buyRVA);
                        fn(charStore, id, NULL);
                        bought++;
                        Log("Bought %s: id=%d", items[i].name, id);
                    }
                }
                *(bool*)((uint8_t*)profile + 0x1DF4) = true; // _forceSaveToPlayfab
                snprintf(g_statusMsg, sizeof(g_statusMsg), "Saved %d items to cloud!", bought);
                Log("PersistUnlocks: saved %d items, _forceSaveToPlayfab=true", bought);
            }
        }
    } __except(1) {
        snprintf(g_statusMsg, sizeof(g_statusMsg), "ERROR: Exception during save");
    }
    g_statusTime = GetTickCount();
}

} // namespace Cosmetics
