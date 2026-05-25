#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string>
#include <vector>
#include <stdarg.h>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "MinHook.h"
#pragma comment(lib, "d3d11.lib")

#if defined(CM_ENABLE_RAW_SDK_INCLUDE)
#include "CombatMaster_SDK_Full.hpp"
#endif

// ============================================================
// IL2CPP & ESP Types
// ============================================================
struct Vector3 { float x, y, z; };
struct Matrix4x4 { float m[4][4]; };
struct Il2CppObject { void* klass; void* monitor; };
struct Il2CppList { Il2CppObject obj; void* _items; int _size; };
struct Il2CppArray { Il2CppObject obj; void* bounds; uintptr_t max_length; };
struct Il2CppString { Il2CppObject obj; int32_t length; wchar_t chars[1]; };

#define OFF_PLAYERROOT_HEALTH      0xB8
#define OFF_PLAYERROOT_ISOWNER     0x108
#define OFF_PLAYERROOT_ISREAL      0x132
#define OFF_PLAYERHEALTH_HP        0xD0
#define OFF_PLAYERHEALTH_ARMOR     0xCC
#define OFF_PLAYERROOT_HEADHITBOX  0x58
#define OFF_PLAYERROOT_THIRDPERSONVIEW 0x88
#define OFF_PLAYERROOT_HITBOXBODY  0x90
#define OFF_PLAYERROOT_CHARACTERCONTROLLER 0xA0
#define OFF_PLAYERROOT_MOVEMENT    0xB0
#define OFF_PLAYERROOT_CACHEDPLAYERDATA 0x150
#define OFF_PLAYERMOBVIEW_HEADTRANSFORM 0x40
#define OFF_PLAYERMOBVIEW_NECKTRANSFORM 0x48
#define OFF_PLAYERMOBVIEW_CHESTTRANSFORM 0x58
#define OFF_PLAYERMOBVIEW_SPINETRANSFORM 0x60
#define OFF_PLAYERMOBVIEW_LKNEETRANSFORM 0x68
#define OFF_PLAYERMOBVIEW_RKNEETRANSFORM 0x70
#define OFF_BOLTHITBOX_CENTER      0x28
#define OFF_BOLTHITBOX_BOXSIZE     0x34
#define OFF_BOLTHITBOXBODY_HITBOXES 0x30
#define OFF_PLAYERMOVEMENT_VELOCITY 0x80
#define OFF_PLAYERMOVEMENT_ISINAIR 0x94
#define OFF_PLAYERMOVEMENT_ISFALLING 0x96
#define OFF_PLAYERMOVEMENT_ISMOVEUPINPUT 0xA5
#define OFF_PLAYERMOVEMENT_GRAVITY  0xBC
#define OFF_PLAYERMOVEMENT_ISGROUNDED 0xC0

// Is*Available RVAs (patched to always return true for unlock-all)
static const uintptr_t g_unlockRVAs[] = {
    0x351E860,  // CharacterStore::IsEmblemAvailable
    0x351E740,  // CharacterStore::IsCallingCardAvailable
    0x351ED10,  // CharacterStore::IsOperatorAvailable
    0x351E9A0,  // CharacterStore::IsOperatorAccessoryAvailable
    0x351F0E0,  // CharacterStore::IsWristbandAvailable
    0x351EE30,  // CharacterStore::IsVehicleAvailable
    0x35172A0,  // ArsenalStore::IsBlueprintAvailable
    0x3518260,  // ArsenalStore::IsCharmAvailable
    0x3517500,  // ArsenalStore::IsCamoAvailable
    0x35183D0,  // ArsenalStore::IsDetailColorAvailable
    0x3518CA0,  // ArsenalStore::IsReticleAvailable
    0x3518F40,  // ArsenalStore::IsWeaponAvailable
    0x35170F0,  // ArsenalStore::IsBattlePerkAvailable
};
#define UNLOCK_PATCH_COUNT (sizeof(g_unlockRVAs) / sizeof(g_unlockRVAs[0]))

struct BytePatch {
    uint8_t* addr;
    uint8_t original[3];
    bool active;
};
static BytePatch g_unlockPatches[UNLOCK_PATCH_COUNT] = {};
static bool g_unlockAllActive = false;

typedef void*  (*t_domain_get)();
typedef void*  (*t_thread_attach)(void* domain);
typedef void*  (*t_class_from_name)(void* image, const char* ns, const char* name);
typedef void** (*t_domain_get_assemblies)(void* domain, size_t* count);
typedef void*  (*t_class_get_method_from_name)(void* klass, const char* name, int argsCount);

static t_domain_get            fn_domain_get;
static t_thread_attach         fn_thread_attach;
static t_class_from_name       fn_class_from_name;
static t_domain_get_assemblies fn_domain_get_assemblies;
static t_class_get_method_from_name fn_class_get_method_from_name;


typedef void    (*t_get_position_injected)(Vector3* out, void* transform, void* methodInfo);
typedef void*   (*t_get_main_camera)(void* methodInfo);
typedef void    (*t_get_worldToCamera)(Matrix4x4* out, void* camera, void* methodInfo);
typedef void    (*t_get_projectionMatrix)(Matrix4x4* out, void* camera, void* methodInfo);
typedef void*   (*t_component_get_transform)(void* component, void* methodInfo);
typedef void    (*t_set_time_scale)(float value);
typedef int     (*t_playerroot_get_teamid)(void* player, void* methodInfo);
typedef bool    (*t_playerhealth_bool)(void* health, void* methodInfo);
typedef void    (*t_WorldToScreenPoint)(Vector3* out, void* camera, Vector3* pos, void* methodInfo);
typedef int     (*t_charactercontroller_move)(void* characterController, Vector3 motion, void* methodInfo);
typedef void*   (*t_netplayerdata_string)(void* netPlayerData, void* methodInfo);
typedef bool    (*t_playerroot_bool)(void* player, void* methodInfo);

static t_get_position_injected fn_get_position;
static t_get_main_camera       fn_get_main_camera;
static t_get_worldToCamera     fn_get_worldToCamera;
static t_get_projectionMatrix  fn_get_projectionMatrix;
static t_component_get_transform fn_component_get_transform;
static t_set_time_scale        fn_set_time_scale;
static t_playerroot_get_teamid fn_playerroot_get_teamid;
static t_playerhealth_bool     fn_playerhealth_get_is_dead;
static t_playerhealth_bool     fn_playerhealth_get_is_downed;
static t_WorldToScreenPoint    fn_WorldToScreenPoint;
static t_charactercontroller_move fn_charactercontroller_move;
static t_netplayerdata_string  fn_battleextensions_nickname;
static t_netplayerdata_string  fn_battleextensions_playfabid;
static t_playerroot_bool       fn_playerroot_get_is_visible;
static void* g_getMainCameraMethodInfo = NULL;
static void* g_getWorldToCameraMethodInfo = NULL;
static void* g_getProjectionMatrixMethodInfo = NULL;
static void* g_componentGetTransformMethodInfo = NULL;
static void* g_getPositionMethodInfo = NULL;
static void* g_playerRootGetTeamIdMethodInfo = NULL;
static void* g_playerHealthIsDeadMethodInfo = NULL;
static void* g_playerHealthIsDownedMethodInfo = NULL;
static void* g_worldToScreenPointMethodInfo = NULL;
static void* g_characterControllerMoveMethodInfo = NULL;
static void* g_battleExtensionsNicknameMethodInfo = NULL;
static void* g_battleExtensionsPlayFabIdMethodInfo = NULL;
static void* g_playerRootGetIsVisibleMethodInfo = NULL;

static HMODULE g_module = NULL;
static HANDLE g_dataThread = NULL;
static volatile LONG g_mainThreadStarted = 0;
static volatile LONG g_running = 1;
static volatile LONG g_unloadRequested = 0;
static volatile LONG g_inPresent = 0;
static volatile LONG g_presentThreadAttached = 0;
static bool g_csInitialized = false;
static HANDLE g_singleInstanceMutex = NULL;
static void* g_staticFields = NULL;
static int g_dbg_pR = 0, g_dbg_pHP = 0, g_dbg_pTr = 0, g_dbg_pW2S = 0;
static void* g_dbg_w2s_ptr = NULL;
static bool g_noRecoil = false;
static bool g_noSpread = false;
static bool g_unlockAll = false;
static void* g_gameClientStaticFields = NULL;
static char g_saveStatusMsg[128] = "";
static DWORD g_saveStatusTime = 0;
static bool g_espBox = true;
static bool g_espHealth = true;
static bool g_espArmor = true;
static bool g_espLines = true;
static bool g_espNames = true;
static bool g_espDistance = true;
static bool g_visibleCheck = true;
static bool g_teamCheck = true;
static int g_dbg_deadFiltered = 0, g_dbg_downedFiltered = 0, g_dbg_noHead = 0, g_dbg_visible = 0, g_dbg_occluded = 0;
static bool g_aimbotEnabled = true;
static bool g_drawAimbotFov = true;
static float g_aimbotFov = 250.0f;
static float g_aimbotSmooth = 3.0f;
static bool g_aimbotLockActive = false;
static bool g_aimbotTargetActive = false;
static float g_aimbotStableX = 0.0f;
static float g_aimbotStableY = 0.0f;
static float g_aimbotTargetX = 0.0f;
static float g_aimbotTargetY = 0.0f;
static float g_aimbotResidualX = 0.0f;
static float g_aimbotResidualY = 0.0f;

