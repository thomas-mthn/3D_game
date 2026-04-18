#ifndef DRAW_H
#define DRAW_H

#if !defined(__wasm__) && !defined(__linux__)
#include "win32/w_gdi.h"
#endif

#include "vec2.h"
#include "vec3.h"
#include "texture.h"
#include "string.h"
#include "memory.h"

typedef enum{
	RENDER_BACKEND_SOFTWARE,
	RENDER_BACKEND_GDI,
	RENDER_BACKEND_GL,
} RenderBackend;

structure(Scanline){
    int16* begin;
    int16* end;
};

structure(ScanlineColor){
	Vec2 begin;
	Vec2 end;
};

structure(ScanlineTexture){
	Vec2* begin;
	Vec2* end;
};

structure(ScanlineZ){
    int begin;
    int end;
};

structure(DrawSurface){
	int* data;
	int width;
	int height;
	RenderBackend backend;

	int window_width;
	int window_height;

    //winapi
	void* window_context;
	void* gdi_context;
	void* gl_context;
	void* gdi_bitmap;

    //X11
    size_t window;
    void* display;
    int screen;
	
	Scanline scanline;
	ScanlineColor* scanline_color;
	ScanlineTexture scanline_texture;
    ScanlineZ* scanline_z;

    MemoryArena fb_meta_arena; 
    
#if !defined(__wasm__) && !defined(__linux__)
	BitmapInfo soft_bitmapinfo;
#endif
};

structure(LightmapTree);
    
void surfaceInit(DrawSurface* surface);
void surfaceDestroy(DrawSurface* surface);
void surfaceClear(DrawSurface* surface);
void surfaceChangeSize(DrawSurface* surface,int width,int height);
void surfaceChangeBackend(DrawSurface* surface,RenderBackend backend);
void surfaceBlit(DrawSurface* surface);

void drawLine(DrawSurface* surface,int x1,int y1,int x2,int y2,Vec3 color);
void drawSegment(DrawSurface* surface,int x1,int y1,int x2,int y2,int thickness,Vec3 color);
void drawSegment3d(DrawSurface* surface,Vec3* coordinats,int thickness,Vec3 color);
void drawPolygon(DrawSurface* surface,Vec2* coordinats,int n_point,Vec3 color);
void drawPolygon3d(DrawSurface* surface,Vec3* coordinats,Vec3 color);
void drawColoredPolygon(DrawSurface* surface,Vec2* coordinats,Vec3* color,int n_point);
void drawColoredPolygon3d(DrawSurface* surface,Vec3* coordinats,Vec3* color,LightmapTree* lightmap);
void drawTexturePolygon(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3 color,int n_point);
void drawTexturePolygon3d(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3 color,int n_point);
void drawColoredTexturePolygon(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3* color,int n_point);
void drawColoredTexturePolygon3d(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color,LightmapTree* lightmap);
void drawSkyboxPolygon3d(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color);
void drawCircle(DrawSurface* surface,int x,int y,int radius,Vec3 color);
void drawEllipses(DrawSurface* surface,int x,int y,int size_x,int size_y,Vec3 color);
void drawCircle3d(DrawSurface* surface,Vec3* coordinates,Vec3 color);
void drawRing(DrawSurface* surface,int x,int y,int radius,int thickness,Vec3 color);
void drawRectangle(DrawSurface* surface,int x,int y,int size_x,int size_y,Vec3 color);
void drawStringEx(DrawSurface* surface,int x,int y,String string,int scale,Vec3 color,int thickness);
void drawNumber(DrawSurface* surface,int x,int y,int number,int scale);

static void drawString(DrawSurface* surface,int x,int y,String string,int scale,Vec3 color){
    drawStringEx(surface,x,y,string,scale,color,FIXED_ONE >> 3);
}

static void drawSquare(DrawSurface* surface,int x,int y,int size,Vec3 color){
	drawRectangle(surface,x,y,size,size,color);
}

static void drawFrame(DrawSurface* surface,int x,int y,int size_x,int size_y,Vec3 color,int thickness){
	drawRectangle(surface,x,y,size_x,thickness,color);
	drawRectangle(surface,x,y,thickness,size_y,color);
	drawRectangle(surface,x,y + size_y - thickness,size_x,thickness,color);
	drawRectangle(surface,x + size_x - thickness,y,thickness,size_y,color);
}

static int transformDraw(int size,int v){
    v *= size / 2;
    v >>= FIXED_PRECISION;
    v += size / 2;
    return v;
}

static int scaleDraw(int size,int v){
    return v * (size / 2) >> FIXED_PRECISION;
}

extern DrawSurface g_surface;

#endif
