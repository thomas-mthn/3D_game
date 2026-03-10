#ifndef SOFT_DRAW_H
#define SOFT_DRAW_H

//color is an int (hex XXBBGGRR) Red Green Blue

#include "draw.h"

void createSurfaceSoft(DrawSurface* surface);
void destroySurfaceSoft(DrawSurface* surface);
void blitSurfaceSoft(DrawSurface surface);
void surfaceClearSoft(DrawSurface* surface);

void drawLineSoft(DrawSurface surface,int x1,int y1,int x2,int y2,Vec3 color);
void drawPolygonSoft(DrawSurface surface,Vec2* coordinats,int n_point,Vec3 color);
void drawPolygon3dSoft(DrawSurface surface,Vec3* coordinats,Vec3* fog_color,int* fog_density,Vec3 color);
void drawColoredPolygonSoft(DrawSurface surface,Vec2* coordinats,Vec3* color,int n_point);
void drawColoredPolygon3dSoft(DrawSurface surface,Vec3* coordinats,Vec3* color,Vec3* fog_color,int* fog_strength);
void drawTexturePolygonSoft(DrawSurface surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3 color,int n_point);
void drawTexturePolygon3dSoft(DrawSurface surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3 color,int n_point);
void drawColoredTexturePolygonSoft(DrawSurface surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3* color,int n_point);
void drawColoredTexturePolygon3dSoft(DrawSurface surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color,Vec3* fog_color,int* fog_strength);
void drawSegmentSoft(DrawSurface surface,int x1,int y1,int x2,int y2,int thickness,Vec3 color);
void drawEllipsesSoft(DrawSurface surface,int x,int y,int size_x,int size_y,Vec3 color);
void drawRingSoft(DrawSurface surface,int x,int y,int radius,int thickness,Vec3 color);
void drawRectangleSoft(DrawSurface surface,int x,int y,int size_x,int size_y,Vec3 color);

#endif
