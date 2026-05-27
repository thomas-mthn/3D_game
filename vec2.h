#ifndef VEC2_H
#define VEC2_H

#include "tmath.h"
#include "real.h"

typedef union{
    struct{real x,y;};
    real a[2];
} Vec2;

enum{
    VEC2_X,
    VEC2_Y,
};

static Vec2 vec2Single(real value){
    return (Vec2){value,value};
}

static Vec2 vec2Add(Vec2 v,Vec2 a){
    return (Vec2){v.x + a.x,v.y + a.y};
}

static Vec2 vec2Sub(Vec2 v,Vec2 a){
    return (Vec2){v.x - a.x,v.y - a.y};
}

static Vec2 vec2Mul(Vec2 v,Vec2 a){
    return (Vec2){realMulR(v.x,a.x),realMulR(v.y,a.y)};
}

static Vec2 vec2Div(Vec2 v,Vec2 a){
    return (Vec2){realDivR(v.x,a.x),realDivR(v.y,a.y)};
}

static Vec2 vec2AddS(Vec2 v,int a){
    v.x += a;
    v.y += a;
    return v;
}

static Vec2 vec2SubS(Vec2 v,int a){
    v.x -= a;
    v.y -= a;
    return v;
}

static Vec2 vec2MulS(Vec2 v,real a){
    v.x = realMulR(v.x,a);
    v.y = realMulR(v.y,a);
    return v;
}

static Vec2 vec2DivS(Vec2 v,real a){
    realDiv(&v.x,a);
    realDiv(&v.y,a);
    return v;
}

static Vec2 vec2Shr(Vec2 v,int a){
    if(IS_FLOAT(real))
        return vec2DivS(v,1 << a);
    return (Vec2){(int)v.x >> a,(int)v.y >> a};
}

static Vec2 vec2Shl(Vec2 v,int a){
    if(IS_FLOAT(real))
        return vec2MulS(v,1 << a);
    return (Vec2){(int)v.x << a,(int)v.y << a};
}

static real vec2Length(Vec2 v){
    return tSqrt(realMulR(v.x,v.x) + realMulR(v.y,v.y));
}

static Vec2 vec2Normalize(Vec2 v){
    real length = tInverseSqrt(realMulR(v.x,v.x) + realMulR(v.y,v.y));
    if(!length)
        return (Vec2){0};
    v = vec2MulS(v,length);
    return v;
}

static real vec2Distance(Vec2 v1,Vec2 v2){
    return vec2Length(vec2Sub(v1,v2));
}

static real vec2Dot(Vec2 v1,Vec2 v2){
    return realMulR(v1.x,v2.x) + realMulR(v1.y,v2.y);
}

static Vec2 vec2Direction(Vec2 from,Vec2 to){
    return vec2Normalize(vec2Sub(to,from));
}

static Vec2 vec2Perpendicular(Vec2 v){
    return (Vec2){-v.y,v.x};
}

static Vec2 vec2Rotate(Vec2 v,real theta){
	Vec2 r;
	r.x = realMulR(v.x,tCos(theta)) - realMulR(v.y,tSin(theta));
	r.y = realMulR(v.x,tSin(theta)) + realMulR(v.y,tCos(theta));
	return r;
}

static Vec2 vec2Mix(Vec2 v1,Vec2 v2,real mix){
    return (Vec2){tMix(v1.x,v2.x,mix),tMix(v1.y,v2.y,mix)};
}

static Vec2 vec2Rnd(void){
    real angle = tRnd() & ((int)FIXED_ONE - 1);
    return (Vec2){tCos(angle),tSin(angle)};
}

#endif
