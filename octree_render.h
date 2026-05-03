#ifndef OCTREE_RENDER_H
#define OCTREE_RENDER_H

#include "langext.h"
#include "vec3.h"
#include "vec2.h"

structure(DrawSurface);
structure(Voxel);
structure(Texture);
structure(LightmapTree);

structure(DrawPrimitive){
    DrawPrimitive* next;

    bool has_lighting : 1;
    bool is_sprite : 1;
    bool smooth_lighting : 1;

    enum{
        PRIMITIVE_QUAD,
        PRIMITIVE_TRIANGLE,
        PRIMITIVE_LINE,
        PRIMITIVE_CIRCLE,
        PRIMITIVE_ELLIPSIS,
        PRIMITIVE_FRAME,
    } type;
    
    Texture* texture;
    Vec2 texture_crd[4];
    union{
        Vec3 position[4];
        Vec2 position_sprite[4];
    };
    Vec2 sprite_size;
    Vec3 luxel_colors[4];
    LightmapTree* lightmap;
    Vec3 luminance;
    int thickness;
};

DrawPrimitive* primitiveToDraw(void);
void octreeDrawList(void);
void octreeDraw(Voxel* voxel);
void voxelModelRasterize(DrawSurface* surface,Vec2 model_angle,Vec3* luminance,Voxel* voxel,Vec3 camera_position,int camera_distance);

#endif
