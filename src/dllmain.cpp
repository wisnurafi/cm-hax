// src/dllmain.cpp
// DLL entry. Single MainThread is started under a process-local mutex so
// duplicate injections become no-ops.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core/globals.h"
#include "core/hooks.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*reserved*/) {
    if (reason == DLL_PROCESS_ATTACH) {
        Runtime::g_module = hModule;
        InterlockedExchange(&Runtime::g_running, 1);
        InterlockedExchange(&Runtime::g_unloadRequested, 0);
        DisableThreadLibraryCalls(hModule);
        if (InterlockedCompareExchange(&Runtime::g_mainThreadStarted, 1, 0) == 0) {
            HANDLE hThread = CreateThread(0, 0, Hooks::MainThread, 0, 0, 0);
            if (hThread) CloseHandle(hThread);
        }
    }
    return TRUE;
}
