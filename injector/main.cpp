// injector.cpp - Manual Map injector (stealth)
// No LoadLibrary, no PEB entry. DLL is invisible to module enumeration.
// Uses CreateRemoteThread to execute the mapping shellcode.
// Compile: cl /EHsc /O2 injector.cpp /link /OUT:injector.exe advapi32.lib
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

// ================================================================
// Structures
// ================================================================
typedef BOOL(WINAPI* DllMain_t)(HINSTANCE, DWORD, LPVOID);
typedef HMODULE(WINAPI* pLoadLibraryA)(LPCSTR);
typedef FARPROC(WINAPI* pGetProcAddress)(HMODULE, LPCSTR);
typedef BOOL(WINAPI* pRtlAddFunctionTable)(PRUNTIME_FUNCTION, DWORD, DWORD64);

struct ManualMapData {
    LPVOID imageBase;
    pLoadLibraryA fnLoadLibraryA;
    pGetProcAddress fnGetProcAddress;
    pRtlAddFunctionTable fnRtlAddFunctionTable;
};

struct InjectionTarget {
    const char* displayName;
    const char* dllName;
    const char* successMessage;
};

// ================================================================
// Shellcode - runs inside target process
// IMPORTANT: This function must NOT reference any global/static data,
// string literals, or call any function not passed through ManualMapData.
// Compile with /O2 to minimize compiler-generated helpers.
// ================================================================
#pragma runtime_checks("", off)
#pragma optimize("", off)
static DWORD WINAPI Shellcode(ManualMapData* pData) {
    if (!pData) return 1;

    // Guard against double-execution (APC queued to multiple threads)
    // Use imageBase as the "already ran" flag — first thread to enter wins.
    BYTE* base = (BYTE*)pData->imageBase;
    if (!base) return 0; // Already executed by another thread

    // Atomically claim execution by zeroing imageBase
    // (simple race guard — not perfect but sufficient for our use case)
    pData->imageBase = NULL;

    IMAGE_DOS_HEADER* pDos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS* pNt = (IMAGE_NT_HEADERS*)(base + pDos->e_lfanew);
    IMAGE_OPTIONAL_HEADER* pOpt = &pNt->OptionalHeader;

    pLoadLibraryA _LoadLibraryA = pData->fnLoadLibraryA;
    pGetProcAddress _GetProcAddress = pData->fnGetProcAddress;
    pRtlAddFunctionTable _RtlAddFunctionTable = pData->fnRtlAddFunctionTable;

    // 1. Process relocations
    ULONGLONG delta = (ULONGLONG)(base - pOpt->ImageBase);
    if (delta) {
        IMAGE_DATA_DIRECTORY* relocDir = &pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if (relocDir->Size) {
            IMAGE_BASE_RELOCATION* pReloc = (IMAGE_BASE_RELOCATION*)(base + relocDir->VirtualAddress);
            while (pReloc->VirtualAddress) {
                DWORD count = (pReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                WORD* pEntry = (WORD*)(pReloc + 1);
                for (DWORD i = 0; i < count; i++) {
                    if ((pEntry[i] >> 12) == IMAGE_REL_BASED_DIR64) {
                        ULONGLONG* pPatch = (ULONGLONG*)(base + pReloc->VirtualAddress + (pEntry[i] & 0xFFF));
                        *pPatch += delta;
                    }
                }
                pReloc = (IMAGE_BASE_RELOCATION*)((BYTE*)pReloc + pReloc->SizeOfBlock);
            }
        }
    }

    // 2. Resolve imports
    IMAGE_DATA_DIRECTORY* importDir = &pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDir->Size) {
        IMAGE_IMPORT_DESCRIPTOR* pImport = (IMAGE_IMPORT_DESCRIPTOR*)(base + importDir->VirtualAddress);
        while (pImport->Name) {
            char* modName = (char*)(base + pImport->Name);
            HMODULE hMod = _LoadLibraryA(modName);
            if (hMod) {
                IMAGE_THUNK_DATA* pThunk = (IMAGE_THUNK_DATA*)(base + pImport->OriginalFirstThunk);
                IMAGE_THUNK_DATA* pFunc = (IMAGE_THUNK_DATA*)(base + pImport->FirstThunk);
                if (!pImport->OriginalFirstThunk) {
                    pThunk = pFunc;
                }
                while (pThunk->u1.AddressOfData) {
                    if (IMAGE_SNAP_BY_ORDINAL64(pThunk->u1.Ordinal)) {
                        pFunc->u1.Function = (ULONGLONG)_GetProcAddress(hMod, (LPCSTR)(pThunk->u1.Ordinal & 0xFFFF));
                    } else {
                        IMAGE_IMPORT_BY_NAME* pName = (IMAGE_IMPORT_BY_NAME*)(base + pThunk->u1.AddressOfData);
                        pFunc->u1.Function = (ULONGLONG)_GetProcAddress(hMod, pName->Name);
                    }
                    pThunk++;
                    pFunc++;
                }
            }
            pImport++;
        }
    }

    // 3. Register exception handlers (x64 SEH)
    IMAGE_DATA_DIRECTORY* exceptDir = &pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (exceptDir->Size && _RtlAddFunctionTable) {
        PRUNTIME_FUNCTION pFuncs = (PRUNTIME_FUNCTION)(base + exceptDir->VirtualAddress);
        DWORD count = exceptDir->Size / sizeof(RUNTIME_FUNCTION);
        _RtlAddFunctionTable(pFuncs, count, (DWORD64)base);
    }

    // 4. Execute TLS callbacks
    IMAGE_DATA_DIRECTORY* tlsDir = &pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
    if (tlsDir->Size) {
        IMAGE_TLS_DIRECTORY* pTls = (IMAGE_TLS_DIRECTORY*)(base + tlsDir->VirtualAddress);
        PIMAGE_TLS_CALLBACK* ppCallback = (PIMAGE_TLS_CALLBACK*)pTls->AddressOfCallBacks;
        if (ppCallback) {
            while (*ppCallback) {
                (*ppCallback)((PVOID)base, DLL_PROCESS_ATTACH, NULL);
                ppCallback++;
            }
        }
    }

    // 5. Call DllMain
    if (pOpt->AddressOfEntryPoint) {
        DllMain_t pDllMain = (DllMain_t)(base + pOpt->AddressOfEntryPoint);
        pDllMain((HINSTANCE)base, DLL_PROCESS_ATTACH, NULL);
    }

    // Zero remaining data to remove traces
    pData->fnLoadLibraryA = NULL;
    pData->fnGetProcAddress = NULL;
    pData->fnRtlAddFunctionTable = NULL;

    return 0;
}
static DWORD WINAPI ShellcodeEnd() { return 0; }
#pragma optimize("", on)
#pragma runtime_checks("", restore)