void ResetLog() {
    FILE* f = fopen("C:\\Users\\NullSyntax\\Desktop\\esp_log.txt", "w");
    if (!f) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "==== New imgui_esp session PID %lu at %04u-%02u-%02u %02u:%02u:%02u ====\n",
        GetCurrentProcessId(), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    fclose(f);
}

void Log(const char* format, ...) {
    FILE* f = fopen("C:\\Users\\NullSyntax\\Desktop\\esp_log.txt", "a");
    if (!f) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%02u:%02u:%02u.%03u pid:%lu tid:%lu] ",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        GetCurrentProcessId(), GetCurrentThreadId());
    
    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);
    fprintf(f, "\n");
    fclose(f);
}

bool g_espEnabled = false;
static bool g_showMenu = true;

float g_dbg_posX = 0.0f, g_dbg_posY = 0.0f, g_dbg_posZ = 0.0f;
float g_dbg_sx = 0.0f, g_dbg_sy = 0.0f, g_dbg_boxH = 0.0f;
void* g_dbg_transform = NULL;
int g_dbg_hpVal = 0;

struct PlayerData {
    bool valid;
    bool isReal;
    int hp;
    int armor;
    bool hasHead;
    bool visible;
    float distance;
    char name[48];
    Vector3 rootPos;
    Vector3 headPos;
    Vector3 boxPoints[64];
    int boxPointCount;
};

bool IsPlausiblePtr(void* ptr) {
    uintptr_t value = (uintptr_t)ptr;
    return value >= 0x10000 && value < 0x0000800000000000;
}

bool TryGetTransformPosition(void* transform, Vector3& out) {
    if (!IsPlausiblePtr(transform) || !fn_get_position) return false;

    __try {
        fn_get_position(&out, transform, g_getPositionMethodInfo);
        return isfinite(out.x) && isfinite(out.y) && isfinite(out.z);
    } __except(1) {
        return false;
    }
}

void EnsurePresentThreadAttached() {
    if (InterlockedCompareExchange(&g_presentThreadAttached, 1, 0) == 0) {
        if (fn_thread_attach && fn_domain_get) {
            __try {
                fn_thread_attach(fn_domain_get());
            } __except(1) {}
        }
    }
}

bool TryGetComponentPosition(void* component, Vector3& out) {
    if (!IsPlausiblePtr(component) || !fn_component_get_transform) return false;

    __try {
        void* transform = fn_component_get_transform(component, g_componentGetTransformMethodInfo);
        return TryGetTransformPosition(transform, out);
    } __except(1) {
        return false;
    }
}

bool TryGetPlayerTeamId(void* player, int& outTeamId) {
    if (!IsPlausiblePtr(player) || !fn_playerroot_get_teamid || !g_playerRootGetTeamIdMethodInfo) return false;

    __try {
        outTeamId = fn_playerroot_get_teamid(player, g_playerRootGetTeamIdMethodInfo);
        return outTeamId >= -1 && outTeamId < 128;
    } __except(1) {
        return false;
    }
}

bool CopyIl2CppStringUtf8(void* stringObj, char* out, size_t outSize) {
    if (!out || outSize == 0) return false;
    out[0] = '\0';
    if (!IsPlausiblePtr(stringObj)) return false;

    __try {
        Il2CppString* s = (Il2CppString*)stringObj;
        if (s->length <= 0 || s->length > 64) return false;

        int written = WideCharToMultiByte(CP_UTF8, 0, s->chars, s->length, out, (int)outSize - 1, NULL, NULL);
        if (written <= 0) return false;

        out[written] = '\0';
        return true;
    } __except(1) {
        out[0] = '\0';
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

        if (fn_battleextensions_nickname && g_battleExtensionsNicknameMethodInfo) {
            void* nickname = fn_battleextensions_nickname(netPlayerData, g_battleExtensionsNicknameMethodInfo);
            if (CopyIl2CppStringUtf8(nickname, out, outSize)) return true;
        }

        if (fn_battleextensions_playfabid && g_battleExtensionsPlayFabIdMethodInfo) {
            void* playfabId = fn_battleextensions_playfabid(netPlayerData, g_battleExtensionsPlayFabIdMethodInfo);
            if (CopyIl2CppStringUtf8(playfabId, out, outSize)) return true;
        }
    } __except(1) {
        out[0] = '\0';
    }

    return false;
}

