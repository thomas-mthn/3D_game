#ifndef VEC3_H
#define VEC3_H

#include "tmath.h"
#include "fixed.h"

typedef enum{
    VEC3_X,
    VEC3_Y,
    VEC3_Z,
} Vec3Axis;

typedef union{
    struct{int x,y,z;};
    int a[3];
} Vec3;

static Vec3 vec3Single(int value){
    return (Vec3){value,value,value};
}

static Vec3 vec3Add(Vec3 v,Vec3 a){
    return (Vec3){v.x + a.x,v.y + a.y,v.z + a.z};
}

static Vec3 vec3Sub(Vec3 v,Vec3 a){
    return (Vec3){v.x - a.x,v.y - a.y,v.z - a.z};
}

static Vec3 vec3Mul(Vec3 v,Vec3 a){
    return (Vec3){fixedMulR(v.x,a.x),fixedMulR(v.y,a.y),fixedMulR(v.z,a.z)};
}

static Vec3 vec3Div(Vec3 v,Vec3 a){
    return (Vec3){fixedDivR(v.x,a.x),fixedDivR(v.y,a.y),fixedDivR(v.z,a.z)};
}

static Vec3 vec3Shr(Vec3 v,int a){
    return (Vec3){v.x >> a,v.y >> a,v.z >> a};
}

static Vec3 vec3Shl(Vec3 v,int a){
    return (Vec3){v.x << a,v.y << a,v.z << a};
}

static Vec3 vec3AddS(Vec3 v,int a){
    v.x += a;
    v.y += a;
    v.z += a;
    return v;
}

static Vec3 vec3SubS(Vec3 v,int a){
    v.x -= a;
    v.y -= a;
    v.z -= a;
    return v;
}

static Vec3 vec3MulS(Vec3 v,int a){
    v.x = fixedMulR(v.x,a);
    v.y = fixedMulR(v.y,a);
    v.z = fixedMulR(v.z,a);
    return v;
}

static Vec3 vec3DivS(Vec3 v,int a){
    fixedDiv(&v.x,a);
    fixedDiv(&v.y,a);
    fixedDiv(&v.z,a);
    return v;
}

static int vec3LengthSquare(Vec3 v){
    return fixedMulR(v.x,v.x) + fixedMulR(v.y,v.y) + fixedMulR(v.z,v.z);
}

static int vec3Length(Vec3 v){
    return tSqrt(vec3LengthSquare(v));
}

static Vec3 vec3Normalize(Vec3 v){
    int length = tInverseSqrt(vec3LengthSquare(v));
    if(!length)
        return v;
    v = vec3MulS(v,length);
    return v;
}

static int vec3Distance(Vec3 v1,Vec3 v2){
    return vec3Length(vec3Sub(v1,v2));
}

static int vec3DistanceSquare(Vec3 v1,Vec3 v2){
    return vec3LengthSquare(vec3Sub(v1,v2));
}

static int vec3Dot(Vec3 v1,Vec3 v2){
    return fixedMulR(v1.x,v2.x) + fixedMulR(v1.y,v2.y) + fixedMulR(v1.z,v2.z);
}

static Vec3 vec3Cross(Vec3 v1,Vec3 v2){
    return (Vec3){
        fixedMulR(v1.y,v2.z) - fixedMulR(v2.y,v1.z),
        fixedMulR(v1.z,v2.x) - fixedMulR(v2.z,v1.x),
        fixedMulR(v1.x,v2.y) - fixedMulR(v2.x,v1.y)
    };
}

static Vec3 vec3Direction(Vec3 from,Vec3 to){
    return vec3Normalize(vec3Sub(to,from));
}

static Vec3 vec3Reflect(Vec3 v,Vec3 n){
	return vec3Sub(v,vec3Mul(vec3Single(fixedMulR(FIXED_ONE * 2,vec3Dot(n,v))),n));
}

static Vec3 vec3Refract(Vec3 v,Vec3 n,int eta){
    int k = FIXED_ONE - fixedMulR(fixedMulR(eta,eta),(FIXED_ONE - fixedMulR(vec3Dot(n,v),vec3Dot(n,v))));
    if (k < 0)
        return (Vec3){0};
    else
        return vec3Sub(vec3MulS(v,eta),vec3MulS(n,(fixedMulR(eta,vec3Dot(n,v)) + tSqrt(k))));
}

static Vec3 vec3Mix(Vec3 v1,Vec3 v2,int mix){
    return (Vec3){tMix(v1.x,v2.x,mix),tMix(v1.y,v2.y,mix),tMix(v1.z,v2.z,mix)};
}

static Vec3 vec3Rnd(void){
    Vec3 random;
    do{
        random = (Vec3){
            tRnd() % (FIXED_ONE * 2) - FIXED_ONE,
            tRnd() % (FIXED_ONE * 2) - FIXED_ONE,
            tRnd() % (FIXED_ONE * 2) - FIXED_ONE,
        };
    } while(vec3Dot(random,random) > FIXED_ONE);

    return vec3Normalize(random);
}

#endif
