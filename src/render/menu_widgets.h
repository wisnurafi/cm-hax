// render/menu_widgets.h
// Custom ImGui widgets used by the menu (toggle switch, pill, sidebar entry).
// All widgets use the central palette in MenuStyle and are stateless from the
// caller's perspective; per-widget animation state is keyed on the label.
#pragma once
#include "imgui.h"

namespace Widgets {
    // Section header with an accent square + label and a thin underline.
    void SectionHeader(const char* label);

    // Animated toggle switch. Returns true on click (caller handles side-effects).
    bool Toggle(const char* label, bool* v);

    // Pill button used by the hitbox selector.
    bool Pill(const char* label, bool active, float minWidth = 0.0f);

    struct PillItem { int flag; const char* label; };
    bool PillBitmaskRow(const char* id, int* mask, const PillItem* items, int count);

    // Header status pill (small rounded badge with tinted bg/border).
    void HeaderPill(const char* text, ImVec4 color);

    // Sidebar entry. Animated accent stripe + glyph + label. Returns true on click.
    enum SidebarGlyph {
        GLYPH_ESP = 0,
        GLYPH_AIM,
        GLYPH_COMBAT,
        GLYPH_COSMETICS,
        GLYPH_MISC,
    };
    bool SidebarButton(const char* label, int glyph, bool active);

    // Keybind capture button. Click to enter "waiting" mode; the next pressed
    // key (mouse / keyboard) writes into *vk. Pressing ESC clears the bind.
    // `unbound` is the label shown when *vk == 0 (typically "Unbound").
    // Returns true if *vk changed this frame.
    bool Keybind(const char* id, int* vk, const char* unbound = "Unbound");
}
