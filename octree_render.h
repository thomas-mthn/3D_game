#ifndef OCTREE_RENDER_H
#define OCTREE_RENDER_H

#include "langext.h"
#include "vec3.h"
#include "vec2.h"

structure(DrawSurface);
structure(Voxel);
structure(Texture);
structure(LightmapTree);

structure(PolygonToDraw){
    PolygonToDraw* next;

    bool has_lighting : 1;
    
    Texture* texture;
    Vec2 texture_crd[4];
    Vec3 position[4];
    Vec3 luxel_colors[4];
    LightmapTree* lightmap;
    Vec3 luminance;
};

PolygonToDraw* polygonToDrawAdd(void);
void octreeDrawList(void);
void octreeDraw(Voxel* voxel);
void voxelModelRasterize(DrawSurface* surface,Vec2 model_angle,Vec3* luminance,Voxel* voxel,Vec3 camera_position,int camera_distance);

#endif