float Distance3D(Vector3 a, Vector3 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

bool TryGetPlayerVisibleState(void* targetPlayer, bool& visible) {
    visible = true;
    if (!g_visibleCheck) return true;
    if (!IsPlausiblePtr(targetPlayer)) return false;

    __try {
        if (fn_playerroot_get_is_visible && g_playerRootGetIsVisibleMethodInfo) {
            visible = fn_playerroot_get_is_visible(targetPlayer, g_playerRootGetIsVisibleMethodInfo);
            return true;
        }

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
        if (fn_playerhealth_get_is_dead && g_playerHealthIsDeadMethodInfo) {
            isDead = fn_playerhealth_get_is_dead(healthComp, g_playerHealthIsDeadMethodInfo);
        }
        if (fn_playerhealth_get_is_downed && g_playerHealthIsDownedMethodInfo) {
            isDowned = fn_playerhealth_get_is_downed(healthComp, g_playerHealthIsDownedMethodInfo);
        }
        return true;
    } __except(1) {
        return false;
    }
}


void ApplyUnlockAll() {
    if (g_unlockAllActive) return;
    HMODULE hProject = GetModuleHandleA("Project.dll");
    if (!hProject) return;
    uintptr_t base = (uintptr_t)hProject;
    uint8_t patch[] = { 0xB0, 0x01, 0xC3 }; // mov al, 1; ret
    int patched = 0;
    for (int i = 0; i < (int)UNLOCK_PATCH_COUNT; i++) {
        uint8_t* addr = (uint8_t*)(base + g_unlockRVAs[i]);
        g_unlockPatches[i].addr = addr;
        g_unlockPatches[i].active = false;
        DWORD oldProtect;
        if (VirtualProtect(addr, 3, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(g_unlockPatches[i].original, addr, 3);
            memcpy(addr, patch, 3);
            VirtualProtect(addr, 3, oldProtect, &oldProtect);
            g_unlockPatches[i].active = true;
            patched++;
        }
    }
    g_unlockAllActive = true;
    Log("UnlockAll: patched %d/%d Is*Available functions", patched, (int)UNLOCK_PATCH_COUNT);
}

void RestoreUnlockAll() {
    if (!g_unlockAllActive) return;
    int restored = 0;
    for (int i = 0; i < (int)UNLOCK_PATCH_COUNT; i++) {
        if (!g_unlockPatches[i].active || !g_unlockPatches[i].addr) continue;
        DWORD oldProtect;
        if (VirtualProtect(g_unlockPatches[i].addr, 3, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(g_unlockPatches[i].addr, g_unlockPatches[i].original, 3);
            VirtualProtect(g_unlockPatches[i].addr, 3, oldProtect, &oldProtect);
            g_unlockPatches[i].active = false;
            restored++;
        }
    }
    g_unlockAllActive = false;
    Log("UnlockAll: restored %d functions", restored);
}

void AddBoxPoint(PlayerData& p, Vector3 point) {
    if (p.boxPointCount >= 64) return;
    if (!isfinite(point.x) || !isfinite(point.y) || !isfinite(point.z)) return;
    p.boxPoints[p.boxPointCount++] = point;
}

bool IsValidHeadPosition(Vector3 rootPos, Vector3 headPos) {
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

void AddHitboxBounds(PlayerData& p, void* hitbox) {
    if (!IsPlausiblePtr(hitbox)) return;

    __try {
        Vector3 base;
        if (!TryGetComponentPosition(hitbox, base)) return;

        Vector3 center = *(Vector3*)((uint8_t*)hitbox + OFF_BOLTHITBOX_CENTER);
        Vector3 size = *(Vector3*)((uint8_t*)hitbox + OFF_BOLTHITBOX_BOXSIZE);
        if (!isfinite(center.x) || !isfinite(center.y) || !isfinite(center.z)) return;
        if (!isfinite(size.x) || !isfinite(size.y) || !isfinite(size.z)) return;

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

        if (p.boxPointCount >= 8) return;

        void* mobView = *(void**)((uint8_t*)player + OFF_PLAYERROOT_THIRDPERSONVIEW);
        if (IsPlausiblePtr(mobView)) {
            const int offsets[] = {
                OFF_PLAYERMOBVIEW_HEADTRANSFORM,
                OFF_PLAYERMOBVIEW_NECKTRANSFORM,
                OFF_PLAYERMOBVIEW_CHESTTRANSFORM,
                OFF_PLAYERMOBVIEW_SPINETRANSFORM,
                OFF_PLAYERMOBVIEW_LKNEETRANSFORM,
                OFF_PLAYERMOBVIEW_RKNEETRANSFORM
            };
            for (int i = 0; i < (int)(sizeof(offsets) / sizeof(offsets[0])); i++) {
                Vector3 point;
                void* transform = *(void**)((uint8_t*)mobView + offsets[i]);
                if (TryGetTransformPosition(transform, point)) {
                    AddBoxPoint(p, point);
                }
            }
        }
    } __except(1) {}

    AddBoxPoint(p, p.rootPos);
    AddBoxPoint(p, p.headPos);
}

// ============================================================
// W2S
// ============================================================
static inline float UnityMatrixGet(const Matrix4x4* matrix, int row, int col) {
    return matrix->m[col][row];
}

bool WorldToScreen(Vector3 world, Matrix4x4* view, Matrix4x4* proj, int screenW, int screenH, float& outX, float& outY) {
    float cx = UnityMatrixGet(view, 0, 0)*world.x + UnityMatrixGet(view, 0, 1)*world.y + UnityMatrixGet(view, 0, 2)*world.z + UnityMatrixGet(view, 0, 3);
    float cy = UnityMatrixGet(view, 1, 0)*world.x + UnityMatrixGet(view, 1, 1)*world.y + UnityMatrixGet(view, 1, 2)*world.z + UnityMatrixGet(view, 1, 3);
    float cz = UnityMatrixGet(view, 2, 0)*world.x + UnityMatrixGet(view, 2, 1)*world.y + UnityMatrixGet(view, 2, 2)*world.z + UnityMatrixGet(view, 2, 3);
    float cw = UnityMatrixGet(view, 3, 0)*world.x + UnityMatrixGet(view, 3, 1)*world.y + UnityMatrixGet(view, 3, 2)*world.z + UnityMatrixGet(view, 3, 3);

    float px = UnityMatrixGet(proj, 0, 0)*cx + UnityMatrixGet(proj, 0, 1)*cy + UnityMatrixGet(proj, 0, 2)*cz + UnityMatrixGet(proj, 0, 3)*cw;
    float py = UnityMatrixGet(proj, 1, 0)*cx + UnityMatrixGet(proj, 1, 1)*cy + UnityMatrixGet(proj, 1, 2)*cz + UnityMatrixGet(proj, 1, 3)*cw;
    float pw = UnityMatrixGet(proj, 3, 0)*cx + UnityMatrixGet(proj, 3, 1)*cy + UnityMatrixGet(proj, 3, 2)*cz + UnityMatrixGet(proj, 3, 3)*cw;

    if (pw <= 0.001f) return false;

    float ndcX = px / pw;
    float ndcY = py / pw;

    outX = (ndcX + 1.0f) * 0.5f * screenW;
    outY = (1.0f - ndcY) * 0.5f * screenH;

    return true;
}

float ClampFloat(float value, float minValue, float maxValue);

bool WorldToScreenUnity(void* camera, Vector3 world, Matrix4x4* view, Matrix4x4* proj, int screenW, int screenH, float& outX, float& outY) {
    if (!isfinite(world.x) || !isfinite(world.y) || !isfinite(world.z)) return false;

    if (IsPlausiblePtr(camera) && fn_WorldToScreenPoint && g_worldToScreenPointMethodInfo) {
        Vector3 screen;
        __try {
            fn_WorldToScreenPoint(&screen, camera, &world, g_worldToScreenPointMethodInfo);
        } __except(1) {
            screen = { 0.0f, 0.0f, -1.0f };
        }

        if (isfinite(screen.x) && isfinite(screen.y) && isfinite(screen.z) && screen.z > 0.001f) {
            outX = screen.x;
            outY = (float)screenH - screen.y;
            return isfinite(outX) && isfinite(outY);
        }
    }

    return WorldToScreen(world, view, proj, screenW, screenH, outX, outY);
}

void DrawShadowText(ImDrawList* drawList, ImVec2 pos, ImU32 color, const char* text) {
    drawList->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 220), text);
    drawList->AddText(pos, color, text);
}

void DrawCornerBox(ImDrawList* drawList, float x, float y, float w, float h, ImU32 color) {
    float lineW = ClampFloat(w * 0.28f, 7.0f, 18.0f);
    float lineH = ClampFloat(h * 0.22f, 9.0f, 24.0f);
    float thickness = 1.8f;
    ImU32 outline = IM_COL32(0, 0, 0, 210);

    drawList->AddRect(ImVec2(x - 1.0f, y - 1.0f), ImVec2(x + w + 1.0f, y + h + 1.0f), outline, 2.0f, 0, 1.0f);

    drawList->AddLine(ImVec2(x, y), ImVec2(x + lineW, y), color, thickness);
    drawList->AddLine(ImVec2(x, y), ImVec2(x, y + lineH), color, thickness);
    drawList->AddLine(ImVec2(x + w, y), ImVec2(x + w - lineW, y), color, thickness);
    drawList->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + lineH), color, thickness);
    drawList->AddLine(ImVec2(x, y + h), ImVec2(x + lineW, y + h), color, thickness);
    drawList->AddLine(ImVec2(x, y + h), ImVec2(x, y + h - lineH), color, thickness);
    drawList->AddLine(ImVec2(x + w, y + h), ImVec2(x + w - lineW, y + h), color, thickness);
    drawList->AddLine(ImVec2(x + w, y + h), ImVec2(x + w, y + h - lineH), color, thickness);
}

void DrawVerticalBar(ImDrawList* drawList, float x, float y, float h, float pct, ImU32 fillColor) {
    pct = ClampFloat(pct, 0.0f, 1.0f);
    float w = 3.0f;
    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(8, 10, 14, 190), 1.0f);
    drawList->AddRect(ImVec2(x - 1.0f, y - 1.0f), ImVec2(x + w + 1.0f, y + h + 1.0f), IM_COL32(0, 0, 0, 210), 1.0f);
    float fillH = h * pct;
    drawList->AddRectFilled(ImVec2(x, y + h - fillH), ImVec2(x + w, y + h), fillColor, 1.0f);
}

void ApplyMenuStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowPadding = ImVec2(12.0f, 10.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 7.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 5.0f);
    style.ScrollbarSize = 12.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.92f, 0.95f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.50f, 0.56f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.065f, 0.085f, 0.96f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.08f, 0.10f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.20f, 0.26f, 0.32f, 0.75f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.20f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.28f, 0.36f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.11f, 0.14f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.22f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.22f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.55f, 0.86f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.14f, 0.18f, 0.23f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.30f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.26f, 0.42f, 0.55f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.13f, 0.18f, 0.23f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.27f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.22f, 0.34f, 0.45f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.10f, 0.13f, 0.17f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.20f, 0.34f, 0.46f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.16f, 0.24f, 0.31f, 1.00f);
}

