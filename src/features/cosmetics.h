// features/cosmetics.h
// Unlock-all byte patches + cloud save flow.
#pragma once

namespace Cosmetics {
    // True after ApplyUnlockAll() has run successfully.
    bool IsUnlockActive();

    void ApplyUnlockAll();
    void RestoreUnlockAll();

    // Persists currently equipped items to PlayFab. Updates the message
    // strings used by the menu.
    void SaveEquippedToCloud();

    // Status feedback for the menu.
    extern char  g_statusMsg[128];
    extern unsigned long g_statusTime;
}
