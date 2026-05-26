// utils/memory.h
// Pointer and memory utilities (validation, byte patches).
#pragma once
#include <stdint.h>

bool IsPlausiblePtr(void* ptr);

struct BytePatch {
    uint8_t* addr;
    uint8_t  original[3];
    bool     active;
};
