// render/menu_style.cpp
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "menu_style.h"

namespace MenuStyle {

ImFont* g_fontBody  = NULL;
ImFont* g_fontTitle = NULL;
ImFont* g_fontSmall = NULL;

// Glassmorphism dark palette.
// Base: near-black with a cool blue undertone.
// Panels: translucent dark glass (alpha < 1.0) so the game bleeds through.
// Accent: icy blue-white, used sparingly with soft glow.
// Borders: very faint luminous edges simulating light refraction on glass.
const ImVec4 kAccent       = ImVec4(0.45f, 0.82f, 1.00f, 1.00f);
const ImVec4 kAccentSoft   = ImVec4(0.45f, 0.82f, 1.00f, 0.40f);
const ImVec4 kAccentDim    = ImVec4(0.45f, 0.82f, 1.00f, 0.12f);
const ImVec4 kAccentDimmer = ImVec4(0.45f, 0.82f, 1.00f, 0.05f);
const ImVec4 kSuccess      = ImVec4(0.35f, 0.92f, 0.55f, 1.00f);
const ImVec4 kWarning      = ImVec4(1.00f, 0.78f, 0.28f, 1.00f);
const ImVec4 kDanger       = ImVec4(0.95f, 0.35f, 0.35f, 1.00f);
const ImVec4 kMutedText    = ImVec4(0.45f, 0.50f, 0.58f, 1.00f);
const ImVec4 kSubtleText   = ImVec4(0.32f, 0.36f, 0.42f, 1.00f);
const ImVec4 kCardBg       = ImVec4(0.06f, 0.07f, 0.09f, 0.65f);
const ImVec4 kCardBorder   = ImVec4(0.20f, 0.24f, 0.30f, 0.25f);

void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.OversampleH = 3;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = true;

    char winDir[MAX_PATH] = {};
    GetWindowsDirectoryA(winDir, MAX_PATH);

    char segoe[MAX_PATH], segoesb[MAX_PATH];
    snprintf(segoe,   sizeof(segoe),   "%s\\Fonts\\segoeui.ttf", winDir);
    snprintf(segoesb, sizeof(segoesb), "%s\\Fonts\\seguisb.ttf", winDir);

    if (GetFileAttributesA(segoe) != INVALID_FILE_ATTRIBUTES) {
        g_fontBody  = io.Fonts->AddFontFromFileTTF(segoe, 15.0f, &cfg);
        g_fontSmall = io.Fonts->AddFontFromFileTTF(segoe, 12.0f, &cfg);
    }
    if (GetFileAttributesA(segoesb) != INVALID_FILE_ATTRIBUTES) {
        g_fontTitle = io.Fonts->AddFontFromFileTTF(segoesb, 20.0f, &cfg);
    } else if (GetFileAttributesA(segoe) != INVALID_FILE_ATTRIBUTES) {
        g_fontTitle = io.Fonts->AddFontFromFileTTF(segoe, 20.0f, &cfg);
    }

    if (!g_fontBody)  g_fontBody  = io.Fonts->AddFontDefault();
    if (!g_fontSmall) g_fontSmall = g_fontBody;
    if (!g_fontTitle) g_fontTitle = g_fontBody;

    io.FontDefault = g_fontBody;
}

void Apply() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Glassmorphism: slightly rounded, generous padding, no harsh edges.
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.WindowRounding = 10.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowPadding = ImVec2(0.0f, 0.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 5.0f);
    style.IndentSpacing = 16.0f;
    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = 8.0f;

    ImVec4* colors = style.Colors;

    // Text
    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.94f, 0.97f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.32f, 0.36f, 0.42f, 1.00f);

    // Backgrounds: translucent dark glass
    colors[ImGuiCol_WindowBg]              = ImVec4(0.04f, 0.045f, 0.06f, 0.92f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.05f, 0.055f, 0.07f, 0.60f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.06f, 0.065f, 0.08f, 0.94f);

    // Borders: faint luminous glass edges
    colors[ImGuiCol_Border]                = ImVec4(0.22f, 0.26f, 0.32f, 0.30f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Frames: frosted glass input fields
    colors[ImGuiCol_FrameBg]               = ImVec4(0.08f, 0.09f, 0.12f, 0.70f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.12f, 0.14f, 0.18f, 0.80f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.16f, 0.18f, 0.24f, 0.85f);

    // Title
    colors[ImGuiCol_TitleBg]               = ImVec4(0.04f, 0.045f, 0.06f, 0.95f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.06f, 0.065f, 0.08f, 0.95f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.04f, 0.045f, 0.06f, 0.80f);

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.04f, 0.045f, 0.06f, 0.00f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.18f, 0.20f, 0.26f, 0.60f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.25f, 0.28f, 0.35f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.32f, 0.36f, 0.44f, 0.90f);

    // Interactive: accent-tinted
    colors[ImGuiCol_CheckMark]             = ImVec4(0.45f, 0.82f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.45f, 0.82f, 1.00f, 0.85f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.60f, 0.90f, 1.00f, 1.00f);

    // Buttons: glass surface
    colors[ImGuiCol_Button]                = ImVec4(0.10f, 0.12f, 0.15f, 0.65f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.15f, 0.18f, 0.22f, 0.80f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.20f, 0.24f, 0.30f, 0.90f);

    // Headers
    colors[ImGuiCol_Header]                = ImVec4(0.08f, 0.10f, 0.13f, 0.65f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.14f, 0.16f, 0.21f, 0.80f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.18f, 0.21f, 0.27f, 0.90f);

    // Separators
    colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.24f, 0.30f, 0.20f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.30f, 0.35f, 0.42f, 0.60f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.40f, 0.46f, 0.55f, 0.80f);

    // Resize grip
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.15f, 0.18f, 0.22f, 0.30f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.25f, 0.30f, 0.36f, 0.60f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.35f, 0.40f, 0.48f, 0.80f);

    // Tabs
    colors[ImGuiCol_Tab]                   = ImVec4(0.06f, 0.07f, 0.09f, 0.70f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.14f, 0.16f, 0.21f, 0.85f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.10f, 0.12f, 0.16f, 0.90f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.05f, 0.055f, 0.07f, 0.70f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.08f, 0.09f, 0.12f, 0.80f);

    // Plots
    colors[ImGuiCol_PlotLines]             = ImVec4(0.45f, 0.82f, 1.00f, 0.80f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.60f, 0.90f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.45f, 0.82f, 1.00f, 0.80f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.60f, 0.90f, 1.00f, 1.00f);

    // Misc
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.20f, 0.35f, 0.50f, 0.45f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.45f, 0.82f, 1.00f, 0.75f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.45f, 0.82f, 1.00f, 0.55f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.55f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.02f, 0.02f, 0.03f, 0.60f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.02f, 0.02f, 0.03f, 0.60f);
}

} // namespace MenuStyle
