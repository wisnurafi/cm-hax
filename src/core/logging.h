// core/logging.h
// Lightweight logger that appends to a file. Resolves the log path on first
// use, preferring %APPDATA%\CmHax (always writable) over Desktop (often
// hijacked by OneDrive Known Folder Move or Controlled Folder Access).
#pragma once

void        ResetLog();
void        Log(const char* format, ...);

// Returns the resolved absolute log path. Safe to call before ResetLog().
// Returns NULL only if every fallback failed (extremely unlikely).
const char* LogPath();
