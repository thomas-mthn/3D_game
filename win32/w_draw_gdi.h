#ifndef DRAW_GDI_H
#define DRAW_GDI_H

#include "../draw.h"

bool createSurfaceGDI(DrawSurface* surface);
void destroySurfaceGDI(DrawSurface* surface);
void blitSurfaceGDI(DrawSurface surface);
void drawLineGDI(DrawSurface surface,int x1,int y1,int x2,int y2,Vec3 color);
void drawRectangleGDI(DrawSurface surface,int x,int y,int size_x,int size_y,Vec3 color);
void drawCircleGDI(DrawSurface surface,int x,int y,int radius,Vec3 color);
void drawSegmentGDI(DrawSurface surface,int x1,int y1,int x2,int y2,int thickness,Vec3 color);

#endif
