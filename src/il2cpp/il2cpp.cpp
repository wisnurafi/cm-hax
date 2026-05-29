// il2cpp/il2cpp.cpp
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

#include "il2cpp.h"
#include "../core/logging.h"

namespace IL2CPP {
    t_domain_get                   fn_domain_get                   = NULL;
    t_thread_attach                fn_thread_attach                = NULL;
    t_class_from_name              fn_class_from_name              = NULL;
    t_domain_get_assemblies        fn_domain_get_assemblies        = NULL;
    t_class_get_method_from_name   fn_class_get_method_from_name   = NULL;

    t_get_position_injected        fn_get_position                 = NULL;
    t_get_main_camera              fn_get_main_camera              = NULL;
    t_get_worldToCamera            fn_get_worldToCamera            = NULL;
    t_get_projectionMatrix         fn_get_projectionMatrix         = NULL;
    t_component_get_transform      fn_component_get_transform      = NULL;
    t_playerroot_get_teamid        fn_playerroot_get_teamid        = NULL;
    t_playerhealth_bool            fn_playerhealth_get_is_dead     = NULL;
    t_playerhealth_bool            fn_playerhealth_get_is_downed   = NULL;
    t_WorldToScreenPoint           fn_WorldToScreenPoint           = NULL;
    t_charactercontroller_move     fn_charactercontroller_move     = NULL;
    t_netplayerdata_string         fn_battleextensions_nickname    = NULL;
    t_netplayerdata_string         fn_battleextensions_playfabid   = NULL;
    t_playerroot_bool              fn_playerroot_get_is_visible    = NULL;

    void* g_getMainCameraMethodInfo            = NULL;
    void* g_getWorldToCameraMethodInfo         = NULL;
    void* g_getProjectionMatrixMethodInfo      = NULL;
    void* g_componentGetTransformMethodInfo    = NULL;
    void* g_getPositionMethodInfo              = NULL;
    void* g_playerRootGetTeamIdMethodInfo      = NULL;
    void* g_playerHealthIsDeadMethodInfo       = NULL;
    void* g_playerHealthIsDownedMethodInfo     = NULL;
    void* g_worldToScreenPointMethodInfo       = NULL;
    void* g_characterControllerMoveMethodInfo  = NULL;
    void* g_battleExtensionsNicknameMethodInfo = NULL;
    void* g_battleExtensionsPlayFabIdMethodInfo= NULL;
    void* g_playerRootGetIsVisibleMethodInfo   = NULL;

    HMODULE hProject = NULL;

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

    void EnsureThreadAttached() {
        if (fn_thread_attach && fn_domain_get) {
            __try { fn_thread_attach(fn_domain_get()); } __except(1) {}
        }
    }

    static volatile LONG s_presentThreadAttached = 0;
    void EnsurePresentThreadAttached() {
        if (InterlockedCompareExchange(&s_presentThreadAttached, 1, 0) == 0) {
            EnsureThreadAttached();
        }
    }

    static void ResolveCoreModule(uintptr_t gameBase) {
        void* coreModule = FindImage("UnityEngine.CoreModule");
        if (!coreModule || !fn_class_get_method_from_name) {
            fn_get_position = (t_get_position_injected)(gameBase + 0x2FA9830); // fallback
            return;
        }

        if (void* transformClass = fn_class_from_name(coreModule, "UnityEngine", "Transform")) {
            if (void* mi = fn_class_get_method_from_name(transformClass, "get_position", 0)) {
                g_getPositionMethodInfo = mi;
                fn_get_position = (t_get_position_injected)(*(void**)mi);
            } else {
                fn_get_position = (t_get_position_injected)(gameBase + 0x2FA9830); // fallback
            }
        }

        if (void* componentClass = fn_class_from_name(coreModule, "UnityEngine", "Component")) {
            if (void* mi = fn_class_get_method_from_name(componentClass, "get_transform", 0)) {
                g_componentGetTransformMethodInfo = mi;
                fn_component_get_transform = (t_component_get_transform)(*(void**)mi);
                Log("Component.get_transform: %p", (void*)fn_component_get_transform);
            }
        }

        void* physicsModule = FindImage("UnityEngine.PhysicsModule");
        if (!physicsModule) physicsModule = coreModule;
        if (physicsModule) {
            if (void* ccClass = fn_class_from_name(physicsModule, "UnityEngine", "CharacterController")) {
                if (void* mi = fn_class_get_method_from_name(ccClass, "Move", 1)) {
                    g_characterControllerMoveMethodInfo = mi;
                    fn_charactercontroller_move = (t_charactercontroller_move)(*(void**)mi);
                    Log("CharacterController.Move: %p", (void*)fn_charactercontroller_move);
                } else {
                    Log("CharacterController.Move not found");
                }
            } else {
                Log("CharacterController class not found");
            }
        }

        if (void* cameraClass = fn_class_from_name(coreModule, "UnityEngine", "Camera")) {
            if (void* mi = fn_class_get_method_from_name(cameraClass, "get_main", 0)) {
                g_getMainCameraMethodInfo = mi;
                fn_get_main_camera = (t_get_main_camera)(*(void**)mi);
                Log("Camera.get_main: %p", (void*)fn_get_main_camera);
            }
            if (void* mi = fn_class_get_method_from_name(cameraClass, "get_worldToCameraMatrix", 0)) {
                g_getWorldToCameraMethodInfo = mi;
                fn_get_worldToCamera = (t_get_worldToCamera)(*(void**)mi);
                Log("Camera.get_worldToCameraMatrix: %p", (void*)fn_get_worldToCamera);
            }
            if (void* mi = fn_class_get_method_from_name(cameraClass, "get_projectionMatrix", 0)) {
                g_getProjectionMatrixMethodInfo = mi;
                fn_get_projectionMatrix = (t_get_projectionMatrix)(*(void**)mi);
                Log("Camera.get_projectionMatrix: %p", (void*)fn_get_projectionMatrix);
            }
            if (void* mi = fn_class_get_method_from_name(cameraClass, "WorldToScreenPoint", 1)) {
                g_worldToScreenPointMethodInfo = mi;
                fn_WorldToScreenPoint = (t_WorldToScreenPoint)(*(void**)mi);
                Log("Camera.WorldToScreenPoint: %p", (void*)fn_WorldToScreenPoint);
            }
        }

        Log("CoreModule: %p", coreModule);
        if (!fn_get_position) {
            fn_get_position = (t_get_position_injected)(gameBase + 0x2FA9830);
            Log("Using fallback GetPosition: %p", fn_get_position);
        }
    }

