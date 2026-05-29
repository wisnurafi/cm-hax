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
#include "../../third_party/imgui/imgui.h"
#include "../core/globals.h"
#include "../core/config.h"
#include "../core/logging.h"
#include "../game/il2cpp.h"
#include "../aimbot/aimbot.h"
#include "../aimbot/hitbox.h"
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
    { "Debug",     "Diagnostics",   Widgets::GLYPH_DEBUG },
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
    Widgets::Toggle("FPS Overlay",   &g_state.showFpsOverlay);

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
}

static void RenderTabAim() {
    Widgets::SectionHeader("Aimbot");
    Widgets::Toggle("Enable",          &g_state.aimbotEnabled);
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
}

static void RenderTabCombat() {
    Widgets::SectionHeader("Weapon");
    Widgets::Toggle("No Recoil", &g_state.noRecoil);
    Widgets::Toggle("No Spread", &g_state.noSpread);

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, kMutedText);
    ImGui::TextWrapped("Zeroes recoil/spread on the local weapon each frame. Disable before exiting a match for a cleaner restore.");
    ImGui::PopStyleColor();
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

static void RenderTabDebug() {
    Widgets::SectionHeader("Module");
    ImGui::Text("Project.dll:  %p", GetModuleHandleA("Project.dll"));
    ImGui::Text("StaticFields: %p", g_state.staticFields);
    ImGui::Text("GameClient:   %p", g_state.gameClientStaticFields);

    Widgets::SectionHeader("Log file");
    const char* logPath = LogPath();
    ImGui::TextWrapped("%s", logPath ? logPath : "(no writable location found)");
    if (logPath) {
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
    }

    Widgets::SectionHeader("Resolved IL2CPP exports");
    ImGui::Text("domain_get:                  %p", (void*)IL2CPP::fn_domain_get);
    ImGui::Text("thread_attach:               %p", (void*)IL2CPP::fn_thread_attach);
    ImGui::Text("class_from_name:             %p", (void*)IL2CPP::fn_class_from_name);
    ImGui::Text("domain_get_assemblies:       %p", (void*)IL2CPP::fn_domain_get_assemblies);
    ImGui::Text("class_get_method_from_name:  %p", (void*)IL2CPP::fn_class_get_method_from_name);

    Widgets::SectionHeader("Resolved methods");
    ImGui::Text("Camera.get_main:             %p", (void*)IL2CPP::fn_get_main_camera);
    ImGui::Text("Camera.get_worldToCamera:    %p", (void*)IL2CPP::fn_get_worldToCamera);
    ImGui::Text("Camera.get_projectionMatrix: %p", (void*)IL2CPP::fn_get_projectionMatrix);
    ImGui::Text("Camera.WorldToScreenPoint:   %p", (void*)IL2CPP::fn_WorldToScreenPoint);
    ImGui::Text("Component.get_transform:     %p", (void*)IL2CPP::fn_component_get_transform);
    ImGui::Text("Transform.get_position:      %p", (void*)IL2CPP::fn_get_position);
    ImGui::Text("PlayerRoot.get_TeamId:       %p", (void*)IL2CPP::fn_playerroot_get_teamid);
    ImGui::Text("PlayerRoot.get_IsVisible:    %p", (void*)IL2CPP::fn_playerroot_get_is_visible);
    ImGui::Text("PlayerHealth.get_IsDead:     %p", (void*)IL2CPP::fn_playerhealth_get_is_dead);
    ImGui::Text("PlayerHealth.get_IsDowned:   %p", (void*)IL2CPP::fn_playerhealth_get_is_downed);

    Widgets::SectionHeader("Counters");
    ImGui::Text("Players  R:%d  HP:%d  Tr:%d  OK:%d",
        g_state.dbg_pR, g_state.dbg_pHP, g_state.dbg_pTr, g_state.dbg_pW2S);
    ImGui::Text("Filtered dead:%d  downed:%d  noHead:%d",
        g_state.dbg_deadFiltered, g_state.dbg_downedFiltered, g_state.dbg_noHead);
    ImGui::Text("Visible:%d  Hidden:%d", g_state.dbg_visible, g_state.dbg_occluded);
}

