// render/esp_render.h
// World-space ESP renderer. Reads from Runtime::g_espData under the lock,
// projects bones/box points to screen, draws the overlay, and feeds the
// chosen aim target into the aimbot smoother.
#pragma once

namespace ESPRender {
    void Draw();
}
