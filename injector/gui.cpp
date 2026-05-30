// injector/gui.cpp
// Standalone ImGui + D3D11 window for the injector.
// Matches the in-game menu's frosted glass style.
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <d3d11.h>
#include <stdio.h>
#include <string.h>

#include "gui.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Forward declarations from main.cpp
extern int ManualMapInject(DWORD pid, const char* dllPath);
extern DWORD FindProcess(const char* name);
extern int EnableDebugPrivilege();
extern int ResolveDllPath(const char* dllName, char* dllPath, size_t dllPathSize);

namespace GUI {

// D3D11 state
static ID3D11Device*           g_pd3dDevice = NULL;
static ID3D11DeviceContext*    g_pd3dContext = NULL;
static IDXGISwapChain*         g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_pRTV = NULL;
static HWND                    g_hwnd = NULL;
static WNDCLASSEXW             g_wc = {};

// Fonts
static ImFont* g_fontBody  = NULL;
static ImFont* g_fontTitle = NULL;
static ImFont* g_fontSmall = NULL;

// Injector state
static char g_log[4096] = {};
static int  g_logLen = 0;
static bool g_injecting = false;
static bool g_injected = false;
static HANDLE g_injectThread = NULL;

struct InjectArgs {
    char dllPath[MAX_PATH];
    char dllName[64];
};
static InjectArgs g_injectArgs = {};

static void AppendLog(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    int len = (int)strlen(buf);
    if (g_logLen + len + 2 < (int)sizeof(g_log)) {
        memcpy(g_log + g_logLen, buf, len);
        g_logLen += len;
        g_log[g_logLen++] = '\n';
        g_log[g_logLen] = '\0';
    }
}

// Injection thread
static DWORD WINAPI InjectThreadProc(LPVOID param) {
    InjectArgs* args = (InjectArgs*)param;
    
    AppendLog("[*] Waiting for CombatMaster.exe...");
    
    // Check if Steam is running, open it if not
    DWORD steamPid = FindProcess("steam.exe");
    if (!steamPid) {
        AppendLog("[*] Opening Steam...");
        char steamPath[MAX_PATH] = {};
        DWORD steamPathLen = sizeof(steamPath);
        HKEY hKey = NULL;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            if (RegQueryValueExA(hKey, "SteamExe", NULL, NULL, (LPBYTE)steamPath, &steamPathLen) == ERROR_SUCCESS) {
                ShellExecuteA(NULL, "open", steamPath, NULL, NULL, SW_SHOWDEFAULT);
            }
            RegCloseKey(hKey);
        }
        while (!steamPid) {
            steamPid = FindProcess("steam.exe");
            if (!steamPid) Sleep(500);
        }
        AppendLog("[+] Steam detected.");
        Sleep(3000);
    }

    // Bring Steam to front
    DWORD pid = FindProcess("CombatMaster.exe");
    if (!pid) {
        ShellExecuteA(NULL, "open", "steam://open/games/details/2105420", NULL, NULL, SW_SHOWDEFAULT);
        AppendLog("[*] Press PLAY in Steam.");
        
        while (!pid) {
            pid = FindProcess("CombatMaster.exe");
            if (!pid) Sleep(100);
        }
    }
    
    AppendLog("[+] Game found (PID: %lu)", pid);
    AppendLog("[*] Waiting for Project.dll...");

    // Wait for Project.dll
    bool foundDll = false;
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProc) {
        for (int attempt = 0; attempt < 120; attempt++) {
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
            if (snap != INVALID_HANDLE_VALUE) {
                MODULEENTRY32 me;
                me.dwSize = sizeof(me);
                if (Module32First(snap, &me)) {
                    do {
                        if (_stricmp(me.szModule, "Project.dll") == 0) {
                            foundDll = true;
                            break;
                        }
                    } while (Module32Next(snap, &me));
                }
                CloseHandle(snap);
            }
            if (foundDll) break;
            Sleep(500);
        }
        CloseHandle(hProc);
    }

    if (foundDll) {
        AppendLog("[+] Project.dll loaded.");
        Sleep(3000);
    } else {
        AppendLog("[!] Timeout waiting for Project.dll. Trying anyway...");
    }

    AppendLog("[*] Injecting %s...", args->dllName);
    
    if (ManualMapInject(pid, args->dllPath)) {
        AppendLog("[+] Injection successful!");
        AppendLog("[+] HWID spoof active. You're safe.");
        g_injected = true;
    } else {
        AppendLog("[!] Injection failed!");
    }

    g_injecting = false;
    return 0;
}

