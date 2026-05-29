// render/radar.h
// Top-down 2D radar overlay. Draws enemy dots relative to the local player's
// position and camera yaw. Consumes the same SharedESPData snapshot that the
// ESP renderer uses — no additional IL2CPP calls needed.
#pragma once

namespace Radar {
    void Draw();
}
