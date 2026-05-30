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
    float thickness = g_state.espBoxThickness;
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

static void DrawStandardBox(ImDrawList* drawList, float x, float y, float w, float h, ImU32 color) {
    float thickness = g_state.espBoxThickness;
    ImU32 outline = IM_COL32(0, 0, 0, 210);
    drawList->AddRect(ImVec2(x - 1.0f, y - 1.0f), ImVec2(x + w + 1.0f, y + h + 1.0f), outline, 0.0f, 0, 1.0f);
    drawList->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), color, 0.0f, 0, thickness);
}

static void DrawFilledBox(ImDrawList* drawList, float x, float y, float w, float h, ImU32 color) {
    // Filled with low alpha background + colored border
    ImVec4 col4 = ImGui::ColorConvertU32ToFloat4(color);
    ImU32 fillColor = ImGui::ColorConvertFloat4ToU32(ImVec4(col4.x, col4.y, col4.z, col4.w * 0.15f));
    float thickness = g_state.espBoxThickness;
    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), fillColor, 0.0f);
    drawList->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), color, 0.0f, 0, thickness);
}

static void DrawESPBox(ImDrawList* drawList, float x, float y, float w, float h, ImU32 color) {
    switch (g_state.espBoxStyle) {
        case 0:  DrawStandardBox(drawList, x, y, w, h, color); break;
        case 1:  DrawCornerBox(drawList, x, y, w, h, color);   break;
        case 2:  DrawFilledBox(drawList, x, y, w, h, color);   break;
        default: DrawCornerBox(drawList, x, y, w, h, color);   break;
    }
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
    bool anyConsumer = g_state.espEnabled || g_state.aimbotEnabled || g_state.triggerEnabled || g_state.radarEnabled || g_state.oofEnabled;
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
    int   bestTargetIdx = -1;
    bool foundTarget = false;

    float lockedBestDrift = 99999.0f;
    float lockedTargetX = 0.0f, lockedTargetY = 0.0f;
    int   lockedTargetIdx = -1;
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

        if (projectedPoints < 2) {
            // Even if we can't draw a full box, still check triggerbot using
            // the player's head/root position (handles scoped/close-range case)
            if (g_state.triggerEnabled && !triggerOnTarget) {
                if (!g_state.visibleCheck || p.visible) {
                    Vector3 trigPos = p.hasHead ? p.headPos : p.rootPos;
                    float tsx, tsy;
                    if (WorldToScreen(localData.camera, trigPos, &localData.vMatrix, &localData.pMatrix, screenW, screenH, tsx, tsy)) {
                        if (g_state.triggerFov > 0.0f) {
                            float tdx = tsx - cx;
                            float tdy = tsy - cy;
                            if (sqrtf(tdx * tdx + tdy * tdy) < g_state.triggerFov) {
                                triggerOnTarget = true;
                            }
                        } else {
                            // Fallback: check if crosshair is near the projected point
                            float nearDist = sqrtf((tsx - cx) * (tsx - cx) + (tsy - cy) * (tsy - cy));
                            if (nearDist < 30.0f) {
                                triggerOnTarget = true;
                            }
                        }
                    }
                }
            }
            continue;
        }

        float boxX = minX - 4.0f;
        float boxY = minY - 4.0f;
        float boxW = (maxX - minX) + 8.0f;
        float boxH = (maxY - minY) + 8.0f;
        if (boxW < 8.0f || boxH < 12.0f) continue;

        ImU32 accentColor = p.visible
            ? (p.isReal ? ColorVecToU32(g_state.colEnemy) : ColorVecToU32(g_state.colBot))
            : ColorVecToU32(g_state.colOccluded);

        if (g_state.espEnabled && g_state.espBox) {
            DrawESPBox(drawList, boxX, boxY, boxW, boxH, accentColor);
        }

        // Triggerbot detection.
        // If triggerFov > 0, use FOV circle detection (distance from crosshair
        // to closest point on bbox). If 0, use bbox-contains test.
        if (g_state.triggerEnabled && !triggerOnTarget) {
            if (!g_state.visibleCheck || p.visible) {
                if (g_state.triggerFov > 0.0f) {
                    // FOV-based trigger: distance from crosshair to nearest edge of bbox
                    float closestX = ClampFloat(cx, boxX, boxX + boxW);
                    float closestY = ClampFloat(cy, boxY, boxY + boxH);
                    float tdx = closestX - cx;
                    float tdy = closestY - cy;
                    float tDist = sqrtf(tdx * tdx + tdy * tdy);
                    if (tDist < g_state.triggerFov) {
                        triggerOnTarget = true;
                    }
                } else {
                    // Bbox-based trigger (original behavior)
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
        }

        if (g_state.espEnabled && g_state.espSkeleton) {
            ImU32 boneColor = p.visible ? ColorVecToU32(g_state.colSkeleton) : ColorVecToU32(g_state.colOccluded);
            ImU32 boneOutline = IM_COL32(0, 0, 0, 160);
            static const int kBoneEdges[][2] = {
                { BONE_HEAD,  BONE_NECK  },
                { BONE_NECK,  BONE_CHEST },
                { BONE_CHEST, BONE_SPINE },
                { BONE_SPINE, BONE_ROOT  },
                { BONE_CHEST, BONE_LKNEE }, // shoulder to arm approximation
                { BONE_CHEST, BONE_RKNEE }, // shoulder to arm approximation
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
            float boneThickness = 1.6f;
            for (int e = 0; e < (int)(sizeof(kBoneEdges) / sizeof(kBoneEdges[0])); e++) {
                int a = kBoneEdges[e][0];
                int c = kBoneEdges[e][1];
                if (!boneOnScreen[a] || !boneOnScreen[c]) continue;
                // Draw outline first for cleaner look
                drawList->AddLine(
                    ImVec2(boneScreen[a][0], boneScreen[a][1]),
                    ImVec2(boneScreen[c][0], boneScreen[c][1]),
                    boneOutline, boneThickness + 1.2f);
                drawList->AddLine(
                    ImVec2(boneScreen[a][0], boneScreen[a][1]),
                    ImVec2(boneScreen[c][0], boneScreen[c][1]),
                    boneColor, boneThickness);
            }
            // Draw joint dots for a cleaner look
            for (int b = 0; b < BONE_COUNT; b++) {
                if (!boneOnScreen[b]) continue;
                drawList->AddCircleFilled(ImVec2(boneScreen[b][0], boneScreen[b][1]), 2.2f, boneOutline);
                drawList->AddCircleFilled(ImVec2(boneScreen[b][0], boneScreen[b][1]), 1.4f, boneColor);
            }
        }

        if (g_state.espEnabled && g_state.espHead && p.hasHead) {
            float hsx, hsy;
            if (WorldToScreen(localData.camera, p.headPos, &localData.vMatrix, &localData.pMatrix, screenW, screenH, hsx, hsy)) {
                ImU32 headColor = p.visible ? ColorVecToU32(g_state.colEnemy) : ColorVecToU32(g_state.colOccluded);
                drawList->AddCircle(ImVec2(hsx, hsy), 4.0f, IM_COL32(0, 0, 0, 200), 12, 2.0f);
                drawList->AddCircle(ImVec2(hsx, hsy), 4.0f, headColor, 12, 1.4f);
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
                // Sticky/Human target persistence: if we have a locked target,
                // strongly prefer keeping it unless it's invalid.
                int currentLockedIdx = Aim::GetLockedTargetIndex();

                if (aimKeyDown && Aim::TargetActive() && currentLockedIdx == i) {
                    // This is our currently locked target — always prefer it
                    float driftX = aimSX - Aim::TargetX();
                    float driftY = aimSY - Aim::TargetY();
                    float drift = sqrtf(driftX * driftX + driftY * driftY);
                    lockedBestDrift = drift;
                    lockedTargetX = aimSX;
                    lockedTargetY = aimSY;
                    lockedTargetIdx = i;
                    foundLockedTarget = true;
                } else if (aimKeyDown && Aim::TargetActive() && currentLockedIdx >= 0) {
                    // We have a locked target but this isn't it.
                    // In Sticky mode: ignore other targets entirely.
                    // In Human mode: consider switching only if cooldown elapsed
                    //                and this target is significantly closer.
                    if (g_state.aimTargetMethod == Aim::TGT_HUMAN && Aim::CanSwitchTarget()) {
                        float advantage = lockedBestDrift - fovDist;
                        if (advantage > 60.0f && fovDist < bestFov) {
                            bestFov = fovDist;
                            bestTargetX = aimSX;
                            bestTargetY = aimSY;
                            bestTargetIdx = i;
                            foundTarget = true;
                        }
                    }
                    // Sticky mode: do nothing, keep current lock
                } else {
                    // No locked target yet — pick closest
                    if (fovDist < bestFov) {
                        bestFov = fovDist;
                        bestTargetX = aimSX;
                        bestTargetY = aimSY;
                        bestTargetIdx = i;
                        foundTarget = true;
                    }
                }
            }
        }
    }

    // ---- Out-of-FOV arrows ---------------------------------------------------
    // Draw arrows pointing toward enemies that are off-screen but within max distance.
    if (g_state.oofEnabled) {
        float centerX = (float)screenW * 0.5f;
        float centerY = (float)screenH * 0.5f;
        float arrowRadius = g_state.oofCircleRadius;
        ImU32 arrowColor = ColorVecToU32(g_state.colOofArrow);

        for (int i = 0; i < localData.playerCount; i++) {
            PlayerData& p = localData.players[i];
            if (!p.valid || p.hp <= 0) continue;
            if (p.distance > g_state.oofMaxDistance) continue;

            // Try to project the player's head/root position
            Vector3 targetPos = p.hasHead ? p.headPos : p.rootPos;
            float sx, sy;
            bool onScreen = WorldToScreen(localData.camera, targetPos,
                &localData.vMatrix, &localData.pMatrix, screenW, screenH, sx, sy);

            // Only draw arrow if player is off-screen
            if (onScreen && sx >= 0 && sx <= screenW && sy >= 0 && sy <= screenH) continue;

            // Calculate direction from screen center to the projected point
            // For behind-camera players, use the matrix to get a direction
            float dirX, dirY;
            if (!onScreen) {
                // Player is behind camera — use view matrix to get direction
                float vx = UnityMatrixGet(&localData.vMatrix, 0, 0)*targetPos.x + UnityMatrixGet(&localData.vMatrix, 0, 1)*targetPos.y + UnityMatrixGet(&localData.vMatrix, 0, 2)*targetPos.z + UnityMatrixGet(&localData.vMatrix, 0, 3);
                float vy = UnityMatrixGet(&localData.vMatrix, 1, 0)*targetPos.x + UnityMatrixGet(&localData.vMatrix, 1, 1)*targetPos.y + UnityMatrixGet(&localData.vMatrix, 1, 2)*targetPos.z + UnityMatrixGet(&localData.vMatrix, 1, 3);
                dirX = -vx;  // invert because behind camera
                dirY = vy;
            } else {
                dirX = sx - centerX;
                dirY = sy - centerY;
            }

            float len = sqrtf(dirX * dirX + dirY * dirY);
            if (len < 1.0f) continue;
            dirX /= len;
            dirY /= len;

            // Arrow tip position on the circle
            float tipX = centerX + dirX * arrowRadius;
            float tipY = centerY + dirY * arrowRadius;

            // Arrow size scales with proximity
            float distFactor = 1.0f - ClampFloat(p.distance / g_state.oofMaxDistance, 0.0f, 1.0f);
            float arrowSize = 6.0f + distFactor * 6.0f; // 6-12px

            // Perpendicular direction for arrow base
            float perpX = -dirY;
            float perpY = dirX;

            // Triangle points
            ImVec2 tip(tipX, tipY);
            ImVec2 base1(tipX - dirX * arrowSize + perpX * arrowSize * 0.5f,
                         tipY - dirY * arrowSize + perpY * arrowSize * 0.5f);
            ImVec2 base2(tipX - dirX * arrowSize - perpX * arrowSize * 0.5f,
                         tipY - dirY * arrowSize - perpY * arrowSize * 0.5f);

            drawList->AddTriangleFilled(tip, base1, base2, arrowColor);
            drawList->AddTriangle(tip, base1, base2, IM_COL32(0, 0, 0, 180), 1.0f);
        }
    }

    if (g_state.aimbotEnabled && g_state.drawAimbotFov) {
        ImVec2 center((float)screenW * 0.5f, (float)screenH * 0.5f);
        drawList->AddCircle(center, g_state.aimbotFov, ColorVecToU32(g_state.colAimbotFov), 64, 1.2f);
    }

    if (g_state.triggerEnabled && g_state.drawTriggerFov && g_state.triggerFov > 0.0f) {
        ImVec2 center((float)screenW * 0.5f, (float)screenH * 0.5f);
        drawList->AddCircle(center, g_state.triggerFov, ColorVecToU32(g_state.colTriggerFov), 64, 1.2f);
    }

    if (foundLockedTarget) {
        bestTargetX = lockedTargetX;
        bestTargetY = lockedTargetY;
        bestTargetIdx = lockedTargetIdx;
        foundTarget = true;
    }

    if (g_state.aimbotEnabled && foundTarget && aimKeyDown) {
        // Track target switches
        int prevIdx = Aim::GetLockedTargetIndex();
        if (prevIdx != bestTargetIdx && prevIdx >= 0) {
            Aim::NotifyTargetSwitch();
        }
        Aim::SetLockedTargetIndex(bestTargetIdx);
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
