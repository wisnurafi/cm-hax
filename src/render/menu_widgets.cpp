// render/menu_widgets.cpp
// Glassmorphism dark widget library. Frosted surfaces, soft luminous borders,
// restrained accent glow. Every interactive element feels like touching glass.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "menu_widgets.h"
#include "menu_style.h"
#include "../utils/math.h"
#include "../utils/input.h"

namespace Widgets {

using MenuStyle::kAccent;
using MenuStyle::kAccentSoft;
using MenuStyle::kCardBg;
using MenuStyle::kCardBorder;
using MenuStyle::kSubtleText;
using MenuStyle::kMutedText;

// ---- helpers ----------------------------------------------------------------

// ImGui's ## convention: everything after ## is a hidden ID suffix.
static const char* DisplayEnd(const char* label) {
    const char* p = label;
    while (*p) {
        if (p[0] == '#' && p[1] == '#') return p;
        p++;
    }
    return p;
}

// Smooth animation keyed by label pointer.
static float Animate(const char* key, float target, int slots, const char** keyTable, float* anim) {
    int slot = -1;
    for (int i = 0; i < slots; i++) { if (keyTable[i] == key) { slot = i; break; } }
    if (slot < 0) {
        for (int i = 0; i < slots; i++) { if (keyTable[i] == NULL) { keyTable[i] = key; slot = i; break; } }
    }
    if (slot < 0) slot = 0;
    float dt = ImGui::GetIO().DeltaTime;
    if (dt <= 0.0f) dt = 0.016f;
    anim[slot] += (target - anim[slot]) * ClampFloat(dt * 10.0f, 0.0f, 1.0f);
    return anim[slot];
}

// ---- section header ---------------------------------------------------------
void SectionHeader(const char* label) {
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float lineH = ImGui::GetTextLineHeight();

    // Accent bar — thin, rounded, soft glow
    float barW = 3.0f;
    float barH = lineH * 0.7f;
    float barY = p.y + (lineH - barH) * 0.5f;
    ImU32 accentCol = ImGui::ColorConvertFloat4ToU32(kAccent);
    ImU32 glowCol   = ImGui::ColorConvertFloat4ToU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.15f));

    // Soft glow behind bar
    dl->AddRectFilled(
        ImVec2(p.x - 2.0f, barY - 2.0f),
        ImVec2(p.x + barW + 2.0f, barY + barH + 2.0f),
        glowCol, 3.0f);
    dl->AddRectFilled(
        ImVec2(p.x, barY),
        ImVec2(p.x + barW, barY + barH),
        accentCol, 2.0f);

    ImGui::SetCursorScreenPos(ImVec2(p.x + 12.0f, p.y));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f, 0.91f, 0.95f, 1.00f));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();

    // Subtle separator line with fade
    ImVec2 cp = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImU32 lineLeft  = ImGui::ColorConvertFloat4ToU32(ImVec4(0.22f, 0.26f, 0.32f, 0.25f));
    ImU32 lineRight = ImGui::ColorConvertFloat4ToU32(ImVec4(0.22f, 0.26f, 0.32f, 0.0f));
    dl->AddRectFilledMultiColor(
        ImVec2(cp.x, cp.y + 2.0f),
        ImVec2(cp.x + w, cp.y + 3.0f),
        lineLeft, lineRight, lineRight, lineLeft);
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
}

