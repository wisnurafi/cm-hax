// core/logging.cpp
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "logging.h"

// Resolved on first use. Tries paths in priority order, picks the first one
// we can actually open for writing. The previous implementation always wrote
// to %USERPROFILE%\Desktop, which is silently broken on a lot of consumer
// installs (OneDrive Known Folder Move redirects it; Defender Controlled
// Folder Access blocks writes from non-allowlisted processes), causing the
// log file to never appear and removing all field-debug ability.
static char  s_logPath[MAX_PATH] = {};
static bool  s_logPathResolved   = false;

// True if `dir` exists or was created. Treats "already exists, not a dir" as
// a failure so we don't try to write a file inside a stray file.
static bool EnsureDirectoryExists(const char* dir) {
    DWORD attr = GetFileAttributesA(dir);
    if (attr != INVALID_FILE_ATTRIBUTES) {
        return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    if (CreateDirectoryA(dir, NULL)) return true;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

// Side effect: creates the file if it didn't exist. That's intentional - if
// we keep the candidate, we're going to write to it anyway. Rejected
// candidates fail before this point so they never create stray files.
static bool TestWritable(const char* path) {
    FILE* f = fopen(path, "a");
    if (!f) return false;
    fclose(f);
    return true;
}

// `<envVar>\CmHax\cm_hax_log.txt`, creating the folder if missing. Used for
// both APPDATA (Roaming) and LOCALAPPDATA fallbacks.
static bool TryEnvFolderPath(const char* envVar, char* out, size_t outSize) {
    char base[MAX_PATH];
    DWORD n = GetEnvironmentVariableA(envVar, base, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return false;

    char folder[MAX_PATH];
    if (snprintf(folder, sizeof(folder), "%s\\CmHax", base) >= (int)sizeof(folder)) return false;
    if (!EnsureDirectoryExists(folder)) return false;

    if (snprintf(out, (int)outSize, "%s\\cm_hax_log.txt", folder) >= (int)outSize) return false;
    return TestWritable(out);
}

// `<dir of this DLL>\cm_hax_log.txt`. Uses GetModuleHandleEx so we don't
// have to depend on Runtime::g_module being initialized yet.
static bool TryNextToDllPath(char* out, size_t outSize) {
    HMODULE self = NULL;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&TryNextToDllPath, &self) || !self) {
        return false;
    }
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(self, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return false;

    char* slash = strrchr(buf, '\\');
    if (!slash) return false;
    *(slash + 1) = '\0';

    if (strlen(buf) + strlen("cm_hax_log.txt") + 1 > outSize) return false;
    snprintf(out, (int)outSize, "%scm_hax_log.txt", buf);
    return TestWritable(out);
}

// `<envVar>\<subdir>\cm_hax_log.txt` for legacy/desktop fallbacks where we
// don't want to create a CmHax subfolder.
static bool TryEnvSubdirPath(const char* envVar, const char* subdir,
                             char* out, size_t outSize) {
    char base[MAX_PATH];
    DWORD n = GetEnvironmentVariableA(envVar, base, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return false;

    if (snprintf(out, (int)outSize, "%s\\%s\\cm_hax_log.txt", base, subdir) >= (int)outSize) return false;
    return TestWritable(out);
}

static void ResolveLogPathOnce() {
    if (s_logPathResolved) return;
    s_logPathResolved = true;

    // 1. %APPDATA%\CmHax - same folder Config uses. Always writable, not
    //    affected by OneDrive Known Folder Move or Defender Controlled
    //    Folder Access. This is the new default.
    if (TryEnvFolderPath("APPDATA", s_logPath, sizeof(s_logPath))) return;

    // 2. %LOCALAPPDATA%\CmHax - belt-and-suspenders for the rare case where
    //    Roaming is mapped to a network share that's offline.
    if (TryEnvFolderPath("LOCALAPPDATA", s_logPath, sizeof(s_logPath))) return;

    // 3. Next to the DLL. Useful for portable copies where the user dropped
    //    everything in one folder.
    if (TryNextToDllPath(s_logPath, sizeof(s_logPath))) return;

    // 4. %TEMP%\cm_hax_log.txt - last resort under the user profile.
    {
        char temp[MAX_PATH];
        DWORD n = GetEnvironmentVariableA("TEMP", temp, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            if (snprintf(s_logPath, sizeof(s_logPath), "%s\\cm_hax_log.txt", temp) < (int)sizeof(s_logPath)
                && TestWritable(s_logPath)) {
                return;
            }
        }
    }

    // 5. Old Desktop path. Kept solely for backwards compatibility with
    //    users who were already looking there. Often broken (OneDrive/CFA),
    //    which is why it's last.
    if (TryEnvSubdirPath("USERPROFILE", "Desktop", s_logPath, sizeof(s_logPath))) return;

    // 6. CWD. Almost always writable, but where we land depends on how the
    //    process was launched - log "lost" in the game install dir is fine
    //    if everything else failed.
    snprintf(s_logPath, sizeof(s_logPath), "cm_hax_log.txt");
    if (!TestWritable(s_logPath)) {
        s_logPath[0] = '\0'; // nothing worked; Log() and ResetLog() become no-ops
    }
}

const char* LogPath() {
    ResolveLogPathOnce();
    return s_logPath[0] ? s_logPath : NULL;
}

void ResetLog() {
    ResolveLogPathOnce();
    if (!s_logPath[0]) return;

    FILE* f = fopen(s_logPath, "w");
    if (!f) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "==== New Cm-Hax session PID %lu at %04u-%02u-%02u %02u:%02u:%02u ====\n",
        GetCurrentProcessId(), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    fprintf(f, "Log path: %s\n", s_logPath);
    fclose(f);
}

void Log(const char* format, ...) {
    ResolveLogPathOnce();
    if (!s_logPath[0]) return;

    FILE* f = fopen(s_logPath, "a");
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
