// render/menu.cpp
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <math.h>
#include <stdio.h>

#include "menu.h"
#include "menu_style.h"
#include "menu_widgets.h"
#include "imgui.h"
#include "../core/globals.h"
#include "../core/config.h"
#include "../core/logging.h"
#include "../il2cpp/il2cpp.h"
#include "../features/aimbot/aimbot.h"
#include "../features/aimbot/hitbox.h"
#include "../features/triggerbot/triggerbot.h"
#include "../features/cosmetics.h"
#include "../utils/math.h"
#include "../utils/input.h"

// Forward-declare to avoid a cyclic include against core/hooks.h.
namespace Hooks { void RequestUnload(); }

namespace Menu {

using namespace MenuStyle;

struct TabSpec {
    const char* label;
    const char* hint;
    int         glyph;
};

static const TabSpec g_tabs[TAB_COUNT] = {
    { "ESP",       "Visuals",       Widgets::GLYPH_ESP },
    { "Aim",       "Targeting",     Widgets::GLYPH_AIM },
    { "Combat",    "Recoil/spread", Widgets::GLYPH_COMBAT },
    { "Cosmetics", "Unlocks/save",  Widgets::GLYPH_COSMETICS },
    { "Misc",      "Tools & info",  Widgets::GLYPH_MISC },
};

// =====================================================================
// Per-tab renderers
// =====================================================================
static void RenderTabESP() {
    Widgets::SectionHeader("Master");
    Widgets::Toggle("Enable ESP", &g_state.espEnabled);

    Widgets::SectionHeader("Visuals");
    ImGui::Columns(2, "esp_visual_columns", false);
    Widgets::Toggle("Box",       &g_state.espBox);
    Widgets::Toggle("Names",     &g_state.espNames);
    Widgets::Toggle("Distance",  &g_state.espDistance);
    Widgets::Toggle("Skeleton",  &g_state.espSkeleton);
    ImGui::NextColumn();
    Widgets::Toggle("Health",    &g_state.espHealth);
    Widgets::Toggle("Armor",     &g_state.espArmor);
    Widgets::Toggle("Snaplines", &g_state.espLines);
    ImGui::Columns(1);

    Widgets::SectionHeader("Filters");
    Widgets::Toggle("Team Check",    &g_state.teamCheck);
    Widgets::Toggle("Visible Check", &g_state.visibleCheck);

    Widgets::SectionHeader("Colors");
    ImGuiColorEditFlags cf = ImGuiColorEditFlags_AlphaBar
                           | ImGuiColorEditFlags_AlphaPreviewHalf
                           | ImGuiColorEditFlags_NoInputs;
    ImGui::Columns(2, "esp_color_columns", false);
    ImGui::ColorEdit4("Enemy",    g_state.colEnemy,    cf);
    ImGui::ColorEdit4("Bot",      g_state.colBot,      cf);
    ImGui::ColorEdit4("Occluded", g_state.colOccluded, cf);
    ImGui::ColorEdit4("Name",     g_state.colName,     cf);
    ImGui::ColorEdit4("Distance", g_state.colDistance, cf);
    ImGui::ColorEdit4("Snapline", g_state.colSnapline, cf);
    ImGui::NextColumn();
    ImGui::ColorEdit4("Skeleton", g_state.colSkeleton, cf);
    ImGui::ColorEdit4("HP High",  g_state.colHpHigh,   cf);
    ImGui::ColorEdit4("HP Mid",   g_state.colHpMid,    cf);
    ImGui::ColorEdit4("HP Low",   g_state.colHpLow,    cf);
    ImGui::ColorEdit4("Armor",    g_state.colArmor,    cf);
    ImGui::Columns(1);

    Widgets::SectionHeader("Radar");
    Widgets::Toggle("Enable Radar", &g_state.radarEnabled);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##radar_radius", &g_state.radarRadius, 40.0f, 200.0f, "Size   %.0f px");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##radar_scale",  &g_state.radarScale,  10.0f, 200.0f, "Zoom   %.0f m");
    ImGui::PushFont(g_fontSmall);
    ImGui::TextColored(kMutedText, "Lower zoom = closer view. Higher zoom = wider area.");
    ImGui::PopFont();
}

static void RenderTabAim() {
    Widgets::SectionHeader("Aimbot");
    Widgets::Toggle("Enable Aimbot",   &g_state.aimbotEnabled);
    ImGui::SameLine(0.0f, 24.0f);
    Widgets::Toggle("Draw FOV Circle", &g_state.drawAimbotFov);

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##fov", &g_state.aimbotFov, 25.0f, 600.0f, "FOV   %.0f");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##smooth", &g_state.aimbotSmooth, 1.0f, 15.0f, "Smooth %.1f");
    if (g_state.aimbotSmooth < 1.0f) g_state.aimbotSmooth = 1.0f;

    Widgets::SectionHeader("Activation");
    {
        // Mode pills (radio-style: clicking sets the mode, doesn't toggle).
        static const Widgets::PillItem kAlways[] = { { 1, "Always Active" } };
        static const Widgets::PillItem kHold[]   = { { 1, "Hold Key"      } };

        int alwaysMask = (g_state.aimActivationMode == Aim::ACT_ALWAYS) ? 1 : 0;
        int holdMask   = (g_state.aimActivationMode == Aim::ACT_HOLD)   ? 1 : 0;
        if (Widgets::PillBitmaskRow("act_always", &alwaysMask, kAlways, 1)) {
            g_state.aimActivationMode = Aim::ACT_ALWAYS;
        }
        ImGui::SameLine(0.0f, 8.0f);
        if (Widgets::PillBitmaskRow("act_hold", &holdMask, kHold, 1)) {
            g_state.aimActivationMode = Aim::ACT_HOLD;
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::PushFont(g_fontSmall);
        if (g_state.aimActivationMode == Aim::ACT_HOLD) {
            ImGui::TextColored(kSubtleText, "Bind");
            ImGui::SameLine(0.0f, 10.0f);
            Widgets::Keybind("aim.bind", &g_state.aimActivationKey, "Unbound");
            if (g_state.aimActivationKey == 0) {
                ImGui::SameLine(0.0f, 12.0f);
                ImGui::TextColored(kWarning, "no key bound - aim is inactive");
            } else {
                ImGui::SameLine(0.0f, 12.0f);
                ImGui::TextColored(kSubtleText, "hold to aim . click to rebind . ESC clears");
            }
        } else {
            ImGui::TextColored(kSubtleText, "Aim runs continuously while Enable is on.");
        }
        ImGui::PopFont();
    }

    Widgets::SectionHeader("Hitboxes");
    static const Widgets::PillItem kPills[] = {
        { HITBOX_HEAD,    "Head"    },
        { HITBOX_CHEST,   "Chest"   },
        { HITBOX_STOMACH, "Stomach" },
        { HITBOX_LEGS,    "Legs"    },
    };
    Widgets::PillBitmaskRow("hitboxes", &g_state.aimbotHitboxMask,
                            kPills, (int)(sizeof(kPills) / sizeof(kPills[0])));

    int selected = CountHitboxBits(g_state.aimbotHitboxMask);
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::PushFont(g_fontSmall);
    ImGui::TextColored(kSubtleText, "Preferred");
    ImGui::SameLine();
    if (selected == 0) {
        ImGui::TextColored(kMutedText, "Head (auto)");
    } else {
        ImGui::TextColored(kAccent, "%s", PreferredHitboxLabel(g_state.aimbotHitboxMask));
        if (selected > 1) {
            ImGui::SameLine();
            ImGui::TextColored(kSubtleText, " . %d enabled", selected);
        }
    }
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, kMutedText);
    ImGui::TextWrapped("Targets are scanned by priority (head > chest > stomach > legs).");
    ImGui::PopStyleColor();

    // ---- Triggerbot ------------------------------------------------------
    Widgets::SectionHeader("Triggerbot");
    Widgets::Toggle("Enable Triggerbot", &g_state.triggerEnabled);

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::PushFont(g_fontSmall);
    ImGui::TextColored(kSubtleText, "Precision");
    ImGui::PopFont();
    {
        // Radio-style precision pills. Pill widget is bitmask-based so we
        // give each its own single-bit pseudo-mask and translate clicks
        // into a discrete enum write.
        static const Widgets::PillItem kPrecise[]    = { { 1, "Precise"    } };
        static const Widgets::PillItem kBalanced[]   = { { 1, "Balanced"   } };
        static const Widgets::PillItem kAggressive[] = { { 1, "Aggressive" } };

        int preciseMask    = (g_state.triggerPrecision == 0) ? 1 : 0;
        int balancedMask   = (g_state.triggerPrecision == 1) ? 1 : 0;
        int aggressiveMask = (g_state.triggerPrecision == 2) ? 1 : 0;

        if (Widgets::PillBitmaskRow("trig_prec_p", &preciseMask, kPrecise, 1)) {
            g_state.triggerPrecision = 0;
        }
        ImGui::SameLine(0.0f, 8.0f);
        if (Widgets::PillBitmaskRow("trig_prec_b", &balancedMask, kBalanced, 1)) {
            g_state.triggerPrecision = 1;
        }
        ImGui::SameLine(0.0f, 8.0f);
        if (Widgets::PillBitmaskRow("trig_prec_a", &aggressiveMask, kAggressive, 1)) {
            g_state.triggerPrecision = 2;
        }

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::PushFont(g_fontSmall);
        const char* hint = "";
        switch (g_state.triggerPrecision) {
            case 0: hint = "Crosshair must sit solidly on the enemy. Best for snipers / single shot."; break;
            case 1: hint = "Detection matches the visible box. Good default for most weapons."; break;
            case 2: hint = "Fires near the enemy outline. Forgiving for SMGs / shotguns."; break;
        }
        ImGui::TextColored(kMutedText, "%s", hint);
        ImGui::PopFont();
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##trig_hold",   &g_state.triggerHoldMs,    5, 200,  "Hold     %d ms");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderInt("##trig_refire", &g_state.triggerRefireMs,  10, 1000, "Refire   %d ms");

    Widgets::SectionHeader("Trigger Activation");
    {
        // Mirror the aim activation control: pill row + optional bind.
        static const Widgets::PillItem kTrigAlways[] = { { 1, "Always Active" } };
        static const Widgets::PillItem kTrigHold[]   = { { 1, "Hold Key"      } };

        int alwaysMask = (g_state.triggerActivationMode == Trigger::ACT_ALWAYS) ? 1 : 0;
        int holdMask   = (g_state.triggerActivationMode == Trigger::ACT_HOLD)   ? 1 : 0;
        if (Widgets::PillBitmaskRow("trig_act_always", &alwaysMask, kTrigAlways, 1)) {
            g_state.triggerActivationMode = Trigger::ACT_ALWAYS;
        }
        ImGui::SameLine(0.0f, 8.0f);
        if (Widgets::PillBitmaskRow("trig_act_hold", &holdMask, kTrigHold, 1)) {
            g_state.triggerActivationMode = Trigger::ACT_HOLD;
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::PushFont(g_fontSmall);
        if (g_state.triggerActivationMode == Trigger::ACT_HOLD) {
            ImGui::TextColored(kSubtleText, "Bind");
            ImGui::SameLine(0.0f, 10.0f);
            Widgets::Keybind("trigger.bind", &g_state.triggerActivationKey, "Unbound");
            if (g_state.triggerActivationKey == 0) {
                ImGui::SameLine(0.0f, 12.0f);
                ImGui::TextColored(kWarning, "no key bound - trigger is inactive");
            } else {
                ImGui::SameLine(0.0f, 12.0f);
                ImGui::TextColored(kSubtleText, "hold to fire . click to rebind . ESC clears");
            }
        } else {
            ImGui::TextColored(kWarning, "Always Active will fire whenever your crosshair sits on an enemy.");
        }
        ImGui::PopFont();
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, kMutedText);
    ImGui::TextWrapped("Auto-fires LMB while the crosshair is on an enemy. Honors the team and visible filters from the ESP tab.");
    ImGui::PopStyleColor();
}

static void RenderTabCombat() {
    Widgets::SectionHeader("Weapon");
    Widgets::Toggle("No Recoil", &g_state.noRecoil);
    Widgets::Toggle("No Spread", &g_state.noSpread);

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, kMutedText);
    ImGui::TextWrapped("Zeroes recoil/spread on the local weapon each frame. Disable before exiting a match for a cleaner restore.");
    ImGui::PopStyleColor();

    Widgets::SectionHeader("Movement");
    Widgets::Toggle("Bunny Hop",         &g_state.bunnyhopEnabled);
    Widgets::Toggle("Infinite Sprint",   &g_state.infiniteSprintEnabled);
    Widgets::Toggle("Auto Slide-Cancel", &g_state.autoSlideCancelEnabled);

    if (g_state.autoSlideCancelEnabled) {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderInt("##slide_frames", &g_state.slideCancelFrames, 3, 30, "Cancel after %d frames");
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::PushFont(g_fontSmall);
    ImGui::TextColored(kMutedText, "Bunny Hop: auto-jump on landing while Space is held.");
    ImGui::TextColored(kMutedText, "Infinite Sprint: stamina never depletes.");
    ImGui::TextColored(kMutedText, "Slide-Cancel: auto-cancels slide into jump after N frames.");
    ImGui::PopFont();
}

static void RenderTabCosmetics() {
    Widgets::SectionHeader("Unlock");
    bool prev = g_state.unlockAll;
    Widgets::Toggle("Unlock All Skins", &g_state.unlockAll);
    if (prev != g_state.unlockAll) {
        if (g_state.unlockAll) Cosmetics::ApplyUnlockAll();
        else                   Cosmetics::RestoreUnlockAll();
    }
    ImGui::SameLine(0.0f, 12.0f);
    if (Cosmetics::IsUnlockActive()) Widgets::HeaderPill("ACTIVE", kSuccess);
    else                             Widgets::HeaderPill("OFF",    kSubtleText);

    ImGui::PushStyleColor(ImGuiCol_Text, kMutedText);
    ImGui::TextWrapped("Patches all Is*Available checks to true. Equip what you want, then save to keep it.");
    ImGui::PopStyleColor();

    Widgets::SectionHeader("Save");
    bool canSave = g_state.gameClientStaticFields != NULL;
    if (!canSave) ImGui::BeginDisabled();
    if (ImGui::Button("Save Equipped to Cloud", ImVec2(-1.0f, 0.0f))) {
        Cosmetics::SaveEquippedToCloud();
    }
    if (!canSave) ImGui::EndDisabled();

    if (Cosmetics::g_statusMsg[0] && (GetTickCount() - Cosmetics::g_statusTime) < 5000) {
        bool isError = (Cosmetics::g_statusMsg[0] == 'E');
        ImGui::TextColored(isError ? kDanger : kSuccess, "%s", Cosmetics::g_statusMsg);
    } else if (!canSave) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText);
        ImGui::TextWrapped("GameClient static fields not resolved yet. Wait for the main menu / match to fully load.");
        ImGui::PopStyleColor();
    }
}

// =====================================================================
// Misc tab
// =====================================================================

static const char* DeriveStatusLabel(ImVec4* outColor) {
    if (!GetModuleHandleA("Project.dll")) {
        if (outColor) *outColor = kSubtleText;
        return "Game not detected";
    }
    if (!IL2CPP::fn_domain_get) {
        if (outColor) *outColor = kWarning;
        return "Booting";
    }
    if (!g_state.staticFields) {
        if (outColor) *outColor = kWarning;
        return "Waiting for match";
    }
    if (outColor) *outColor = kSuccess;
    return "Ready";
}

static void RenderTabMisc() {
    Widgets::SectionHeader("Status");
    {
        ImVec4 statusCol = kSubtleText;
        const char* status = DeriveStatusLabel(&statusCol);
        ImGui::TextColored(kSubtleText, "State:");
        ImGui::SameLine(0.0f, 8.0f);
        Widgets::HeaderPill(status, statusCol);

        ImGui::PushFont(g_fontSmall);
        ImGui::TextColored(kMutedText, "Enemies tracked: %d visible / %d hidden",
                           g_state.dbg_visible, g_state.dbg_occluded);
        ImGui::TextColored(kMutedText, "Triggerbot: %s",
            !g_state.triggerEnabled         ? "off"
            : Trigger::DebugIsClicking()    ? "firing"
            : Trigger::IsActivationActive() ? "armed"
                                            : "idle");
        ImGui::PopFont();
    }

    Widgets::SectionHeader("Display");
    Widgets::Toggle("FPS Overlay", &g_state.showFpsOverlay);

    Widgets::SectionHeader("Config");
    if (ImGui::Button("Save Config", ImVec2(140.0f, 0.0f))) {
        Config::Save();
    }
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.22f, 0.085f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.36f, 0.13f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.52f, 0.18f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.00f, 0.85f, 0.85f, 1.0f));
    if (ImGui::Button("Unload", ImVec2(140.0f, 0.0f))) {
        Hooks::RequestUnload();
    }
    ImGui::PopStyleColor(4);

    if (Config::g_statusMsg[0] && (GetTickCount() - Config::g_statusTime) < 4000) {
        ImGui::PushFont(g_fontSmall);
        ImGui::TextColored(kAccent, "%s", Config::g_statusMsg);
        ImGui::PopFont();
    }

    Widgets::SectionHeader("Log file");
    const char* logPath = LogPath();
    if (logPath) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMutedText);
        ImGui::TextWrapped("%s", logPath);
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        if (ImGui::Button("Copy path", ImVec2(110.0f, 0.0f))) {
            ImGui::SetClipboardText(logPath);
        }
        ImGui::SameLine(0.0f, 8.0f);
        if (ImGui::Button("Open folder", ImVec2(110.0f, 0.0f))) {
            char folder[MAX_PATH];
            strncpy(folder, logPath, sizeof(folder) - 1);
            folder[sizeof(folder) - 1] = '\0';
            char* slash = strrchr(folder, '\\');
            if (slash) {
                *slash = '\0';
                ShellExecuteA(NULL, "open", folder, NULL, NULL, SW_SHOWNORMAL);
            }
        }
    } else {
        ImGui::TextColored(kWarning, "Couldn't open a writable log location.");
    }

    Widgets::SectionHeader("About");
    ImGui::PushStyleColor(ImGuiCol_Text, kMutedText);
    ImGui::TextWrapped("Cm-Hax  -  Combat Master internal");
    ImGui::PopStyleColor();
}

