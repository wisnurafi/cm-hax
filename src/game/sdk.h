// game/sdk.h
// Minimal IL2CPP runtime structs used across the project.
// Mirrors Unity 6000.0.60. Only the fields we touch are listed.
#pragma once
#include <stdint.h>

struct Il2CppObject { void* klass; void* monitor; };
struct Il2CppList   { Il2CppObject obj; void* _items; int _size; };
struct Il2CppArray  { Il2CppObject obj; void* bounds; uintptr_t max_length; };
struct Il2CppString { Il2CppObject obj; int32_t length; wchar_t chars[1]; };
