// core/globals.cpp
#include "globals.h"

CmState g_state;

namespace Runtime {
    HMODULE         g_module               = NULL;
    HANDLE          g_dataThread           = NULL;
    volatile LONG   g_mainThreadStarted    = 0;
    volatile LONG   g_running              = 1;
    volatile LONG   g_unloadRequested      = 0;
    volatile LONG   g_inPresent            = 0;
    bool            g_csInitialized        = false;
    HANDLE          g_singleInstanceMutex  = NULL;

    SharedESPData     g_espData = {};
    CRITICAL_SECTION  g_espCs   = {};
}
