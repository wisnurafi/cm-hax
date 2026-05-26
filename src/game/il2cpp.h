// game/il2cpp.h
// IL2CPP API resolution + cached function pointers / methodInfo blobs.
// Symbol exports are renamed in Combat Master (e.g. il2cpp_domain_get -> ApzldXylDWR).
// All runtime calls go through the typedefs declared here.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include "../utils/math.h"

// Generic IL2CPP exports
typedef void*  (*t_domain_get)();
typedef void*  (*t_thread_attach)(void* domain);
typedef void*  (*t_class_from_name)(void* image, const char* ns, const char* name);
typedef void** (*t_domain_get_assemblies)(void* domain, size_t* count);
typedef void*  (*t_class_get_method_from_name)(void* klass, const char* name, int argsCount);

// Specific game / Unity methods we resolve via class_get_method_from_name
typedef void  (*t_get_position_injected)(Vector3* out, void* transform, void* methodInfo);
typedef void* (*t_get_main_camera)(void* methodInfo);
typedef void  (*t_get_worldToCamera)(Matrix4x4* out, void* camera, void* methodInfo);
typedef void  (*t_get_projectionMatrix)(Matrix4x4* out, void* camera, void* methodInfo);
typedef void* (*t_component_get_transform)(void* component, void* methodInfo);
typedef int   (*t_playerroot_get_teamid)(void* player, void* methodInfo);
typedef bool  (*t_playerhealth_bool)(void* health, void* methodInfo);
typedef void  (*t_WorldToScreenPoint)(Vector3* out, void* camera, Vector3* pos, void* methodInfo);
typedef int   (*t_charactercontroller_move)(void* characterController, Vector3 motion, void* methodInfo);
typedef void* (*t_netplayerdata_string)(void* netPlayerData, void* methodInfo);
typedef bool  (*t_playerroot_bool)(void* player, void* methodInfo);

namespace IL2CPP {
    // Resolved at startup. Kept in extern globals so call sites stay tight.
    extern t_domain_get                   fn_domain_get;
    extern t_thread_attach                fn_thread_attach;
    extern t_class_from_name              fn_class_from_name;
    extern t_domain_get_assemblies        fn_domain_get_assemblies;
    extern t_class_get_method_from_name   fn_class_get_method_from_name;

    extern t_get_position_injected        fn_get_position;
    extern t_get_main_camera              fn_get_main_camera;
    extern t_get_worldToCamera            fn_get_worldToCamera;
    extern t_get_projectionMatrix         fn_get_projectionMatrix;
    extern t_component_get_transform      fn_component_get_transform;
    extern t_playerroot_get_teamid        fn_playerroot_get_teamid;
    extern t_playerhealth_bool            fn_playerhealth_get_is_dead;
    extern t_playerhealth_bool            fn_playerhealth_get_is_downed;
    extern t_WorldToScreenPoint           fn_WorldToScreenPoint;
    extern t_charactercontroller_move     fn_charactercontroller_move;
    extern t_netplayerdata_string         fn_battleextensions_nickname;
    extern t_netplayerdata_string         fn_battleextensions_playfabid;
    extern t_playerroot_bool              fn_playerroot_get_is_visible;

    // MethodInfo* values to pass alongside the function pointers.
    extern void* g_getMainCameraMethodInfo;
    extern void* g_getWorldToCameraMethodInfo;
    extern void* g_getProjectionMatrixMethodInfo;
    extern void* g_componentGetTransformMethodInfo;
    extern void* g_getPositionMethodInfo;
    extern void* g_playerRootGetTeamIdMethodInfo;
    extern void* g_playerHealthIsDeadMethodInfo;
    extern void* g_playerHealthIsDownedMethodInfo;
    extern void* g_worldToScreenPointMethodInfo;
    extern void* g_characterControllerMoveMethodInfo;
    extern void* g_battleExtensionsNicknameMethodInfo;
    extern void* g_battleExtensionsPlayFabIdMethodInfo;
    extern void* g_playerRootGetIsVisibleMethodInfo;

    // Project.dll cached after we resolve the IL2CPP exports.
    extern HMODULE hProject;

    // Walks the assembly list and returns the image whose name contains `name`.
    void* FindImage(const char* name);

    // Resolve all IL2CPP exports + Unity / game methods. Called once from
    // MainThread. Returns true when the bare minimum (domain_get, thread_attach,
    // domain_get_assemblies, class_get_method_from_name, class_from_name) was
    // resolved successfully.
    bool ResolveAll(HMODULE projectDll);

    // Attaches the calling thread to the IL2CPP domain. Safe to call repeatedly.
    void EnsureThreadAttached();

    // Convenience: thread_attach(domain_get())
    void EnsurePresentThreadAttached();
}
