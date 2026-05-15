#ifndef SPAN_H
#define SPAN_H

#include "langext.h"
#include "vec3.h"
#include "vec2.h"

structure(LightmapTree);
structure(Texture);
structure(DrawSurface);

void spanQuadAdd(DrawSurface* surface,Vec2* coords_2d,Vec3 color);
void spanQuad3dAdd(DrawSurface* surface,Vec3* coordinats,Vec3 color);
void spanQuad3dLightingAdd(DrawSurface* surface,Vec3* coordinats,Vec3* color,LightmapTree* lightmap);
void spanSpriteAdd(DrawSurface* surface,Texture* texture,Vec2* coords_2d);
void spanEllipsesAdd(DrawSurface* surface,real cx,real cy,real size_x,real size_y,Vec3 color);
void spanQuad3dLightingTextureAdd(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color,LightmapTree* lightmap,int n_vertex);
void spanQuad3dTextureAdd(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3 color,int n_point);
void spanSegmentAdd(DrawSurface* surface,real x1,real y1,real x2,real y2,int thickness,Vec3 color);

void spanDrawList(DrawSurface* surface);

#endif
