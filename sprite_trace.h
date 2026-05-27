#ifndef SPRITE_TRACE_H
#define SPRITE_TRACE_H

#include "langext.h"
#include "vec3.h"
#include "vec2.h"

structure(Cubemap);
structure(Texture);

structure(TracePrimitive){
    enum{
        MODEL_ELLIPSOID,
        MODEL_CYLINDER,
    } type;
    Vec3 position;
    Vec3 color;
    struct{
        Vec3 radius; 
    } ellipsoid;
    struct{
        Vec3 axis;
        real radius;
    } cylinder;
};

structure(ModelSprite){
    bool is_voxel;
    int n_ellipsoid;
    TracePrimitive ellipsoid[];
};

void ellipsoidModelCubemapGenerate(Cubemap* cubemap,Vec3 position,int reflect_factor);
void ellipsoidModelGenerate(Cubemap* cubemap,Vec3 position,Vec3 size,Texture* texture,ModelSprite* model,Vec2 model_angle,bool angle_player,real projection_size,real scale);
void spriteRender3d(Vec3 position,real entity_size,Texture* texture);

#endif
