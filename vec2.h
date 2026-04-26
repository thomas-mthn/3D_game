#ifndef VEC2_H
#define VEC2_H

#include "tmath.h"
#include "fixed.h"

structure(Vec2){
    int x;
    int y;
};

enum{
    VEC2_X,
    VEC2_Y,
};

static Vec2 vec2Single(int value){
    return (Vec2){value,value};
}

static Vec2 vec2Add(Vec2 v,Vec2 a){
    return (Vec2){v.x + a.x,v.y + a.y};
}

static Vec2 vec2Sub(Vec2 v,Vec2 a){
    return (Vec2){v.x - a.x,v.y - a.y};
}

static Vec2 vec2Mul(Vec2 v,Vec2 a){
    return (Vec2){fixedMulR(v.x,a.x),fixedMulR(v.y,a.y)};
}

static Vec2 vec2Div(Vec2 v,Vec2 a){
    return (Vec2){fixedDivR(v.x,a.x),fixedDivR(v.y,a.y)};
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

static Vec2 vec2MulS(Vec2 v,int a){
    v.x = fixedMulR(v.x,a);
    v.y = fixedMulR(v.y,a);
    return v;
}

static Vec2 vec2DivS(Vec2 v,int a){
    fixedDiv(&v.x,a);
    fixedDiv(&v.y,a);
    return v;
}

static Vec2 vec2Shr(Vec2 v,int a){
    return (Vec2){v.x >> a,v.y >> a};
}

static Vec2 vec2Shl(Vec2 v,int a){
    return (Vec2){v.x << a,v.y << a};
}

static int vec2Length(Vec2 v){
    return tSqrt((fixedMulR(v.x,v.x) + fixedMulR(v.y,v.y)));
}

static Vec2 vec2Normalize(Vec2 v){
    int length = tInverseSqrt(fixedMulR(v.x,v.x) + fixedMulR(v.y,v.y));
    if(!length)
        return v;
    v = vec2MulS(v,length);
    return v;
}

static int vec2Distance(Vec2 v1,Vec2 v2){
    Vec2 relative = {v1.x - v2.x,v1.y - v2.y};
    return vec2Length(relative);
}

static int vec2Dot(Vec2 v1,Vec2 v2){
    return fixedMulR(v1.x,v2.x) + fixedMulR(v1.y,v2.y);
}

static Vec2 vec2Direction(Vec2 from,Vec2 to){
    return vec2Normalize(vec2Sub(to,from));
}

static Vec2 vec2Perpendicular(Vec2 v){
    return (Vec2){-v.y,v.x};
}

static Vec2 vec2Rotate(Vec2 v,int theta){
	Vec2 r;
	r.x = fixedMulR(v.x,tCos(theta)) - fixedMulR(v.y,tSin(theta));
	r.y = fixedMulR(v.x,tSin(theta)) + fixedMulR(v.y,tCos(theta));
	return r;
}

static Vec2 vec2Mix(Vec2 v1,Vec2 v2,int mix){
    return (Vec2){tMix(v1.x,v2.x,mix),tMix(v1.y,v2.y,mix)};
}

static Vec2 vec2Rnd(void){
    int angle = tRnd() & (FIXED_ONE - 1);
    return (Vec2){tCos(angle),tSin(angle)};
}

#endif
