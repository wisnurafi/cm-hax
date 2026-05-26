// utils/math.h
// Basic geometric types and math helpers shared across modules.
#pragma once

struct Vector3   { float x, y, z; };
struct Matrix4x4 { float m[4][4]; };

float ClampFloat(float value, float minValue, float maxValue);
float Distance3D(Vector3 a, Vector3 b);

static inline float ImMax2(float a, float b) { return a > b ? a : b; }
