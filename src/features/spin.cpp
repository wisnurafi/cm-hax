// features/spin.cpp
// Spin: rotates the local player's third-person model Y-axis each tick.
// Other players/spectators see you spinning. Your local view is unaffected.
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <math.h>

#include "spin.h"
#include "../core/globals.h"
#include "../il2cpp/il2cpp.h"
#include "../il2cpp/offsets.h"
#include "../utils/memory.h"

namespace Spin {

// set_rotation signature (same as silent aim uses)
typedef void (__fastcall* t_set_rotation)(void* transform, void* quat, void* methodInfo);
static t_set_rotation fn_set_rotation = nullptr;

static float s_angle = 0.0f; // current spin angle in radians
static DWORD s_lastTick = 0; // for real delta time

struct Quat { float x, y, z, w; };

// Y-axis rotation quaternion
static Quat YawQuat(float radians) {
    Quat q;
    q.x = 0.0f;
    q.y = sinf(radians * 0.5f);
    q.z = 0.0f;
    q.w = cosf(radians * 0.5f);
    return q;
}

void Tick(void* myPlayer) {
    if (!g_state.spinEnabled || !myPlayer) return;

    // Resolve set_rotation once
    if (!fn_set_rotation) {
        uintptr_t base = (uintptr_t)IL2CPP::hProject;
        if (!base) return;
        fn_set_rotation = (t_set_rotation)(base + 0x2FAA7B0);
    }

    __try {
        // Get third-person view (PlayerMobView)
        void* mobView = *(void**)((uint8_t*)myPlayer + OFF_PLAYERROOT_THIRDPERSONVIEW);
        if (!IsPlausiblePtr(mobView)) return;

        // Get the mob view's transform via Component.get_transform
        void* mobTransform = IL2CPP::fn_component_get_transform(mobView, nullptr);
        if (!IsPlausiblePtr(mobTransform)) return;

        // Advance spin angle using real delta time
        DWORD now = GetTickCount();
        float dt = 0.016f; // fallback
        if (s_lastTick > 0 && now > s_lastTick) {
            dt = (float)(now - s_lastTick) / 1000.0f;
            if (dt > 0.1f) dt = 0.1f; // clamp to avoid huge jumps
        }
        s_lastTick = now;

        float speed = g_state.spinSpeed; // rotations per second
        s_angle += speed * 6.28318f * dt; // 2*PI * speed * dt
        if (s_angle > 6.28318f) s_angle -= 6.28318f;

        // Apply Y rotation to the third-person model
        Quat q = YawQuat(s_angle);
        fn_set_rotation(mobTransform, &q, nullptr);

    } __except(1) {}
}

} // namespace Spin
