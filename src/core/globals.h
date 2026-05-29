// core/globals.h
// Shared mutable state for the cheat. Bundling the toggles into a single
// struct keeps call sites tidy and makes it obvious what the cheat tracks.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../il2cpp/player.h"
#include "../utils/math.h"

// User-facing settings + a few runtime status fields. Anything the user
// can flip from the menu lives here, anything serialized by the config
// system lives here, and anything the renderer/aimbot reads lives here.
struct CmState {
    // ESP feature toggles
    bool  espEnabled   = false;
    bool  espBox       = true;
    bool  espHealth    = true;
    bool  espArmor     = true;
    bool  espLines     = true;
    bool  espNames     = true;
    bool  espDistance  = true;
    bool  espSkeleton  = true;
    bool  visibleCheck = true;
    bool  teamCheck    = true;
    bool  showFpsOverlay = true;

    // Aim
    bool  aimbotEnabled  = true;
    bool  drawAimbotFov  = true;
    float aimbotFov      = 250.0f;
    float aimbotSmooth   = 3.0f;
    int   aimbotHitboxMask = 1; // HITBOX_HEAD by default

    // Activation. 0 = Always Active, 1 = Hold Key.
    // Default key is RMB to preserve previous "hold RMB to aim" behavior.
    int   aimActivationMode = 1;
    int   aimActivationKey  = 0x02; // VK_RBUTTON

    // Combat
    bool  noRecoil = false;
    bool  noSpread = false;

    // Triggerbot. Off-by-default with no key bound, so a fresh install can't
    // fire on its own. The renderer fills triggerCrosshairOnTarget once per
    // frame; the trigger state machine reads it from there.
    bool  triggerEnabled        = false;
    int   triggerHoldMs         = 25;     // LEFTDOWN -> LEFTUP duration
    int   triggerRefireMs       = 80;     // gap between clicks
    int   triggerPrecision      = 1;      // 0 = Precise, 1 = Balanced, 2 = Aggressive
    int   triggerActivationMode = 1;      // 0 = Always, 1 = Hold
    int   triggerActivationKey  = 0;      // VK; 0 = unbound

    // Cosmetics
    bool  unlockAll = false;

    // ESP colors (RGBA, 0..1)
    float colEnemy[4]    = { 1.00f, 0.28f, 0.28f, 0.96f };
    float colBot[4]      = { 1.00f, 0.70f, 0.26f, 0.92f };
    float colOccluded[4] = { 0.53f, 0.57f, 0.65f, 0.73f };
    float colName[4]     = { 0.96f, 0.97f, 1.00f, 0.96f };
    float colDistance[4] = { 0.73f, 0.80f, 0.90f, 0.84f };
    float colSnapline[4] = { 0.82f, 0.88f, 0.96f, 0.41f };
    float colSkeleton[4] = { 1.00f, 1.00f, 1.00f, 0.95f };
    float colHpHigh[4]   = { 0.31f, 0.92f, 0.47f, 0.96f };
    float colHpMid[4]    = { 0.96f, 0.80f, 0.25f, 0.96f };
    float colHpLow[4]    = { 1.00f, 0.31f, 0.31f, 0.96f };
    float colArmor[4]    = { 0.27f, 0.65f, 1.00f, 0.96f };

    // UI
    int  activeTab = 0;
    bool showMenu  = true;

    // Runtime status (not persisted)
    int  dbg_pR = 0, dbg_pHP = 0, dbg_pTr = 0, dbg_pW2S = 0;
    int  dbg_deadFiltered = 0, dbg_downedFiltered = 0, dbg_noHead = 0;
    int  dbg_visible = 0, dbg_occluded = 0;

    // Resolved at startup, used by features
    void* staticFields           = NULL;
    void* gameClientStaticFields = NULL;
};

extern CmState g_state;

// Module + lifecycle handles shared across files.
namespace Runtime {
    extern HMODULE         g_module;
    extern HANDLE          g_dataThread;
    extern volatile LONG   g_mainThreadStarted;
    extern volatile LONG   g_running;
    extern volatile LONG   g_unloadRequested;
    extern volatile LONG   g_inPresent;
    extern bool            g_csInitialized;
    extern HANDLE          g_singleInstanceMutex;

    // ESP collector output (data thread -> render thread). Guarded by g_espCs.
    struct SharedESPData {
        Matrix4x4  vMatrix;
        Matrix4x4  pMatrix;
        void*      camera;
        PlayerData players[64];
        int        playerCount;
        int        pR, pHP, pTr, pW2S;
    };
    extern SharedESPData     g_espData;
    extern CRITICAL_SECTION  g_espCs;
}
