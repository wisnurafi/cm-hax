// core/logging.cpp
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "logging.h"

// Resolved on the first call. Falls back to the current directory if the
// USERPROFILE env var isn't available.
static char  s_logPath[MAX_PATH] = {};
static bool  s_logPathResolved   = false;

static const char* ResolveLogPath() {
    if (s_logPathResolved) return s_logPath;
    s_logPathResolved = true;

    char userProfile[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableA("USERPROFILE", userProfile, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        snprintf(s_logPath, sizeof(s_logPath), "%s\\Desktop\\cm_hax_log.txt", userProfile);
    } else {
        snprintf(s_logPath, sizeof(s_logPath), "cm_hax_log.txt");
    }
    return s_logPath;
}

void ResetLog() {
    FILE* f = fopen(ResolveLogPath(), "w");
    if (!f) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "==== New Cm-Hax session PID %lu at %04u-%02u-%02u %02u:%02u:%02u ====\n",
        GetCurrentProcessId(), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    fclose(f);
}

void Log(const char* format, ...) {
    FILE* f = fopen(ResolveLogPath(), "a");
    if (!f) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%02u:%02u:%02u.%03u pid:%lu tid:%lu] ",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        GetCurrentProcessId(), GetCurrentThreadId());

    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);
    fprintf(f, "\n");
    fclose(f);
}