// ================================================================
// Process utilities
// ================================================================
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

// ================================================================
// Manual Map injection
// ================================================================
int ManualMapInject(DWORD pid, const char* dllPath) {
    printf("[*] Opening process %lu...\n", pid);

    HANDLE hProc = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
        FALSE, pid
    );

    if (!hProc) {
        printf("[!] OpenProcess failed: %lu\n", GetLastError());
        return 0;
    }

    // Read DLL from disk
    HANDLE hFile = CreateFileA(dllPath, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("[!] Failed to open DLL file: %lu\n", GetLastError());
        CloseHandle(hProc);
        return 0;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    BYTE* rawDll = (BYTE*)VirtualAlloc(NULL, fileSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!rawDll) {
        printf("[!] Local VirtualAlloc failed\n");
        CloseHandle(hFile);
        CloseHandle(hProc);
        return 0;
    }

    DWORD bytesRead = 0;
    ReadFile(hFile, rawDll, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    if (bytesRead != fileSize) {
        printf("[!] Failed to read entire DLL\n");
        VirtualFree(rawDll, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 0;
    }

    // Validate PE
    IMAGE_DOS_HEADER* pDos = (IMAGE_DOS_HEADER*)rawDll;
    if (pDos->e_magic != IMAGE_DOS_SIGNATURE) {
        printf("[!] Invalid DOS signature\n");
        VirtualFree(rawDll, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 0;
    }

    IMAGE_NT_HEADERS* pNt = (IMAGE_NT_HEADERS*)(rawDll + pDos->e_lfanew);
    if (pNt->Signature != IMAGE_NT_SIGNATURE) {
        printf("[!] Invalid NT signature\n");
        VirtualFree(rawDll, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 0;
    }

    if (pNt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) {
        printf("[!] DLL is not x64\n");
        VirtualFree(rawDll, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 0;
    }

    IMAGE_OPTIONAL_HEADER* pOpt = &pNt->OptionalHeader;

    // Allocate memory in target process for the image
    printf("[*] Allocating %lu bytes in target process...\n", pOpt->SizeOfImage);
    LPVOID remoteImage = VirtualAllocEx(hProc, NULL, pOpt->SizeOfImage,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteImage) {
        printf("[!] VirtualAllocEx for image failed: %lu\n", GetLastError());
        VirtualFree(rawDll, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 0;
    }

    printf("[*] Remote image base: %p\n", remoteImage);

    // Prepare local mapped image
    BYTE* localImage = (BYTE*)VirtualAlloc(NULL, pOpt->SizeOfImage,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!localImage) {
        printf("[!] Local image alloc failed\n");
        VirtualFreeEx(hProc, remoteImage, 0, MEM_RELEASE);
        VirtualFree(rawDll, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 0;
    }

    // Copy headers
    memcpy(localImage, rawDll, pOpt->SizeOfHeaders);

    // Copy sections
    IMAGE_SECTION_HEADER* pSections = IMAGE_FIRST_SECTION(pNt);
    for (WORD i = 0; i < pNt->FileHeader.NumberOfSections; i++) {
        if (pSections[i].SizeOfRawData > 0) {
            memcpy(localImage + pSections[i].VirtualAddress,
                rawDll + pSections[i].PointerToRawData,
                pSections[i].SizeOfRawData);
        }
    }

    // Write mapped image to target
    if (!WriteProcessMemory(hProc, remoteImage, localImage, pOpt->SizeOfImage, NULL)) {
        printf("[!] WriteProcessMemory (image) failed: %lu\n", GetLastError());
        VirtualFree(localImage, 0, MEM_RELEASE);
        VirtualFreeEx(hProc, remoteImage, 0, MEM_RELEASE);
        VirtualFree(rawDll, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 0;
    }

    VirtualFree(localImage, 0, MEM_RELEASE);
    VirtualFree(rawDll, 0, MEM_RELEASE);

    // Prepare ManualMapData
    ManualMapData mapData = {};
    mapData.imageBase = remoteImage;
    mapData.fnLoadLibraryA = (pLoadLibraryA)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    mapData.fnGetProcAddress = (pGetProcAddress)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetProcAddress");
    mapData.fnRtlAddFunctionTable = (pRtlAddFunctionTable)GetProcAddress(GetModuleHandleA("kernel32.dll"), "RtlAddFunctionTable");

    // Calculate shellcode size
    SIZE_T shellcodeSize = (SIZE_T)((BYTE*)ShellcodeEnd - (BYTE*)Shellcode);
    if (shellcodeSize == 0 || shellcodeSize > 0x10000) {
        shellcodeSize = 0x1000; // safe fallback
    }

    printf("[*] Shellcode size: %zu bytes\n", shellcodeSize);

    // Allocate shellcode + data in target
    SIZE_T totalSize = shellcodeSize + sizeof(ManualMapData) + 64;
    LPVOID remoteAlloc = VirtualAllocEx(hProc, NULL, totalSize,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteAlloc) {
        printf("[!] VirtualAllocEx for shellcode failed: %lu\n", GetLastError());
        VirtualFreeEx(hProc, remoteImage, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 0;
    }

    // Layout: [ManualMapData][padding][Shellcode]
    BYTE* remoteDataAddr = (BYTE*)remoteAlloc;
    BYTE* remoteCodeAddr = (BYTE*)remoteAlloc + sizeof(ManualMapData) + 16;

    // Write data
    if (!WriteProcessMemory(hProc, remoteDataAddr, &mapData, sizeof(mapData), NULL)) {
        printf("[!] WriteProcessMemory (data) failed: %lu\n", GetLastError());
        VirtualFreeEx(hProc, remoteAlloc, 0, MEM_RELEASE);
        VirtualFreeEx(hProc, remoteImage, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 0;
    }

    // Write shellcode
    if (!WriteProcessMemory(hProc, remoteCodeAddr, (LPVOID)Shellcode, shellcodeSize, NULL)) {
        printf("[!] WriteProcessMemory (shellcode) failed: %lu\n", GetLastError());
        VirtualFreeEx(hProc, remoteAlloc, 0, MEM_RELEASE);
        VirtualFreeEx(hProc, remoteImage, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 0;
    }

    // ============================================================
    // Execute shellcode via CreateRemoteThread
    // Start address points to our shellcode in anonymous RWX memory,
    // NOT to LoadLibrary. This is still stealthy because:
    // - No LoadLibrary call to detect
    // - No module list entry created
    // - Thread start address is in unbacked anonymous memory
    // ============================================================
    printf("[*] Executing shellcode...\n");
    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0,
        (LPTHREAD_START_ROUTINE)remoteCodeAddr,
        remoteDataAddr, 0, NULL);
    if (!hThread) {
        printf("[!] CreateRemoteThread failed: %lu\n", GetLastError());
        VirtualFreeEx(hProc, remoteAlloc, 0, MEM_RELEASE);
        VirtualFreeEx(hProc, remoteImage, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 0;
    }

    DWORD waitResult = WaitForSingleObject(hThread, 15000);
    if (waitResult == WAIT_TIMEOUT) {
        printf("[!] Shellcode execution timed out\n");
        CloseHandle(hThread);
        CloseHandle(hProc);
        return 0;
    }

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);

    if (exitCode != 0) {
        printf("[!] Shellcode returned error: %lu\n", exitCode);
        VirtualFreeEx(hProc, remoteAlloc, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 0;
    }

    // Clean up shellcode allocation (image stays — it's our DLL)
    VirtualFreeEx(hProc, remoteAlloc, 0, MEM_RELEASE);
    CloseHandle(hProc);

    printf("[+] Manual map injection successful!\n");
    return 1;
}

// ================================================================
// UI / CLI
// ================================================================
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
        "cm_hax.dll",
        "Menu DLL injected (manual map). No module list entry created."
    };
    const InjectionTarget dumperTarget = {
        "Dumper",
        "dumper.dll",
        "Dumper DLL injected (manual map). Check the desktop dumper log/output."
    };

    printf("============================================================\n");
    printf("  Combat Master - Stealth Injector (Manual Map)\n");
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

    // Check if game is already running
    DWORD pid = FindProcess(procName);
    if (!pid) {
        // Make sure Steam is open
        DWORD steamPid = FindProcess("steam.exe");
        if (!steamPid) {
            printf("[*] Opening Steam...\n");
            char steamPath[MAX_PATH] = {};
            DWORD steamPathLen = sizeof(steamPath);
            HKEY hKey = NULL;
            if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                if (RegQueryValueExA(hKey, "SteamExe", NULL, NULL, (LPBYTE)steamPath, &steamPathLen) == ERROR_SUCCESS) {
                    ShellExecuteA(NULL, "open", steamPath, NULL, NULL, SW_SHOWDEFAULT);
                }
                RegCloseKey(hKey);
            }
            printf("[*] Waiting for Steam...\n");
            while (!steamPid) {
                steamPid = FindProcess("steam.exe");
                if (!steamPid) Sleep(500);
            }
            printf("[+] Steam is ready.\n\n");
            Sleep(3000);
        } else {
            // Steam is running but might be minimized — use steam:// protocol
            // to force the library window open (this always brings Steam to front)
            ShellExecuteA(NULL, "open", "steam://open/games/details/2105420", NULL, NULL, SW_SHOWDEFAULT);
            Sleep(1000);
        }

        printf("[*] Now press PLAY on Combat Master in Steam.\n");
        printf("[*] Waiting for %s...\n", procName);
        printf("[*] HWID spoof activates automatically before login.\n\n");

        while (!pid) {
            pid = FindProcess(procName);
            if (!pid) Sleep(100);
        }
    } else {
        printf("[*] %s already running.\n", procName);
    }

    printf("[+] Found %s (PID: %lu)\n", procName, pid);

    // Wait for Project.dll to be loaded in the target process.
    // This ensures the game's IL2CPP runtime is mapped before we inject.
    printf("[*] Waiting for Project.dll to load...\n");
    {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!hProc) {
            // Fallback to fixed wait if we can't open the process yet
            printf("[*] Can't query process, waiting 5 seconds...\n");
            Sleep(5000);
        } else {
            bool found = false;
            for (int attempt = 0; attempt < 120; attempt++) { // 60 seconds max
                HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
                if (snap != INVALID_HANDLE_VALUE) {
                    MODULEENTRY32 me;
                    me.dwSize = sizeof(me);
                    if (Module32First(snap, &me)) {
                        do {
                            if (_stricmp(me.szModule, "Project.dll") == 0) {
                                found = true;
                                break;
                            }
                        } while (Module32Next(snap, &me));
                    }
                    CloseHandle(snap);
                }
                if (found) break;
                Sleep(500);
            }
            CloseHandle(hProc);
            if (found) {
                printf("[+] Project.dll detected! Waiting for initialization...\n");
                Sleep(3000); // Wait for DLL to fully initialize its exports
            } else {
                printf("[!] Timed out waiting for Project.dll (60s). Attempting injection anyway...\n");
            }
        }
    }

    // Inject
    if (ManualMapInject(pid, dllPath)) {
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
