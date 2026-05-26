// core/config.h
// JSON-style config persistence. Stored at:
//   %APPDATA%\CmHax\cm_hax.cfg
// Falls back to a sibling-DLL location when AppData isn't available.
// Auto-migrates an old DLL-folder config the first time it's loaded.
#pragma once

namespace Config {
    void Load();
    void Save();

    // Footer hint string + last update tick for the UI.
    extern char  g_statusMsg[96];
    extern unsigned long g_statusTime;
}