// ---- toggle switch ----------------------------------------------------------
bool Toggle(const char* label, bool* v) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    float h = ImGui::GetTextLineHeight() + 4.0f;
    float w = h * 1.85f;
    ImVec2 sw_a = p;
    ImVec2 sw_b = ImVec2(p.x + w, p.y + h);

    ImGui::InvisibleButton(label, ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    if (clicked) { *v = !*v; }

    static const char* keys[64] = {};
    static float anim[64] = {};
    float t = Animate(label, *v ? 1.0f : 0.0f, 64, keys, anim);

    // Glass track background
    ImVec4 trackCol = ImVec4(
        0.08f + t * (kAccent.x * 0.20f),
        0.09f + t * (kAccent.y * 0.20f),
        0.12f + t * (kAccent.z * 0.25f),
        0.70f + t * 0.15f);
    ImU32 track = ImGui::ColorConvertFloat4ToU32(trackCol);

    // Border: luminous when active, subtle when off
    ImVec4 borderCol = ImVec4(
        0.20f + t * (kAccent.x - 0.20f) * 0.6f,
        0.24f + t * (kAccent.y - 0.24f) * 0.6f,
        0.30f + t * (kAccent.z - 0.30f) * 0.6f,
        0.25f + t * 0.35f);
    if (hovered) { borderCol.w += 0.15f; }
    ImU32 border = ImGui::ColorConvertFloat4ToU32(borderCol);

    dl->AddRectFilled(sw_a, sw_b, track, h * 0.5f);
    dl->AddRect(sw_a, sw_b, border, h * 0.5f, 0, 1.0f);

    // Knob with soft glow when active
    float knobR = h * 0.38f;
    float knobX = sw_a.x + h * 0.5f + (w - h) * t;
    float knobY = sw_a.y + h * 0.5f;

    if (t > 0.05f) {
        ImU32 glow = ImGui::ColorConvertFloat4ToU32(
            ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.12f * t));
        dl->AddCircleFilled(ImVec2(knobX, knobY), knobR + 5.0f, glow);
    }

    ImU32 knobCol = ImGui::ColorConvertFloat4ToU32(
        ImVec4(0.90f + 0.05f * t, 0.92f + 0.04f * t, 0.96f + 0.02f * t, 1.0f));
    dl->AddCircleFilled(ImVec2(knobX, knobY), knobR, knobCol);

    // Label
    ImGui::SameLine(0.0f, 12.0f);
    ImGui::AlignTextToFramePadding();
    ImVec4 textCol = *v
        ? ImVec4(0.92f, 0.94f, 0.97f, 1.0f)
        : ImVec4(0.55f, 0.58f, 0.64f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, textCol);
    ImGui::TextUnformatted(label, DisplayEnd(label));
    ImGui::PopStyleColor();
    return clicked;
}

// ---- pill -------------------------------------------------------------------
bool Pill(const char* label, bool active, float minWidth) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 textSize = ImGui::CalcTextSize(label);
    float padX = 16.0f;
    float padY = 7.0f;
    float w = textSize.x + padX * 2.0f;
    if (w < minWidth) w = minWidth;
    float h = textSize.y + padY * 2.0f;

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(label, ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();

    static const char* keys[128] = {};
    static float anim[128] = {};
    float target = active ? 1.0f : (hovered ? 0.35f : 0.0f);
    float t = Animate(label, target, 128, keys, anim);

    // Soft outer glow when active
    if (t > 0.05f) {
        ImU32 glow = ImGui::ColorConvertFloat4ToU32(
            ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.10f * t));
        dl->AddRectFilled(
            ImVec2(p.x - 3.0f, p.y - 3.0f),
            ImVec2(p.x + w + 3.0f, p.y + h + 3.0f),
            glow, (h + 6.0f) * 0.5f);
    }

    // Glass surface
    ImVec4 bgVec = ImVec4(
        0.08f + 0.08f * t,
        0.09f + 0.10f * t,
        0.12f + 0.14f * t,
        0.55f + 0.25f * t);
    ImVec4 borderVec = ImVec4(
        kCardBorder.x + (kAccent.x - kCardBorder.x) * t * 0.7f,
        kCardBorder.y + (kAccent.y - kCardBorder.y) * t * 0.7f,
        kCardBorder.z + (kAccent.z - kCardBorder.z) * t * 0.7f,
        0.30f + 0.40f * t);

    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), ImGui::ColorConvertFloat4ToU32(bgVec), h * 0.5f);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), ImGui::ColorConvertFloat4ToU32(borderVec), h * 0.5f, 0, 1.0f);

    // Text
    ImVec4 textVec = ImVec4(
        0.55f + 0.37f * t,
        0.58f + 0.36f * t,
        0.64f + 0.33f * t,
        1.0f);
    dl->AddText(
        ImVec2(p.x + (w - textSize.x) * 0.5f, p.y + (h - textSize.y) * 0.5f),
        ImGui::ColorConvertFloat4ToU32(textVec),
        label);
    return clicked;
}

