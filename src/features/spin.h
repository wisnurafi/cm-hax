// features/spin.h
// Spin: rotates the local player's third-person model so spectators/enemies
// see you spinning. Does not affect your local camera or aim.
#pragma once

namespace Spin {
    // Called from ESP data thread each tick
    void Tick(void* myPlayer);
}
