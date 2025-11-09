#ifndef XV6_MATH_H
#define XV6_MATH_H

#define INFINITY (1.0f / 0.0f)
#define NAN (0.0f / 0.0f)
#define PI 3.14159265358979323846f
#define PI_2 1.57079632679489661923f
#define PI_4 0.78539816339744830962f
#define EPSILON 1e-5f

typedef unsigned int uint32_t;

// Math functions
float sqrtf_new(float x);
float expf_new(float x);
float logf_new(float x);
float powf_new(float x, float y);
float sinf_new(float x);
float cosf_new(float x);
float fabsf_new(float x);

// Helper functions
float reduce_angle(float x);

#endif