bool PillBitmaskRow(const char* id, int* mask, const PillItem* items, int count) {
    bool changed = false;
    float avail = ImGui::GetContentRegionAvail().x;
    float cursorX = 0.0f;
    float spacing = 8.0f;
    float minPillW = 80.0f;

    ImGui::PushID(id);
    for (int i = 0; i < count; i++) {
        ImVec2 sz = ImGui::CalcTextSize(items[i].label);
        float w = sz.x + 32.0f;
        if (w < minPillW) w = minPillW;
        if (i > 0 && cursorX + w + spacing <= avail) {
            ImGui::SameLine(0.0f, spacing);
        } else if (i > 0) {
            cursorX = 0.0f;
        }
        bool active = ((*mask) & items[i].flag) != 0;
        if (Pill(items[i].label, active, minPillW)) {
            *mask ^= items[i].flag;
            changed = true;
        }
        cursorX += w + spacing;
    }
    ImGui::PopID();
    return changed;
}

void HeaderPill(const char* text, ImVec4 color) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 ts = ImGui::CalcTextSize(text);
    float padX = 8.0f, padY = 3.0f;
    ImVec2 a = p;
    ImVec2 b = ImVec2(p.x + ts.x + padX * 2.0f, p.y + ts.y + padY * 2.0f);
    ImU32 bg     = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 0.12f));
    ImU32 border = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 0.40f));
    dl->AddRectFilled(a, b, bg, 4.0f);
    dl->AddRect(a, b, border, 4.0f, 0, 1.0f);
    dl->AddText(ImVec2(p.x + padX, p.y + padY), ImGui::ColorConvertFloat4ToU32(color), text);
    ImGui::Dummy(ImVec2(b.x - a.x, b.y - a.y));
}

// ---- sidebar ----------------------------------------------------------------
static void DrawSidebarGlyph(ImDrawList* dl, int glyph, ImVec2 origin, float size, ImU32 col) {
    float cx = origin.x + size * 0.5f;
    float cy = origin.y + size * 0.5f;
    float r  = size * 0.40f;
    float th = 1.4f;
    switch (glyph) {
        case GLYPH_ESP: {
            float w = r;
            float h = r * 0.65f;
            dl->PathClear();
            dl->PathLineTo(ImVec2(cx - w, cy));
            dl->PathBezierCubicCurveTo(
                ImVec2(cx - w * 0.5f, cy - h * 1.2f),
                ImVec2(cx + w * 0.5f, cy - h * 1.2f),
                ImVec2(cx + w, cy));
            dl->PathBezierCubicCurveTo(
                ImVec2(cx + w * 0.5f, cy + h * 1.2f),
                ImVec2(cx - w * 0.5f, cy + h * 1.2f),
                ImVec2(cx - w, cy));
            dl->PathStroke(col, ImDrawFlags_Closed, th);
            dl->AddCircleFilled(ImVec2(cx, cy), h * 0.55f, col);
        } break;
        case GLYPH_AIM: {
            dl->AddCircle(ImVec2(cx, cy), r,        col, 32, th);
            dl->AddCircle(ImVec2(cx, cy), r * 0.55f, col, 24, th);
            dl->AddLine(ImVec2(cx - r - 1.0f, cy), ImVec2(cx - r * 0.55f + 1.0f, cy), col, th);
            dl->AddLine(ImVec2(cx + r + 1.0f, cy), ImVec2(cx + r * 0.55f - 1.0f, cy), col, th);
            dl->AddLine(ImVec2(cx, cy - r - 1.0f), ImVec2(cx, cy - r * 0.55f + 1.0f), col, th);
            dl->AddLine(ImVec2(cx, cy + r + 1.0f), ImVec2(cx, cy + r * 0.55f - 1.0f), col, th);
        } break;
        case GLYPH_COMBAT: {
            float w = r * 0.85f;
            float h = r;
            ImVec2 pts[6] = {
                ImVec2(cx + w * 0.20f, cy - h),
                ImVec2(cx - w * 0.55f, cy + h * 0.10f),
                ImVec2(cx - w * 0.05f, cy + h * 0.10f),
                ImVec2(cx - w * 0.20f, cy + h),
                ImVec2(cx + w * 0.55f, cy - h * 0.10f),
                ImVec2(cx + w * 0.05f, cy - h * 0.10f),
            };
            dl->AddConvexPolyFilled(pts, 6, col);
        } break;
        case GLYPH_COSMETICS: {
            ImVec2 pts[4] = {
                ImVec2(cx, cy - r),
                ImVec2(cx + r * 0.85f, cy),
                ImVec2(cx, cy + r),
                ImVec2(cx - r * 0.85f, cy),
            };
            dl->AddPolyline(pts, 4, col, ImDrawFlags_Closed, th);
            dl->AddLine(ImVec2(cx - r * 0.55f, cy - r * 0.30f),
                        ImVec2(cx + r * 0.55f, cy - r * 0.30f), col, th);
        } break;
        case GLYPH_MISC: {
            float dotR = size * 0.10f;
            float gap = size * 0.22f;
            dl->AddCircleFilled(ImVec2(cx - gap, cy), dotR, col);
            dl->AddCircleFilled(ImVec2(cx,       cy), dotR, col);
            dl->AddCircleFilled(ImVec2(cx + gap, cy), dotR, col);
        } break;
    }
}

