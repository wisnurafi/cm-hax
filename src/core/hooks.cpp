// core/hooks.cpp
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <stdint.h>
#include <stdio.h>

#include "hooks.h"
#include "globals.h"
#include "config.h"
#include "logging.h"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include "MinHook.h"

#include "../il2cpp/il2cpp.h"
#include "../il2cpp/sdk.h"
#include "../utils/memory.h"

#include "../features/esp.h"
#include "../features/cosmetics.h"

#include "../render/menu.h"
#include "../render/menu_style.h"
#include "../render/esp_render.h"
#include "../render/overlay.h"
#include "../render/radar.h"

#pragma comment(lib, "d3d11.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Hooks {

typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef HRESULT(__stdcall* ResizeBuffers_t)(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

static Present_t       oPresent       = NULL;
static ResizeBuffers_t oResizeBuffers = NULL;

static ID3D11Device*           pDevice              = NULL;
static ID3D11DeviceContext*    pContext             = NULL;
static ID3D11RenderTargetView* mainRenderTargetView = NULL;
static HWND                    g_hwnd               = NULL;
static WNDPROC                 oWndProc             = NULL;
static bool                    s_imguiInitialized   = false;

// ============================================================
// Window proc subclass
// ============================================================
static LRESULT __stdcall WndProcHook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && wParam == VK_END) {
        RequestUnload();
        return TRUE;
    }
    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT) {
        g_state.showMenu = !g_state.showMenu;
        return TRUE;
    }

    if (g_state.showMenu && ImGui::GetCurrentContext() != NULL) {
        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        ImGuiIO& io = ImGui::GetIO();
        if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) return TRUE;
        if (io.WantCaptureKeyboard && uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST) return TRUE;
    }

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

// ============================================================
// Cleanup + unload
// ============================================================
static void CleanupResources() {
    Log("Unload cleanup started");

    __try { Config::Save(); } __except(1) { Log("SaveConfig threw during cleanup"); }

    g_state.espEnabled = false;
    g_state.noRecoil   = false;
    g_state.noSpread   = false;
    Cosmetics::RestoreUnlockAll();
    g_state.unlockAll = false;
    InterlockedExchange(&Runtime::g_running, 0);

    if (Runtime::g_dataThread) {
        WaitForSingleObject(Runtime::g_dataThread, 1500);
        CloseHandle(Runtime::g_dataThread);
        Runtime::g_dataThread = NULL;
    }

    DWORD start = GetTickCount();
    while (InterlockedCompareExchange(&Runtime::g_inPresent, 0, 0) != 0 && GetTickCount() - start < 2000) {
        Sleep(10);
    }

    MH_DisableHook(MH_ALL_HOOKS);
    Sleep(100);

    if (g_hwnd && oWndProc) {
        SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)oWndProc);
        oWndProc = NULL;
    }

    if (s_imguiInitialized && ImGui::GetCurrentContext() != NULL) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        s_imguiInitialized = false;
    }

    if (mainRenderTargetView) { mainRenderTargetView->Release(); mainRenderTargetView = NULL; }
    if (pContext)             { pContext->Release();             pContext             = NULL; }
    if (pDevice)              { pDevice->Release();              pDevice              = NULL; }

    MH_RemoveHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if (Runtime::g_csInitialized) {
        DeleteCriticalSection(&Runtime::g_espCs);
        Runtime::g_csInitialized = false;
    }

    if (Runtime::g_singleInstanceMutex) {
        ReleaseMutex(Runtime::g_singleInstanceMutex);
        CloseHandle(Runtime::g_singleInstanceMutex);
        Runtime::g_singleInstanceMutex = NULL;
    }

    Log("Unload cleanup finished");
}

static DWORD WINAPI UnloadThread(LPVOID /*p*/) {
    CleanupResources();
    HMODULE module = Runtime::g_module;
    if (module) FreeLibraryAndExitThread(module, 0);
    return 0;
}

void RequestUnload() {
    if (InterlockedCompareExchange(&Runtime::g_unloadRequested, 1, 0) != 0) return;

    Log("Unload requested by END key/menu");
    g_state.showMenu = false;
    g_state.espEnabled = false;

    HANDLE hThread = CreateThread(0, 0, UnloadThread, 0, 0, 0);
    if (hThread) CloseHandle(hThread);
}

