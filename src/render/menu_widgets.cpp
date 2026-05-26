// render/menu_widgets.cpp
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
using MenuStyle::kCardBorder;
using MenuStyle::kSubtleText;

// ---- shared per-widget animation slots --------------------------------------
// Each widget keys an animation by its label pointer. Cheap; ~64-128 slots
// is plenty for the menu and labels are stable string literals.

static float Animate(const char* key, float target, int slots, const char** keyTable, float* anim) {
    int slot = -1;
    for (int i = 0; i < slots; i++) { if (keyTable[i] == key) { slot = i; break; } }
    if (slot < 0) {
        for (int i = 0; i < slots; i++) { if (keyTable[i] == NULL) { keyTable[i] = key; slot = i; break; } }
    }
    if (slot < 0) slot = 0;
    float dt = ImGui::GetIO().DeltaTime;
    if (dt <= 0.0f) dt = 0.016f;
    anim[slot] += (target - anim[slot]) * ClampFloat(dt * 14.0f, 0.0f, 1.0f);
    return anim[slot];
}

// ---- section header ---------------------------------------------------------
void SectionHeader(const char* label) {
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float lineH = ImGui::GetTextLineHeight();
    ImU32 ac = ImGui::GetColorU32(kAccent);
    dl->AddRectFilled(
        ImVec2(p.x, p.y + lineH * 0.18f),
        ImVec2(p.x + 3.0f, p.y + lineH * 0.85f),
        ac, 1.0f);
    ImGui::SetCursorScreenPos(ImVec2(p.x + 10.0f, p.y));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.98f, 1.00f, 1.00f));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImVec2 cp = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    dl->AddLine(
        ImVec2(cp.x, cp.y + 1.0f),
        ImVec2(cp.x + w, cp.y + 1.0f),
        ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
}

// ---- toggle switch ----------------------------------------------------------
bool Toggle(const char* label, bool* v) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    float h = ImGui::GetTextLineHeight() + 4.0f;
    float w = h * 1.9f;
    ImVec2 sw_a = p;
    ImVec2 sw_b = ImVec2(p.x + w, p.y + h);

    ImGui::InvisibleButton(label, ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    if (clicked) { *v = !*v; }

    static const char* keys[64] = {};
    static float anim[64] = {};
    float t = Animate(label, *v ? 1.0f : 0.0f, 64, keys, anim);

    ImU32 bg = ImGui::ColorConvertFloat4ToU32(ImVec4(
        ((1 - t) * 0.090f + t * (kAccent.x * 0.45f)),
        ((1 - t) * 0.110f + t * (kAccent.y * 0.45f)),
        ((1 - t) * 0.140f + t * (kAccent.z * 0.65f)),
        1.0f));
    ImU32 border = ImGui::GetColorU32(hovered ? kAccentSoft : kCardBorder);

    dl->AddRectFilled(sw_a, sw_b, bg, h * 0.5f);
    dl->AddRect      (sw_a, sw_b, border, h * 0.5f, 0, 1.0f);

    float knobR = h * 0.42f;
    float knobX = sw_a.x + h * 0.5f + (w - h) * t;
    float knobY = sw_a.y + h * 0.5f;
    if (t > 0.05f) {
        dl->AddCircleFilled(ImVec2(knobX, knobY), knobR + 4.0f,
            ImGui::ColorConvertFloat4ToU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.18f * t)));
    }
    dl->AddCircleFilled(ImVec2(knobX, knobY), knobR,
        ImGui::GetColorU32(ImVec4(0.95f, 0.97f, 1.0f, 1.0f)));

    ImGui::SameLine(0.0f, 10.0f);
    ImGui::AlignTextToFramePadding();
    ImVec4 textCol = *v ? ImVec4(0.94f, 0.97f, 1.0f, 1.0f) : ImVec4(0.78f, 0.82f, 0.88f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, textCol);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    return clicked;
}

