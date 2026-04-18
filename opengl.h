#ifndef OPENGL_H
#define OPENGL_H

#include "draw.h"

structure(LightmapTree);

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

void drawLineGL(DrawSurface* surface,int x1,int y1,int x2,int y2,Vec3 color);
void drawSegmentGL(DrawSurface* surface,int x1,int y1,int x2,int y2,int thickness,Vec3 color);
void drawSegment3dGL(DrawSurface* surface,Vec3* coordinats,int thickness,Vec3 color);
void drawRectangleGL(DrawSurface* surface,int x,int y,int size_x,int size_y,Vec3 color);
void drawEllipsesGL(DrawSurface* surface,int x,int y,int size_x,int size_y,Vec3 color);
void drawCircle3dGL(DrawSurface* surface,Vec3* coordinates,Vec3 color);
void drawPolygonGL(DrawSurface* surface,Vec2* coordinats,int n_point,Vec3 color);
void drawPolygon3dGL(DrawSurface* surface,Vec3* coordinats,Vec3 color);
void drawColoredPolygonGL(DrawSurface* surface,Vec2* coordinats,Vec3* color,int n_point);
void drawColoredPolygon3dGL(DrawSurface* surface,Vec3* coordinats,Vec3* color,LightmapTree* lightmap);
void drawTexturePolygonGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3 color,int n_point);
void drawTexturePolygon3dGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3 color,int n_point);
void drawColoredTexturePolygonGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3* color,int n_point);
void drawColoredTexturePolygon3dGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color,LightmapTree* lightmap);

void drawColoredTextureSkyboxPolygon3dGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color);

void textureUpdateGL(Texture* texture);

#endif
