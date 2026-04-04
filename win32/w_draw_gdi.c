#include "w_draw_gdi.h"
#include "w_gdi.h"
#include "w_user.h"

#include "../main.h"

bool createSurfaceGDI(DrawSurface* surface){
	surface->gdi_context = CreateCompatibleDC(surface->window_context);
    surface->gdi_bitmap = CreateCompatibleBitmap(surface->window_context,surface->width,surface->height);
    SelectObject(surface->gdi_context,surface->gdi_bitmap);
	return true;
}

void destroySurfaceGDI(DrawSurface* surface){
	DeleteDC(surface->gdi_context);
    DeleteObject(surface->gdi_bitmap);
}

void blitSurfaceGDI(DrawSurface surface){
	StretchBlt(surface.window_context,0,0,surface.window_width,surface.window_height,surface.gdi_context,0,0,surface.width,surface.height,0x00CC0020);
}

void drawLineGDI(DrawSurface surface,int x1,int y1,int x2,int y2,Vec3 color){
	int pixel_color = colorToPixelColor(color);

	x1 = transformDraw(surface.height,x1);
    y1 = transformDraw(surface.width ,y1);
    x2 = transformDraw(surface.height,x2);
    y2 = transformDraw(surface.width ,y2);

	pixel_color = (pixel_color << 16 & 0xFF0000) | (pixel_color >> 16 & 0x0000FF) | (pixel_color & 0x00FF00);
	void* pen = CreatePen(PS_SOLID,1,pixel_color);
	SelectObject(surface.gdi_context,pen);
	MoveToEx(surface.gdi_context,y1,x1,0);
	LineTo(surface.gdi_context,y2,x2);
	DeleteObject(pen);
}

void drawRectangleGDI(DrawSurface surface,int x,int y,int size_x,int size_y,Vec3 color){
	int pixel_color = colorToPixelColor(color);

	pixel_color = (pixel_color << 16 & 0xFF0000) | (pixel_color >> 16 & 0x0000FF) | (pixel_color & 0x00FF00);
	void* brush = CreateSolidBrush(pixel_color);
	FillRect(surface.gdi_context,&(Rect){y,x,y + size_y,x + size_x},brush);
	DeleteObject(brush);
}

void drawCircleGDI(DrawSurface surface,int x,int y,int radius,Vec3 color){
	int pixel_color = colorToPixelColor(color);

	x = transformDraw(surface.height,x);
    y = transformDraw(surface.width,y);
	pixel_color = (pixel_color << 16 & 0xFF0000) | (pixel_color >> 16 & 0x0000FF) | (pixel_color & 0x00FF00);
	void* brush = CreateSolidBrush(pixel_color);
	SelectObject(surface.gdi_context,CreatePen(PS_NULL,0,0x000000));
	SelectObject(surface.gdi_context,brush);
	Ellipse(surface.gdi_context,scaleDraw(surface.width,y - radius),scaleDraw(surface.height,x - radius),scaleDraw(surface.width,y + radius),scaleDraw(surface.height,x + radius));
	DeleteObject(brush);
}

void drawSegmentGDI(DrawSurface surface,int x1,int y1,int x2,int y2,int thickness,Vec3 color){
	int pixel_color = colorToPixelColor(color);

	pixel_color = (pixel_color << 16 & 0xFF0000) | (pixel_color >> 16 & 0x0000FF) | (pixel_color & 0x00FF00);
	void* pen = CreatePen(PS_SOLID,thickness,pixel_color);
	SelectObject(surface.gdi_context,pen);
	MoveToEx(surface.gdi_context,y1,x1,0);
	LineTo(surface.gdi_context,y2,x2);
	DeleteObject(pen);
}
