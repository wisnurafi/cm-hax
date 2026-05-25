// injector.cpp - Watches for CombatMaster.exe and injects the selected DLL
// Compile: cl /EHsc /O2 injector.cpp /link /OUT:injector.exe
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "advapi32.lib")

struct InjectionTarget {
    const char* displayName;
    const char* dllName;
    const char* successMessage;
};

DWORD FindProcess(const char* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    
    DWORD pid = 0;
    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, name) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32Next(snap, &pe));
    }
    
    CloseHandle(snap);
    return pid;
}

int EnableDebugPrivilege() {
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        return 0;
    }

    TOKEN_PRIVILEGES tp;
    LUID luid;
    if (!LookupPrivilegeValueA(NULL, SE_DEBUG_NAME, &luid)) {
        CloseHandle(token);
        return 0;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), NULL, NULL);
    DWORD err = GetLastError();
    CloseHandle(token);

    return err == ERROR_SUCCESS;
}

int InjectDLL(DWORD pid, const char* dllPath) {
    printf("[*] Opening process %lu...\n", pid);
    
    HANDLE hProc = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | 
        PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
        FALSE, pid
    );
    
    if (!hProc) {
        printf("[!] OpenProcess failed: %lu\n", GetLastError());
        return 0;
    }
    
    // Allocate memory in target for DLL path
    size_t pathLen = strlen(dllPath) + 1;
    LPVOID remoteMem = VirtualAllocEx(hProc, NULL, pathLen, 
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) {
        printf("[!] VirtualAllocEx failed: %lu\n", GetLastError());
        CloseHandle(hProc);
        return 0;
    }
    
    // Write DLL path
    if (!WriteProcessMemory(hProc, remoteMem, dllPath, pathLen, NULL)) {
        printf("[!] WriteProcessMemory failed: %lu\n", GetLastError());
        VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 0;
    }
    
    // Get LoadLibraryA address
    HMODULE hKernel = GetModuleHandleA("kernel32.dll");
    FARPROC loadLib = GetProcAddress(hKernel, "LoadLibraryA");
    
    // Create remote thread
    printf("[*] Injecting DLL...\n");
    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, 
                                         (LPTHREAD_START_ROUTINE)loadLib,
                                         remoteMem, 0, NULL);
    if (!hThread) {
        printf("[!] CreateRemoteThread failed: %lu\n", GetLastError());
        VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 0;
    }
    
    // Wait for injection
    WaitForSingleObject(hThread, 10000);
    
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    
    CloseHandle(hThread);
    VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProc);
    
    if (exitCode == 0) {
        printf("[!] LoadLibrary returned NULL - DLL may have failed to load\n");
        return 0;
    }
    
    printf("[+] DLL injected successfully! Module handle: 0x%lx\n", exitCode);
    return 1;
}

int ResolveDllPath(const char* dllName, char* dllPath, size_t dllPathSize) {
    if (!GetModuleFileNameA(NULL, dllPath, (DWORD)dllPathSize)) {
        return 0;
    }

    char* lastSlash = strrchr(dllPath, '\\');
    if (lastSlash) {
        size_t dirLen = (size_t)(lastSlash - dllPath) + 1;
        if (dirLen + strlen(dllName) + 1 > dllPathSize) {
            return 0;
        }
        strcpy_s(lastSlash + 1, dllPathSize - dirLen, dllName);
    } else {
        if (strlen(dllName) + 1 > dllPathSize) {
            return 0;
        }
        strcpy_s(dllPath, dllPathSize, dllName);
    }

    return 1;
}

const InjectionTarget* SelectTargetFromArgs(int argc, char* argv[], const InjectionTarget* menuTarget, const InjectionTarget* dumperTarget) {
    if (argc <= 1) {
        return NULL;
    }

    if (_stricmp(argv[1], "--menu") == 0 || _stricmp(argv[1], "--esp") == 0) {
        return menuTarget;
    }

    if (_stricmp(argv[1], "--dump") == 0 || _stricmp(argv[1], "--dumper") == 0) {
        return dumperTarget;
    }

    return NULL;
}

const InjectionTarget* PromptForTarget(const InjectionTarget* menuTarget, const InjectionTarget* dumperTarget) {
    for (;;) {
        printf("Choose DLL to inject:\n");
        printf("  1) Menu  (%s)\n", menuTarget->dllName);
        printf("  2) Dumper (%s)\n", dumperTarget->dllName);
        printf("  q) Quit\n\n");
        printf("> ");

        char input[32] = {};
        if (!fgets(input, sizeof(input), stdin)) {
            return NULL;
        }

        if (input[0] == '1') {
            return menuTarget;
        }
        if (input[0] == '2') {
            return dumperTarget;
        }
        if (input[0] == 'q' || input[0] == 'Q') {
            return NULL;
        }

        printf("\n[!] Unknown choice. Please enter 1, 2, or q.\n\n");
    }
}

int main(int argc, char* argv[]) {
    const InjectionTarget menuTarget = {
        "Menu",
        "esp_imgui.dll",
        "Menu DLL injected. The overlay should initialize in game."
    };
    const InjectionTarget dumperTarget = {
        "Dumper",
        "dumper.dll",
        "Dumper DLL injected. Check the desktop dumper log/output after it finishes."
    };

    printf("============================================================\n");
    printf("  Combat Master - DLL Injector\n");
    printf("============================================================\n\n");

    const InjectionTarget* target = SelectTargetFromArgs(argc, argv, &menuTarget, &dumperTarget);
    if (!target) {
        target = PromptForTarget(&menuTarget, &dumperTarget);
    }

    if (!target) {
        printf("\n[*] Exiting.\n");
        return 0;
    }

    char dllPath[MAX_PATH];
    if (!ResolveDllPath(target->dllName, dllPath, sizeof(dllPath))) {
        printf("[!] Failed to resolve DLL path for %s\n", target->dllName);
        return 1;
    }

    // Check DLL exists
    if (GetFileAttributesA(dllPath) == INVALID_FILE_ATTRIBUTES) {
        printf("[!] DLL not found: %s\n", dllPath);
        printf("[!] Make sure %s is in the same folder as injector.exe\n", target->dllName);
        return 1;
    }

    printf("[*] Selected: %s\n", target->displayName);
    printf("[*] DLL path: %s\n", dllPath);
    if (!EnableDebugPrivilege()) {
        printf("[!] Could not enable debug privilege. If injection fails, run as Administrator.\n");
    }
    printf("\n");
    
    const char* procName = "CombatMaster.exe";
    printf("[*] Waiting for %s...\n", procName);
    printf("[*] Launch the game now!\n\n");
    
    DWORD pid = 0;
    while (!pid) {
        pid = FindProcess(procName);
        if (!pid) {
            Sleep(100); // Poll every 100ms
        }
    }
    
    printf("[+] Found %s (PID: %lu)\n", procName, pid);
    
    // Wait a moment for the process to initialize
    printf("[*] Waiting 3 seconds for process initialization...\n");
    Sleep(3000);
    
    // Inject
    if (InjectDLL(pid, dllPath)) {
        printf("[+] Injection complete!\n");
        printf("[*] %s\n", target->successMessage);
        Sleep(1000);
        return 0;
    } else {
        printf("[!] Injection failed!\n");
        Sleep(3000);
        return 1;
    }
}
