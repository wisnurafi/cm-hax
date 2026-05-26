// utils/memory.cpp
#include "memory.h"

bool IsPlausiblePtr(void* ptr) {
    uintptr_t value = (uintptr_t)ptr;
    return value >= 0x10000 && value < 0x0000800000000000;
}