// ---- keybind capture --------------------------------------------------------
bool Keybind(const char* id, int* vk, const char* unbound) {
    static const char* keys[16] = {};
    static bool        waiting[16] = {};
    static int         firstFrame[16] = {};
    int slot = -1;
    for (int i = 0; i < 16; i++) { if (keys[i] == id) { slot = i; break; } }
    if (slot < 0) {
        for (int i = 0; i < 16; i++) { if (keys[i] == NULL) { keys[i] = id; slot = i; break; } }
    }
    if (slot < 0) slot = 0;

    bool changed = false;
    bool isWaiting = waiting[slot];

    const char* shown = isWaiting ? "Press a key..."
                       : (*vk == 0 ? unbound : Input::DisplayName(*vk));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 textSize = ImGui::CalcTextSize(shown);
    float padX = 16.0f, padY = 7.0f;
    float w = textSize.x + padX * 2.0f;
    if (w < 120.0f) w = 120.0f;
    float h = textSize.y + padY * 2.0f;

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(id);
    ImGui::InvisibleButton("##keybind", ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    ImGui::PopID();

    if (clicked) {
        waiting[slot] = !waiting[slot];
        isWaiting = waiting[slot];
        firstFrame[slot] = 1;
    }

    if (isWaiting) {
        if (Input::IsKeyDown(VK_ESCAPE)) {
            *vk = 0;
            waiting[slot] = false;
            firstFrame[slot] = 0;
            changed = true;
        } else {
            int pressed = Input::PollFirstPressed();
            if (pressed != 0) {
                if (firstFrame[slot] && pressed == VK_LBUTTON) {
                    // wait for release
                } else {
                    *vk = pressed;
                    waiting[slot] = false;
                    firstFrame[slot] = 0;
                    changed = true;
                }
            } else {
                firstFrame[slot] = 0;
            }
        }
    }

    // Animation
    static const char* gKeys[16] = {};
    static float gAnim[16] = {};
    int gSlot = -1;
    for (int i = 0; i < 16; i++) { if (gKeys[i] == id) { gSlot = i; break; } }
    if (gSlot < 0) {
        for (int i = 0; i < 16; i++) { if (gKeys[i] == NULL) { gKeys[i] = id; gSlot = i; break; } }
    }
    if (gSlot < 0) gSlot = 0;
    float target = isWaiting ? 1.0f : (hovered ? 0.40f : 0.0f);
    float dt = ImGui::GetIO().DeltaTime;
    if (dt <= 0.0f) dt = 0.016f;
    gAnim[gSlot] += (target - gAnim[gSlot]) * ClampFloat(dt * 10.0f, 0.0f, 1.0f);
    float t = gAnim[gSlot];

    // Glow
    if (t > 0.05f) {
        ImU32 glow = ImGui::ColorConvertFloat4ToU32(
            ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.10f * t));
        dl->AddRectFilled(
            ImVec2(p.x - 3.0f, p.y - 3.0f),
            ImVec2(p.x + w + 3.0f, p.y + h + 3.0f),
            glow, (h + 6.0f) * 0.5f);
    }

    // Glass surface
    ImVec4 bgVec = ImVec4(
        0.08f + 0.08f * t,
        0.09f + 0.10f * t,
        0.12f + 0.14f * t,
        0.55f + 0.25f * t);
    ImVec4 borderVec = ImVec4(
        kCardBorder.x + (kAccent.x - kCardBorder.x) * t * 0.7f,
        kCardBorder.y + (kAccent.y - kCardBorder.y) * t * 0.7f,
        kCardBorder.z + (kAccent.z - kCardBorder.z) * t * 0.7f,
        0.30f + 0.40f * t);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), ImGui::ColorConvertFloat4ToU32(bgVec), h * 0.5f);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), ImGui::ColorConvertFloat4ToU32(borderVec), h * 0.5f, 0, 1.0f);

    // Text
    ImVec4 textVec = isWaiting
        ? ImVec4(0.92f, 0.94f, 0.97f, 1.00f)
        : ImVec4(0.55f + 0.37f * t, 0.58f + 0.36f * t, 0.64f + 0.33f * t, 1.0f);
    dl->AddText(
        ImVec2(p.x + (w - textSize.x) * 0.5f, p.y + (h - textSize.y) * 0.5f),
        ImGui::ColorConvertFloat4ToU32(textVec),
        shown);
    return changed;
}