// ---- pill -------------------------------------------------------------------
bool Pill(const char* label, bool active, float minWidth) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 textSize = ImGui::CalcTextSize(label);
    float padX = 14.0f;
    float padY = 6.0f;
    float w = textSize.x + padX * 2.0f;
    if (w < minWidth) w = minWidth;
    float h = textSize.y + padY * 2.0f;

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(label, ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();

    static const char* keys[128] = {};
    static float anim[128] = {};
    float target = active ? 1.0f : (hovered ? 0.4f : 0.0f);
    float t = Animate(label, target, 128, keys, anim);

    if (t > 0.05f) {
        ImU32 glow = ImGui::ColorConvertFloat4ToU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.18f * t));
        dl->AddRectFilled(
            ImVec2(p.x - 2.0f, p.y - 2.0f),
            ImVec2(p.x + w + 2.0f, p.y + h + 2.0f),
            glow, (h + 4.0f) * 0.5f);
    }

    ImVec4 bgVec = ImVec4(0.060f + 0.140f * t, 0.078f + 0.220f * t, 0.100f + 0.350f * t, 1.0f);
    ImVec4 borderVec = ImVec4(
        kCardBorder.x + (kAccent.x - kCardBorder.x) * t,
        kCardBorder.y + (kAccent.y - kCardBorder.y) * t,
        kCardBorder.z + (kAccent.z - kCardBorder.z) * t,
        0.55f + 0.45f * t);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), ImGui::ColorConvertFloat4ToU32(bgVec), h * 0.5f);
    dl->AddRect      (p, ImVec2(p.x + w, p.y + h), ImGui::ColorConvertFloat4ToU32(borderVec), h * 0.5f, 0, 1.0f);

    ImVec4 textVec = ImVec4(
        0.72f + 0.25f * t,
        0.78f + 0.20f * t,
        0.86f + 0.13f * t,
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
    float minPillW = 78.0f;

    ImGui::PushID(id);
    for (int i = 0; i < count; i++) {
        ImVec2 sz = ImGui::CalcTextSize(items[i].label);
        float w = sz.x + 28.0f;
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
    float padX = 7.0f, padY = 2.0f;
    ImVec2 a = p;
    ImVec2 b = ImVec2(p.x + ts.x + padX * 2.0f, p.y + ts.y + padY * 2.0f);
    ImU32 bg     = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 0.18f));
    ImU32 border = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 0.55f));
    dl->AddRectFilled(a, b, bg, 4.0f);
    dl->AddRect(a, b, border, 4.0f);
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
        case GLYPH_ESP: { // eye outline + iris
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
        case GLYPH_AIM: { // target reticle
            dl->AddCircle(ImVec2(cx, cy), r,        col, 32, th);
            dl->AddCircle(ImVec2(cx, cy), r * 0.55f, col, 24, th);
            dl->AddLine(ImVec2(cx - r - 1.0f, cy), ImVec2(cx - r * 0.55f + 1.0f, cy), col, th);
            dl->AddLine(ImVec2(cx + r + 1.0f, cy), ImVec2(cx + r * 0.55f - 1.0f, cy), col, th);
            dl->AddLine(ImVec2(cx, cy - r - 1.0f), ImVec2(cx, cy - r * 0.55f + 1.0f), col, th);
            dl->AddLine(ImVec2(cx, cy + r + 1.0f), ImVec2(cx, cy + r * 0.55f - 1.0f), col, th);
        } break;
        case GLYPH_COMBAT: { // lightning bolt
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
        case GLYPH_COSMETICS: { // diamond
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
        case GLYPH_DEBUG: { // 2x2 grid
            float s = r * 0.55f;
            float g = 1.6f;
            dl->AddRect(ImVec2(cx - s, cy - s), ImVec2(cx - g, cy - g), col, 1.0f, 0, th);
            dl->AddRect(ImVec2(cx + g, cy - s), ImVec2(cx + s, cy - g), col, 1.0f, 0, th);
            dl->AddRect(ImVec2(cx - s, cy + g), ImVec2(cx - g, cy + s), col, 1.0f, 0, th);
            dl->AddRect(ImVec2(cx + g, cy + g), ImVec2(cx + s, cy + s), col, 1.0f, 0, th);
        } break;
    }
}

// ---- keybind capture --------------------------------------------------------
// Persists "waiting for input" per id. Clicking the bind starts capture; the
// next bindable VK (mouse or keyboard) is stored. ESC clears the bind.
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
    float padX = 14.0f, padY = 6.0f;
    float w = textSize.x + padX * 2.0f;
    if (w < 110.0f) w = 110.0f;
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

    // Capture: poll for the first physically held key while waiting.
    if (isWaiting) {
        if (Input::IsKeyDown(VK_ESCAPE)) {
            *vk = 0;
            waiting[slot] = false;
            firstFrame[slot] = 0;
            changed = true;
        } else {
            int pressed = Input::PollFirstPressed();
            if (pressed != 0) {
                // Skip the LMB that started the capture (require a fresh press).
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

    static const char* gKeys[16] = {};
    static float gAnim[16] = {};
    int gSlot = -1;
    for (int i = 0; i < 16; i++) { if (gKeys[i] == id) { gSlot = i; break; } }
    if (gSlot < 0) {
        for (int i = 0; i < 16; i++) { if (gKeys[i] == NULL) { gKeys[i] = id; gSlot = i; break; } }
    }
    if (gSlot < 0) gSlot = 0;
    float target = isWaiting ? 1.0f : (hovered ? 0.45f : 0.0f);
    float dt = ImGui::GetIO().DeltaTime;
    if (dt <= 0.0f) dt = 0.016f;
    gAnim[gSlot] += (target - gAnim[gSlot]) * ClampFloat(dt * 12.0f, 0.0f, 1.0f);
    float t = gAnim[gSlot];

    if (t > 0.05f) {
        ImU32 glow = ImGui::ColorConvertFloat4ToU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.18f * t));
        dl->AddRectFilled(
            ImVec2(p.x - 2.0f, p.y - 2.0f),
            ImVec2(p.x + w + 2.0f, p.y + h + 2.0f),
            glow, (h + 4.0f) * 0.5f);
    }

    ImVec4 bgVec = ImVec4(0.060f + 0.140f * t, 0.078f + 0.220f * t, 0.100f + 0.350f * t, 1.0f);
    ImVec4 borderVec = ImVec4(
        kCardBorder.x + (kAccent.x - kCardBorder.x) * t,
        kCardBorder.y + (kAccent.y - kCardBorder.y) * t,
        kCardBorder.z + (kAccent.z - kCardBorder.z) * t,
        0.55f + 0.45f * t);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), ImGui::ColorConvertFloat4ToU32(bgVec), h * 0.5f);
    dl->AddRect      (p, ImVec2(p.x + w, p.y + h), ImGui::ColorConvertFloat4ToU32(borderVec), h * 0.5f, 0, 1.0f);

    ImVec4 textVec = isWaiting
        ? ImVec4(0.97f, 0.99f, 1.00f, 1.00f)
        : ImVec4(0.78f + 0.18f * t, 0.83f + 0.14f * t, 0.90f + 0.10f * t, 1.0f);
    dl->AddText(
        ImVec2(p.x + (w - textSize.x) * 0.5f, p.y + (h - textSize.y) * 0.5f),
        ImGui::ColorConvertFloat4ToU32(textVec),
        shown);
    return changed;
}

