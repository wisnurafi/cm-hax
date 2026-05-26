// aimbot/hitbox.cpp
#include <stddef.h>
#include "hitbox.h"

// Order = display order in UI. Priority drives target selection when multiple
// are enabled. Stomach intentionally uses the spine bone on this rig.
const HitboxSpec g_hitboxes[] = {
    { HITBOX_HEAD,    "Head",    60, BONE_HEAD,    -1         },
    { HITBOX_CHEST,   "Chest",   40, BONE_CHEST,   -1         },
    { HITBOX_STOMACH, "Stomach", 30, BONE_SPINE,   -1         }, // spine = stomach proxy on this rig
    { HITBOX_LEGS,    "Legs",    10, BONE_LKNEE,   BONE_RKNEE },
};
const int g_hitboxCount = (int)(sizeof(g_hitboxes) / sizeof(g_hitboxes[0]));

static bool TryResolveSpecBone(const HitboxSpec& h, const PlayerData& p, Vector3& out, int& boneUsedOut) {
    if (h.primaryBone < 0 || h.primaryBone >= BONE_COUNT) return false;
    if (!p.boneValid[h.primaryBone]) return false;

    boneUsedOut = h.primaryBone;
    if (h.altBone >= 0 && h.altBone < BONE_COUNT && p.boneValid[h.altBone]) {
        const Vector3& a = p.bones[h.primaryBone];
        const Vector3& b = p.bones[h.altBone];
        out.x = (a.x + b.x) * 0.5f;
        out.y = (a.y + b.y) * 0.5f;
        out.z = (a.z + b.z) * 0.5f;
    } else {
        out = p.bones[h.primaryBone];
    }
    return true;
}

bool ResolveAimHitbox(const PlayerData& p, int mask, AimHitboxResolution& out) {
    out.spec = NULL;
    out.boneUsed = BONE_HEAD;
    out.isFallback = false;

    int bestPriority = -1;
    bool found = false;

    if (mask != 0) {
        for (int i = 0; i < g_hitboxCount; i++) {
            const HitboxSpec& h = g_hitboxes[i];
            if ((mask & h.flag) == 0) continue;
            if (h.priority <= bestPriority) continue;

            Vector3 pos;
            int boneUsed = -1;
            if (!TryResolveSpecBone(h, p, pos, boneUsed)) {
                continue; // honest skip - no silent remap
            }

            out.world = pos;
            out.spec = &h;
            out.boneUsed = boneUsed;
            bestPriority = h.priority;
            found = true;
        }
    }

    if (found) return true;

    // Fallback: prefer the head; otherwise the root.
    out.isFallback = true;
    if (p.hasHead) {
        out.world = p.headPos;
        out.boneUsed = BONE_HEAD;
        return true;
    }
    if (p.boneValid[BONE_ROOT]) {
        out.world = p.bones[BONE_ROOT];
        out.boneUsed = BONE_ROOT;
        return true;
    }
    return false;
}

const char* PreferredHitboxLabel(int mask) {
    if (mask == 0) return "Head (auto)";
    int bestPriority = -1;
    const HitboxSpec* best = NULL;
    for (int i = 0; i < g_hitboxCount; i++) {
        if ((mask & g_hitboxes[i].flag) == 0) continue;
        if (g_hitboxes[i].priority > bestPriority) {
            bestPriority = g_hitboxes[i].priority;
            best = &g_hitboxes[i];
        }
    }
    return best ? best->label : "Head (auto)";
}

int CountHitboxBits(int mask) {
    int n = 0;
    for (int i = 0; i < g_hitboxCount; i++) {
        if (mask & g_hitboxes[i].flag) n++;
    }
    return n;
}

int HitboxValidMask() {
    int m = 0;
    for (int i = 0; i < g_hitboxCount; i++) m |= g_hitboxes[i].flag;
    return m;
}
