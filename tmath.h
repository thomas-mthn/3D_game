#ifndef TMATH_H
#define TMATH_H

#include "real.h"

#define M_PI 0x000325A1

#ifdef _MSC_VER
#include <intrin.h>
#elif defined(__GNUC__)
#include <immintrin.h>
#endif

static unsigned tHash(unsigned x){
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
	return x;
}

static unsigned tRnd(void){
    static unsigned v = 2463534242u;
    v ^= v << 13;
    v ^= v >> 17;
    v ^= v << 5;
    return v;
}

static bool tRndChance(int change){
    return tRnd() % change == 0;
}

static real realRandom(real range){
    if(IS_FLOAT(real))
        return (float)(tRnd() % 0x10000) / 0x10000 * range;
    return tRnd() % (int)range;
}

static float tAbsf(float v){
    return v < 0.0f ? -v : v;
}

static int tAbsi(int v){
    return v < 0 ? -v : v;
}

#define tAbs(X) _Generic((X),     \
                    float: tAbsf,  \
                    default: tAbsi  \
              )(X)

static int tMaxi(int v1,int v2){
    return v1 > v2 ? v1 : v2;
}

static float tMaxf(float v1,float v2){
    return v1 > v2 ? v1 : v2;
}

#define tMax(X,Y) _Generic((X),     \
                    float: tMaxf,  \
                    default: tMaxi  \
                           )(X,Y)

static int tMini(int v1,int v2){
    return v1 < v2 ? v1 : v2;
}

static float tMinf(float v1,float v2){
    return v1 < v2 ? v1 : v2;
}

#define tMin(X,Y) _Generic((X),     \
                    float: tMinf,  \
                    default: tMini  \
                           )(X,Y)

static int tClampi(int v,int max,int min){
    return tMin(tMax(v,max),min);
}

static float tClampf(float v,float max,float min){
    return tMin(tMax(v,max),min);
}

#define tClamp(X,Y,Z) _Generic((X),     \
                    float: tClampf,  \
                    default: tClampi  \
                               )(X,Y,Z)

static real tFloor(real value){
    if(IS_FLOAT(real)){
        __m128 v = _mm_set_ss(value);
        return _mm_cvtss_f32(_mm_floor_ss(v,v));
    }
    return value - (int)value;
}

static real tFract(real value){
    if(IS_FLOAT(real))
        return value - tFloor(value);
    
    int value_i = value;
    return value_i & (int)FIXED_ONE - 1;
}

static real tFractU(real value){
    if(IS_FLOAT(real))
        return value - (int)value;
    
    int value_i = value;
    return value_i & (int)FIXED_ONE - 1;
}

static real tMix(real v1,real v2,real mix){
    return v1 + realMulR(v2 - v1,mix);
}

static unsigned x86Rdtsc(void){
#ifdef _MSC_VER
	return __rdtsc();
#else
	return 0;
#endif
}

static real tSqrt(real value){
    if(IS_FLOAT(real))
        return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(value)));
#if !defined(__wasm__)
    return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss((float)value / FIXED_ONE))) * FIXED_ONE;
#else
    return __builtin_sqrtf((float)value / FIXED_ONE) * FIXED_ONE;
#endif
}

static real tInverseSqrt(real value){
    if(IS_FLOAT(real))
        return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(value)));
#if !defined(__wasm__)
    return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss((float)value / FIXED_ONE))) * FIXED_ONE;
#else
    return (1.0f / __builtin_sqrtf((float)value / FIXED_ONE)) * FIXED_ONE;
#endif
}

static real tReciprocal(real value){
    if(IS_FLOAT(real))
        return _mm_cvtss_f32(_mm_rcp_ss(_mm_set_ss(value)));
    return realDivR(FIXED_ONE,value);
}

real tCos(real value);

static real tSin(real value){
    return tCos(value - (FIXED_ONE / 4));
}

real tArcSin(real angle);
real tArcTan2(real y,real x);

#endif
