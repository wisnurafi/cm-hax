// features/esp.cpp
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp.h"
#include "combat.h"
#include "../core/globals.h"
#include "../core/logging.h"
#include "../game/il2cpp.h"
#include "../game/player.h"
#include "../game/offsets.h"
#include "../game/sdk.h"
#include "../utils/math.h"
#include "../utils/memory.h"

namespace ESP {

DWORD WINAPI DataThread(LPVOID /*lpReserved*/) {
    if (IL2CPP::fn_thread_attach && IL2CPP::fn_domain_get) {
        IL2CPP::fn_thread_attach(IL2CPP::fn_domain_get());
    }
    Log("ESPDataThread running; ESP starts disabled until enabled in menu");

    while (InterlockedCompareExchange(&Runtime::g_running, 1, 1) != 0) {
        Sleep(16); // ~60fps data collection

        if (!g_state.espEnabled
            || !g_state.staticFields
            || !IL2CPP::fn_get_main_camera
            || !IL2CPP::fn_get_position
            || !IL2CPP::fn_get_worldToCamera
            || !IL2CPP::fn_get_projectionMatrix
            || !IL2CPP::fn_component_get_transform) {
            continue;
        }

        Runtime::SharedESPData newData;
        memset(&newData, 0, sizeof(newData));

        void* allPlayersPtr = *(void**)((uint8_t*)g_state.staticFields + 0x18);
        if (!allPlayersPtr) continue;
        Il2CppList* allPlayers = (Il2CppList*)allPlayersPtr;
        if (!allPlayers->_items || allPlayers->_size <= 0) continue;

        void** itemsArray = (void**)((uint8_t*)allPlayers->_items + 0x20);
        void* myPlayer = *(void**)((uint8_t*)g_state.staticFields + 0x8);
        int myTeamId = -999;
        bool haveMyTeam = Player::TryGetPlayerTeamId(myPlayer, myTeamId);
        Vector3 myPos = {};
        bool haveMyPos = Player::TryGetComponentPosition(myPlayer, myPos);

        Combat::ApplyWeaponPatches(myPlayer);

        void* camera = NULL;
        __try {
            camera = IL2CPP::fn_get_main_camera(IL2CPP::g_getMainCameraMethodInfo);
            if (!camera) continue;
            newData.camera = camera;

            IL2CPP::fn_get_worldToCamera (&newData.vMatrix, camera, IL2CPP::g_getWorldToCameraMethodInfo);
            IL2CPP::fn_get_projectionMatrix(&newData.pMatrix, camera, IL2CPP::g_getProjectionMatrixMethodInfo);
        } __except(1) {
            Log("Camera matrix collection faulted; disabling ESP");
            g_state.espEnabled = false;
            continue;
        }

        int pR = 0, pHP = 0, pTr = 0, pW2S = 0;
        int deadFiltered = 0, downedFiltered = 0, noHead = 0;
        int visibleCount = 0, occludedCount = 0;

        for (int i = 0; i < allPlayers->_size && i < 64; i++) {
            __try {
                void* player = itemsArray[i];
                if (!IsPlausiblePtr(player) || player == myPlayer) continue;

                if (g_state.teamCheck && haveMyTeam) {
                    int playerTeamId = -999;
                    if (Player::TryGetPlayerTeamId(player, playerTeamId) && playerTeamId == myTeamId) {
                        continue;
                    }
                }

                bool isReal = *(bool*)((uint8_t*)player + OFF_PLAYERROOT_ISREAL);
                pR++;

                void* healthComp = *(void**)((uint8_t*)player + OFF_PLAYERROOT_HEALTH);
                int hp = 0, armor = 0;
                if (!IsPlausiblePtr(healthComp)) continue;

                hp    = *(int*)((uint8_t*)healthComp + OFF_PLAYERHEALTH_HP);
                armor = *(int*)((uint8_t*)healthComp + OFF_PLAYERHEALTH_ARMOR);
                pHP++;

                bool isDead = false, isDowned = false;
                Player::TryGetPlayerDeathState(healthComp, isDead, isDowned);
                if (isDead || hp <= 0) { deadFiltered++; continue; }
                if (isDowned)          { downedFiltered++; continue; }

                Vector3 pos;
                if (!Player::TryGetComponentPosition(player, pos)) continue;
                pTr++;

                Vector3 headPos;
                bool hasHead = Player::TryGetPlayerHeadPosition(player, pos, headPos);
                if (!hasHead) {
                    headPos = pos;
                    noHead++;
                }

                PlayerData& outPlayer = newData.players[newData.playerCount];
                outPlayer.valid    = true;
                outPlayer.isReal   = isReal;
                outPlayer.hp       = hp;
                outPlayer.armor    = armor;
                outPlayer.hasHead  = hasHead;
                outPlayer.distance = haveMyPos ? Distance3D(myPos, pos) : 0.0f;

                bool hasLos = true;
                if (Player::TryGetPlayerVisibleState(player, hasLos, g_state.visibleCheck)) {
                    if (hasLos) visibleCount++;
                    else        occludedCount++;
                }
                outPlayer.visible = hasLos;
                if (!Player::TryGetPlayerName(player, outPlayer.name, sizeof(outPlayer.name))) {
                    snprintf(outPlayer.name, sizeof(outPlayer.name), "%s", isReal ? "Player" : "Bot");
                }
                outPlayer.rootPos = pos;
                outPlayer.headPos = headPos;
                outPlayer.boxPointCount = 0;
                Player::AddPlayerHitboxPoints(player, outPlayer);
                if (outPlayer.boxPointCount < 2) continue;

                newData.playerCount++;
                pW2S++;
            } __except(1) {}
        }

        newData.pR  = pR;
        newData.pHP = pHP;
        newData.pTr = pTr;
        newData.pW2S = pW2S;
        g_state.dbg_deadFiltered   = deadFiltered;
        g_state.dbg_downedFiltered = downedFiltered;
        g_state.dbg_noHead         = noHead;
        g_state.dbg_visible        = visibleCount;
        g_state.dbg_occluded       = occludedCount;

        EnterCriticalSection(&Runtime::g_espCs);
        Runtime::g_espData = newData;
        LeaveCriticalSection(&Runtime::g_espCs);
    }
    Log("ESPDataThread exiting");
    return 0;
}

} // namespace ESP
