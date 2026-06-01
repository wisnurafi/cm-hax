// features/silentaim.cpp
// Silent aim via VEH + INT3 on UseShootingWeapon only.
// Modifies pivot transform + command input angles on the shot frame.
// Restores pivot via HWBP, restores angles via HWBP.
// Works within a configurable FOV cone — the server has angle tolerance
// but rejects extreme deviations.
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <math.h>

#include "silentaim.h"
#include "../core/globals.h"
#include "../core/logging.h"
#include "../il2cpp/il2cpp.h"
#include "../il2cpp/sdk.h"
#include "../il2cpp/player.h"
#include "../il2cpp/offsets.h"
#include "../utils/memory.h"
#include "../utils/math.h"
#include "../utils/xorstr.h"
#include "aimbot/hitbox.h"

namespace SilentAim {

typedef void (__fastcall* t_get_rotation)(void* outQuat, void* transform, void* methodInfo);
typedef void (__fastcall* t_set_rotation)(void* transform, void* quat, void* methodInfo);
static t_get_rotation fn_get_rotation = nullptr;
static t_set_rotation fn_set_rotation = nullptr;

typedef void* (__fastcall* t_get_input)(void* playerCommand, void* methodInfo);
typedef void  (__fastcall* t_set_float)(void* input, float value, void* methodInfo);
typedef float (__fastcall* t_get_float)(void* input, void* methodInfo);
static t_get_input fn_get_input = nullptr;
static t_set_float fn_set_verticalRotation = nullptr;
static t_set_float fn_set_horizontalRotation = nullptr;
static t_get_float fn_get_verticalRotation = nullptr;
static t_get_float fn_get_horizontalRotation = nullptr;

static uint8_t* s_hookAddr = nullptr;
static uint8_t  s_originalByte = 0;
static PVOID    s_vehHandle = nullptr;
static bool     g_initialized = false;

static volatile LONG s_singleStepping = 0;
static volatile LONG s_waitingReturn = 0;

struct Quat { float x, y, z, w; };

static Quat    s_savedRotation = { 0, 0, 0, 1 };
static void*   s_savedPivot = nullptr;
static void*   s_savedInput = nullptr;
static float   s_savedVertRot = 0.0f;
static float   s_savedHorizRot = 0.0f;
static uint64_t s_returnAddr = 0;

static const float kPI = 3.14159265358979f;
static const float kRad2Deg = 180.0f / kPI;

static Quat LookRotation(Vector3 forward) {
    float len = sqrtf(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    if (len < 0.0001f) return { 0, 0, 0, 1 };
    forward.x /= len; forward.y /= len; forward.z /= len;

    Vector3 up = { 0, 1, 0 };
    Vector3 right = {
        up.y * forward.z - up.z * forward.y,
        up.z * forward.x - up.x * forward.z,
        up.x * forward.y - up.y * forward.x
    };
    float rLen = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
    if (rLen < 0.0001f) { right = { 1, 0, 0 }; rLen = 1.0f; }
    right.x /= rLen; right.y /= rLen; right.z /= rLen;

    up.x = forward.y * right.z - forward.z * right.y;
    up.y = forward.z * right.x - forward.x * right.z;
    up.z = forward.x * right.y - forward.y * right.x;

    float m00 = right.x, m01 = up.x, m02 = forward.x;
    float m10 = right.y, m11 = up.y, m12 = forward.y;
    float m20 = right.z, m21 = up.z, m22 = forward.z;

    Quat q;
    float trace = m00 + m11 + m22;
    if (trace > 0.0f) {
        float s = 0.5f / sqrtf(trace + 1.0f);
        q.w = 0.25f / s;
        q.x = (m21 - m12) * s;
        q.y = (m02 - m20) * s;
        q.z = (m10 - m01) * s;
    } else if (m00 > m11 && m00 > m22) {
        float s = 2.0f * sqrtf(1.0f + m00 - m11 - m22);
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    } else if (m11 > m22) {
        float s = 2.0f * sqrtf(1.0f + m11 - m00 - m22);
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m21 + m12) / s;
    } else {
        float s = 2.0f * sqrtf(1.0f + m22 - m00 - m11);
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m21 + m12) / s;
        q.z = 0.25f * s;
    }
    float qLen = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (qLen > 0.0001f) { q.x /= qLen; q.y /= qLen; q.z /= qLen; q.w /= qLen; }
    return q;
}

static void DirToAngles(Vector3 dir, float& outPitch, float& outYaw) {
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len < 0.0001f) { outPitch = 0; outYaw = 0; return; }
    dir.x /= len; dir.y /= len; dir.z /= len;
    outPitch = asinf(dir.y) * kRad2Deg;
    outYaw = atan2f(dir.x, dir.z) * kRad2Deg;
}

