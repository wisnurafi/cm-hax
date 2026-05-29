// utils/stealth.h
// Anti-detection utilities: PE header erasure, string encryption, etc.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

namespace Stealth {

// Wipes the PE header (DOS + NT headers + section table) from the loaded
// module's memory. Call once after all initialization is complete.
// After this, memory scanners cannot identify the region as a PE image.
inline void ErasePEHeaders(HMODULE hModule) {
    if (!hModule) return;

    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)hModule;
    PIMAGE_NT_HEADERS pNt  = (PIMAGE_NT_HEADERS)((BYTE*)hModule + pDos->e_lfanew);

    // Size to erase: everything up to and including the section headers
    DWORD headerSize = pNt->OptionalHeader.SizeOfHeaders;
    if (headerSize == 0) headerSize = 0x1000; // fallback to one page

    DWORD oldProtect = 0;
    if (VirtualProtect((LPVOID)hModule, headerSize, PAGE_READWRITE, &oldProtect)) {
        SecureZeroMemory((LPVOID)hModule, headerSize);
        VirtualProtect((LPVOID)hModule, headerSize, oldProtect, &oldProtect);
    }
}

} // namespace Stealth