float ClampFloat(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

void ResetAimbotState() {
    g_aimbotLockActive = false;
    g_aimbotTargetActive = false;
    g_aimbotResidualX = 0.0f;
    g_aimbotResidualY = 0.0f;
}

LONG RoundMouseDelta(float value) {
    if (value > 0.0f) return (LONG)(value + 0.5f);
    if (value < 0.0f) return (LONG)(value - 0.5f);
    return 0;
}

void ApplyAimbot(float targetX, float targetY, float centerX, float centerY) {
    if ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) == 0) {
        ResetAimbotState();
        return;
    }

    float smooth = ClampFloat(g_aimbotSmooth, 1.0f, 15.0f);

    if (!g_aimbotLockActive) {
        g_aimbotStableX = targetX;
        g_aimbotStableY = targetY;
        g_aimbotLockActive = true;
    } else {
        float jumpX = targetX - g_aimbotStableX;
        float jumpY = targetY - g_aimbotStableY;
        float jumpDist = sqrtf(jumpX * jumpX + jumpY * jumpY);
        float resetDistance = ClampFloat(g_aimbotFov * 0.45f, 55.0f, 180.0f);

        if (jumpDist > resetDistance) {
            g_aimbotStableX = targetX;
            g_aimbotStableY = targetY;
            g_aimbotResidualX = 0.0f;
            g_aimbotResidualY = 0.0f;
        } else {
            float targetAlpha = ClampFloat(0.70f / smooth, 0.16f, 0.70f);
            g_aimbotStableX += jumpX * targetAlpha;
            g_aimbotStableY += jumpY * targetAlpha;
        }
    }

    float dx = g_aimbotStableX - centerX;
    float dy = g_aimbotStableY - centerY;

    if (fabsf(dx) < 0.75f && fabsf(dy) < 0.75f) {
        g_aimbotResidualX = 0.0f;
        g_aimbotResidualY = 0.0f;
        return;
    }

    float distance = sqrtf(dx * dx + dy * dy);
    float gain = ClampFloat(0.34f / smooth, 0.045f, 0.34f);
    float maxStep = ClampFloat(14.0f + distance * 0.12f, 24.0f, 72.0f);

    float stepX = dx * gain;
    float stepY = dy * gain;

    stepX = ClampFloat(stepX, -maxStep, maxStep);
    stepY = ClampFloat(stepY, -maxStep, maxStep);

    if (fabsf(stepX) > fabsf(dx)) stepX = dx;
    if (fabsf(stepY) > fabsf(dy)) stepY = dy;

    g_aimbotResidualX += stepX;
    g_aimbotResidualY += stepY;

    LONG moveX = RoundMouseDelta(g_aimbotResidualX);
    LONG moveY = RoundMouseDelta(g_aimbotResidualY);

    if ((dx > 0.0f && moveX < 0) || (dx < 0.0f && moveX > 0) || fabsf((float)moveX) > fabsf(dx)) moveX = 0;
    if ((dy > 0.0f && moveY < 0) || (dy < 0.0f && moveY > 0) || fabsf((float)moveY) > fabsf(dy)) moveY = 0;

    if (moveX == 0 && moveY == 0) return;

    g_aimbotResidualX -= (float)moveX;
    g_aimbotResidualY -= (float)moveY;
    mouse_event(MOUSEEVENTF_MOVE, (DWORD)moveX, (DWORD)moveY, 0, 0);
}

// ============================================================
// D3D11 Hooks & ImGui
// ============================================================
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef HRESULT(__stdcall* ResizeBuffers_t)(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

Present_t oPresent;
ResizeBuffers_t oResizeBuffers;

ID3D11Device* pDevice = NULL;
ID3D11DeviceContext* pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView = NULL;
HWND window = NULL;
WNDPROC oWndProc;
bool init = false;

struct SharedESPData {
    Matrix4x4 vMatrix;
    Matrix4x4 pMatrix;
    void* camera;
    PlayerData players[64];
    int playerCount;
    int pR, pHP, pTr, pW2S;
};

SharedESPData g_espData;
CRITICAL_SECTION g_espCs;

void RequestUnload();

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && wParam == VK_END) {
        RequestUnload();
        return true;
    }

    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT) {
        g_showMenu = !g_showMenu;
        return true;
    }

    if (g_showMenu && ImGui::GetCurrentContext() != NULL) {
        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        ImGuiIO& io = ImGui::GetIO();
        
        // Block mouse messages from game so we can click ImGui safely
        if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) return true;
        
        // Block keyboard messages if ImGui wants them
        if (io.WantCaptureKeyboard && uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST) return true;
    }

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

DWORD WINAPI ESPDataThread(LPVOID lpReserved) {
    if (fn_thread_attach && fn_domain_get) {
        fn_thread_attach(fn_domain_get());
    }
    Log("ESPDataThread running; ESP starts disabled until enabled in menu");

    while (InterlockedCompareExchange(&g_running, 1, 1) != 0) {
        Sleep(16); // ~60fps data collection

        if (!g_espEnabled || !g_staticFields || !fn_get_main_camera || !fn_get_position || !fn_get_worldToCamera || !fn_get_projectionMatrix || !fn_component_get_transform) {
            continue;
        }

        SharedESPData newData;
        memset(&newData, 0, sizeof(newData));

        void* allPlayersPtr = *(void**)((uint8_t*)g_staticFields + 0x18);
        if (!allPlayersPtr) continue;
        Il2CppList* allPlayers = (Il2CppList*)allPlayersPtr;
        if (!allPlayers->_items || allPlayers->_size <= 0) continue;

        void** itemsArray = (void**)((uint8_t*)allPlayers->_items + 0x20);
        void* myPlayer = *(void**)((uint8_t*)g_staticFields + 0x8);
        int myTeamId = -999;
        bool haveMyTeam = TryGetPlayerTeamId(myPlayer, myTeamId);
        Vector3 myPos = {};
        bool haveMyPos = TryGetComponentPosition(myPlayer, myPos);

        if (myPlayer && (g_noRecoil || g_noSpread)) {
            __try {
                void* cameraController = *(void**)((uint8_t*)myPlayer + 0x48); // _cameraController
                if (cameraController) {
                    void* recoilWeaponInfo = *(void**)((uint8_t*)cameraController + 0x188); // _recoilWeaponInfo
                    if (recoilWeaponInfo) {
                        if (g_noRecoil) {
                            *(float*)((uint8_t*)recoilWeaponInfo + 0x60) = 0.0f; // RecoilKickPower
                            *(float*)((uint8_t*)recoilWeaponInfo + 0x7C) = 0.0f; // CamRecoilPowerRange.x
                            *(float*)((uint8_t*)recoilWeaponInfo + 0x80) = 0.0f; // CamRecoilPowerRange.y
                            *(float*)((uint8_t*)cameraController + 0x194) = 0.0f; // _recoilPower
                            *(float*)((uint8_t*)cameraController + 0x198) = 0.0f; // _recoilAnimValue
                        }
                        if (g_noSpread) {
                            *(float*)((uint8_t*)recoilWeaponInfo + 0x34) = 0.0f; // SpreadVerMinMax.x
                            *(float*)((uint8_t*)recoilWeaponInfo + 0x38) = 0.0f; // SpreadVerMinMax.y
                            *(float*)((uint8_t*)recoilWeaponInfo + 0x3C) = 0.0f; // SpreadHorMinMax.x
                            *(float*)((uint8_t*)recoilWeaponInfo + 0x40) = 0.0f; // SpreadHorMinMax.y
                        }
                    }
                }
            } __except(1) {}
        }

        void* camera = NULL;
        __try {
            camera = fn_get_main_camera(g_getMainCameraMethodInfo);
            if (!camera) continue;
            newData.camera = camera;

            fn_get_worldToCamera(&newData.vMatrix, camera, g_getWorldToCameraMethodInfo);
            fn_get_projectionMatrix(&newData.pMatrix, camera, g_getProjectionMatrixMethodInfo);
        } __except(1) {
            Log("Camera matrix collection faulted; disabling ESP");
            g_espEnabled = false;
            continue;
        }

        int pR = 0, pHP = 0, pTr = 0, pW2S = 0;
        int deadFiltered = 0, downedFiltered = 0, noHead = 0;
        int visibleCount = 0, occludedCount = 0;

        for (int i = 0; i < allPlayers->_size && i < 64; i++) {
            __try {
                void* player = itemsArray[i];
                if (!IsPlausiblePtr(player) || player == myPlayer) continue;

                if (g_teamCheck && haveMyTeam) {
                    int playerTeamId = -999;
                    if (TryGetPlayerTeamId(player, playerTeamId) && playerTeamId == myTeamId) {
                        continue;
                    }
                }

                bool isReal = *(bool*)((uint8_t*)player + OFF_PLAYERROOT_ISREAL);
                pR++;

                void* healthComp = *(void**)((uint8_t*)player + OFF_PLAYERROOT_HEALTH);
                int hp = 0, armor = 0;
                if (!IsPlausiblePtr(healthComp)) continue;

                hp = *(int*)((uint8_t*)healthComp + OFF_PLAYERHEALTH_HP);
                armor = *(int*)((uint8_t*)healthComp + OFF_PLAYERHEALTH_ARMOR);
                pHP++;

                bool isDead = false, isDowned = false;
                TryGetPlayerDeathState(healthComp, isDead, isDowned);
                if (isDead || hp <= 0) {
                    deadFiltered++;
                    continue;
                }
                if (isDowned) {
                    downedFiltered++;
                    continue;
                }

                Vector3 pos;
                if (!TryGetComponentPosition(player, pos)) continue;
                pTr++;

                Vector3 headPos;
                bool hasHead = TryGetPlayerHeadPosition(player, pos, headPos);
                if (!hasHead) {
                    headPos = pos;
                    noHead++;
                }

                PlayerData& outPlayer = newData.players[newData.playerCount];
                outPlayer.valid = true;
                outPlayer.isReal = isReal;
                outPlayer.hp = hp;
                outPlayer.armor = armor;
                outPlayer.hasHead = hasHead;
                outPlayer.distance = haveMyPos ? Distance3D(myPos, pos) : 0.0f;
                bool hasLos = true;
                if (TryGetPlayerVisibleState(player, hasLos)) {
                    if (hasLos) visibleCount++;
                    else occludedCount++;
                }
                outPlayer.visible = hasLos;
                if (!TryGetPlayerName(player, outPlayer.name, sizeof(outPlayer.name))) {
                    snprintf(outPlayer.name, sizeof(outPlayer.name), "%s", isReal ? "Player" : "Bot");
                }
                outPlayer.rootPos = pos;
                outPlayer.headPos = headPos;
                outPlayer.boxPointCount = 0;
                AddPlayerHitboxPoints(player, outPlayer);
                if (outPlayer.boxPointCount < 2) continue;

                newData.playerCount++;
                pW2S++; // Track successfully processed players
            } __except(1) {}
        }

        newData.pR = pR;
        newData.pHP = pHP;
        newData.pTr = pTr;
        newData.pW2S = pW2S;
        g_dbg_deadFiltered = deadFiltered;
        g_dbg_downedFiltered = downedFiltered;
        g_dbg_noHead = noHead;
        g_dbg_visible = visibleCount;
        g_dbg_occluded = occludedCount;

        EnterCriticalSection(&g_espCs);
        g_espData = newData;
        LeaveCriticalSection(&g_espCs);
    }
    Log("ESPDataThread exiting");
    return 0;
}

