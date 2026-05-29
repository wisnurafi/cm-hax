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

    // Idempotent retry of the PlayerRoot + GameClient static-field scans.
    // Cheap when both are already resolved (early-outs). Safe to call from
    // any thread that's been attached to the IL2CPP domain.
    //
    // The scans depend on the target class's static constructor having run,
    // which doesn't happen until the relevant subsystem initializes
    // (PlayerRoot.cctor fires on first instantiation, i.e. when a match
    // actually starts). MainThread runs both once at startup; the ESP data
    // thread polls this from its loop so injection-while-in-lobby still
    // self-heals once the user enters a match.
    void PollStaticFields();
}
