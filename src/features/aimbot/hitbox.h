// aimbot/hitbox.h
// Hitbox selector / resolver. Pure logic, no rendering or input.
#pragma once
#include "../../il2cpp/player.h"

enum HitboxFlag {
    HITBOX_HEAD    = 1 << 0,
    HITBOX_CHEST   = 1 << 1,
    HITBOX_STOMACH = 1 << 2,
    HITBOX_LEGS    = 1 << 3,
};

struct HitboxSpec {
    int         flag;        // HitboxFlag
    const char* label;       // short UI label
    int         priority;    // higher = preferred
    int         primaryBone; // BONE_*
    int         altBone;     // -1 if none. Averages with primary (e.g. legs avg L+R knee).
};

extern const HitboxSpec g_hitboxes[];
extern const int        g_hitboxCount;

struct AimHitboxResolution {
    Vector3            world;
    const HitboxSpec*  spec;       // NULL when falling back to head/root
    int                boneUsed;   // BONE_*
    bool               isFallback; // true when no enabled hitbox resolved
};

// Returns true if any aim point was resolved (either a real hitbox or a
// fallback). The resolver is honest: it never silently remaps one hitbox
// to a different body part. Skipped hitboxes simply don't participate.
bool ResolveAimHitbox(const PlayerData& p, int mask, AimHitboxResolution& out);

const char* PreferredHitboxLabel(int mask);
int         CountHitboxBits(int mask);
int         HitboxValidMask(); // OR of all currently-supported flags
