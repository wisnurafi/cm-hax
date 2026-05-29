// render/overlay.cpp
#include <stdio.h>
#include "overlay.h"
#include "menu_style.h"
#include "imgui.h"
#include "../core/globals.h"
#include "../utils/math.h"

namespace Overlay {

void Draw() {
    if (!g_state.showFpsOverlay) return;

    ImGuiIO& io = ImGui::GetIO();
    static float s_frameMs = 16.6f;
    float dt = io.DeltaTime > 0.0f ? io.DeltaTime : 0.016f;
    s_frameMs += ((dt * 1000.0f) - s_frameMs) * ClampFloat(dt * 4.0f, 0.0f, 1.0f);
    float fps = (s_frameMs > 0.001f) ? (1000.0f / s_frameMs) : 0.0f;

    ImDrawList* fg = ImGui::GetForegroundDrawList();
    float padX = 14.0f, padY = 10.0f;

    ImGui::PushFont(MenuStyle::g_fontSmall);
    char fpsBuf[24];
    snprintf(fpsBuf, sizeof(fpsBuf), "%.0f FPS", fps);
    ImVec2 fpsSz = ImGui::CalcTextSize(fpsBuf);
    ImGui::PopFont();

    float w = fpsSz.x + 28.0f;
    float h = fpsSz.y + 10.0f;
    ImVec2 a = ImVec2(padX, padY);
    ImVec2 b = ImVec2(a.x + w, a.y + h);

    fg->AddRectFilled(a, b,
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.020f, 0.026f, 0.036f, 0.62f)),
        h * 0.5f);
    fg->AddRect(a, b,
        ImGui::ColorConvertFloat4ToU32(ImVec4(MenuStyle::kAccent.x, MenuStyle::kAccent.y, MenuStyle::kAccent.z, 0.18f)),
        h * 0.5f, 0, 1.0f);

    float dotR = 3.0f;
    float dotX = a.x + 11.0f;
    float dotY = (a.y + b.y) * 0.5f;
    ImVec4 dotCol = g_state.espEnabled ? MenuStyle::kSuccess : MenuStyle::kSubtleText;
    fg->AddCircleFilled(ImVec2(dotX, dotY), dotR + 3.0f,
        ImGui::ColorConvertFloat4ToU32(ImVec4(dotCol.x, dotCol.y, dotCol.z, 0.22f)));
    fg->AddCircleFilled(ImVec2(dotX, dotY), dotR,
        ImGui::ColorConvertFloat4ToU32(dotCol));

    ImGui::PushFont(MenuStyle::g_fontSmall);
    fg->AddText(
        ImVec2(dotX + 9.0f, (a.y + b.y) * 0.5f - fpsSz.y * 0.5f),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.92f, 0.96f, 1.0f, 0.92f)),
        fpsBuf);
    ImGui::PopFont();
}

} // namespace Overlay
