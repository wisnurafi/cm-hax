// features/esp.h
// ESP data collection thread. Walks the PlayerRoot list every ~16ms,
// resolves bones / hitboxes / health and writes the results into
// Runtime::g_espData under a critical section for the renderer to consume.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace ESP {
    DWORD WINAPI DataThread(LPVOID lpReserved);
}
