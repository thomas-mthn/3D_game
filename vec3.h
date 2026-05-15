#ifndef VEC3_H
#define VEC3_H

#include "tmath.h"
#include "real.h"

typedef enum{
    VEC3_X,
    VEC3_Y,
    VEC3_Z,
} Vec3Axis;

typedef union{
    struct{real x,y,z;};
    real a[3];
} Vec3;

static Vec3i vec3ToVec3i(Vec3 v){
    return (Vec3i){intToReal(v.x),intToReal(v.y),intToReal(v.z)};
}

static Vec3 vec3Single(real value){
    return (Vec3){value,value,value};
}

static Vec3 vec3Add(Vec3 v,Vec3 a){
    return (Vec3){v.x + a.x,v.y + a.y,v.z + a.z};
}

static Vec3 vec3Sub(Vec3 v,Vec3 a){
    return (Vec3){v.x - a.x,v.y - a.y,v.z - a.z};
}

static Vec3 vec3Mul(Vec3 v,Vec3 a){
    return (Vec3){realMulR(v.x,a.x),realMulR(v.y,a.y),realMulR(v.z,a.z)};
}

static Vec3 vec3Div(Vec3 v,Vec3 a){
    return (Vec3){realDivR(v.x,a.x),realDivR(v.y,a.y),realDivR(v.z,a.z)};
}

static Vec3 vec3AddS(Vec3 v,real a){
    v.x += a;
    v.y += a;
    v.z += a;
    return v;
}

static Vec3 vec3SubS(Vec3 v,real a){
    v.x -= a;
    v.y -= a;
    v.z -= a;
    return v;
}

static Vec3 vec3MulS(Vec3 v,real a){
    v.x = realMulR(v.x,a);
    v.y = realMulR(v.y,a);
    v.z = realMulR(v.z,a);
    return v;
}

static Vec3 vec3DivS(Vec3 v,real a){
    realDiv(&v.x,a);
    realDiv(&v.y,a);
    realDiv(&v.z,a);
    return v;
}

static Vec3 vec3Shr(Vec3 v,int a){
    if(IS_FLOAT(real))
        return vec3DivS(v,1 << a);
    
    return (Vec3){(int)v.x >> a,(int)v.y >> a,(int)v.z >> a};
}

static Vec3 vec3Shl(Vec3 v,int a){
    if(IS_FLOAT(real))
        return vec3MulS(v,1 << a);
    
    return (Vec3){(int)v.x << a,(int)v.y << a,(int)v.z << a};
}

static real vec3LengthSquare(Vec3 v){
    return realMulR(v.x,v.x) + realMulR(v.y,v.y) + realMulR(v.z,v.z);
}

static real vec3Length(Vec3 v){
    return tSqrt(vec3LengthSquare(v));
}

static Vec3 vec3Normalize(Vec3 v){
    real length = tInverseSqrt(vec3LengthSquare(v));
    if(!length)
        return v;
    v = vec3MulS(v,length);
    return v;
}

static real vec3Distance(Vec3 v1,Vec3 v2){
    return vec3Length(vec3Sub(v1,v2));
}

static real vec3DistanceSquare(Vec3 v1,Vec3 v2){
    return vec3LengthSquare(vec3Sub(v1,v2));
}

static real vec3Dot(Vec3 v1,Vec3 v2){
#if 0
    if(IS_FLOAT(real))
        return _mm_cvtss_f32(_mm_dp_ps(_mm_loadu_ps(&v1.x),_mm_loadu_ps(&v2.x),0x71));
#endif
    return realMulR(v1.x,v2.x) + realMulR(v1.y,v2.y) + realMulR(v1.z,v2.z);
}

static Vec3 vec3Cross(Vec3 v1,Vec3 v2){
    return (Vec3){
        realMulR(v1.y,v2.z) - realMulR(v2.y,v1.z),
        realMulR(v1.z,v2.x) - realMulR(v2.z,v1.x),
        realMulR(v1.x,v2.y) - realMulR(v2.x,v1.y)
    };
}

static Vec3 vec3Direction(Vec3 from,Vec3 to){
    return vec3Normalize(vec3Sub(to,from));
}

static Vec3 vec3Reflect(Vec3 v,Vec3 n){
	return vec3Sub(v,vec3Mul(vec3Single(realMulR(FIXED_ONE * 2,vec3Dot(n,v))),n));
}

static Vec3 vec3Refract(Vec3 v,Vec3 n,real eta){
    real k = FIXED_ONE - realMulR(realMulR(eta,eta),(FIXED_ONE - realMulR(vec3Dot(n,v),vec3Dot(n,v))));
    if (k < 0)
        return (Vec3){0};
    else
        return vec3Sub(vec3MulS(v,eta),vec3MulS(n,(realMulR(eta,vec3Dot(n,v)) + tSqrt(k))));
}

static Vec3 vec3Mix(Vec3 v1,Vec3 v2,real mix){
    return (Vec3){tMix(v1.x,v2.x,mix),tMix(v1.y,v2.y,mix),tMix(v1.z,v2.z,mix)};
}

static Vec3 vec3Rnd(void){
    Vec3 random;
    do{
        random = (Vec3){
            realRandom(FIXED_ONE * 2) - FIXED_ONE,
            realRandom(FIXED_ONE * 2) - FIXED_ONE,
            realRandom(FIXED_ONE * 2) - FIXED_ONE,
        };
    } while(vec3Dot(random,random) > FIXED_ONE);

    return vec3Normalize(random);
}

#endif