static void RenderActiveTab() {
    switch (g_state.activeTab) {
        case TAB_ESP:        RenderTabESP();        break;
        case TAB_AIM:        RenderTabAim();        break;
        case TAB_COMBAT:     RenderTabCombat();     break;
        case TAB_COSMETICS:  RenderTabCosmetics();  break;
        case TAB_MISC:       RenderTabMisc();       break;
        default:             RenderTabESP();        break;
    }
}

// =====================================================================
// Window chrome
// =====================================================================
void Draw() {
    ImGui::GetIO().MouseDrawCursor = g_state.showMenu;
    if (!g_state.showMenu) return;

    const float kMenuW = 640.0f;
    const float kMenuH = 480.0f;
    const float kHeaderH = 60.0f;
    const float kFooterH = 46.0f;
    const float kSidebarW = 176.0f;

    ImGui::SetNextWindowSize(ImVec2(kMenuW, kMenuH), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(560.0f, 380.0f), ImVec2(1200.0f, 900.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::Begin("##cm_menu", &g_state.showMenu,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(2);

    ImVec2 winPos  = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    ImDrawList* fg = ImGui::GetWindowDrawList();

    float t_now = (float)ImGui::GetTime();
    float pulse = 0.5f + 0.5f * sinf(t_now * 1.6f);

    // ---- Header ------------------------------------------------------
    {
        ImVec2 a = winPos;
        ImVec2 b = ImVec2(winPos.x + winSize.x, winPos.y + kHeaderH);

        // Frosted glass header: translucent dark with subtle gradient
        ImU32 topCol    = ImGui::GetColorU32(ImVec4(0.06f, 0.07f, 0.09f, 0.88f));
        ImU32 bottomCol = ImGui::GetColorU32(ImVec4(0.04f, 0.045f, 0.06f, 0.92f));
        fg->AddRectFilledMultiColor(a, b, topCol, topCol, bottomCol, bottomCol);

        // Bottom edge: soft luminous line (glass refraction effect)
        ImU32 edgeLeft  = ImGui::ColorConvertFloat4ToU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.20f + 0.08f * pulse));
        ImU32 edgeMid   = ImGui::ColorConvertFloat4ToU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.08f));
        ImU32 edgeRight = ImGui::ColorConvertFloat4ToU32(ImVec4(0.22f, 0.26f, 0.32f, 0.15f));
        fg->AddRectFilledMultiColor(
            ImVec2(a.x, b.y - 1.0f),
            ImVec2(a.x + winSize.x * 0.4f, b.y),
            edgeLeft, edgeMid, edgeMid, edgeLeft);
        fg->AddRectFilledMultiColor(
            ImVec2(a.x + winSize.x * 0.4f, b.y - 1.0f),
            ImVec2(b.x, b.y),
            edgeMid, edgeRight, edgeRight, edgeMid);
    }

    // Brand
    {
        float padL = 22.0f;
        float topY = 14.0f;
        ImVec2 dp = ImVec2(winPos.x + padL + 2.0f, winPos.y + topY + 9.0f);
        float r = 4.0f;
        fg->AddCircleFilled(dp, r + 5.0f,
            ImGui::ColorConvertFloat4ToU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.18f * pulse)));
        fg->AddCircleFilled(dp, r,
            ImGui::ColorConvertFloat4ToU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.95f)));

        ImGui::PushFont(g_fontTitle);
        ImGui::SetCursorPos(ImVec2(padL + 18.0f, topY - 4.0f));
        ImGui::TextColored(ImVec4(0.97f, 0.99f, 1.00f, 1.00f), "CM");
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::TextColored(kAccent, "HAX");
        ImGui::PopFont();

        ImGui::SetCursorPos(ImVec2(padL + 18.0f, topY + 18.0f));
        ImGui::PushFont(g_fontSmall);
        ImGui::TextColored(kSubtleText, "Internal . IL2CPP");
        ImGui::PopFont();
    }

    // Right header stats + status
    {
        float right = winSize.x - 22.0f;

        char visBuf[16], trkBuf[16];
        snprintf(visBuf, sizeof(visBuf), "%d", g_state.dbg_visible);
        snprintf(trkBuf, sizeof(trkBuf), "%d", g_state.dbg_pW2S);

        ImVec2 visSz = ImGui::CalcTextSize(visBuf);
        ImVec2 trkSz = ImGui::CalcTextSize(trkBuf);

        ImGui::PushFont(g_fontSmall);
        ImVec2 visLbl = ImGui::CalcTextSize("VISIBLE");
        ImVec2 trkLbl = ImGui::CalcTextSize("TRACKED");
        ImGui::PopFont();

        float colGap = 26.0f;
        float colW   = (visSz.x > visLbl.x ? visSz.x : visLbl.x);
        float colW2  = (trkSz.x > trkLbl.x ? trkSz.x : trkLbl.x);

        float colXTrk = right - colW2;
        float colXVis = colXTrk - colGap - colW;

        float numY = 12.0f;
        ImGui::SetCursorPos(ImVec2(colXVis + (colW  - visSz.x) * 0.5f, numY));
        ImGui::TextColored(ImVec4(0.94f, 0.97f, 1.0f, 1.0f), "%s", visBuf);
        ImGui::SetCursorPos(ImVec2(colXTrk + (colW2 - trkSz.x) * 0.5f, numY));
        ImGui::TextColored(ImVec4(0.94f, 0.97f, 1.0f, 1.0f), "%s", trkBuf);

        ImGui::PushFont(g_fontSmall);
        float lblY = numY + visSz.y + 2.0f;
        ImGui::SetCursorPos(ImVec2(colXVis + (colW  - visLbl.x) * 0.5f, lblY));
        ImGui::TextColored(kSubtleText, "VISIBLE");
        ImGui::SetCursorPos(ImVec2(colXTrk + (colW2 - trkLbl.x) * 0.5f, lblY));
        ImGui::TextColored(kSubtleText, "TRACKED");
        ImGui::PopFont();

        const char* statusText = g_state.espEnabled ? "ACTIVE" : "STANDBY";
        ImGui::PushFont(g_fontSmall);
        ImVec2 sts = ImGui::CalcTextSize(statusText);
        ImGui::PopFont();

        float statusGap = 28.0f;
        float statusRight = colXVis - statusGap;
        float midY  = (numY + lblY + ImGui::GetTextLineHeight()) * 0.5f - 1.0f;
        float dotR  = 3.5f + 0.4f * (g_state.espEnabled ? pulse : 0.0f);
        ImVec2 dotPos = ImVec2(winPos.x + statusRight - sts.x - 10.0f, winPos.y + midY);

        if (g_state.espEnabled) {
            fg->AddCircleFilled(dotPos, dotR + 4.0f,
                ImGui::ColorConvertFloat4ToU32(ImVec4(kSuccess.x, kSuccess.y, kSuccess.z, 0.18f * pulse)));
        }
        fg->AddCircleFilled(dotPos, dotR,
            ImGui::ColorConvertFloat4ToU32(g_state.espEnabled ? kSuccess : kSubtleText));

        ImGui::PushFont(g_fontSmall);
        ImGui::SetCursorScreenPos(ImVec2(dotPos.x + 8.0f, dotPos.y - sts.y * 0.5f));
        ImGui::TextColored(g_state.espEnabled ? kSuccess : kSubtleText, "%s", statusText);
        ImGui::PopFont();
    }

    ImGui::SetCursorPos(ImVec2(0.0f, kHeaderH));

    // ---- Body --------------------------------------------------------
    float bodyH = winSize.y - kHeaderH - kFooterH;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.045f, 0.06f, 0.70f));
    ImGui::BeginChild("##sidebar", ImVec2(kSidebarW, bodyH), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 14.0f));
    ImGui::Indent(14.0f);

    ImGui::PushFont(g_fontSmall);
    ImGui::TextColored(kSubtleText, "MENU");
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    for (int i = 0; i < TAB_COUNT; i++) {
        if (Widgets::SidebarButton(g_tabs[i].label, g_tabs[i].glyph, g_state.activeTab == i)) {
            g_state.activeTab = i;
        }
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
    }

    ImGui::Unindent(14.0f);
    ImGui::EndChild();

    // Soft divider — frosted glass edge between sidebar and content
    {
        ImVec2 a = ImVec2(winPos.x + kSidebarW, winPos.y + kHeaderH);
        ImVec2 b = ImVec2(a.x + 1.0f, winPos.y + kHeaderH + bodyH);
        ImU32 divCol = ImGui::GetColorU32(ImVec4(0.22f, 0.26f, 0.32f, 0.20f));
        fg->AddRectFilled(a, b, divCol);
    }

    ImGui::SameLine(0.0f, 0.0f);

    ImGui::BeginChild("##content", ImVec2(0.0f, bodyH), false);
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::Indent(20.0f);

    ImGui::PushFont(g_fontTitle);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.97f, 0.99f, 1.0f, 1.0f));
    ImGui::TextUnformatted(g_tabs[g_state.activeTab].label);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::PushFont(g_fontSmall);
    ImGui::TextColored(kSubtleText, "%s", g_tabs[g_state.activeTab].hint);
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::PushItemWidth(-1.0f);
    RenderActiveTab();
    ImGui::PopItemWidth();

    ImGui::Unindent(20.0f);
    ImGui::EndChild();

    // ---- Footer ------------------------------------------------------
    {
        ImVec2 a = ImVec2(winPos.x, winPos.y + kHeaderH + bodyH);
        ImVec2 b = ImVec2(winPos.x + winSize.x, winPos.y + winSize.y);

        // Frosted glass footer
        ImU32 topCol    = ImGui::GetColorU32(ImVec4(0.05f, 0.055f, 0.07f, 0.85f));
        ImU32 bottomCol = ImGui::GetColorU32(ImVec4(0.035f, 0.04f, 0.055f, 0.90f));
        fg->AddRectFilledMultiColor(a, b, topCol, topCol, bottomCol, bottomCol);

        // Top edge: luminous glass border
        ImU32 edgeCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.22f, 0.26f, 0.32f, 0.25f));
        fg->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, a.y), edgeCol, 1.0f);
    }

    // Hotkeys + cfg status. Save / Unload buttons live on the Misc tab now.
    {
        float padL = 18.0f;
        float baseY = kHeaderH + bodyH + 14.0f;
        ImGui::PushFont(g_fontSmall);

        struct KeyCap { const char* key; const char* label; };
        const KeyCap caps[] = {
            { "INS", "menu"   },
            { "END", "unload" },
        };

        float cursorX = padL;
        for (int i = 0; i < (int)(sizeof(caps) / sizeof(caps[0])); i++) {
            ImVec2 ks = ImGui::CalcTextSize(caps[i].key);
            float kx = winPos.x + cursorX;
            float ky = winPos.y + baseY - 2.0f;
            float kw = ks.x + 10.0f;
            float kh = ks.y + 4.0f;

            fg->AddRectFilled(
                ImVec2(kx, ky), ImVec2(kx + kw, ky + kh),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.060f, 0.075f, 0.100f, 1.0f)), 3.0f);
            fg->AddRect(
                ImVec2(kx, ky), ImVec2(kx + kw, ky + kh),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.130f, 0.170f, 0.220f, 0.95f)), 3.0f, 0, 1.0f);
            fg->AddText(
                ImVec2(kx + 5.0f, ky + 2.0f),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.85f, 0.92f, 1.0f, 1.0f)),
                caps[i].key);

            ImGui::SetCursorPos(ImVec2(cursorX + kw + 6.0f, baseY));
            ImGui::TextColored(kMutedText, "%s", caps[i].label);

            ImVec2 lblSz = ImGui::CalcTextSize(caps[i].label);
            cursorX += kw + 6.0f + lblSz.x + 14.0f;
        }

        if (Config::g_statusMsg[0] && (GetTickCount() - Config::g_statusTime) < 4000) {
            ImGui::SetCursorPos(ImVec2(cursorX, baseY));
            ImGui::TextColored(kAccent, "  %s", Config::g_statusMsg);
        }

        // Right-aligned status pill: derives from runtime state, no buttons.
        {
            ImVec4 statusCol = kSubtleText;
            const char* status = DeriveStatusLabel(&statusCol);
            ImVec2 sz = ImGui::CalcTextSize(status);
            float padX = 9.0f, padY = 3.0f;
            float pillW = sz.x + padX * 2.0f;
            float pillH = sz.y + padY * 2.0f;
            float px = winPos.x + winSize.x - pillW - 18.0f;
            float py = winPos.y + baseY - 2.0f;

            ImU32 bg     = ImGui::ColorConvertFloat4ToU32(ImVec4(statusCol.x, statusCol.y, statusCol.z, 0.18f));
            ImU32 border = ImGui::ColorConvertFloat4ToU32(ImVec4(statusCol.x, statusCol.y, statusCol.z, 0.55f));
            fg->AddRectFilled(ImVec2(px, py), ImVec2(px + pillW, py + pillH), bg, pillH * 0.5f);
            fg->AddRect      (ImVec2(px, py), ImVec2(px + pillW, py + pillH), border, pillH * 0.5f, 0, 1.0f);
            fg->AddText      (ImVec2(px + padX, py + padY),
                              ImGui::ColorConvertFloat4ToU32(statusCol), status);
        }

        ImGui::PopFont();
    }

    ImGui::End();
}

} // namespace Menu
