// render/esp_render.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdio.h>

#include "esp_render.h"
#include "menu_style.h"
#include "imgui.h"
#include "../core/globals.h"
#include "../il2cpp/il2cpp.h"
#include "../il2cpp/player.h"
#include "../features/aimbot/aimbot.h"
#include "../features/aimbot/hitbox.h"
#include "../features/triggerbot/triggerbot.h"
#include "../utils/math.h"
#include "../utils/memory.h"

namespace ESPRender {

// ---- world-to-screen ------------------------------------------------------
static inline float UnityMatrixGet(const Matrix4x4* m, int row, int col) {
    return m->m[col][row];
}

static bool WorldToScreenMatrix(Vector3 world, Matrix4x4* view, Matrix4x4* proj,
                                int screenW, int screenH, float& outX, float& outY) {
    float cx = UnityMatrixGet(view, 0, 0)*world.x + UnityMatrixGet(view, 0, 1)*world.y + UnityMatrixGet(view, 0, 2)*world.z + UnityMatrixGet(view, 0, 3);
    float cy = UnityMatrixGet(view, 1, 0)*world.x + UnityMatrixGet(view, 1, 1)*world.y + UnityMatrixGet(view, 1, 2)*world.z + UnityMatrixGet(view, 1, 3);
    float cz = UnityMatrixGet(view, 2, 0)*world.x + UnityMatrixGet(view, 2, 1)*world.y + UnityMatrixGet(view, 2, 2)*world.z + UnityMatrixGet(view, 2, 3);
    float cw = UnityMatrixGet(view, 3, 0)*world.x + UnityMatrixGet(view, 3, 1)*world.y + UnityMatrixGet(view, 3, 2)*world.z + UnityMatrixGet(view, 3, 3);

    float px = UnityMatrixGet(proj, 0, 0)*cx + UnityMatrixGet(proj, 0, 1)*cy + UnityMatrixGet(proj, 0, 2)*cz + UnityMatrixGet(proj, 0, 3)*cw;
    float py = UnityMatrixGet(proj, 1, 0)*cx + UnityMatrixGet(proj, 1, 1)*cy + UnityMatrixGet(proj, 1, 2)*cz + UnityMatrixGet(proj, 1, 3)*cw;
    float pw = UnityMatrixGet(proj, 3, 0)*cx + UnityMatrixGet(proj, 3, 1)*cy + UnityMatrixGet(proj, 3, 2)*cz + UnityMatrixGet(proj, 3, 3)*cw;

    if (pw <= 0.001f) return false;

    float ndcX = px / pw;
    float ndcY = py / pw;
    outX = (ndcX + 1.0f) * 0.5f * screenW;
    outY = (1.0f - ndcY) * 0.5f * screenH;
    return true;
}

// Try the IL2CPP Camera.WorldToScreenPoint first, fall back to manual matrix mul.
static bool WorldToScreen(void* camera, Vector3 world, Matrix4x4* view, Matrix4x4* proj,
                          int screenW, int screenH, float& outX, float& outY) {
    if (!isfinite(world.x) || !isfinite(world.y) || !isfinite(world.z)) return false;

    if (IsPlausiblePtr(camera) && IL2CPP::fn_WorldToScreenPoint && IL2CPP::g_worldToScreenPointMethodInfo) {
        Vector3 screen;
        __try {
            IL2CPP::fn_WorldToScreenPoint(&screen, camera, &world, IL2CPP::g_worldToScreenPointMethodInfo);
        } __except(1) {
            screen = { 0.0f, 0.0f, -1.0f };
        }

        if (isfinite(screen.x) && isfinite(screen.y) && isfinite(screen.z) && screen.z > 0.001f) {
            outX = screen.x;
            outY = (float)screenH - screen.y;
            return isfinite(outX) && isfinite(outY);
        }
    }

    return WorldToScreenMatrix(world, view, proj, screenW, screenH, outX, outY);
}

// ---- ESP draw helpers -----------------------------------------------------
static inline ImU32 ColorVecToU32(const float c[4]) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3]));
}

static void DrawShadowText(ImDrawList* drawList, ImVec2 pos, ImU32 color, const char* text) {
    drawList->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 220), text);
    drawList->AddText(pos, color, text);
}