static void StartInject(const char* dllName) {
    if (g_injecting) return;
    
    if (!ResolveDllPath(dllName, g_injectArgs.dllPath, sizeof(g_injectArgs.dllPath))) {
        AppendLog("[!] Failed to resolve DLL path");
        return;
    }
    if (GetFileAttributesA(g_injectArgs.dllPath) == INVALID_FILE_ATTRIBUTES) {
        AppendLog("[!] DLL not found: %s", g_injectArgs.dllPath);
        return;
    }
    
    strncpy_s(g_injectArgs.dllName, dllName, sizeof(g_injectArgs.dllName) - 1);
    g_injecting = true;
    g_injected = false;
    g_logLen = 0;
    g_log[0] = '\0';
    
    EnableDebugPrivilege();
    g_injectThread = CreateThread(NULL, 0, InjectThreadProc, &g_injectArgs, 0, NULL);
    if (g_injectThread) CloseHandle(g_injectThread);
}

// Style (matches menu)
static void ApplyStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.WindowRounding = 10.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(16.0f, 16.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]             = ImVec4(0.92f, 0.94f, 0.97f, 1.00f);
    colors[ImGuiCol_TextDisabled]     = ImVec4(0.32f, 0.36f, 0.42f, 1.00f);
    colors[ImGuiCol_WindowBg]         = ImVec4(0.04f, 0.045f, 0.06f, 0.97f);
    colors[ImGuiCol_ChildBg]          = ImVec4(0.05f, 0.055f, 0.07f, 0.60f);
    colors[ImGuiCol_Border]           = ImVec4(0.22f, 0.26f, 0.32f, 0.30f);
    colors[ImGuiCol_FrameBg]          = ImVec4(0.08f, 0.09f, 0.12f, 0.70f);
    colors[ImGuiCol_FrameBgHovered]   = ImVec4(0.12f, 0.14f, 0.18f, 0.80f);
    colors[ImGuiCol_FrameBgActive]    = ImVec4(0.16f, 0.18f, 0.24f, 0.85f);
    colors[ImGuiCol_Button]           = ImVec4(0.10f, 0.12f, 0.15f, 0.65f);
    colors[ImGuiCol_ButtonHovered]    = ImVec4(0.15f, 0.18f, 0.22f, 0.80f);
    colors[ImGuiCol_ButtonActive]     = ImVec4(0.20f, 0.24f, 0.30f, 0.90f);
    colors[ImGuiCol_ScrollbarBg]      = ImVec4(0.04f, 0.045f, 0.06f, 0.00f);
    colors[ImGuiCol_ScrollbarGrab]    = ImVec4(0.18f, 0.20f, 0.26f, 0.60f);
}

static void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.OversampleH = 3;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = true;

    char winDir[MAX_PATH] = {};
    GetWindowsDirectoryA(winDir, MAX_PATH);

    char segoe[MAX_PATH], segoesb[MAX_PATH];
    snprintf(segoe,   sizeof(segoe),   "%s\\Fonts\\segoeui.ttf", winDir);
    snprintf(segoesb, sizeof(segoesb), "%s\\Fonts\\seguisb.ttf", winDir);

    if (GetFileAttributesA(segoe) != INVALID_FILE_ATTRIBUTES) {
        g_fontBody  = io.Fonts->AddFontFromFileTTF(segoe, 15.0f, &cfg);
        g_fontSmall = io.Fonts->AddFontFromFileTTF(segoe, 12.0f, &cfg);
    }
    if (GetFileAttributesA(segoesb) != INVALID_FILE_ATTRIBUTES) {
        g_fontTitle = io.Fonts->AddFontFromFileTTF(segoesb, 22.0f, &cfg);
    }

    if (!g_fontBody)  g_fontBody  = io.Fonts->AddFontDefault();
    if (!g_fontSmall) g_fontSmall = g_fontBody;
    if (!g_fontTitle) g_fontTitle = g_fontBody;
    io.FontDefault = g_fontBody;
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL levels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
        levels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dContext);
    if (hr != S_OK) return false;

    ID3D11Texture2D* pBackBuffer = NULL;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRTV);
    pBackBuffer->Release();
    return true;
}

