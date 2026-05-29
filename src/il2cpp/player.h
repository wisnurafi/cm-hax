// il2cpp/player.h
// Per-player data captured by the ESP collector and consumed by the renderer
// and aimbot. Also holds bone projection helpers.
#pragma once
#include "../utils/math.h"

// Skeleton bones (indices into PlayerData.bones[])
enum BoneIndex {
    BONE_HEAD = 0,
    BONE_NECK,
    BONE_CHEST,
    BONE_SPINE,
    BONE_LKNEE,
    BONE_RKNEE,
    BONE_ROOT,
    BONE_COUNT
};

struct PlayerData {
    bool    valid;
    bool    isReal;
    int     hp;
    int     armor;
    bool    hasHead;
    bool    visible;
    float   distance;
    char    name[48];
    Vector3 rootPos;
    Vector3 headPos;
    Vector3 boxPoints[64];
    int     boxPointCount;
    Vector3 bones[BONE_COUNT];
    bool    boneValid[BONE_COUNT];
};

// Pointer-safe wrappers around IL2CPP runtime calls. All return false on
// faults / invalid pointers and are safe to call from any thread that has
// been attached to the IL2CPP domain.
namespace Player {
    bool TryGetTransformPosition(void* transform, Vector3& out);
    bool TryGetComponentPosition(void* component, Vector3& out);
    bool TryGetPlayerTeamId(void* player, int& outTeamId);
    bool TryGetPlayerName(void* player, char* out, size_t outSize);
    bool TryGetPlayerVisibleState(void* targetPlayer, bool& visible, bool useGameQuery);
    bool TryGetPlayerDeathState(void* healthComp, bool& isDead, bool& isDowned);
    bool TryGetPlayerHeadPosition(void* player, Vector3 rootPos, Vector3& out);

    // Box / bone collection. `pd` is filled in.
    void AddBoxPoint(PlayerData& p, Vector3 point);
    void AddPlayerHitboxPoints(void* player, PlayerData& p);
}
