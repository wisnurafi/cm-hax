// features/magicbullet.h
// Magic Bullet: shifts the shot origin position so your current look direction
// raycasts through the nearest enemy. Separate from silent aim.
#pragma once

namespace MagicBullet {
    bool Init();
    void Shutdown();
    bool IsInitialized();
}
