#ifndef REAL_H
#define REAL_H

#include "langext.h"

#define FIXED_PRECISION 16

#if 0

typedef int32 real;

#define FIXED_ONE (1 << FIXED_PRECISION)
#define REAL_MAX INT_MAX;
#define REAL_MIN INT_MIN;

#else

typedef float real;

#include <float.h>
#define FIXED_ONE 1.0f
#define REAL_MAX FLT_MAX
#define REAL_MIN FLT_MIN

#endif

#define REAL_UNIT (FIXED_ONE / 0x100)
#define REAL_EPSILON (FIXED_ONE / 0x1000)

#define IS_FLOAT(T) _Generic(((T){0}), \
    float: 1,                          \
    default: 0)

static int fixedDivR(int value_1,int value_2){
    int result;

    if(!value_2)
        return INT_MAX;
    
#if INTPTR_MAX != INT64_MAX && defined(_MSC_VER) && !defined(__wasm__)
    if(((value_1 / value_2) < 0 ? -(value_1 / value_2) : (value_1 / value_2)) >= FIXED_ONE / 2)
        return INT_MAX;
    _asm {
        mov     eax, value_1
            cdq 
            shld    edx, eax, FIXED_PRECISION
            sal     eax, FIXED_PRECISION
            idiv    value_2
            mov     result, eax
            }
#elif INTPTR_MAX != INT64_MAX && defined(__GNUC__) && !defined(__wasm__)
    if(((value_1 / value_2) < 0 ? -(value_1 / value_2) : (value_1 / value_2)) >= FIXED_ONE / 2)
        return INT_MAX;
    asm (
         "movl %1, %%eax\n\t"
         "cdq\n\t"
         "shldl $16, %%eax, %%edx\n\t"
         "sall $16, %%eax\n\t"
         "idivl %2\n\t"
         "movl %%eax, %0\n\t"
         : "=m" (result)
         : "r" (value_1),
           "r" (value_2)
         : "%eax", "%ecx", "%edx"
         );
#else
    result = ((int64)value_1 << FIXED_PRECISION) / value_2;
#endif
    return result;
}

static bool isInfinite(real x){
    if(IS_FLOAT(real))
       return x == 1.0f / 0.0f || x == -1.0f / 0.0f;
    return false;
}

static real realDivR(real value_1,real value_2){
    if(IS_FLOAT(real))
        return value_1 / value_2;
    return fixedDivR(value_1,value_2);
}

static int fixedMulR(int value_1,int value_2){
    int result;
    int value_i1 = value_1;
    int value_i2 = value_2;
    
#if INTPTR_MAX != INT64_MAX && defined(_MSC_VER) && !defined(__wasm__)
    _asm {
        mov     eax,value_1
        imul    value_2
        shrd    eax,edx,FIXED_PRECISION
        mov     result, eax
    }
#else
    result = ((int64)value_i1 * value_i2) >> FIXED_PRECISION;
#endif
    return result;
}

static real realMulR(real value_1,real value_2){
    if(IS_FLOAT(real))
        return value_1 * value_2;

    return fixedMulR(value_1,value_2);
}

static void realMul(real* value,real a){
    *value = realMulR(*value,a);
}

static void realDiv(real* value,real a){
    *value = realDivR(*value,a);
}

static int realToInt(real value){
    if(IS_FLOAT(real))
        return value;
    int value_i = value;
    return value_i >> FIXED_PRECISION;
}

static real intToReal(int value){
    if(IS_FLOAT(real))
        return value;
    int value_i = value;
    return value_i << FIXED_PRECISION;
}

static real realShr(real r,int s){
    if(IS_FLOAT(real))
        return realDivR(r,1 << s);
    return (int)r >> s;
}

static real realShl(real r,int s){
    if(IS_FLOAT(real))
        return realMulR(r,1 << s);
    return (int)r << s;
}

#endif
