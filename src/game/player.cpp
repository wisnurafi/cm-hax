// game/player.cpp
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

#include "player.h"
#include "il2cpp.h"
#include "offsets.h"
#include "sdk.h"
#include "../utils/memory.h"
#include "../utils/strings.h"

namespace Player {

bool TryGetTransformPosition(void* transform, Vector3& out) {
    if (!IsPlausiblePtr(transform) || !IL2CPP::fn_get_position) return false;

    __try {
        IL2CPP::fn_get_position(&out, transform, IL2CPP::g_getPositionMethodInfo);
        return isfinite(out.x) && isfinite(out.y) && isfinite(out.z);
    } __except(1) {
        return false;
    }
}

bool TryGetComponentPosition(void* component, Vector3& out) {
    if (!IsPlausiblePtr(component) || !IL2CPP::fn_component_get_transform) return false;

    __try {
        void* transform = IL2CPP::fn_component_get_transform(component, IL2CPP::g_componentGetTransformMethodInfo);
        return TryGetTransformPosition(transform, out);
    } __except(1) {
        return false;
    }
}

bool TryGetPlayerTeamId(void* player, int& outTeamId) {
    if (!IsPlausiblePtr(player) || !IL2CPP::fn_playerroot_get_teamid || !IL2CPP::g_playerRootGetTeamIdMethodInfo) return false;

    __try {
        outTeamId = IL2CPP::fn_playerroot_get_teamid(player, IL2CPP::g_playerRootGetTeamIdMethodInfo);
        return outTeamId >= -1 && outTeamId < 128;
    } __except(1) {
        return false;
    }
}

bool TryGetPlayerName(void* player, char* out, size_t outSize) {
    if (!out || outSize == 0) return false;
    out[0] = '\0';
    if (!IsPlausiblePtr(player)) return false;

    __try {
        void* netPlayerData = *(void**)((uint8_t*)player + OFF_PLAYERROOT_CACHEDPLAYERDATA);
        if (!IsPlausiblePtr(netPlayerData)) return false;

        if (IL2CPP::fn_battleextensions_nickname && IL2CPP::g_battleExtensionsNicknameMethodInfo) {
            void* nickname = IL2CPP::fn_battleextensions_nickname(netPlayerData, IL2CPP::g_battleExtensionsNicknameMethodInfo);
            if (CopyIl2CppStringUtf8(nickname, out, outSize)) return true;
        }

        if (IL2CPP::fn_battleextensions_playfabid && IL2CPP::g_battleExtensionsPlayFabIdMethodInfo) {
            void* playfabId = IL2CPP::fn_battleextensions_playfabid(netPlayerData, IL2CPP::g_battleExtensionsPlayFabIdMethodInfo);
            if (CopyIl2CppStringUtf8(playfabId, out, outSize)) return true;
        }
    } __except(1) {
        out[0] = '\0';
    }

    return false;
}

bool TryGetPlayerVisibleState(void* targetPlayer, bool& visible, bool useGameQuery) {
    visible = true;
    if (!useGameQuery) return true;
    if (!IsPlausiblePtr(targetPlayer)) return false;

    __try {
        if (IL2CPP::fn_playerroot_get_is_visible && IL2CPP::g_playerRootGetIsVisibleMethodInfo) {
            visible = IL2CPP::fn_playerroot_get_is_visible(targetPlayer, IL2CPP::g_playerRootGetIsVisibleMethodInfo);
            return true;
        }

        // Fallback: read the _isVisible bool field directly.
        visible = *(bool*)((uint8_t*)targetPlayer + 0x10A);
        return true;
    } __except(1) {
        visible = true;
        return false;
    }
}

bool TryGetPlayerDeathState(void* healthComp, bool& isDead, bool& isDowned) {
    isDead = false;
    isDowned = false;
    if (!IsPlausiblePtr(healthComp)) return false;

    __try {
        if (IL2CPP::fn_playerhealth_get_is_dead && IL2CPP::g_playerHealthIsDeadMethodInfo) {
            isDead = IL2CPP::fn_playerhealth_get_is_dead(healthComp, IL2CPP::g_playerHealthIsDeadMethodInfo);
        }
        if (IL2CPP::fn_playerhealth_get_is_downed && IL2CPP::g_playerHealthIsDownedMethodInfo) {
            isDowned = IL2CPP::fn_playerhealth_get_is_downed(healthComp, IL2CPP::g_playerHealthIsDownedMethodInfo);
        }
        return true;
    } __except(1) {
        return false;
    }
}

static bool IsValidHeadPosition(Vector3 rootPos, Vector3 headPos) {
    if (!isfinite(headPos.x) || !isfinite(headPos.y) || !isfinite(headPos.z)) return false;
    float height = headPos.y - rootPos.y;
    return height > 0.35f && height < 2.8f;
}

bool TryGetPlayerHeadPosition(void* player, Vector3 rootPos, Vector3& out) {
    if (!IsPlausiblePtr(player)) return false;

    __try {
        void* mobView = *(void**)((uint8_t*)player + OFF_PLAYERROOT_THIRDPERSONVIEW);
        if (IsPlausiblePtr(mobView)) {
            void* headTransform = *(void**)((uint8_t*)mobView + OFF_PLAYERMOBVIEW_HEADTRANSFORM);
            if (TryGetTransformPosition(headTransform, out) && IsValidHeadPosition(rootPos, out)) return true;
        }

        void* headHitbox = *(void**)((uint8_t*)player + OFF_PLAYERROOT_HEADHITBOX);
        Vector3 hitboxBase;
        if (TryGetComponentPosition(headHitbox, hitboxBase)) {
            Vector3 center = *(Vector3*)((uint8_t*)headHitbox + OFF_BOLTHITBOX_CENTER);
            Vector3 centeredHead = { hitboxBase.x + center.x, hitboxBase.y + center.y, hitboxBase.z + center.z };
            if (IsValidHeadPosition(rootPos, centeredHead)) {
                out = centeredHead;
                return true;
            }
            if (IsValidHeadPosition(rootPos, hitboxBase)) {
                out = hitboxBase;
                return true;
            }
        }
    } __except(1) {}

    return false;
}

void AddBoxPoint(PlayerData& p, Vector3 point) {
    if (p.boxPointCount >= 64) return;
    if (!isfinite(point.x) || !isfinite(point.y) || !isfinite(point.z)) return;
    p.boxPoints[p.boxPointCount++] = point;
}

static void AddHitboxBounds(PlayerData& p, void* hitbox) {
    if (!IsPlausiblePtr(hitbox)) return;

    __try {
        Vector3 base;
        if (!TryGetComponentPosition(hitbox, base)) return;

        Vector3 center = *(Vector3*)((uint8_t*)hitbox + OFF_BOLTHITBOX_CENTER);
        Vector3 size   = *(Vector3*)((uint8_t*)hitbox + OFF_BOLTHITBOX_BOXSIZE);
        if (!isfinite(center.x) || !isfinite(center.y) || !isfinite(center.z)) return;
        if (!isfinite(size.x)   || !isfinite(size.y)   || !isfinite(size.z))   return;

        if (fabsf(size.x) < 0.001f && fabsf(size.y) < 0.001f && fabsf(size.z) < 0.001f) {
            AddBoxPoint(p, { base.x + center.x, base.y + center.y, base.z + center.z });
            return;
        }

        Vector3 half = { fabsf(size.x) * 0.5f, fabsf(size.y) * 0.5f, fabsf(size.z) * 0.5f };
        if (half.x > 1.5f || half.y > 1.5f || half.z > 1.5f) return;

        Vector3 c = { base.x + center.x, base.y + center.y, base.z + center.z };
        for (int ix = -1; ix <= 1; ix += 2) {
            for (int iy = -1; iy <= 1; iy += 2) {
                for (int iz = -1; iz <= 1; iz += 2) {
                    AddBoxPoint(p, { c.x + half.x * ix, c.y + half.y * iy, c.z + half.z * iz });
                }
            }
        }
    } __except(1) {}
}

void AddPlayerHitboxPoints(void* player, PlayerData& p) {
    if (!IsPlausiblePtr(player)) return;

    __try {
        void* hitboxBody = *(void**)((uint8_t*)player + OFF_PLAYERROOT_HITBOXBODY);
        if (IsPlausiblePtr(hitboxBody)) {
            void* hitboxesObj = *(void**)((uint8_t*)hitboxBody + OFF_BOLTHITBOXBODY_HITBOXES);
            if (IsPlausiblePtr(hitboxesObj)) {
                Il2CppArray* hitboxes = (Il2CppArray*)hitboxesObj;
                uintptr_t count = hitboxes->max_length;
                if (count > 0 && count < 64) {
                    void** items = (void**)((uint8_t*)hitboxesObj + 0x20);
                    for (uintptr_t i = 0; i < count && p.boxPointCount < 64; i++) {
                        AddHitboxBounds(p, items[i]);
                    }
                }
            }
        }

        void* mobView = *(void**)((uint8_t*)player + OFF_PLAYERROOT_THIRDPERSONVIEW);
        if (IsPlausiblePtr(mobView)) {
            const struct { int offset; int boneIndex; } boneMap[] = {
                { OFF_PLAYERMOBVIEW_HEADTRANSFORM,  BONE_HEAD  },
                { OFF_PLAYERMOBVIEW_NECKTRANSFORM,  BONE_NECK  },
                { OFF_PLAYERMOBVIEW_CHESTTRANSFORM, BONE_CHEST },
                { OFF_PLAYERMOBVIEW_SPINETRANSFORM, BONE_SPINE },
                { OFF_PLAYERMOBVIEW_LKNEETRANSFORM, BONE_LKNEE },
                { OFF_PLAYERMOBVIEW_RKNEETRANSFORM, BONE_RKNEE },
            };
            for (int i = 0; i < (int)(sizeof(boneMap) / sizeof(boneMap[0])); i++) {
                Vector3 point;
                void* transform = *(void**)((uint8_t*)mobView + boneMap[i].offset);
                if (TryGetTransformPosition(transform, point)) {
                    p.bones[boneMap[i].boneIndex] = point;
                    p.boneValid[boneMap[i].boneIndex] = true;
                    if (p.boxPointCount < 8) AddBoxPoint(p, point);
                }
            }
        }
    } __except(1) {}

    // Always include root + head as fallback box anchors and as the root bone.
    p.bones[BONE_ROOT] = p.rootPos;
    p.boneValid[BONE_ROOT] = true;
    if (p.hasHead && !p.boneValid[BONE_HEAD]) {
        p.bones[BONE_HEAD] = p.headPos;
        p.boneValid[BONE_HEAD] = true;
    }

    AddBoxPoint(p, p.rootPos);
    AddBoxPoint(p, p.headPos);
}

} // namespace Player
