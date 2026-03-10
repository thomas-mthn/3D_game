#ifndef SPAN_H
#define SPAN_H

#include "langext.h"
#include "vec3.h"
#include "vec2.h"

structure(Texture);
structure(DrawSurface);

void spanQuadAdd(DrawSurface* surface,Vec3* coordinats,Vec3 color);
void spanQuadLightingAdd(DrawSurface* surface,Vec3* coordinats,Vec3* color);
void spanQuadLightingTextureAdd(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color);

void spanDrawList(void);

#endif