// ============================================================
// D3D11 hooks
// ============================================================
static HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (InterlockedCompareExchange(&Runtime::g_unloadRequested, 0, 0) != 0) {
        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    InterlockedIncrement(&Runtime::g_inPresent);
    if (!s_imguiInitialized) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice))) {
            pDevice->GetImmediateContext(&pContext);
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            g_hwnd = sd.OutputWindow;

            ID3D11Texture2D* pBackBuffer = NULL;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
            pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
            pBackBuffer->Release();

            oWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProcHook);

            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = NULL;
            io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
            MenuStyle::LoadFonts();
            MenuStyle::Apply();
            ImGui_ImplWin32_Init(g_hwnd);
            ImGui_ImplDX11_Init(pDevice, pContext);
            s_imguiInitialized = true;
        } else {
            HRESULT hr = oPresent(pSwapChain, SyncInterval, Flags);
            InterlockedDecrement(&Runtime::g_inPresent);
            return hr;
        }
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    Menu::Draw();

    if (InterlockedCompareExchange(&Runtime::g_unloadRequested, 0, 0) == 0) {
        ESPRender::Draw();
    }

    if (InterlockedCompareExchange(&Runtime::g_unloadRequested, 0, 0) == 0) {
        Overlay::Draw();
    }

    if (InterlockedCompareExchange(&Runtime::g_unloadRequested, 0, 0) == 0) {
        Radar::Draw();
    }

    ImGui::Render();
    pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    HRESULT hr = oPresent(pSwapChain, SyncInterval, Flags);
    InterlockedDecrement(&Runtime::g_inPresent);
    return hr;
}

static HRESULT __stdcall hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
    if (InterlockedCompareExchange(&Runtime::g_unloadRequested, 0, 0) != 0) {
        return oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    }

    if (mainRenderTargetView) {
        if (pContext) pContext->OMSetRenderTargets(0, 0, 0);
        mainRenderTargetView->Release();
        mainRenderTargetView = NULL;
    }
    HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    ID3D11Texture2D* pBackBuffer = NULL;
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
// MainThread: IL2CPP boot + D3D11 hook install
// ============================================================
static void* FindStaticFields(void* classObj, void (*onFound)(void* candidate, int offset)) {
    return NULL; // unused, here to keep the file pattern open if needed later
}

// ----------------------------------------------------------------
// Static-field scans. Both classes hold their static block at an
// offset inside the IL2CPP class object that's only populated after
// the class's static constructor (.cctor) has run.
//
// PlayerRoot.cctor fires on first instantiation (match start).
// GameClient.cctor fires when the singleton initializes.
//
// On some machines (and any injection-before-match flow) these
// haven't run yet at MainThread time, so the original one-shot
// scan would fail and the cheat would stay gated forever. Splitting
// these out lets the ESP data thread retry them.
// ----------------------------------------------------------------

// Find the PlayerRoot class. Cached after the first successful lookup so
// every retry doesn't re-walk the assembly list.
static void* s_playerRootClass = NULL;

static void* ResolvePlayerRootClass() {
    if (s_playerRootClass) return s_playerRootClass;
    if (!IL2CPP::fn_class_from_name || !IL2CPP::fn_domain_get || !IL2CPP::fn_domain_get_assemblies) {
        return NULL;
    }

    void* battleImage = IL2CPP::FindImage("_CombatMaster.Battle");
    if (battleImage) {
        s_playerRootClass = IL2CPP::fn_class_from_name(battleImage,
            "CombatMaster.Battle.Gameplay.Player", "PlayerRoot");
        if (s_playerRootClass) return s_playerRootClass;
    }

    size_t asmCount = 0;
    void** asms = IL2CPP::fn_domain_get_assemblies(IL2CPP::fn_domain_get(), &asmCount);
    if (!asms) return NULL;
    for (size_t i = 0; i < asmCount; i++) {
        void* assembly = asms[i];
        if (!assembly) continue;
        void* image = *(void**)assembly;
        if (!image) continue;
        s_playerRootClass = IL2CPP::fn_class_from_name(image,
            "CombatMaster.Battle.Gameplay.Player", "PlayerRoot");
        if (s_playerRootClass) return s_playerRootClass;
    }
    return NULL;
}

// PlayerRoot static block validation: AllPlayers list lives at +0x18 of the
// static block, so a candidate is "real" if it points at a plausible
// Il2CppList<PlayerRoot> (size in [0,100], non-null _items).
static bool ScanPlayerRootStatics() {
    if (g_state.staticFields) return true;

    void* playerRootClass = ResolvePlayerRootClass();
    if (!playerRootClass) return false;

    uint8_t* classBytes = (uint8_t*)playerRootClass;
    for (int off = 0xB0; off < 0x200; off += 8) {
        __try {
            void* candidate = *(void**)(classBytes + off);
            if (!candidate || (uintptr_t)candidate < 0x10000) continue;
            void* testAllPlayers = *(void**)((uint8_t*)candidate + 0x18);
            if (testAllPlayers && (uintptr_t)testAllPlayers > 0x10000) {
                Il2CppList* list = (Il2CppList*)testAllPlayers;
                if (list->_size >= 0 && list->_size < 100 && list->_items
                    && (uintptr_t)list->_items > 0x10000) {
                    g_state.staticFields = candidate;
                    Log("staticFields found at offset 0x%X: %p", off, candidate);
                    return true;
                }
            }
        } __except(1) {}
    }
    return false;
}

// GameClient class is cached the same way.
static void* s_gameClientClass = NULL;