void DrawESP() {
    if (!g_espEnabled) return;
    EnsurePresentThreadAttached();

    SharedESPData localData;
    EnterCriticalSection(&g_espCs);
    localData = g_espData;
    LeaveCriticalSection(&g_espCs);

    if (localData.playerCount == 0) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    int screenW = (int)ImGui::GetIO().DisplaySize.x;
    int screenH = (int)ImGui::GetIO().DisplaySize.y;

    float bestFov = 99999.0f;
    float bestTargetX = 0.0f;
    float bestTargetY = 0.0f;
    bool foundTarget = false;
    float lockedBestDrift = 99999.0f;
    float lockedTargetX = 0.0f;
    float lockedTargetY = 0.0f;
    bool foundLockedTarget = false;
    bool aimKeyDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

    for (int i = 0; i < localData.playerCount; i++) {
        PlayerData& p = localData.players[i];
        if (!p.valid) continue;
        if (p.hp <= 0) continue;

        float minX = 100000.0f, minY = 100000.0f;
        float maxX = -100000.0f, maxY = -100000.0f;
        int projectedPoints = 0;

        for (int b = 0; b < p.boxPointCount && b < 64; b++) {
            float px, py;
            if (WorldToScreenUnity(localData.camera, p.boxPoints[b], &localData.vMatrix, &localData.pMatrix, screenW, screenH, px, py)) {
                if (px < minX) minX = px;
                if (px > maxX) maxX = px;
                if (py < minY) minY = py;
                if (py > maxY) maxY = py;
                projectedPoints++;
            }
        }

        if (projectedPoints < 2) continue;

        float boxX = minX - 4.0f;
        float boxY = minY - 4.0f;
        float boxW = (maxX - minX) + 8.0f;
        float boxH = (maxY - minY) + 8.0f;
        if (boxW < 8.0f || boxH < 12.0f) continue;

        ImU32 accentColor = p.visible
            ? (p.isReal ? IM_COL32(255, 72, 72, 245) : IM_COL32(255, 178, 66, 235))
            : IM_COL32(135, 145, 165, 185);

        if (g_espBox) {
            DrawCornerBox(drawList, boxX, boxY, boxW, boxH, accentColor);
        }

        if (g_espNames && p.name[0]) {
            ImVec2 textSize = ImGui::CalcTextSize(p.name);
            float textX = boxX + (boxW - textSize.x) * 0.5f;
            float textY = boxY - textSize.y - 5.0f;
            if (textY < 2.0f) textY = boxY + 2.0f;

            ImVec2 textPos(textX, textY);
            DrawShadowText(drawList, textPos, p.visible ? IM_COL32(245, 248, 255, 245) : IM_COL32(175, 185, 200, 210), p.name);
        }

        if (g_espHealth && p.hp > 0) {
            float hpPct = p.hp / 100.0f;
            if (hpPct > 1.0f) hpPct = 1.0f;
            ImU32 hpColor = hpPct > 0.55f ? IM_COL32(80, 235, 120, 245) : (hpPct > 0.25f ? IM_COL32(245, 205, 65, 245) : IM_COL32(255, 80, 80, 245));
            DrawVerticalBar(drawList, boxX - 7.0f, boxY, boxH, hpPct, hpColor);
        }

        if (g_espArmor && p.armor > 0) {
            float arPct = p.armor / 100.0f;
            if (arPct > 1.0f) arPct = 1.0f;
            DrawVerticalBar(drawList, boxX + boxW + 4.0f, boxY, boxH, arPct, IM_COL32(70, 165, 255, 245));
        }

        if (g_espLines) {
            ImU32 lineColor = p.visible ? IM_COL32(210, 225, 245, 105) : IM_COL32(135, 145, 165, 70);
            drawList->AddLine(ImVec2((float)screenW * 0.5f, (float)screenH - 2.0f), ImVec2(boxX + boxW * 0.5f, boxY + boxH), lineColor, 1.0f);
        }

        float labelY = boxY + boxH + 4.0f;
        if (g_espDistance && p.distance > 0.01f) {
            char distanceText[32];
            snprintf(distanceText, sizeof(distanceText), "%.0fm", p.distance);
            ImVec2 textSize = ImGui::CalcTextSize(distanceText);
            DrawShadowText(drawList, ImVec2(boxX + (boxW - textSize.x) * 0.5f, labelY), IM_COL32(185, 205, 230, 215), distanceText);
        }

        float headSX = 0.0f, headSY = 0.0f;
        if (p.hasHead && WorldToScreenUnity(localData.camera, p.headPos, &localData.vMatrix, &localData.pMatrix, screenW, screenH, headSX, headSY)) {
            float distToCrosshairX = headSX - (screenW / 2.0f);
            float distToCrosshairY = headSY - (screenH / 2.0f);
            float fovDist = sqrtf(distToCrosshairX*distToCrosshairX + distToCrosshairY*distToCrosshairY);
            
            if (g_aimbotEnabled && fovDist < g_aimbotFov && (!g_visibleCheck || p.visible)) {
                if (aimKeyDown && g_aimbotTargetActive) {
                    float driftX = headSX - g_aimbotTargetX;
                    float driftY = headSY - g_aimbotTargetY;
                    float drift = sqrtf(driftX * driftX + driftY * driftY);
                    float lockRadius = ClampFloat(g_aimbotFov * 0.35f, 45.0f, 130.0f);
                    if (drift < lockRadius && drift < lockedBestDrift) {
                        lockedBestDrift = drift;
                        lockedTargetX = headSX;
                        lockedTargetY = headSY;
                        foundLockedTarget = true;
                    }
                }

                if (fovDist < bestFov) {
                    bestFov = fovDist;
                    bestTargetX = headSX;
                    bestTargetY = headSY;
                    foundTarget = true;
                }
            }
        }
    }

    if (g_aimbotEnabled && g_drawAimbotFov) {
        ImVec2 center((float)screenW * 0.5f, (float)screenH * 0.5f);
        drawList->AddCircle(center, g_aimbotFov, IM_COL32(255, 255, 255, 95), 64, 1.2f);
    }

    if (foundLockedTarget) {
        bestTargetX = lockedTargetX;
        bestTargetY = lockedTargetY;
        foundTarget = true;
    }

    if (g_aimbotEnabled && foundTarget && aimKeyDown) {
        g_aimbotTargetActive = true;
        g_aimbotTargetX = bestTargetX;
        g_aimbotTargetY = bestTargetY;
        ApplyAimbot(bestTargetX, bestTargetY, (float)screenW * 0.5f, (float)screenH * 0.5f);
    } else {
        ResetAimbotState();
    }

    g_dbg_pR = localData.pR;
    g_dbg_pHP = localData.pHP;
    g_dbg_pTr = localData.pTr;
    g_dbg_pW2S = localData.pW2S;
}

