// features/movement.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "movement.h"
#include "../core/globals.h"
#include "../il2cpp/offsets.h"
#include "../utils/memory.h"
#include "../utils/input.h"

namespace Movement {

// Auto slide-cancel state. We track how many frames we've been sliding
// and inject the cancel sequence at the right time.
static int  s_slideFrameCount = 0;
static bool s_wasSliding      = false;

void ApplyMovementPatches(void* myPlayer) {
    if (!myPlayer) return;
    if (!g_state.bunnyhopEnabled && !g_state.infiniteSprintEnabled && !g_state.autoSlideCancelEnabled) return;

    __try {
        void* movement = *(void**)((uint8_t*)myPlayer + OFF_PLAYERROOT_MOVEMENT);
        if (!IsPlausiblePtr(movement)) return;

        uint8_t* mov = (uint8_t*)movement;

        // ---- Infinite Sprint ----
        // Keep _sprintStamina at max (1.0f) every frame.
        if (g_state.infiniteSprintEnabled) {
            *(float*)(mov + OFF_PLAYERMOVEMENT_SPRINTSTAMINA) = 1.0f;
        }

        // ---- Auto Bunny-Hop ----
        // When grounded and the jump key (space) is held, set _isJumpInput
        // to true. This makes the player jump on the exact frame they land,
        // preserving slide/sprint momentum.
        if (g_state.bunnyhopEnabled) {
            bool isGrounded = *(bool*)(mov + OFF_PLAYERMOVEMENT_ISGROUNDED);
            bool spaceHeld  = Input::IsKeyDown(VK_SPACE);
            if (isGrounded && spaceHeld) {
                *(bool*)(mov + OFF_PLAYERMOVEMENT_ISJUMPINPUT) = true;
            }
        }

        // ---- Auto Slide-Cancel ----
        // Detect slide start. After a configurable number of frames (default
        // ~8, roughly 130ms at 60fps), inject: release crouch + jump input.
        // This mimics the manual slide-cancel tech that skilled players do.
        if (g_state.autoSlideCancelEnabled) {
            bool isSliding = *(bool*)(mov + OFF_PLAYERMOVEMENT_ISSLIDING);

            if (isSliding) {
                s_slideFrameCount++;

                // Cancel after N frames of sliding
                if (s_slideFrameCount >= g_state.slideCancelFrames) {
                    // Release slide/crouch and inject jump
                    *(bool*)(mov + OFF_PLAYERMOVEMENT_ISSLIDING)    = false;
                    *(bool*)(mov + OFF_PLAYERMOVEMENT_ISCROUCHINPUT) = false;
                    *(bool*)(mov + OFF_PLAYERMOVEMENT_ISJUMPINPUT)   = true;
                    s_slideFrameCount = 0;
                }
            } else {
                s_slideFrameCount = 0;
            }

            s_wasSliding = isSliding;
        } else {
            s_slideFrameCount = 0;
            s_wasSliding = false;
        }
    } __except(1) {}
}

} // namespace Movement
