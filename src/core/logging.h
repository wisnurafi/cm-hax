// core/logging.h
// Lightweight logger that appends to a desktop file. Cheap and best-effort.
#pragma once

void ResetLog();
void Log(const char* format, ...);