static float AngleToTarget(Vector3 origin, Vector3 targetPos, float curPitch, float curYaw) {
    Vector3 dir = {
        targetPos.x - origin.x,
        targetPos.y - origin.y,
        targetPos.z - origin.z
    };
    float targetPitch, targetYaw;
    DirToAngles(dir, targetPitch, targetYaw);
    float dPitch = targetPitch - curPitch;
    float dYaw = targetYaw - curYaw;
    while (dYaw > 180.0f) dYaw -= 360.0f;
    while (dYaw < -180.0f) dYaw += 360.0f;
    return sqrtf(dPitch * dPitch + dYaw * dYaw);
}

static void SetHWBP(PCONTEXT ctx, uint64_t addr) {
    ctx->Dr0 = addr;
    ctx->Dr7 = (ctx->Dr7 & ~0xF) | 0x1;
    ctx->Dr7 &= ~(0xF << 16);
}

static void ClearHWBP(PCONTEXT ctx) {
    ctx->Dr0 = 0;
    ctx->Dr7 &= ~0x1;
}

static void ApplySilentAim(PCONTEXT ctx) {
    if (!g_state.silentAimEnabled && !g_state.magicBulletEnabled) return;

    void* playerArming = (void*)ctx->Rcx;
    void* playerCommand = (void*)ctx->R8;
    if (!IsPlausiblePtr(playerArming)) return;

    __try {
        void* playerRoot = *(void**)((uint8_t*)playerArming + 0x120);
        if (!IsPlausiblePtr(playerRoot)) return;

        void* pivotTransform = *(void**)((uint8_t*)playerRoot + 0x30);
        if (!IsPlausiblePtr(pivotTransform)) return;

        Vector3 pivotPos = { 0, 0, 0 };
        if (IL2CPP::fn_get_position) {
            IL2CPP::fn_get_position(&pivotPos, pivotTransform, nullptr);
        }

        // Get current aim angles
        float curPitch = 0.0f, curYaw = 0.0f;
        void* cmdInput = nullptr;
        if (IsPlausiblePtr(playerCommand) && fn_get_input) {
            cmdInput = fn_get_input(playerCommand, nullptr);
            if (IsPlausiblePtr(cmdInput)) {
                curPitch = fn_get_verticalRotation(cmdInput, nullptr);
                curYaw = fn_get_horizontalRotation(cmdInput, nullptr);
            }
        }

        // Find best target within FOV
        Vector3 bestTarget = { 0, 0, 0 };
        float bestAngle = 99999.0f;
        bool found = false;

        EnterCriticalSection(&Runtime::g_espCs);
        int count = Runtime::g_espData.playerCount;
        if (pivotPos.x == 0.0f && pivotPos.y == 0.0f && pivotPos.z == 0.0f) {
            pivotPos = Runtime::g_espData.myPos;
        }
        for (int i = 0; i < count && i < 64; i++) {
            const PlayerData& p = Runtime::g_espData.players[i];
            if (!p.valid || p.hp <= 0) continue;
            if (g_state.visibleCheck && !p.visible) continue;

            // Use hitbox resolver
            AimHitboxResolution res;
            Vector3 targetPos;
            if (ResolveAimHitbox(p, g_state.rageHitboxMask, res)) {
                targetPos = res.world;
            } else if (p.hasHead) {
                targetPos = p.headPos;
            } else {
                targetPos = p.rootPos;
            }

            float angle = AngleToTarget(pivotPos, targetPos, curPitch, curYaw);
            if (angle < bestAngle) {
                bestAngle = angle;
                bestTarget = targetPos;
                found = true;
            }
        }
        LeaveCriticalSection(&Runtime::g_espCs);

        if (!found) return;

        // Clamp to FOV — magic bullet bypasses the limit
        if (!g_state.magicBulletEnabled && bestAngle > g_state.silentAimFov) return;

        // Direction to target
        Vector3 dir = {
            bestTarget.x - pivotPos.x,
            bestTarget.y - pivotPos.y,
            bestTarget.z - pivotPos.z
        };

        // Modify pivot
        fn_get_rotation(&s_savedRotation, pivotTransform, nullptr);
        s_savedPivot = pivotTransform;
        Quat aimQuat = LookRotation(dir);
        fn_set_rotation(pivotTransform, &aimQuat, nullptr);

        // Modify command angles to match (server validates consistency)
        s_savedInput = nullptr;
        if (IsPlausiblePtr(cmdInput)) {
            float aimPitch, aimYaw;
            DirToAngles(dir, aimPitch, aimYaw);
            s_savedVertRot = curPitch;
            s_savedHorizRot = curYaw;
            s_savedInput = cmdInput;
            fn_set_verticalRotation(cmdInput, aimPitch, nullptr);
            fn_set_horizontalRotation(cmdInput, aimYaw, nullptr);
        }

        // HWBP to restore both on return
        s_returnAddr = *(uint64_t*)ctx->Rsp;
        SetHWBP(ctx, s_returnAddr);
        InterlockedExchange(&s_waitingReturn, 1);

    } __except(1) {
        s_savedPivot = nullptr;
        s_savedInput = nullptr;
    }
}