bool SidebarButton(const char* label, int glyph, bool active) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float h = 32.0f;
    float w = ImGui::GetContentRegionAvail().x;

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(label, ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();

    static const char* keys[64] = {};
    static float anim[64] = {};
    float target = active ? 1.0f : (hovered ? 0.45f : 0.0f);
    float t = Animate(label, target, 64, keys, anim);

    if (t > 0.01f) {
        ImU32 bg = ImGui::ColorConvertFloat4ToU32(ImVec4(
            0.060f + 0.090f * t,
            0.080f + 0.130f * t,
            0.110f + 0.170f * t,
            1.0f));
        dl->AddRectFilled(cursor, ImVec2(cursor.x + w, cursor.y + h), bg, 6.0f);
    }

    float stripeH = (h - 12.0f) * (active ? 1.0f : t);
    if (stripeH > 0.5f) {
        float topPad = (h - stripeH) * 0.5f;
        ImU32 stripeCol = ImGui::ColorConvertFloat4ToU32(
            ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.85f * (active ? 1.0f : t)));
        dl->AddRectFilled(
            ImVec2(cursor.x - 1.0f, cursor.y + topPad),
            ImVec2(cursor.x + 3.0f, cursor.y + topPad + stripeH),
            stripeCol, 2.0f);
    }

    float glyphSize = 16.0f;
    float glyphX = cursor.x + 12.0f;
    float glyphY = cursor.y + (h - glyphSize) * 0.5f;
    ImVec4 glyphColV = active
        ? kAccent
        : ImVec4(0.62f + 0.20f * t, 0.68f + 0.20f * t, 0.78f + 0.18f * t, 1.0f);
    DrawSidebarGlyph(dl, glyph, ImVec2(glyphX, glyphY), glyphSize,
                     ImGui::ColorConvertFloat4ToU32(glyphColV));

    ImVec2 ts = ImGui::CalcTextSize(label);
    ImVec4 textCol = active ? ImVec4(0.97f, 0.99f, 1.00f, 1.00f)
                            : ImVec4(0.70f + 0.22f * t, 0.76f + 0.20f * t, 0.84f + 0.14f * t, 1.0f);
    dl->AddText(
        ImVec2(glyphX + glyphSize + 10.0f, cursor.y + (h - ts.y) * 0.5f),
        ImGui::ColorConvertFloat4ToU32(textCol),
        label);

    return clicked;
}

} // namespace Widgets
