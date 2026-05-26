// utils/strings.cpp
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "strings.h"
#include "memory.h"
#include "../game/sdk.h"

bool CopyIl2CppStringUtf8(void* stringObj, char* out, size_t outSize) {
    if (!out || outSize == 0) return false;
    out[0] = '\0';
    if (!IsPlausiblePtr(stringObj)) return false;

    __try {
        Il2CppString* s = (Il2CppString*)stringObj;
        if (s->length <= 0 || s->length > 64) return false;

        int written = WideCharToMultiByte(CP_UTF8, 0, s->chars, s->length,
                                           out, (int)outSize - 1, NULL, NULL);
        if (written <= 0) return false;

        out[written] = '\0';
        return true;
    } __except(1) {
        out[0] = '\0';
        return false;
    }
}