static void RestoreAll() {
    // Restore pivot rotation (silent aim)
    if (s_savedPivot && IsPlausiblePtr(s_savedPivot)) {
        __try {
            fn_set_rotation(s_savedPivot, &s_savedRotation, nullptr);
        } __except(1) {}
    }
    s_savedPivot = nullptr;

    if (s_savedInput && IsPlausiblePtr(s_savedInput)) {
        __try {
            fn_set_verticalRotation(s_savedInput, s_savedVertRot, nullptr);
            fn_set_horizontalRotation(s_savedInput, s_savedHorizRot, nullptr);
        } __except(1) {}
    }
    s_savedInput = nullptr;
}

static LONG NTAPI VehHandler(PEXCEPTION_POINTERS ex) {
    PEXCEPTION_RECORD rec = ex->ExceptionRecord;
    PCONTEXT ctx = ex->ContextRecord;

    if (rec->ExceptionCode == EXCEPTION_BREAKPOINT &&
        (uint8_t*)rec->ExceptionAddress == s_hookAddr) {

        // Apply silent aim
        ApplySilentAim(ctx);

        DWORD oldProt;
        VirtualProtect(s_hookAddr, 1, PAGE_EXECUTE_READWRITE, &oldProt);
        *s_hookAddr = s_originalByte;
        VirtualProtect(s_hookAddr, 1, oldProt, &oldProt);

        ctx->EFlags |= 0x100;
        InterlockedExchange(&s_singleStepping, 1);
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    if (rec->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        if (InterlockedCompareExchange(&s_singleStepping, 0, 1) == 1) {
            DWORD oldProt;
            VirtualProtect(s_hookAddr, 1, PAGE_EXECUTE_READWRITE, &oldProt);
            *s_hookAddr = 0xCC;
            VirtualProtect(s_hookAddr, 1, oldProt, &oldProt);
            ctx->EFlags &= ~0x100;
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        if (InterlockedCompareExchange(&s_waitingReturn, 0, 1) == 1) {
            RestoreAll();
            ClearHWBP(ctx);
            ctx->EFlags &= ~0x100;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

bool IsInitialized() { return g_initialized; }
void PollRestore() {}

bool Init() {
    if (g_initialized) return true;

    uintptr_t base = (uintptr_t)IL2CPP::hProject;
    if (!base) return false;

    fn_get_rotation = (t_get_rotation)(base + 0x2FA9A80);
    fn_set_rotation = (t_set_rotation)(base + 0x2FAA7B0);
    fn_get_input = (t_get_input)(base + 0x720A50);
    fn_get_verticalRotation = (t_get_float)(base + 0x6E7EA0);
    fn_set_verticalRotation = (t_set_float)(base + 0x6EB520);
    fn_get_horizontalRotation = (t_get_float)(base + 0x6E7140);
    fn_set_horizontalRotation = (t_set_float)(base + 0x6E8DA0);

    s_hookAddr = (uint8_t*)(base + 0x34A0650);

    MEMORY_BASIC_INFORMATION mbi = {};
    if (!VirtualQuery(s_hookAddr, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;

    Log("SilentAim: hook @ %p: %02X %02X %02X %02X %02X",
        s_hookAddr, s_hookAddr[0], s_hookAddr[1], s_hookAddr[2], s_hookAddr[3], s_hookAddr[4]);

    s_vehHandle = AddVectoredExceptionHandler(1, VehHandler);
    if (!s_vehHandle) return false;

    s_originalByte = *s_hookAddr;
    DWORD oldProtect;
    if (!VirtualProtect(s_hookAddr, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        RemoveVectoredExceptionHandler(s_vehHandle);
        s_vehHandle = nullptr;
        return false;
    }
    *s_hookAddr = 0xCC;
    VirtualProtect(s_hookAddr, 1, oldProtect, &oldProtect);

    g_initialized = true;
    Log("SilentAim: FOV-limited hook installed (fov=%.1f, orig=0x%02X)", g_state.silentAimFov, s_originalByte);
    return true;
}

void Shutdown() {
    if (!g_initialized) return;

    if (s_hookAddr) {
        DWORD oldProtect;
        if (VirtualProtect(s_hookAddr, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            *s_hookAddr = s_originalByte;
            VirtualProtect(s_hookAddr, 1, oldProtect, &oldProtect);
        }
        s_hookAddr = nullptr;
    }

    if (s_vehHandle) {
        RemoveVectoredExceptionHandler(s_vehHandle);
        s_vehHandle = nullptr;
    }

    g_initialized = false;
    Log("SilentAim: hook removed");
}

} // namespace SilentAim
