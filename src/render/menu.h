// render/menu.h
// Main menu draw routine + tab definitions.
#pragma once

namespace Menu {
    enum Tab {
        TAB_ESP = 0,
        TAB_AIM,
        TAB_COMBAT,
        TAB_COSMETICS,
        TAB_DEBUG,
        TAB_COUNT
    };

    // Draws the menu window (chrome + active tab content). Caller must
    // already be inside an ImGui frame.
    void Draw();
}
