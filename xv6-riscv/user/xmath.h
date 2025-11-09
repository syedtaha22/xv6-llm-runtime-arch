#ifndef XV6_MATH_H
#define XV6_MATH_H

// Mathematical constants
#define INFINITY (1.0f / 0.0f)
#define NAN (0.0f / 0.0f)
#define PI 3.14159265358979323846f
#define PI_2 1.57079632679489661923f
#define PI_4 0.78539816339744830962f
#define EPSILON 1e-5f
#define LN2 0.69314718055994530942f
#define INV_LN2 1.44269504088896340736f

typedef unsigned int uint32_t;

// Core math functions
float xsqrtf(float x);
float xexpf(float x);
float xlogf(float x);
float xpowf(float x, float y);
float xsinf(float x);
float xcosf(float x);
float xtanhf(float x);  
float xfabsf(float x);
float xfloorf(float x);

// Helper functions
float xreduce_angle(float x);

#endif