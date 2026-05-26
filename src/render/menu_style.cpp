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

const ImVec4 kAccent       = ImVec4(0.36f, 0.82f, 1.00f, 1.00f);
const ImVec4 kAccentSoft   = ImVec4(0.36f, 0.82f, 1.00f, 0.55f);
const ImVec4 kAccentDim    = ImVec4(0.36f, 0.82f, 1.00f, 0.18f);
const ImVec4 kAccentDimmer = ImVec4(0.36f, 0.82f, 1.00f, 0.08f);
const ImVec4 kSuccess      = ImVec4(0.42f, 0.95f, 0.58f, 1.00f);
const ImVec4 kWarning      = ImVec4(1.00f, 0.78f, 0.30f, 1.00f);
const ImVec4 kDanger       = ImVec4(1.00f, 0.42f, 0.42f, 1.00f);
const ImVec4 kMutedText    = ImVec4(0.58f, 0.64f, 0.74f, 1.00f);
const ImVec4 kSubtleText   = ImVec4(0.42f, 0.47f, 0.55f, 1.00f);
const ImVec4 kCardBg       = ImVec4(0.038f, 0.046f, 0.060f, 1.00f);
const ImVec4 kCardBorder   = ImVec4(0.075f, 0.095f, 0.125f, 1.00f);

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
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 5.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowPadding = ImVec2(0.0f, 0.0f);   // header/sidebar/footer manage their own padding
    style.FramePadding = ImVec2(9.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 7.0f);
    style.ItemInnerSpacing = ImVec2(7.0f, 5.0f);
    style.IndentSpacing = 16.0f;
    style.ScrollbarSize = 11.0f;
    style.GrabMinSize = 10.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.95f, 0.99f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.42f, 0.47f, 0.55f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.024f, 0.028f, 0.038f, 0.985f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.034f, 0.040f, 0.052f, 1.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.040f, 0.046f, 0.060f, 0.985f);
    colors[ImGuiCol_Border]                = ImVec4(0.090f, 0.110f, 0.140f, 0.95f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.075f, 0.092f, 0.118f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.115f, 0.150f, 0.195f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.150f, 0.205f, 0.270f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.024f, 0.028f, 0.038f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.045f, 0.055f, 0.072f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.024f, 0.028f, 0.038f, 0.85f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.024f, 0.028f, 0.038f, 0.00f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.130f, 0.165f, 0.210f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.190f, 0.250f, 0.320f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.245f, 0.330f, 0.420f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.36f, 0.82f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.36f, 0.82f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.60f, 0.92f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.092f, 0.115f, 0.150f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.150f, 0.205f, 0.270f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.205f, 0.290f, 0.380f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.080f, 0.105f, 0.140f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.130f, 0.180f, 0.235f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.165f, 0.230f, 0.305f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.090f, 0.115f, 0.150f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.205f, 0.290f, 0.380f, 1.00f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.275f, 0.385f, 0.500f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.110f, 0.150f, 0.195f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.205f, 0.290f, 0.380f, 0.85f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.275f, 0.385f, 0.500f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.060f, 0.078f, 0.100f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.150f, 0.205f, 0.270f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.115f, 0.155f, 0.205f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.050f, 0.065f, 0.085f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.085f, 0.115f, 0.150f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.36f, 0.82f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.60f, 0.92f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.36f, 0.82f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.60f, 0.92f, 1.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.205f, 0.290f, 0.380f, 0.55f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.36f, 0.82f, 1.00f, 0.85f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.36f, 0.82f, 1.00f, 0.65f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.65f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.05f, 0.05f, 0.05f, 0.55f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.05f, 0.05f, 0.05f, 0.55f);
}

} // namespace MenuStyle
