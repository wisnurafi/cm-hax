// render/menu_style.h
// Menu palette + ImGui style + font loading.
#pragma once
#include "../../third_party/imgui/imgui.h"

namespace MenuStyle {
    extern ImFont* g_fontBody;
    extern ImFont* g_fontTitle;
    extern ImFont* g_fontSmall;

    // Palette
    extern const ImVec4 kAccent;
    extern const ImVec4 kAccentSoft;
    extern const ImVec4 kAccentDim;
    extern const ImVec4 kAccentDimmer;
    extern const ImVec4 kSuccess;
    extern const ImVec4 kWarning;
    extern const ImVec4 kDanger;
    extern const ImVec4 kMutedText;
    extern const ImVec4 kSubtleText;
    extern const ImVec4 kCardBg;
    extern const ImVec4 kCardBorder;

    // Loads Segoe UI variants from %WINDIR%\Fonts. Falls back to default.
    void LoadFonts();

    // Push the carbon/onyx palette + spacing tweaks.
    void Apply();
}