static void CleanupDeviceD3D() {
    if (g_pRTV)       { g_pRTV->Release();       g_pRTV = NULL; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dContext) { g_pd3dContext->Release(); g_pd3dContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release();  g_pd3dDevice = NULL; }
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
            if (g_pRTV) { g_pRTV->Release(); g_pRTV = NULL; }
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            ID3D11Texture2D* pBackBuffer = NULL;
            g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRTV);
            pBackBuffer->Release();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void RenderUI() {
    const ImVec4 kAccent  = ImVec4(0.45f, 0.82f, 1.00f, 1.00f);
    const ImVec4 kSuccess = ImVec4(0.35f, 0.92f, 0.55f, 1.00f);
    const ImVec4 kDanger  = ImVec4(0.95f, 0.35f, 0.35f, 1.00f);
    const ImVec4 kMuted   = ImVec4(0.45f, 0.50f, 0.58f, 1.00f);

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##main", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Header
    ImGui::PushFont(g_fontTitle);
    ImGui::TextColored(kAccent, "Cm-Hax");
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::PushFont(g_fontSmall);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
    ImGui::TextColored(kMuted, "Stealth Injector");
    ImGui::PopFont();

    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 8));

    // Status indicators
    ImGui::PushFont(g_fontSmall);
    DWORD steamPid = FindProcess("steam.exe");
    DWORD gamePid = FindProcess("CombatMaster.exe");
    ImGui::TextColored(steamPid ? kSuccess : kDanger, "Steam: %s", steamPid ? "Running" : "Not Running");
    ImGui::SameLine(200);
    ImGui::TextColored(gamePid ? kSuccess : kDanger, "Game: %s", gamePid ? "Running" : "Not Running");
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 12));

    // Action buttons
    float btnWidth = (ImGui::GetContentRegionAvail().x - 10.0f) * 0.5f;
    float btnHeight = 40.0f;

    bool disabled = g_injecting || g_injected;
    if (disabled) ImGui::BeginDisabled();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.15f, 0.22f, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.22f, 0.32f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.28f, 0.40f, 1.00f));
    if (ImGui::Button("Load Menu", ImVec2(btnWidth, btnHeight))) {
        StartInject("cm_hax.dll");
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.10f, 0.08f, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.15f, 0.12f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.20f, 0.16f, 1.00f));
    if (ImGui::Button("Dump Offsets", ImVec2(btnWidth, btnHeight))) {
        StartInject("dumper.dll");
    }
    ImGui::PopStyleColor(3);

    if (disabled) ImGui::EndDisabled();

    ImGui::Dummy(ImVec2(0, 12));

    // Status pill
    if (g_injected) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.15f, 0.08f, 0.60f));
        ImGui::BeginChild("##status", ImVec2(-1, 28), true);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
        ImGui::TextColored(kSuccess, "  Injected successfully. HWID spoof is active.");
        ImGui::EndChild();
        ImGui::PopStyleColor();
    } else if (g_injecting) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.05f, 0.60f));
        ImGui::BeginChild("##status", ImVec2(-1, 28), true);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
        ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.28f, 1.0f), "  Injecting...");
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::Dummy(ImVec2(0, 8));

    // Log area
    ImGui::PushFont(g_fontSmall);
    ImGui::TextColored(kMuted, "Log");
    ImGui::PopFont();
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.03f, 0.035f, 0.045f, 0.80f));
    float logHeight = ImGui::GetContentRegionAvail().y - 4.0f;
    ImGui::BeginChild("##log", ImVec2(-1, logHeight), true);
    ImGui::PushFont(g_fontSmall);
    if (g_logLen > 0) {
        ImGui::TextUnformatted(g_log, g_log + g_logLen);
        // Auto-scroll
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f)
            ImGui::SetScrollHereY(1.0f);
    } else {
        ImGui::TextColored(kMuted, "Ready. Click 'Load Menu' or 'Dump Offsets' to begin.");
    }
    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
}

int Run() {
    // Create window
    g_wc = { sizeof(g_wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL),
             NULL, NULL, NULL, NULL, L"CmHaxInjector", NULL };
    RegisterClassExW(&g_wc);
    g_hwnd = CreateWindowW(g_wc.lpszClassName, L"Cm-Hax",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        100, 100, 520, 400, NULL, NULL, g_wc.hInstance, NULL);

    if (!CreateDeviceD3D(g_hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);
        return 1;
    }

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL;

    LoadFonts();
    ApplyStyle();

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

    // Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RenderUI();

        ImGui::Render();
        const float clear[4] = { 0.02f, 0.025f, 0.03f, 1.0f };
        g_pd3dContext->OMSetRenderTargets(1, &g_pRTV, NULL);
        g_pd3dContext->ClearRenderTargetView(g_pRTV, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0); // VSync
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(g_hwnd);
    UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);

    return 0;
}

} // namespace GUI