void CleanupResources() {
    Log("Unload cleanup started");

    g_espEnabled = false;
    g_noRecoil = false;
    g_noSpread = false;
    RestoreUnlockAll();
    g_unlockAll = false;
    InterlockedExchange(&g_running, 0);

    if (g_dataThread) {
        WaitForSingleObject(g_dataThread, 1500);
        CloseHandle(g_dataThread);
        g_dataThread = NULL;
    }

    DWORD start = GetTickCount();
    while (InterlockedCompareExchange(&g_inPresent, 0, 0) != 0 && GetTickCount() - start < 2000) {
        Sleep(10);
    }

    MH_DisableHook(MH_ALL_HOOKS);
    Sleep(100);

    if (window && oWndProc) {
        SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)oWndProc);
        oWndProc = NULL;
    }

    if (init && ImGui::GetCurrentContext() != NULL) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        init = false;
    }

    if (mainRenderTargetView) {
        mainRenderTargetView->Release();
        mainRenderTargetView = NULL;
    }
    if (pContext) {
        pContext->Release();
        pContext = NULL;
    }
    if (pDevice) {
        pDevice->Release();
        pDevice = NULL;
    }

    MH_RemoveHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if (g_csInitialized) {
        DeleteCriticalSection(&g_espCs);
        g_csInitialized = false;
    }

    if (g_singleInstanceMutex) {
        ReleaseMutex(g_singleInstanceMutex);
        CloseHandle(g_singleInstanceMutex);
        g_singleInstanceMutex = NULL;
    }

    Log("Unload cleanup finished");
}

DWORD WINAPI UnloadThread(LPVOID lpReserved) {
    CleanupResources();
    HMODULE module = g_module;
    if (module) {
        FreeLibraryAndExitThread(module, 0);
    }
    return 0;
}

void RequestUnload() {
    if (InterlockedCompareExchange(&g_unloadRequested, 1, 0) != 0) return;

    Log("Unload requested by END key/menu");
    g_showMenu = false;
    g_espEnabled = false;

    HANDLE hThread = CreateThread(0, 0, UnloadThread, 0, 0, 0);
    if (hThread) CloseHandle(hThread);
}

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (InterlockedCompareExchange(&g_unloadRequested, 0, 0) != 0) {
        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    InterlockedIncrement(&g_inPresent);
    if (!init) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice))) {
            pDevice->GetImmediateContext(&pContext);
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            window = sd.OutputWindow;
            ID3D11Texture2D* pBackBuffer;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
            pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
            pBackBuffer->Release();
            oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
            ApplyMenuStyle();
            ImGui_ImplWin32_Init(window);
            ImGui_ImplDX11_Init(pDevice, pContext);
            init = true;
        }
        else {
            HRESULT hr = oPresent(pSwapChain, SyncInterval, Flags);
            InterlockedDecrement(&g_inPresent);
            return hr;
        }
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::GetIO().MouseDrawCursor = g_showMenu;

    if (g_showMenu) {
        ImGui::SetNextWindowSize(ImVec2(360.0f, 310.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Combat Master", &g_showMenu, ImGuiWindowFlags_NoCollapse);

        ImGui::TextColored(ImVec4(0.22f, 0.70f, 1.00f, 1.00f), "Internal");
        ImGui::SameLine();
        ImGui::TextDisabled("| INSERT menu | END unload");
        ImGui::Separator();

        if (ImGui::BeginTabBar("main_tabs")) {
            if (ImGui::BeginTabItem("ESP")) {
                ImGui::Checkbox("Master Enable", &g_espEnabled);
                ImGui::Separator();
                ImGui::Columns(2, "esp_columns", false);
                ImGui::Checkbox("Box", &g_espBox);
                ImGui::Checkbox("Names", &g_espNames);
                ImGui::Checkbox("Distance", &g_espDistance);
                ImGui::NextColumn();
                ImGui::Checkbox("Health", &g_espHealth);
                ImGui::Checkbox("Armor", &g_espArmor);
                ImGui::Columns(1);
                ImGui::Checkbox("Snaplines", &g_espLines);
                ImGui::Checkbox("Team Check", &g_teamCheck);
                ImGui::Checkbox("Visible Check", &g_visibleCheck);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Aim")) {
                ImGui::Checkbox("Enable Aimbot", &g_aimbotEnabled);
                ImGui::Checkbox("Draw FOV Circle", &g_drawAimbotFov);
                ImGui::Spacing();
                ImGui::SliderFloat("FOV", &g_aimbotFov, 25.0f, 600.0f, "%.0f");
                ImGui::SliderFloat("Smooth", &g_aimbotSmooth, 1.0f, 15.0f, "%.1f");
                if (g_aimbotSmooth < 1.0f) g_aimbotSmooth = 1.0f;
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Combat")) {
                ImGui::Checkbox("No Recoil", &g_noRecoil);
                ImGui::Checkbox("No Spread", &g_noSpread);
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "Cosmetics");
                if (ImGui::Checkbox("Unlock All Skins", &g_unlockAll)) {
                    if (g_unlockAll) {
                        ApplyUnlockAll();
                    } else {
                        RestoreUnlockAll();
                    }
                }
                if (g_unlockAllActive) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "ACTIVE");
                }
                ImGui::TextWrapped("Unlocks all items visually. Equip what you want, then save.");
                ImGui::Spacing();
                if (ImGui::Button("Save to Cloud", ImVec2(-1.0f, 0.0f))) {
                    if (!g_gameClientStaticFields) {
                        snprintf(g_saveStatusMsg, sizeof(g_saveStatusMsg), "ERROR: GameClient not resolved");
                        g_saveStatusTime = GetTickCount();
                    } else {
                        __try {
                            uintptr_t gameBase = (uintptr_t)GetModuleHandleA("Project.dll");
                            void* gameClientRef = *(void**)((uint8_t*)g_gameClientStaticFields + 0x0);
                            if (!IsPlausiblePtr(gameClientRef)) {
                                snprintf(g_saveStatusMsg, sizeof(g_saveStatusMsg), "ERROR: GameClient._ref is null");
                            } else {
                                void* profile = *(void**)((uint8_t*)gameClientRef + 0x60);
                                if (!IsPlausiblePtr(profile)) {
                                    snprintf(g_saveStatusMsg, sizeof(g_saveStatusMsg), "ERROR: ProfileCache is null");
                                } else {
                                    // CharacterStore is embedded at profile + 0x7D8
                                    uint8_t* charStore = (uint8_t*)profile + 0x7D8;
                                    typedef void (*t_buy)(void*, int32_t, void*);
                                    // Buy each currently equipped cosmetic so it persists
                                    struct { int offset; uintptr_t buyRVA; const char* name; } items[] = {
                                        { 0x60, 0x351DFC0, "Operator" },     // _equippedOperator
                                        { 0x30, 0x351DE20, "Emblem" },       // _equippedEmblem
                                        { 0x40, 0x351DD50, "CallingCard" },  // _equippedCallingCard
                                        { 0x70, 0x351E160, "Wristband" },    // _equippedWristband
                                    };
                                    int bought = 0;
                                    for (int i = 0; i < 4; i++) {
                                        int32_t id = *(int32_t*)(charStore + items[i].offset);
                                        if (id > 0) {
                                            t_buy fn = (t_buy)(gameBase + items[i].buyRVA);
                                            fn(charStore, id, NULL);
                                            bought++;
                                            Log("Bought %s: id=%d", items[i].name, id);
                                        }
                                    }
                                    // Trigger PlayFab cloud save
                                    *(bool*)((uint8_t*)profile + 0x1DF4) = true;
                                    snprintf(g_saveStatusMsg, sizeof(g_saveStatusMsg), "Saved %d items to cloud!", bought);
                                    Log("PersistUnlocks: saved %d items, _forceSaveToPlayfab=true", bought);
                                }
                            }
                        } __except(1) {
                            snprintf(g_saveStatusMsg, sizeof(g_saveStatusMsg), "ERROR: Exception during save");
                        }
                        g_saveStatusTime = GetTickCount();
                    }
                }
                if (g_saveStatusMsg[0] && (GetTickCount() - g_saveStatusTime) < 5000) {
                    bool isError = (g_saveStatusMsg[0] == 'E');
                    ImGui::TextColored(
                        isError ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : ImVec4(0.3f, 1.0f, 0.3f, 1.0f),
                        "%s", g_saveStatusMsg);
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Debug")) {
                ImGui::Text("Base: %p", GetModuleHandleA("Project.dll"));
                ImGui::Text("Camera: %p", (void*)fn_get_main_camera);
                ImGui::Text("W2S: %p", (void*)fn_WorldToScreenPoint);
                ImGui::Text("StaticFields: %p", g_staticFields);
                ImGui::Text("TeamId: %p", (void*)fn_playerroot_get_teamid);
                ImGui::Text("Players R:%d HP:%d Tr:%d OK:%d", g_dbg_pR, g_dbg_pHP, g_dbg_pTr, g_dbg_pW2S);
                ImGui::Text("Filtered dead:%d downed:%d noHead:%d", g_dbg_deadFiltered, g_dbg_downedFiltered, g_dbg_noHead);
                ImGui::Text("Visible:%d hidden:%d fn:%p", g_dbg_visible, g_dbg_occluded, (void*)fn_playerroot_get_is_visible);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Separator();
        if (ImGui::Button("Unload DLL", ImVec2(-1.0f, 0.0f))) {
           RequestUnload();
        }
        
        ImGui::End();
    }

    if (InterlockedCompareExchange(&g_unloadRequested, 0, 0) == 0) {
        DrawESP();
    }

    ImGui::Render();
    pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    HRESULT hr = oPresent(pSwapChain, SyncInterval, Flags);
    InterlockedDecrement(&g_inPresent);
    return hr;
}

HRESULT __stdcall hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    if (InterlockedCompareExchange(&g_unloadRequested, 0, 0) != 0) {
        return oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    }

    if (mainRenderTargetView) {
        if (pContext) pContext->OMSetRenderTargets(0, 0, 0);
        mainRenderTargetView->Release();
        mainRenderTargetView = NULL;
    }
    HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    ID3D11Texture2D* pBackBuffer;
    if (pDevice && pContext && SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer))) {
        pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
        pBackBuffer->Release();
        pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
    }
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)Width; vp.Height = (FLOAT)Height; vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f; vp.TopLeftX = 0; vp.TopLeftY = 0;
    if (pContext) pContext->RSSetViewports(1, &vp);
    return hr;
}