    static void ResolveBattleModule() {
        void* battleImage = FindImage("_CombatMaster.Battle");
        void* playerRootClass = NULL;

        if (battleImage) {
            playerRootClass = fn_class_from_name(battleImage,
                "CombatMaster.Battle.Gameplay.Player", "PlayerRoot");

            if (void* battleExt = fn_class_from_name(battleImage,
                    "CombatMaster.Battle.GameSession", "BattleExtensions")) {
                if (void* mi = fn_class_get_method_from_name(battleExt, "Nickname", 1)) {
                    g_battleExtensionsNicknameMethodInfo = mi;
                    fn_battleextensions_nickname = (t_netplayerdata_string)(*(void**)mi);
                    Log("BattleExtensions.Nickname: %p", (void*)fn_battleextensions_nickname);
                } else {
                    Log("BattleExtensions.Nickname not found");
                }
                if (void* mi = fn_class_get_method_from_name(battleExt, "PlayFabId", 1)) {
                    g_battleExtensionsPlayFabIdMethodInfo = mi;
                    fn_battleextensions_playfabid = (t_netplayerdata_string)(*(void**)mi);
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
                playerRootClass = fn_class_from_name(image,
                    "CombatMaster.Battle.Gameplay.Player", "PlayerRoot");
                if (playerRootClass) {
                    Log("Found PlayerRoot in image: %s", *(const char**)image);
                    break;
                }
            }
        }

        if (playerRootClass) {
            Log("PlayerRoot Class: %p", playerRootClass);
            if (void* mi = fn_class_get_method_from_name(playerRootClass, "get_TeamId", 0)) {
                g_playerRootGetTeamIdMethodInfo = mi;
                fn_playerroot_get_teamid = (t_playerroot_get_teamid)(*(void**)mi);
                Log("PlayerRoot.get_TeamId: %p", (void*)fn_playerroot_get_teamid);
            } else {
                Log("PlayerRoot.get_TeamId not found; team check will be inactive");
            }
            if (void* mi = fn_class_get_method_from_name(playerRootClass, "get_IsVisible", 0)) {
                g_playerRootGetIsVisibleMethodInfo = mi;
                fn_playerroot_get_is_visible = (t_playerroot_bool)(*(void**)mi);
                Log("PlayerRoot.get_IsVisible: %p", (void*)fn_playerroot_get_is_visible);
            } else {
                Log("PlayerRoot.get_IsVisible not found; using _isVisible field fallback");
            }

            if (battleImage) {
                if (void* phClass = fn_class_from_name(battleImage,
                        "CombatMaster.Battle.Gameplay.Player", "PlayerHealth")) {
                    if (void* mi = fn_class_get_method_from_name(phClass, "get_IsDead", 0)) {
                        g_playerHealthIsDeadMethodInfo = mi;
                        fn_playerhealth_get_is_dead = (t_playerhealth_bool)(*(void**)mi);
                        Log("PlayerHealth.get_IsDead: %p", (void*)fn_playerhealth_get_is_dead);
                    } else {
                        Log("PlayerHealth.get_IsDead not found");
                    }
                    if (void* mi = fn_class_get_method_from_name(phClass, "get_IsDowned", 0)) {
                        g_playerHealthIsDownedMethodInfo = mi;
                        fn_playerhealth_get_is_downed = (t_playerhealth_bool)(*(void**)mi);
                        Log("PlayerHealth.get_IsDowned: %p", (void*)fn_playerhealth_get_is_downed);
                    } else {
                        Log("PlayerHealth.get_IsDowned not found");
                    }
                } else {
                    Log("PlayerHealth class not found");
                }
            }
        }
    }

    bool ResolveAll(HMODULE projectDll) {
        hProject = projectDll;
        fn_domain_get                 = (t_domain_get)               GetProcAddress(projectDll, "ApzldXylDWR");
        fn_thread_attach              = (t_thread_attach)            GetProcAddress(projectDll, "ZxbKYsbhTwf");
        fn_class_from_name            = (t_class_from_name)          GetProcAddress(projectDll, "etIgYqMGBTj");
        fn_domain_get_assemblies      = (t_domain_get_assemblies)    GetProcAddress(projectDll, "gobBtMyApTj");
        fn_class_get_method_from_name = (t_class_get_method_from_name)GetProcAddress(projectDll, "BfmNBPOFdWH");

        if (!fn_domain_get || !fn_thread_attach) return false;

        // Attach domain so subsequent class_from_name calls don't crash
        EnsureThreadAttached();

        ResolveCoreModule((uintptr_t)projectDll);
        ResolveBattleModule();
        return true;
    }
}