static void DrawCornerBox(ImDrawList* drawList, float x, float y, float w, float h, ImU32 color) {
    float lineW = ClampFloat(w * 0.28f, 7.0f, 18.0f);
    float lineH = ClampFloat(h * 0.22f, 9.0f, 24.0f);
    float thickness = 1.8f;
    ImU32 outline = IM_COL32(0, 0, 0, 210);

    drawList->AddRect(ImVec2(x - 1.0f, y - 1.0f), ImVec2(x + w + 1.0f, y + h + 1.0f), outline, 2.0f, 0, 1.0f);

    drawList->AddLine(ImVec2(x, y), ImVec2(x + lineW, y), color, thickness);
    drawList->AddLine(ImVec2(x, y), ImVec2(x, y + lineH), color, thickness);
    drawList->AddLine(ImVec2(x + w, y), ImVec2(x + w - lineW, y), color, thickness);
    drawList->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + lineH), color, thickness);
    drawList->AddLine(ImVec2(x, y + h), ImVec2(x + lineW, y + h), color, thickness);
    drawList->AddLine(ImVec2(x, y + h), ImVec2(x, y + h - lineH), color, thickness);
    drawList->AddLine(ImVec2(x + w, y + h), ImVec2(x + w - lineW, y + h), color, thickness);
    drawList->AddLine(ImVec2(x + w, y + h), ImVec2(x + w, y + h - lineH), color, thickness);
}

static void DrawVerticalBar(ImDrawList* drawList, float x, float y, float h, float pct, ImU32 fillColor) {
    pct = ClampFloat(pct, 0.0f, 1.0f);
    float w = 3.0f;
    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(8, 10, 14, 190), 1.0f);
    drawList->AddRect(ImVec2(x - 1.0f, y - 1.0f), ImVec2(x + w + 1.0f, y + h + 1.0f), IM_COL32(0, 0, 0, 210), 1.0f);
    float fillH = h * pct;
    drawList->AddRectFilled(ImVec2(x, y + h - fillH), ImVec2(x + w, y + h), fillColor, 1.0f);
}

// Map the user-facing trigger precision preset (Precise / Balanced /
// Aggressive) onto a screen-space padding for the bbox-contains test.
// Negative shrinks the detection box; positive grows it. Numbers were
// picked so Balanced matches the visible bbox 1:1, Precise demands a
// solid hit on the silhouette, and Aggressive forgives near-miss edges.
static float TriggerPaddingPx(int precision) {
    switch (precision) {
        case 0:  return -6.0f;  // Precise
        case 1:  return  0.0f;  // Balanced
        case 2:  return  8.0f;  // Aggressive
        default: return  0.0f;
    }
}