// ============================================================
// Initialization
// ============================================================
void* FindImage(const char* name) {
    if (!fn_domain_get_assemblies || !fn_domain_get) return NULL;
    size_t asmCount = 0;
    void* domain = fn_domain_get();
    if (!domain) return NULL;
    void** asms = fn_domain_get_assemblies(domain, &asmCount);
    if (!asms) return NULL;

    for (size_t i = 0; i < asmCount; i++) {
        void* assembly = asms[i];
        if (!assembly) continue;
        void* image = *(void**)assembly;
        if (!image) continue;
        const char* iname = *(const char**)image;
        if (iname && strstr(iname, name)) return image;
    }
    return NULL;
}

DWORD WINAPI MainThread(LPVOID lpReserved) {
    char mutexName[128];
    snprintf(mutexName, sizeof(mutexName), "Local\\imgui_esp_single_init_%lu", GetCurrentProcessId());
    g_singleInstanceMutex = CreateMutexA(NULL, TRUE, mutexName);
    if (!g_singleInstanceMutex) {
        Log("CreateMutexA failed: %lu", GetLastError());
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        Log("Duplicate MainThread blocked by mutex: %s", mutexName);
        CloseHandle(g_singleInstanceMutex);
        g_singleInstanceMutex = NULL;
        return 0;
    }

    ResetLog();
    InitializeCriticalSection(&g_espCs);
    g_csInitialized = true;
    Log("MainThread started");

    // 1. IL2CPP Init
    HMODULE hProject = NULL;
    while (!hProject) {
        hProject = GetModuleHandleA("Project.dll");
        if (!hProject) Sleep(500);
    }
    Log("Project.dll found at %p", hProject);

    fn_domain_get = (t_domain_get)GetProcAddress(hProject, "ApzldXylDWR");
    fn_thread_attach = (t_thread_attach)GetProcAddress(hProject, "ZxbKYsbhTwf");
    fn_class_from_name = (t_class_from_name)GetProcAddress(hProject, "etIgYqMGBTj");
    fn_domain_get_assemblies = (t_domain_get_assemblies)GetProcAddress(hProject, "gobBtMyApTj");
    fn_class_get_method_from_name = (t_class_get_method_from_name)GetProcAddress(hProject, "BfmNBPOFdWH");

    if (!fn_domain_get || !fn_thread_attach) return 1;

    void* domain = fn_domain_get();
    fn_thread_attach(domain);

    uintptr_t gameBase = (uintptr_t)hProject;
    fn_get_main_camera = NULL;
    fn_get_worldToCamera = NULL;
    fn_get_projectionMatrix = NULL;
    fn_component_get_transform = NULL;

    // Dynamically resolve functions
    void* coreModule = FindImage("UnityEngine.CoreModule");
    if (coreModule && fn_class_get_method_from_name) {
        void* transformClass = fn_class_from_name(coreModule, "UnityEngine", "Transform");
        if (transformClass) {
            void* methodInfo = fn_class_get_method_from_name(transformClass, "get_position", 0);
            if (methodInfo) {
                g_getPositionMethodInfo = methodInfo;
                void* methodPtr = *(void**)methodInfo;
                fn_get_position = (t_get_position_injected)methodPtr;
            } else {
                fn_get_position = (t_get_position_injected)(gameBase + 0x2FA9830); // fallback
            }
        }
        
        void* componentClass = fn_class_from_name(coreModule, "UnityEngine", "Component");
        if (componentClass) {
            void* methodInfo = fn_class_get_method_from_name(componentClass, "get_transform", 0);
            if (methodInfo) {
                g_componentGetTransformMethodInfo = methodInfo;
                fn_component_get_transform = (t_component_get_transform)(*(void**)methodInfo);
                Log("Component.get_transform: %p", (void*)fn_component_get_transform);
            }
        }

        void* physicsModule = FindImage("UnityEngine.PhysicsModule");
        if (!physicsModule) physicsModule = coreModule;
        if (physicsModule) {
            void* characterControllerClass = fn_class_from_name(physicsModule, "UnityEngine", "CharacterController");
            if (characterControllerClass) {
                void* moveInfo = fn_class_get_method_from_name(characterControllerClass, "Move", 1);
                if (moveInfo) {
                    g_characterControllerMoveMethodInfo = moveInfo;
                    fn_charactercontroller_move = (t_charactercontroller_move)(*(void**)moveInfo);
                    Log("CharacterController.Move: %p", (void*)fn_charactercontroller_move);
                } else {
                    Log("CharacterController.Move not found");
                }
            } else {
                Log("CharacterController class not found");
            }
        }

        void* cameraClass = fn_class_from_name(coreModule, "UnityEngine", "Camera");
        if (cameraClass) {
            void* mainInfo = fn_class_get_method_from_name(cameraClass, "get_main", 0);
            if (mainInfo) {
                g_getMainCameraMethodInfo = mainInfo;
                fn_get_main_camera = (t_get_main_camera)(*(void**)mainInfo);
                Log("Camera.get_main: %p", (void*)fn_get_main_camera);
            }

            void* worldToCameraInfo = fn_class_get_method_from_name(cameraClass, "get_worldToCameraMatrix", 0);
            if (worldToCameraInfo) {
                g_getWorldToCameraMethodInfo = worldToCameraInfo;
                fn_get_worldToCamera = (t_get_worldToCamera)(*(void**)worldToCameraInfo);
                Log("Camera.get_worldToCameraMatrix: %p", (void*)fn_get_worldToCamera);
            }

            void* projectionInfo = fn_class_get_method_from_name(cameraClass, "get_projectionMatrix", 0);
            if (projectionInfo) {
                g_getProjectionMatrixMethodInfo = projectionInfo;
                fn_get_projectionMatrix = (t_get_projectionMatrix)(*(void**)projectionInfo);
                Log("Camera.get_projectionMatrix: %p", (void*)fn_get_projectionMatrix);
            }

            void* w2sInfo = fn_class_get_method_from_name(cameraClass, "WorldToScreenPoint", 1);
            if (w2sInfo) {
                g_worldToScreenPointMethodInfo = w2sInfo;
                void* methodPtr = *(void**)w2sInfo;
                fn_WorldToScreenPoint = (t_WorldToScreenPoint)methodPtr;
                Log("Camera.WorldToScreenPoint: %p", (void*)fn_WorldToScreenPoint);
            }
        }

        Log("CoreModule: %p", coreModule);
        if (!fn_get_position) {
            fn_get_position = (t_get_position_injected)(gameBase + 0x2FA9830);
            Log("Using fallback GetPosition: %p", fn_get_position);
        }
    } else {
        fn_get_position = (t_get_position_injected)(gameBase + 0x2FA9830); // fallback
    }

    if (!fn_get_main_camera || !fn_get_worldToCamera || !fn_get_projectionMatrix || !fn_component_get_transform) {
        Log("One or more dynamic Unity methods failed to resolve; ESP will remain disabled");
        g_espEnabled = false;
    }

    void* battleImage = FindImage("_CombatMaster.Battle");
    void* playerRootClass = NULL;
    if (battleImage) {
        playerRootClass = fn_class_from_name(battleImage, "CombatMaster.Battle.Gameplay.Player", "PlayerRoot");

        void* battleExtensionsClass = fn_class_from_name(battleImage, "CombatMaster.Battle.GameSession", "BattleExtensions");
        if (battleExtensionsClass) {
            void* nicknameInfo = fn_class_get_method_from_name(battleExtensionsClass, "Nickname", 1);
            if (nicknameInfo) {
                g_battleExtensionsNicknameMethodInfo = nicknameInfo;
                fn_battleextensions_nickname = (t_netplayerdata_string)(*(void**)nicknameInfo);
                Log("BattleExtensions.Nickname: %p", (void*)fn_battleextensions_nickname);
            } else {
                Log("BattleExtensions.Nickname not found");
            }

            void* playfabInfo = fn_class_get_method_from_name(battleExtensionsClass, "PlayFabId", 1);
            if (playfabInfo) {
                g_battleExtensionsPlayFabIdMethodInfo = playfabInfo;
                fn_battleextensions_playfabid = (t_netplayerdata_string)(*(void**)playfabInfo);
                Log("BattleExtensions.PlayFabId: %p", (void*)fn_battleextensions_playfabid);
            }
        } else {
            Log("BattleExtensions class not found; name ESP will use fallback labels");
        }
    }

    if (!playerRootClass) {
        Log("PlayerRoot not found in Battle image, searching all assemblies...");
        size_t asmCount = 0;
        void** asms = fn_domain_get_assemblies(fn_domain_get(), &asmCount);
        for (size_t i = 0; i < asmCount; i++) {
            void* assembly = asms[i];
            if (!assembly) continue;
            void* image = *(void**)assembly;
            if (!image) continue;
            playerRootClass = fn_class_from_name(image, "CombatMaster.Battle.Gameplay.Player", "PlayerRoot");
            if (playerRootClass) {
                Log("Found PlayerRoot in image: %s", *(const char**)image);
                break;
            }
        }
    }

    if (playerRootClass) {
        Log("PlayerRoot Class: %p", playerRootClass);
        void* teamIdInfo = fn_class_get_method_from_name(playerRootClass, "get_TeamId", 0);
        if (teamIdInfo) {
            g_playerRootGetTeamIdMethodInfo = teamIdInfo;
            fn_playerroot_get_teamid = (t_playerroot_get_teamid)(*(void**)teamIdInfo);
            Log("PlayerRoot.get_TeamId: %p", (void*)fn_playerroot_get_teamid);
        } else {
            Log("PlayerRoot.get_TeamId not found; team check will be inactive");
        }

        void* visibleInfo = fn_class_get_method_from_name(playerRootClass, "get_IsVisible", 0);
        if (visibleInfo) {
            g_playerRootGetIsVisibleMethodInfo = visibleInfo;
            fn_playerroot_get_is_visible = (t_playerroot_bool)(*(void**)visibleInfo);
            Log("PlayerRoot.get_IsVisible: %p", (void*)fn_playerroot_get_is_visible);
        } else {
            Log("PlayerRoot.get_IsVisible not found; using _isVisible field fallback");
        }

        if (battleImage) {
            void* playerHealthClass = fn_class_from_name(battleImage, "CombatMaster.Battle.Gameplay.Player", "PlayerHealth");
            if (playerHealthClass) {
                void* isDeadInfo = fn_class_get_method_from_name(playerHealthClass, "get_IsDead", 0);
                if (isDeadInfo) {
                    g_playerHealthIsDeadMethodInfo = isDeadInfo;
                    fn_playerhealth_get_is_dead = (t_playerhealth_bool)(*(void**)isDeadInfo);
                    Log("PlayerHealth.get_IsDead: %p", (void*)fn_playerhealth_get_is_dead);
                } else {
                    Log("PlayerHealth.get_IsDead not found");
                }

                void* isDownedInfo = fn_class_get_method_from_name(playerHealthClass, "get_IsDowned", 0);
                if (isDownedInfo) {
                    g_playerHealthIsDownedMethodInfo = isDownedInfo;
                    fn_playerhealth_get_is_downed = (t_playerhealth_bool)(*(void**)isDownedInfo);
                    Log("PlayerHealth.get_IsDowned: %p", (void*)fn_playerhealth_get_is_downed);
                } else {
                    Log("PlayerHealth.get_IsDowned not found");
                }
            } else {
                Log("PlayerHealth class not found");
            }

        }

        uint8_t* classBytes = (uint8_t*)playerRootClass;
        for (int off = 0xB0; off < 0x200; off += 8) { // Search wider
            __try {
                void* candidate = *(void**)(classBytes + off);
                if (!candidate || (uintptr_t)candidate < 0x10000) continue;
                
                // Static fields usually have pointers to instances at 0x8, 0x18 etc.
                void* testMyPlayer = *(void**)((uint8_t*)candidate + 0x8);
                void* testAllPlayers = *(void**)((uint8_t*)candidate + 0x18);
                
                if (testAllPlayers && (uintptr_t)testAllPlayers > 0x10000) {
                    Il2CppList* list = (Il2CppList*)testAllPlayers;
                    if (list->_size >= 0 && list->_size < 100 && list->_items && (uintptr_t)list->_items > 0x10000) {
                        g_staticFields = candidate;
                        Log("g_staticFields found at offset 0x%X: %p", off, g_staticFields);
                        break;
                    }
                }
            } __except(1) {}
        }
    }

    // Resolve GameClient for persistent cosmetic saves
    {
        void* gameClientClass = NULL;
        size_t asmCount2 = 0;
        void** asms2 = fn_domain_get_assemblies(fn_domain_get(), &asmCount2);
        for (size_t i = 0; i < asmCount2 && !gameClientClass; i++) {
            void* assembly = asms2[i];
            if (!assembly) continue;
            void* image = *(void**)assembly;
            if (!image) continue;
            gameClientClass = fn_class_from_name(image, "CombatMaster.Client", "GameClient");
        }
        if (gameClientClass) {
            Log("GameClient class: %p", gameClientClass);
            uint8_t* gcBytes = (uint8_t*)gameClientClass;
            for (int off = 0xB0; off < 0x200; off += 8) {
                __try {
                    void* candidate = *(void**)(gcBytes + off);
                    if (!candidate || (uintptr_t)candidate < 0x10000) continue;
                    void* ref = *(void**)((uint8_t*)candidate + 0x0);
                    if (IsPlausiblePtr(ref)) {
                        void* profileTest = *(void**)((uint8_t*)ref + 0x60);
                        if (IsPlausiblePtr(profileTest)) {
                            g_gameClientStaticFields = candidate;
                            Log("GameClient statics at 0x%X: %p (_ref=%p)", off, candidate, ref);
                            break;
                        }
                    }
                } __except(1) {}
            }
            if (!g_gameClientStaticFields) Log("GameClient static fields not found");
        } else {
            Log("GameClient class not found");
        }
    }

    // 2. D3D11 Hook Init
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = GetForegroundWindow();
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    
    IDXGISwapChain* swapChain;
    ID3D11Device* dummyDevice;
    ID3D11DeviceContext* dummyContext;

    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &swapChain, &dummyDevice, &featureLevel, &dummyContext) != S_OK) {
        return 1;
    }

    void** pVTable = *reinterpret_cast<void***>(swapChain);
    swapChain->Release();
    dummyDevice->Release();
    dummyContext->Release();

    MH_Initialize();
    MH_CreateHook(pVTable[8], hkPresent, (void**)&oPresent);
    MH_CreateHook(pVTable[13], hkResizeBuffers, (void**)&oResizeBuffers);
    MH_EnableHook(MH_ALL_HOOKS);

    g_dataThread = CreateThread(0, 0, ESPDataThread, 0, 0, 0);
    Log("MainThread init finished, ESPDataThread started");

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = hModule;
        InterlockedExchange(&g_running, 1);
        InterlockedExchange(&g_unloadRequested, 0);
        DisableThreadLibraryCalls(hModule);
        if (InterlockedCompareExchange(&g_mainThreadStarted, 1, 0) == 0) {
            HANDLE hThread = CreateThread(0, 0, MainThread, 0, 0, 0);
            if (hThread) CloseHandle(hThread);
        }
    }
    return TRUE;
}
