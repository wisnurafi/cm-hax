// render/radar.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>

#include "radar.h"
#include "menu_style.h"
#include "imgui.h"
#include "../core/globals.h"
#include "../utils/math.h"

namespace Radar {

// Extract yaw (horizontal rotation) from the view matrix.
// The view matrix's third row gives us the camera forward vector in world space.
static float GetCameraYaw(const Matrix4x4* view) {
    float fx = view->m[0][2];
    float fz = view->m[2][2];
    return atan2f(fx, fz);
}

void Draw() {
    if (!g_state.radarEnabled) return;

    Runtime::SharedESPData localData;
    EnterCriticalSection(&Runtime::g_espCs);
    localData = Runtime::g_espData;
    LeaveCriticalSection(&Runtime::g_espCs);

    if (localData.playerCount == 0) return;

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImGuiIO& io = ImGui::GetIO();

    float radius = g_state.radarRadius;
    float scale  = g_state.radarScale;

    // Position: top-right corner with padding
    float padding = 16.0f;
    float cx = io.DisplaySize.x - radius - padding;
    float cy = radius + padding + 40.0f;

    // Background circle
    ImU32 bgColor     = ImGui::ColorConvertFloat4ToU32(ImVec4(0.02f, 0.025f, 0.035f, 0.75f));
    ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(ImVec4(
        MenuStyle::kAccent.x, MenuStyle::kAccent.y, MenuStyle::kAccent.z, 0.35f));
    ImU32 crossColor  = ImGui::ColorConvertFloat4ToU32(ImVec4(0.15f, 0.18f, 0.22f, 0.6f));

    drawList->AddCircleFilled(ImVec2(cx, cy), radius, bgColor, 64);
    drawList->AddCircle(ImVec2(cx, cy), radius, borderColor, 64, 1.2f);

    // Crosshair lines
    drawList->AddLine(ImVec2(cx - radius * 0.7f, cy), ImVec2(cx + radius * 0.7f, cy), crossColor, 0.8f);
    drawList->AddLine(ImVec2(cx, cy - radius * 0.7f), ImVec2(cx, cy + radius * 0.7f), crossColor, 0.8f);

    // Local player dot (center) — orange/yellow
    ImU32 selfColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.75f, 0.15f, 0.95f));
    drawList->AddCircleFilled(ImVec2(cx, cy), 3.5f, selfColor);

    // Camera yaw for rotation + local player position from the data thread snapshot
    float yaw = GetCameraYaw(&localData.vMatrix);
    Vector3 myPos = localData.myPos;

    // Draw teammates as blue dots
    for (int i = 0; i < localData.teammateCount; i++) {
        const Runtime::TeammateDot& tm = localData.teammates[i];
        if (!tm.valid) continue;

        float dx = tm.pos.x - myPos.x;
        float dz = tm.pos.z - myPos.z;

        float sinY = sinf(-yaw);
        float cosY = cosf(-yaw);
        float rx = dx * cosY - dz * sinY;
        float ry = dx * sinY + dz * cosY;

        float px = rx / scale;
        float py = -ry / scale;

        float dist = sqrtf(px * px + py * py);
        if (dist > radius - 4.0f) {
            float clampScale = (radius - 4.0f) / dist;
            px *= clampScale;
            py *= clampScale;
        }

        ImU32 blueColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.25f, 0.55f, 1.0f, 0.90f));
        drawList->AddCircleFilled(ImVec2(cx + px, cy + py), 3.0f, blueColor);
    }

    // Draw enemies as red dots
    for (int i = 0; i < localData.playerCount; i++) {
        const PlayerData& p = localData.players[i];
        if (!p.valid || p.hp <= 0) continue;

        float dx = p.rootPos.x - myPos.x;
        float dz = p.rootPos.z - myPos.z;

        float sinY = sinf(-yaw);
        float cosY = cosf(-yaw);
        float rx = dx * cosY - dz * sinY;
        float ry = dx * sinY + dz * cosY;

        float px = rx / scale;
        float py = -ry / scale;

        float dist = sqrtf(px * px + py * py);
        if (dist > radius - 4.0f) {
            float clampScale = (radius - 4.0f) / dist;
            px *= clampScale;
            py *= clampScale;
        }

        // Red for enemies, slightly dimmer if occluded
        ImU32 dotColor;
        if (p.visible) {
            dotColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.25f, 0.25f, 0.95f));
        } else {
            dotColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.75f, 0.20f, 0.20f, 0.60f));
        }

        float dotR = p.visible ? 3.5f : 2.5f;
        drawList->AddCircleFilled(ImVec2(cx + px, cy + py), dotR, dotColor);
    }

    // Label
    ImGui::PushFont(MenuStyle::g_fontSmall);
    const char* label = "RADAR";
    ImVec2 labelSz = ImGui::CalcTextSize(label);
    drawList->AddText(
        ImVec2(cx - labelSz.x * 0.5f, cy - radius - labelSz.y - 4.0f),
        ImGui::ColorConvertFloat4ToU32(ImVec4(
            MenuStyle::kAccent.x, MenuStyle::kAccent.y, MenuStyle::kAccent.z, 0.7f)),
        label);
    ImGui::PopFont();
}

} // namespace Radar
