#ifndef OPENGL_H
#define OPENGL_H

#include "draw.h"

structure(LightmapTree);
structure(Cubemap);
structure(Voxel);

extern int g_smaa_max;
extern bool g_vsync;

void vsyncSet(bool value);

bool createSurfaceGL(DrawSurface* surface);
void destroySurfaceGL(DrawSurface* surface);
void surfaceClearGL(DrawSurface* surface);
void blitSurfaceGL(DrawSurface* surface);
void changeSurfaceSizeGL(DrawSurface* surface,int width,int height);
void antiAliasingEnableGL(bool enable);

void antiAliasingSetGL(int amount);
void openglDownloadFramebuffer(Texture texture);
void openglPolygonFill(bool fill);

void deleteTextureGL(unsigned texture);

void drawLineGL(DrawSurface* surface,real x1,real y1,real x2,real y2,Vec3 color);
void drawSegmentGL(DrawSurface* surface,real x1,real y1,real x2,real y2,real thickness,Vec3 color);
void drawSegment3dGL(DrawSurface* surface,Vec3* coordinats,int thickness,Vec3 color);
void drawRectangleGL(DrawSurface* surface,real x,real y,real size_x,real size_y,Vec3 color);
void drawEllipsesGL(DrawSurface* surface,real x,real y,real size_x,real size_y,Vec3 color);
void drawCircle3dGL(DrawSurface* surface,Vec3* coordinates,Vec3 color);
void drawPolygonGL(DrawSurface* surface,Vec2* coordinats,int n_point,Vec3 color);
void drawPolygon3dGL(DrawSurface* surface,Vec3* coordinats,Vec3 color);
void drawColoredPolygonGL(DrawSurface* surface,Vec2* coordinats,Vec3* color,int n_point);
void drawColoredPolygon3dGL(DrawSurface* surface,Vec3* coordinats,Vec3* color,LightmapTree* lightmap);
void drawTexturePolygonGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3 color,int n_point);
void drawTexturePolygon3dGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3 color,int n_point);
void drawColoredTexturePolygonGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3* color,int n_point);
void drawColoredTexturePolygon3dGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color,LightmapTree* lightmap,int n_vertex);
void drawSphereGL(DrawSurface* surface,Vec2* coordinats,Voxel* voxel);
void drawCylinderGL(DrawSurface* surface,Vec2* coordinats,Voxel* voxel);
void drawTorusGL(DrawSurface* surface,Vec2* coordinats,Voxel* voxel);

void drawColoredTextureSkyboxPolygon3dGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color,LightmapTree* lightmap);

void textureUpdateGL(Texture* texture);
void openglUpdateCubemap(Cubemap* cubemap);

typedef enum{
    PROCTEXT_NONE,
    PROCTEXT_BRICK,
    PROCTEXT_VORONOI,
    PROCTEXT_ECOUNT,
} ProcTextType;

void lightmapUploadGL(void);
void drawLightmapPolygon3dGL(DrawSurface* surface,Vec3* coordinats,int lightmap_index,Vec3 normal,int side,Vec3 color,ProcTextType procedural_texture);
void drawLightmapTexturePolygon3dGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,int lightmap_index,int side,Vec3 color);

#endif
