#ifndef SPAN_H
#define SPAN_H

#include "langext.h"
#include "vec3.h"
#include "vec2.h"

structure(Texture);
structure(DrawSurface);

void spanQuadAdd(DrawSurface* surface,Vec2* coords_2d,Vec3 color);
void spanQuad3dAdd(DrawSurface* surface,Vec3* coordinats,Vec3 color);
void spanQuad3dLightingAdd(DrawSurface* surface,Vec3* coordinats,Vec3* color);
void spanSpriteAdd(DrawSurface* surface,Texture* texture,Vec2* coords_2d);
void spanEllipsesAdd(DrawSurface* surface,int cx,int cy,int size_x,int size_y,Vec3 color);
void spanQuad3dLightingTextureAdd(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color);
void spanSegmentAdd(DrawSurface* surface,int x1,int y1,int x2,int y2,int thickness,Vec3 color);

void spanDrawList(void);

#endif
