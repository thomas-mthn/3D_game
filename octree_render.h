#ifndef OCTREE_RENDER_H
#define OCTREE_RENDER_H

extern bool g_render_lines;
extern bool g_render_fog;

void octreeDraw(Voxel* voxel);
void voxelModelRasterize(DrawSurface* surface,Vec2 model_angle,Vec3* luminance,Voxel* voxel,Vec3 camera_position,int camera_distance);

#endif