bool SidebarButton(const char* label, int glyph, bool active) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float h = 34.0f;
    float w = ImGui::GetContentRegionAvail().x;

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(label, ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();

    static const char* keys[64] = {};
    static float anim[64] = {};
    float target = active ? 1.0f : (hovered ? 0.40f : 0.0f);
    float t = Animate(label, target, 64, keys, anim);

    // Glass background on hover/active
    if (t > 0.01f) {
        ImU32 bg = ImGui::ColorConvertFloat4ToU32(ImVec4(
            0.10f + 0.06f * t,
            0.11f + 0.08f * t,
            0.14f + 0.10f * t,
            0.50f * t));
        dl->AddRectFilled(cursor, ImVec2(cursor.x + w, cursor.y + h), bg, 6.0f);
    }

    // Accent stripe (left edge)
    float stripeH = (h - 14.0f) * (active ? 1.0f : t);
    if (stripeH > 0.5f) {
        float topPad = (h - stripeH) * 0.5f;
        ImU32 stripeCol = ImGui::ColorConvertFloat4ToU32(
            ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.80f * (active ? 1.0f : t)));
        // Soft glow behind stripe
        if (active) {
            ImU32 stripeGlow = ImGui::ColorConvertFloat4ToU32(
                ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.10f));
            dl->AddRectFilled(
                ImVec2(cursor.x - 2.0f, cursor.y + topPad - 2.0f),
                ImVec2(cursor.x + 6.0f, cursor.y + topPad + stripeH + 2.0f),
                stripeGlow, 3.0f);
        }
        dl->AddRectFilled(
            ImVec2(cursor.x, cursor.y + topPad),
            ImVec2(cursor.x + 3.0f, cursor.y + topPad + stripeH),
            stripeCol, 2.0f);
    }

    // Glyph
    float glyphSize = 16.0f;
    float glyphX = cursor.x + 14.0f;
    float glyphY = cursor.y + (h - glyphSize) * 0.5f;
    ImVec4 glyphColV = active
        ? kAccent
        : ImVec4(0.45f + 0.25f * t, 0.50f + 0.25f * t, 0.58f + 0.22f * t, 1.0f);
    DrawSidebarGlyph(dl, glyph, ImVec2(glyphX, glyphY), glyphSize,
                     ImGui::ColorConvertFloat4ToU32(glyphColV));

    // Label
    ImVec2 ts = ImGui::CalcTextSize(label);
    ImVec4 textCol = active
        ? ImVec4(0.92f, 0.94f, 0.97f, 1.00f)
        : ImVec4(0.50f + 0.30f * t, 0.55f + 0.28f * t, 0.62f + 0.24f * t, 1.0f);
    dl->AddText(
        ImVec2(glyphX + glyphSize + 12.0f, cursor.y + (h - ts.y) * 0.5f),
        ImGui::ColorConvertFloat4ToU32(textCol),
        label);

    return clicked;
}

} // namespace Widgets