static void RenderActiveTab() {
    switch (g_state.activeTab) {
        case TAB_ESP:        RenderTabESP();        break;
        case TAB_AIM:        RenderTabAim();        break;
        case TAB_COMBAT:     RenderTabCombat();     break;
        case TAB_COSMETICS:  RenderTabCosmetics();  break;
        case TAB_DEBUG:      RenderTabDebug();      break;
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
        ImU32 topCol    = ImGui::GetColorU32(ImVec4(0.054f, 0.068f, 0.090f, 1.0f));
        ImU32 bottomCol = ImGui::GetColorU32(ImVec4(0.026f, 0.032f, 0.044f, 1.0f));
        fg->AddRectFilledMultiColor(a, b, topCol, topCol, bottomCol, bottomCol);

        ImU32 fadeStart = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.45f));
        ImU32 fadeEnd   = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        fg->AddRectFilledMultiColor(
            ImVec2(a.x, b.y - 4.0f), ImVec2(b.x, b.y),
            fadeEnd, fadeEnd, fadeStart, fadeStart);

        float pulseAlpha = 0.30f + 0.20f * pulse;
        ImU32 ac0 = ImGui::ColorConvertFloat4ToU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, pulseAlpha));
        ImU32 ac1 = ImGui::GetColorU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.0f));
        fg->AddRectFilledMultiColor(
            ImVec2(a.x, b.y - 1.0f),
            ImVec2(a.x + 200.0f, b.y),
            ac0, ac1, ac1, ac0);
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

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.020f, 0.024f, 0.034f, 1.0f));
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

    // Soft divider
    {
        ImVec2 a = ImVec2(winPos.x + kSidebarW, winPos.y + kHeaderH);
        ImVec2 b = ImVec2(a.x + 6.0f, winPos.y + kHeaderH + bodyH);
        ImU32 leftCol  = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.35f));
        ImU32 rightCol = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        fg->AddRectFilledMultiColor(a, b, leftCol, rightCol, rightCol, leftCol);
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
        ImU32 topCol    = ImGui::GetColorU32(ImVec4(0.030f, 0.036f, 0.048f, 1.0f));
        ImU32 bottomCol = ImGui::GetColorU32(ImVec4(0.018f, 0.022f, 0.032f, 1.0f));
        fg->AddRectFilledMultiColor(a, b, topCol, topCol, bottomCol, bottomCol);
        fg->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, a.y),
                    ImGui::GetColorU32(ImGuiCol_Border), 1.0f);

        ImU32 ac0 = ImGui::GetColorU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.0f));
        ImU32 ac1 = ImGui::ColorConvertFloat4ToU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.20f));
        fg->AddRectFilledMultiColor(
            ImVec2(b.x - 280.0f, a.y),
            ImVec2(b.x, a.y + 1.0f),
            ac0, ac1, ac1, ac0);
    }

    // Hotkeys + cfg status
    {
        float padL = 18.0f;
        float baseY = kHeaderH + bodyH + 14.0f;
        ImGui::PushFont(g_fontSmall);

        struct KeyCap { const char* key; const char* label; };
        const KeyCap caps[] = {
            { "INS", "menu"   },
            { "END", "unload" },
            { "RMB", "aim"    },
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
        ImGui::PopFont();
    }

    // Save + Unload buttons (cohesive pair)
    {
        const char* saveLabel = "Save";
        const char* btnLabel  = "Unload";
        ImVec2 saveSize = ImVec2(78.0f, 28.0f);
        ImVec2 btnSize  = ImVec2(82.0f, 28.0f);

        float btnY = kHeaderH + bodyH + 9.0f;

        ImGui::SetCursorPos(ImVec2(winSize.x - btnSize.x - 14.0f, btnY));
        {
            ImVec2 sp = ImGui::GetCursorScreenPos();
            fg->AddRectFilled(
                ImVec2(sp.x - 1.0f, sp.y - 1.0f),
                ImVec2(sp.x + btnSize.x + 1.0f, sp.y + btnSize.y + 1.0f),
                ImGui::ColorConvertFloat4ToU32(ImVec4(kDanger.x, kDanger.y, kDanger.z, 0.10f)),
                6.0f);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.22f, 0.085f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.36f, 0.13f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.52f, 0.18f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.00f, 0.85f, 0.85f, 1.0f));
        if (ImGui::Button(btnLabel, btnSize)) {
            Hooks::RequestUnload();
        }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();

        ImGui::SetCursorPos(ImVec2(winSize.x - btnSize.x - saveSize.x - 22.0f, btnY));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.060f, 0.078f, 0.105f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.085f, 0.115f, 0.155f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.115f, 0.155f, 0.205f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.92f, 0.97f, 1.0f, 1.0f));
        if (ImGui::Button(saveLabel, saveSize)) {
            Config::Save();
        }
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);
    }

    ImGui::End();
}

} // namespace Menu
