// core/hooks.h
// D3D11 swapchain hooks + window subclass + lifecycle (Init / RequestUnload).
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Hooks {
    // Boots the IL2CPP resolver, finds static fields, sets up the D3D11 hook,
    // and starts the ESP data thread. Called once from MainThread.
    DWORD WINAPI MainThread(LPVOID lpReserved);

    // Called from the menu / END key. Tears everything down on a worker
    // thread and frees the DLL.
    void RequestUnload();
}
