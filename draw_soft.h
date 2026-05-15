#ifndef SOFT_DRAW_H
#define SOFT_DRAW_H

//color is an int (hex XXBBGGRR) Red Green Blue
#include "langext.h"
#include "vec2.h"
#include "vec3.h"

structure(Texture);
structure(DrawSurface);

bool softSurfaceCreate(DrawSurface* surface);
void softSurfaceDestroy(DrawSurface* surface);
void softSurfaceDestroyMeta(DrawSurface* surface);

void softSurfaceBlit(DrawSurface* surface);
void softSurfaceClear(DrawSurface* surface);
void softSurfaceSizeChange(DrawSurface* surface,int width,int height);

void drawLineSoft(DrawSurface* surface,real x1,real y1,real x2,real y2,Vec3 color);
void drawPolygonSoft(DrawSurface* surface,Vec2* coordinats,int n_point,Vec3 color);
void drawPolygon3dSoft(DrawSurface* surface,Vec3* coordinats,Vec3 color);
void drawColoredPolygonSoft(DrawSurface* surface,Vec2* coordinats,Vec3* color,int n_point);
void drawColoredPolygon3dSoft(DrawSurface* surface,Vec3* coordinats,Vec3* color);
void drawTexturePolygonSoft(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3 color,int n_point);
void drawTexturePolygon3dSoft(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3 color,int n_point);
void drawColoredTexturePolygonSoft(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3* color,int n_point);
void drawColoredTexturePolygon3dSoft(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color);
void drawSegmentSoft(DrawSurface* surface,real x1,real y1,real x2,real y2,real thickness,Vec3 color);
void drawEllipsesSoft(DrawSurface* surface,real x,real y,real size_x,real size_y,Vec3 color);
void drawRingSoft(DrawSurface* surface,real x,real y,real radius,int thickness,Vec3 color);
void drawRectangleSoft(DrawSurface* surface,real x,real y,real size_x,real size_y,Vec3 color);

#endif