static void* ResolveGameClientClass() {
    if (s_gameClientClass) return s_gameClientClass;
    if (!IL2CPP::fn_class_from_name || !IL2CPP::fn_domain_get || !IL2CPP::fn_domain_get_assemblies) {
        return NULL;
    }

    size_t asmCount = 0;
    void** asms = IL2CPP::fn_domain_get_assemblies(IL2CPP::fn_domain_get(), &asmCount);
    if (!asms) return NULL;
    for (size_t i = 0; i < asmCount; i++) {
        void* assembly = asms[i];
        if (!assembly) continue;
        void* image = *(void**)assembly;
        if (!image) continue;
        s_gameClientClass = IL2CPP::fn_class_from_name(image, "CombatMaster.Client", "GameClient");
        if (s_gameClientClass) return s_gameClientClass;
    }
    return NULL;
}

// GameClient static block validation: _ref lives at +0x0, ProfileCache at
// _ref+0x60. We only accept a candidate if both indirections are plausible
// (non-null, non-tiny). Until the singleton wakes up, _ref is null and we
// don't latch.
static bool ScanGameClientStatics() {
    if (g_state.gameClientStaticFields) return true;

    void* gameClientClass = ResolveGameClientClass();
    if (!gameClientClass) return false;

    uint8_t* gcBytes = (uint8_t*)gameClientClass;
    for (int off = 0xB0; off < 0x200; off += 8) {
        __try {
            void* candidate = *(void**)(gcBytes + off);
            if (!candidate || (uintptr_t)candidate < 0x10000) continue;
            void* ref = *(void**)((uint8_t*)candidate + 0x0);
            if (IsPlausiblePtr(ref)) {
                void* profileTest = *(void**)((uint8_t*)ref + 0x60);
                if (IsPlausiblePtr(profileTest)) {
                    g_state.gameClientStaticFields = candidate;
                    Log("GameClient statics at 0x%X: %p (_ref=%p)", off, candidate, ref);
                    return true;
                }
            }
        } __except(1) {}
    }
    return false;
}

// Public retry hook used by the ESP data thread. Idempotent and cheap.
void PollStaticFields() {
    if (!g_state.staticFields)           ScanPlayerRootStatics();
    if (!g_state.gameClientStaticFields) ScanGameClientStatics();
}

DWORD WINAPI MainThread(LPVOID /*p*/) {
    char mutexName[128];
    snprintf(mutexName, sizeof(mutexName), "Local\\cm_hax_single_init_%lu", GetCurrentProcessId());
    Runtime::g_singleInstanceMutex = CreateMutexA(NULL, TRUE, mutexName);
    if (!Runtime::g_singleInstanceMutex) {
        Log("CreateMutexA failed: %lu", GetLastError());
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        Log("Duplicate MainThread blocked by mutex: %s", mutexName);
        CloseHandle(Runtime::g_singleInstanceMutex);
        Runtime::g_singleInstanceMutex = NULL;
        return 0;
    }

    ResetLog();
    InitializeCriticalSection(&Runtime::g_espCs);
    Runtime::g_csInitialized = true;
    Log("MainThread started");

    // 1. Wait for Project.dll, resolve IL2CPP
    HMODULE hProject = NULL;
    while (!hProject) {
        hProject = GetModuleHandleA("Project.dll");
        if (!hProject) Sleep(500);
    }
    Log("Project.dll found at %p", hProject);

    if (!IL2CPP::ResolveAll(hProject)) return 1;

    if (!IL2CPP::fn_get_main_camera || !IL2CPP::fn_get_worldToCamera || !IL2CPP::fn_get_projectionMatrix || !IL2CPP::fn_component_get_transform) {
        Log("One or more dynamic Unity methods failed to resolve; ESP will remain disabled");
        g_state.espEnabled = false;
    }

    // 2-3. Locate PlayerRoot + GameClient static fields.
    //
    // Both rely on the target class's static constructor having run, which
    // doesn't always happen by the time we get here (especially when the
    // user injects from the lobby / loading screen). PollStaticFields() is
    // idempotent; the ESP data thread retries it every cycle until both
    // are resolved, so this initial call just gets us a head start.
    PollStaticFields();
    if (!g_state.staticFields) {
        Log("staticFields not yet resolved at startup; will retry from data thread");
    }
    if (!g_state.gameClientStaticFields) {
        Log("GameClient static fields not yet resolved at startup; will retry from data thread");
    }

    // 4. D3D11 hook
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
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    IDXGISwapChain* swapChain = NULL;
    ID3D11Device* dummyDevice = NULL;
    ID3D11DeviceContext* dummyContext = NULL;

    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
            &swapChain, &dummyDevice, &featureLevel, &dummyContext) != S_OK) {
        return 1;
    }

    void** pVTable = *reinterpret_cast<void***>(swapChain);
    swapChain->Release();
    dummyDevice->Release();
    dummyContext->Release();

    MH_Initialize();
    MH_CreateHook(pVTable[8],  hkPresent,       (void**)&oPresent);
    MH_CreateHook(pVTable[13], hkResizeBuffers, (void**)&oResizeBuffers);
    MH_EnableHook(MH_ALL_HOOKS);

    Runtime::g_dataThread = CreateThread(0, 0, ESP::DataThread, 0, 0, 0);
    Log("MainThread init finished, ESPDataThread started");

    Config::Load();

    return 0;
}

} // namespace Hooks