// ---- main draw ------------------------------------------------------------
void Draw() {
    // Same any-consumer rule as the data thread: render/loop when ESP, aim,
    // or trigger is enabled. Individual visual elements (box, names, hp,
    // skeleton, snapline, fov circle) each gate themselves with their own
    // toggle, so toggling triggerbot on alone won't draw ESP visuals.
    bool anyConsumer = g_state.espEnabled || g_state.aimbotEnabled || g_state.triggerEnabled;
    if (!anyConsumer) {
        Aim::ResetState();
        Trigger::OnFrame(false);
        return;
    }
    IL2CPP::EnsurePresentThreadAttached();

    Runtime::SharedESPData localData;
    EnterCriticalSection(&Runtime::g_espCs);
    localData = Runtime::g_espData;
    LeaveCriticalSection(&Runtime::g_espCs);

    if (localData.playerCount == 0) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    int screenW = (int)ImGui::GetIO().DisplaySize.x;
    int screenH = (int)ImGui::GetIO().DisplaySize.y;

    float bestFov = 99999.0f;
    float bestTargetX = 0.0f, bestTargetY = 0.0f;
    bool foundTarget = false;

    float lockedBestDrift = 99999.0f;
    float lockedTargetX = 0.0f, lockedTargetY = 0.0f;
    bool foundLockedTarget = false;
    bool aimKeyDown = Aim::IsActivationActive();

    // Trigger detection. We don't care about distance/priority - any live
    // enemy whose padded bbox covers the screen center is a valid target.
    // Visible-check is honored when the user has it enabled.
    bool   triggerOnTarget = false;
    float  cx = (float)screenW * 0.5f;
    float  cy = (float)screenH * 0.5f;

    for (int i = 0; i < localData.playerCount; i++) {
        PlayerData& p = localData.players[i];
        if (!p.valid) continue;
        if (p.hp <= 0) continue;

        float minX = 100000.0f, minY = 100000.0f;
        float maxX = -100000.0f, maxY = -100000.0f;
        int projectedPoints = 0;

        for (int b = 0; b < p.boxPointCount && b < 64; b++) {
            float px, py;
            if (WorldToScreen(localData.camera, p.boxPoints[b], &localData.vMatrix, &localData.pMatrix, screenW, screenH, px, py)) {
                if (px < minX) minX = px;
                if (px > maxX) maxX = px;
                if (py < minY) minY = py;
                if (py > maxY) maxY = py;
                projectedPoints++;
            }
        }

        if (projectedPoints < 2) continue;

        float boxX = minX - 4.0f;
        float boxY = minY - 4.0f;
        float boxW = (maxX - minX) + 8.0f;
        float boxH = (maxY - minY) + 8.0f;
        if (boxW < 8.0f || boxH < 12.0f) continue;

        ImU32 accentColor = p.visible
            ? (p.isReal ? ColorVecToU32(g_state.colEnemy) : ColorVecToU32(g_state.colBot))
            : ColorVecToU32(g_state.colOccluded);

        if (g_state.espEnabled && g_state.espBox) {
            DrawCornerBox(drawList, boxX, boxY, boxW, boxH, accentColor);
        }

        // Triggerbot detection (cheap rectangle-contains test).
        // We honor the same gating that ESP does: skip team-mates (already
        // filtered upstream), skip dead (hp<=0 already gated), and require
        // visibility when visibleCheck is on. Padding is derived from the
        // user-facing precision preset.
        if (g_state.triggerEnabled && !triggerOnTarget) {
            if (!g_state.visibleCheck || p.visible) {
                float pad = TriggerPaddingPx(g_state.triggerPrecision);
                float tx0 = boxX - pad;
                float ty0 = boxY - pad;
                float tx1 = boxX + boxW + pad;
                float ty1 = boxY + boxH + pad;
                if (cx >= tx0 && cx <= tx1 && cy >= ty0 && cy <= ty1) {
                    triggerOnTarget = true;
                }
            }
        }

        if (g_state.espEnabled && g_state.espSkeleton) {
            ImU32 boneColor = p.visible ? ColorVecToU32(g_state.colSkeleton) : ColorVecToU32(g_state.colOccluded);
            static const int kBoneEdges[][2] = {
                { BONE_HEAD,  BONE_NECK  },
                { BONE_NECK,  BONE_CHEST },
                { BONE_CHEST, BONE_SPINE },
                { BONE_SPINE, BONE_ROOT  },
                { BONE_ROOT,  BONE_LKNEE },
                { BONE_ROOT,  BONE_RKNEE },
            };
            float boneScreen[BONE_COUNT][2];
            bool boneOnScreen[BONE_COUNT] = {};
            for (int b = 0; b < BONE_COUNT; b++) {
                if (!p.boneValid[b]) continue;
                float sx, sy;
                if (WorldToScreen(localData.camera, p.bones[b], &localData.vMatrix, &localData.pMatrix, screenW, screenH, sx, sy)) {
                    boneScreen[b][0] = sx;
                    boneScreen[b][1] = sy;
                    boneOnScreen[b] = true;
                }
            }
            for (int e = 0; e < (int)(sizeof(kBoneEdges) / sizeof(kBoneEdges[0])); e++) {
                int a = kBoneEdges[e][0];
                int c = kBoneEdges[e][1];
                if (!boneOnScreen[a] || !boneOnScreen[c]) continue;
                drawList->AddLine(
                    ImVec2(boneScreen[a][0], boneScreen[a][1]),
                    ImVec2(boneScreen[c][0], boneScreen[c][1]),
                    boneColor, 1.4f);
            }
        }

        if (g_state.espEnabled && g_state.espNames && p.name[0]) {
            ImVec2 textSize = ImGui::CalcTextSize(p.name);
            float textX = boxX + (boxW - textSize.x) * 0.5f;
            float textY = boxY - textSize.y - 5.0f;
            if (textY < 2.0f) textY = boxY + 2.0f;
            ImU32 nameColor = p.visible ? ColorVecToU32(g_state.colName) : ColorVecToU32(g_state.colOccluded);
            DrawShadowText(drawList, ImVec2(textX, textY), nameColor, p.name);
        }

        if (g_state.espEnabled && g_state.espHealth && p.hp > 0) {
            float hpPct = p.hp / 100.0f;
            if (hpPct > 1.0f) hpPct = 1.0f;
            ImU32 hpColor = hpPct > 0.55f ? ColorVecToU32(g_state.colHpHigh)
                          : (hpPct > 0.25f ? ColorVecToU32(g_state.colHpMid)
                                           : ColorVecToU32(g_state.colHpLow));
            DrawVerticalBar(drawList, boxX - 7.0f, boxY, boxH, hpPct, hpColor);
        }

        if (g_state.espEnabled && g_state.espArmor && p.armor > 0) {
            float arPct = p.armor / 100.0f;
            if (arPct > 1.0f) arPct = 1.0f;
            DrawVerticalBar(drawList, boxX + boxW + 4.0f, boxY, boxH, arPct, ColorVecToU32(g_state.colArmor));
        }

        if (g_state.espEnabled && g_state.espLines) {
            ImU32 lineColor = p.visible ? ColorVecToU32(g_state.colSnapline) : ColorVecToU32(g_state.colOccluded);
            drawList->AddLine(ImVec2((float)screenW * 0.5f, (float)screenH - 2.0f),
                              ImVec2(boxX + boxW * 0.5f, boxY + boxH), lineColor, 1.0f);
        }

        float labelY = boxY + boxH + 4.0f;
        if (g_state.espEnabled && g_state.espDistance && p.distance > 0.01f) {
            char distanceText[32];
            snprintf(distanceText, sizeof(distanceText), "%.0fm", p.distance);
            ImVec2 textSize = ImGui::CalcTextSize(distanceText);
            DrawShadowText(drawList, ImVec2(boxX + (boxW - textSize.x) * 0.5f, labelY),
                           ColorVecToU32(g_state.colDistance), distanceText);
        }

        // Aim target: pick highest-priority enabled hitbox with a valid bone.
        AimHitboxResolution aimHb;
        Vector3 aimWorld;
        bool haveAimWorld = false;
        if (ResolveAimHitbox(p, g_state.aimbotHitboxMask, aimHb)) {
            aimWorld = aimHb.world;
            haveAimWorld = true;
        } else if (p.hasHead) {
            aimWorld = p.headPos;
            haveAimWorld = true;
        }

        float aimSX = 0.0f, aimSY = 0.0f;
        if (haveAimWorld && WorldToScreen(localData.camera, aimWorld, &localData.vMatrix, &localData.pMatrix, screenW, screenH, aimSX, aimSY)) {
            float dx = aimSX - (screenW / 2.0f);
            float dy = aimSY - (screenH / 2.0f);
            float fovDist = sqrtf(dx*dx + dy*dy);

            if (g_state.aimbotEnabled && fovDist < g_state.aimbotFov && (!g_state.visibleCheck || p.visible)) {
                if (aimKeyDown && Aim::TargetActive()) {
                    float driftX = aimSX - Aim::TargetX();
                    float driftY = aimSY - Aim::TargetY();
                    float drift = sqrtf(driftX * driftX + driftY * driftY);
                    float lockRadius = ClampFloat(g_state.aimbotFov * 0.35f, 45.0f, 130.0f);
                    if (drift < lockRadius && drift < lockedBestDrift) {
                        lockedBestDrift = drift;
                        lockedTargetX = aimSX;
                        lockedTargetY = aimSY;
                        foundLockedTarget = true;
                    }
                }

                if (fovDist < bestFov) {
                    bestFov = fovDist;
                    bestTargetX = aimSX;
                    bestTargetY = aimSY;
                    foundTarget = true;
                }
            }
        }
    }

    if (g_state.aimbotEnabled && g_state.drawAimbotFov) {
        ImVec2 center((float)screenW * 0.5f, (float)screenH * 0.5f);
        drawList->AddCircle(center, g_state.aimbotFov, IM_COL32(255, 255, 255, 95), 64, 1.2f);
    }

    if (foundLockedTarget) {
        bestTargetX = lockedTargetX;
        bestTargetY = lockedTargetY;
        foundTarget = true;
    }

    if (g_state.aimbotEnabled && foundTarget && aimKeyDown) {
        Aim::SetTargetActive(true);
        Aim::SetTarget(bestTargetX, bestTargetY);
        Aim::Apply(bestTargetX, bestTargetY, (float)screenW * 0.5f, (float)screenH * 0.5f);
    } else {
        Aim::ResetState();
    }

    // Drive the trigger state machine once per frame regardless of whether
    // we found a target - it needs the false case to release any in-flight
    // click and to reset its refire timer.
    Trigger::OnFrame(triggerOnTarget);

    g_state.dbg_pR  = localData.pR;
    g_state.dbg_pHP = localData.pHP;
    g_state.dbg_pTr = localData.pTr;
    g_state.dbg_pW2S = localData.pW2S;
}

} // namespace ESPRender
