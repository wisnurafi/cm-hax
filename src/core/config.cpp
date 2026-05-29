// core/config.cpp
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "globals.h"
#include "logging.h"
#include "../features/aimbot/hitbox.h"

namespace Config {

char  g_statusMsg[96] = "";
unsigned long g_statusTime = 0;

enum CfgType { CFG_BOOL, CFG_FLOAT, CFG_INT, CFG_COL4 };

struct CfgEntry {
    const char* key;
    int         type;
    void*       ptr;
};

// One row per persisted setting. Add a new entry here to expose it on disk.
static CfgEntry g_entries[] = {
    // ESP
    { "esp.enabled",       CFG_BOOL,  &g_state.espEnabled },
    { "esp.box",           CFG_BOOL,  &g_state.espBox },
    { "esp.health",        CFG_BOOL,  &g_state.espHealth },
    { "esp.armor",         CFG_BOOL,  &g_state.espArmor },
    { "esp.lines",         CFG_BOOL,  &g_state.espLines },
    { "esp.names",         CFG_BOOL,  &g_state.espNames },
    { "esp.distance",      CFG_BOOL,  &g_state.espDistance },
    { "esp.skeleton",      CFG_BOOL,  &g_state.espSkeleton },
    { "esp.visibleCheck",  CFG_BOOL,  &g_state.visibleCheck },
    { "esp.teamCheck",     CFG_BOOL,  &g_state.teamCheck },
    { "ui.fpsOverlay",     CFG_BOOL,  &g_state.showFpsOverlay },

    // Aim
    { "aim.enabled",       CFG_BOOL,  &g_state.aimbotEnabled },
    { "aim.drawFov",       CFG_BOOL,  &g_state.drawAimbotFov },
    { "aim.fov",           CFG_FLOAT, &g_state.aimbotFov },
    { "aim.smooth",        CFG_FLOAT, &g_state.aimbotSmooth },
    { "aim.hitboxMask",    CFG_INT,   &g_state.aimbotHitboxMask },
    { "aim.activationMode",CFG_INT,   &g_state.aimActivationMode },
    { "aim.activationKey", CFG_INT,   &g_state.aimActivationKey },

    // Combat
    { "combat.noRecoil",   CFG_BOOL,  &g_state.noRecoil },
    { "combat.noSpread",   CFG_BOOL,  &g_state.noSpread },

    // Triggerbot
    { "trigger.enabled",          CFG_BOOL,  &g_state.triggerEnabled },
    { "trigger.holdMs",           CFG_INT,   &g_state.triggerHoldMs },
    { "trigger.refireMs",         CFG_INT,   &g_state.triggerRefireMs },
    { "trigger.precision",        CFG_INT,   &g_state.triggerPrecision },
    { "trigger.activationMode",   CFG_INT,   &g_state.triggerActivationMode },
    { "trigger.activationKey",    CFG_INT,   &g_state.triggerActivationKey },

    // Radar
    { "radar.enabled",            CFG_BOOL,  &g_state.radarEnabled },
    { "radar.radius",             CFG_FLOAT, &g_state.radarRadius },
    { "radar.scale",              CFG_FLOAT, &g_state.radarScale },

    // Movement
    { "movement.bunnyhop",        CFG_BOOL,  &g_state.bunnyhopEnabled },
    { "movement.infiniteSprint",  CFG_BOOL,  &g_state.infiniteSprintEnabled },
    { "movement.autoSlideCancel", CFG_BOOL,  &g_state.autoSlideCancelEnabled },
    { "movement.slideCancelFrames", CFG_INT, &g_state.slideCancelFrames },

    // Colors
    { "col.enemy",         CFG_COL4,  g_state.colEnemy },
    { "col.bot",           CFG_COL4,  g_state.colBot },
    { "col.occluded",      CFG_COL4,  g_state.colOccluded },
    { "col.name",          CFG_COL4,  g_state.colName },
    { "col.distance",      CFG_COL4,  g_state.colDistance },
    { "col.snapline",      CFG_COL4,  g_state.colSnapline },
    { "col.skeleton",      CFG_COL4,  g_state.colSkeleton },
    { "col.hpHigh",        CFG_COL4,  g_state.colHpHigh },
    { "col.hpMid",         CFG_COL4,  g_state.colHpMid },
    { "col.hpLow",         CFG_COL4,  g_state.colHpLow },
    { "col.armor",         CFG_COL4,  g_state.colArmor },

    // UI
    { "ui.activeTab",      CFG_INT,   &g_state.activeTab },
};
static const size_t g_entryCount = sizeof(g_entries) / sizeof(g_entries[0]);

// ---- path resolution -------------------------------------------------------

// %APPDATA%\CmHax\cm_hax.cfg, optionally creating the folder.
static bool ResolveAppDataPath(char* out, size_t outSize, bool createFolder) {
    char appData[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return false;

    char folder[MAX_PATH];
    if (snprintf(folder, sizeof(folder), "%s\\CmHax", appData) >= (int)sizeof(folder)) return false;

    if (createFolder) {
        DWORD attr = GetFileAttributesA(folder);
        if (attr == INVALID_FILE_ATTRIBUTES) {
            if (!CreateDirectoryA(folder, NULL)) {
                DWORD err = GetLastError();
                if (err != ERROR_ALREADY_EXISTS) {
                    Log("CreateDirectoryA(%s) failed: %lu", folder, err);
                    return false;
                }
            }
        } else if ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            Log("Cfg path conflict: %s exists but isn't a directory", folder);
            return false;
        }
    }

    if (snprintf(out, outSize, "%s\\cm_hax.cfg", folder) >= (int)outSize) return false;
    return true;
}

// Path next to the DLL. Used as a backcompat read source.
static bool ResolveLegacyPath(char* out, size_t outSize) {
    HMODULE self = Runtime::g_module;
    if (!self) return false;
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(self, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return false;

    char* slash = strrchr(buf, '\\');
    if (!slash) return false;
    *(slash + 1) = '\0';

    if (strlen(buf) + strlen("cm_hax.cfg") + 1 > outSize) return false;
    snprintf(out, outSize, "%scm_hax.cfg", buf);
    return true;
}

// Primary write path.
static bool ResolveWritePath(char* out, size_t outSize) {
    if (ResolveAppDataPath(out, outSize, true)) return true;
    return ResolveLegacyPath(out, outSize);
}

// ---- writer ---------------------------------------------------------------

void Save() {
    char path[MAX_PATH];
    if (!ResolveWritePath(path, sizeof(path))) {
        snprintf(g_statusMsg, sizeof(g_statusMsg), "Cfg: path resolve failed");
        g_statusTime = GetTickCount();
        return;
    }

    FILE* f = fopen(path, "w");
    if (!f) {
        snprintf(g_statusMsg, sizeof(g_statusMsg), "Cfg: write failed");
        g_statusTime = GetTickCount();
        Log("SaveConfig: fopen failed for %s", path);
        return;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"_version\": 1,\n");
    for (size_t i = 0; i < g_entryCount; i++) {
        const CfgEntry& e = g_entries[i];
        const char* sep = (i + 1 < g_entryCount) ? "," : "";
        switch (e.type) {
            case CFG_BOOL:  fprintf(f, "  \"%s\": %s%s\n", e.key, *(bool*)e.ptr ? "true" : "false", sep); break;
            case CFG_FLOAT: fprintf(f, "  \"%s\": %.4f%s\n", e.key, *(float*)e.ptr, sep); break;
            case CFG_INT:   fprintf(f, "  \"%s\": %d%s\n", e.key, *(int*)e.ptr, sep); break;
            case CFG_COL4: {
                float* c = (float*)e.ptr;
                fprintf(f, "  \"%s\": [%.4f, %.4f, %.4f, %.4f]%s\n", e.key, c[0], c[1], c[2], c[3], sep);
            } break;
        }
    }
    fprintf(f, "}\n");
    fclose(f);

    snprintf(g_statusMsg, sizeof(g_statusMsg), "Saved %s", path);
    g_statusTime = GetTickCount();
    Log("SaveConfig: wrote %s", path);
}

// ---- parser ---------------------------------------------------------------

static int FindEntry(const char* key, size_t keyLen) {
    for (size_t i = 0; i < g_entryCount; i++) {
        if (strlen(g_entries[i].key) == keyLen
            && strncmp(g_entries[i].key, key, keyLen) == 0) {
            return (int)i;
        }
    }
    return -1;
}

// Tiny JSON-ish parser tailored to our writer. Accepts:
//   "key": true | false | <number> | [<f>, <f>, <f>, <f>]
static void ParseBuffer(char* buf, int* outLoaded) {
    int loaded = 0;
    char* p = buf;
    while (*p) {
        while (*p && *p != '"') p++;
        if (!*p) break;
        p++;
        char* keyStart = p;
        while (*p && *p != '"') p++;
        if (!*p) break;
        size_t keyLen = (size_t)(p - keyStart);
        p++;

        while (*p && (*p == ' ' || *p == '\t' || *p == ':' || *p == '\n' || *p == '\r')) p++;
        if (!*p) break;

        if (keyLen == 0 || (keyLen >= 1 && keyStart[0] == '_')) {
            while (*p && *p != ',' && *p != '\n' && *p != '}') p++;
            continue;
        }

        int idx = FindEntry(keyStart, keyLen);
        if (idx < 0) {
            if (*p == '[') {
                while (*p && *p != ']') p++;
                if (*p) p++;
            } else {
                while (*p && *p != ',' && *p != '\n' && *p != '}') p++;
            }
            continue;
        }

        const CfgEntry& e = g_entries[idx];
        if (e.type == CFG_BOOL) {
            if (strncmp(p, "true", 4) == 0)       { *(bool*)e.ptr = true;  p += 4; loaded++; }
            else if (strncmp(p, "false", 5) == 0) { *(bool*)e.ptr = false; p += 5; loaded++; }
        } else if (e.type == CFG_FLOAT) {
            char* endp = NULL;
            double v = strtod(p, &endp);
            if (endp != p) { *(float*)e.ptr = (float)v; p = endp; loaded++; }
        } else if (e.type == CFG_INT) {
            char* endp = NULL;
            long v = strtol(p, &endp, 10);
            if (endp != p) { *(int*)e.ptr = (int)v; p = endp; loaded++; }
        } else if (e.type == CFG_COL4) {
            float* c = (float*)e.ptr;
            if (*p == '[') {
                p++;
                for (int k = 0; k < 4; k++) {
                    while (*p == ' ' || *p == '\t' || *p == ',') p++;
                    char* endp = NULL;
                    double v = strtod(p, &endp);
                    if (endp == p) break;
                    c[k] = (float)v;
                    p = endp;
                }
                while (*p && *p != ']') p++;
                if (*p) p++;
                loaded++;
            }
        }

        while (*p && *p != ',' && *p != '\n' && *p != '}') p++;
    }
    if (outLoaded) *outLoaded = loaded;
}

static char* ReadFileFully(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > (long)(64 * 1024)) { fclose(f); return NULL; }

    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

void Load() {
    char primaryPath[MAX_PATH] = {};
    char legacyPath[MAX_PATH]  = {};
    bool havePrimary = ResolveAppDataPath(primaryPath, sizeof(primaryPath), false);
    bool haveLegacy  = ResolveLegacyPath(legacyPath, sizeof(legacyPath));

    const char* readFrom = NULL;
    bool migrating = false;

    if (havePrimary && GetFileAttributesA(primaryPath) != INVALID_FILE_ATTRIBUTES) {
        readFrom = primaryPath;
    } else if (haveLegacy && GetFileAttributesA(legacyPath) != INVALID_FILE_ATTRIBUTES) {
        readFrom = legacyPath;
        migrating = true;
    } else {
        return; // First run.
    }

    char* buf = ReadFileFully(readFrom);
    if (!buf) {
        Log("LoadConfig: read failed for %s", readFrom);
        return;
    }

    int loaded = 0;
    ParseBuffer(buf, &loaded);
    free(buf);

    // Sanity clamps after load.
    if (g_state.aimbotSmooth < 1.0f)  g_state.aimbotSmooth = 1.0f;
    if (g_state.aimbotSmooth > 15.0f) g_state.aimbotSmooth = 15.0f;
    if (g_state.aimbotFov    < 25.0f) g_state.aimbotFov = 25.0f;
    if (g_state.aimbotFov    > 600.0f) g_state.aimbotFov = 600.0f;
    if (g_state.activeTab < 0 || g_state.activeTab > 4) g_state.activeTab = 0;
    if (g_state.aimActivationMode < 0 || g_state.aimActivationMode > 1) g_state.aimActivationMode = 1;
    if (g_state.aimActivationKey  < 0 || g_state.aimActivationKey  > 255) g_state.aimActivationKey = 0;

    // Trigger clamps mirror the slider ranges.
    if (g_state.triggerHoldMs        < 5)    g_state.triggerHoldMs = 5;
    if (g_state.triggerHoldMs        > 200)  g_state.triggerHoldMs = 200;
    if (g_state.triggerRefireMs      < 10)   g_state.triggerRefireMs = 10;
    if (g_state.triggerRefireMs      > 1000) g_state.triggerRefireMs = 1000;
    if (g_state.triggerPrecision     < 0)    g_state.triggerPrecision = 0;
    if (g_state.triggerPrecision     > 2)    g_state.triggerPrecision = 2;
    if (g_state.triggerActivationMode < 0 || g_state.triggerActivationMode > 1) g_state.triggerActivationMode = 1;
    if (g_state.triggerActivationKey  < 0 || g_state.triggerActivationKey  > 255) g_state.triggerActivationKey = 0;

    // Radar clamps.
    if (g_state.radarRadius < 40.0f)  g_state.radarRadius = 40.0f;
    if (g_state.radarRadius > 200.0f) g_state.radarRadius = 200.0f;
    if (g_state.radarScale  < 10.0f)  g_state.radarScale  = 10.0f;
    if (g_state.radarScale  > 200.0f) g_state.radarScale  = 200.0f;

    // Movement clamps.
    if (g_state.slideCancelFrames < 3)  g_state.slideCancelFrames = 3;
    if (g_state.slideCancelFrames > 30) g_state.slideCancelFrames = 30;

    // Strip any hitbox bits we no longer expose (older configs may have them).
    g_state.aimbotHitboxMask &= HitboxValidMask();

    Log("LoadConfig: loaded %d keys from %s", loaded, readFrom);

    if (migrating) {
        Save();
        Log("LoadConfig: migrated legacy config to AppData");
        snprintf(g_statusMsg, sizeof(g_statusMsg), "Migrated %d keys to AppData", loaded);
    } else {
        snprintf(g_statusMsg, sizeof(g_statusMsg), "Loaded %d keys", loaded);
    }
    g_statusTime = GetTickCount();
}

} // namespace Config
