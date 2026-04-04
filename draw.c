#include "draw.h"
#include "draw_soft.h"
#include "font.h"
#include "fixed.h"
#include "main.h"
#include "langext.h"
#include "span.h"

#if !defined(__wasm__) && !defined(__linux__)
#include "win32/w_draw_gdi.h"
#include "win32/w_draw_opengl.h"
#endif

DrawSurface g_surface = {
	.width = 640 * 2,
	.height = 480 * 2,
	.window_width = 2560 / 2,
    .window_height = 1440 / 2,
    .backend = RENDER_BACKEND_SOFTWARE,
};

static void (*destroySurface[])(DrawSurface*) = {
	[RENDER_BACKEND_SOFTWARE] = softSurfaceDestroy,
#if !defined(__wasm__) && !defined(__linux__)
	[RENDER_BACKEND_GDI] = destroySurfaceGDI,
    [RENDER_BACKEND_GL] = destroySurfaceGL,
#endif
};

static bool (*createSurface[])(DrawSurface*) = {
	[RENDER_BACKEND_SOFTWARE] = softSurfaceCreate,
#if !defined(__wasm__) && !defined(__linux__)
	[RENDER_BACKEND_GDI] = createSurfaceGDI,
    [RENDER_BACKEND_GL] = createSurfaceGL,
#endif
};

int transformDraw(int size,int v){
    v *= size / 2;
    v >>= FIXED_PRECISION;
    v += size / 2;
    return v;
}

int scaleDraw(int size,int v){
    return v * (size / 2) >> FIXED_PRECISION;
}

void surfaceInit(DrawSurface* surface){
	if(!createSurface[surface->backend])
	    return;
	if(!createSurface[surface->backend](surface)){
		surface->backend = RENDER_BACKEND_SOFTWARE;
		createSurface[surface->backend](surface);
	}
}

void surfaceDestroy(DrawSurface* surface){
    if(!destroySurface[surface->backend])
        return;
    destroySurface[surface->backend](surface);
}

void surfaceClear(DrawSurface* surface){
    void (*vtable[])(DrawSurface*) = {
#if !defined(__wasm__) && !defined(__linux__)
        [RENDER_BACKEND_GL] = surfaceClearGL,
#endif
        [RENDER_BACKEND_SOFTWARE] = softSurfaceClear,
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
    vtable[surface->backend](surface);
}

void surfaceChangeSize(DrawSurface* surface,int width,int height){
    surface->window_width = width;
    surface->window_height = height;
    void (*vtable[])(DrawSurface*,int,int) = {
        [RENDER_BACKEND_SOFTWARE] = softSurfaceSizeChange,
#if !defined(__wasm__) && !defined(__linux__)
        [RENDER_BACKEND_GL] = changeSurfaceSizeGL,
#endif
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,width,height);
}

 void surfaceChangeBackend(DrawSurface* surface,RenderBackend backend){
    if(surface->backend == backend)
        return;

    destroySurface[surface->backend](surface);
    surface->backend = backend;
	surfaceInit(surface);
}

void surfaceBlit(DrawSurface surface){
    void (*blit[])(DrawSurface) = {
		[RENDER_BACKEND_SOFTWARE] = softSurfaceBlit,
#if !defined(__wasm__) && !defined(__linux__)
	    [RENDER_BACKEND_GDI] = softSurfaceBlit,
        [RENDER_BACKEND_GL] = blitSurfaceGL,
#endif
	};
    if(surface.backend >= countof(blit) || !blit[surface.backend])
        return;
    
    blit[surface.backend](surface);
}

void drawLine(DrawSurface surface,int x1,int y1,int x2,int y2,Vec3 color){
	void (*vtable[])(DrawSurface,int,int,int,int,Vec3) = {
		[RENDER_BACKEND_SOFTWARE] = drawLineSoft,
#if !defined(__wasm__) && !defined(__linux__)
	    [RENDER_BACKEND_GDI] = drawLineGDI,
        [RENDER_BACKEND_GL] = drawLineGL,
#endif
	};
    if(surface.backend >= countof(vtable) || !vtable[surface.backend])
        return;
	vtable[surface.backend](surface,x1,y1,x2,y2,color);
}

void drawSegment(DrawSurface surface,int x1,int y1,int x2,int y2,int thickness,Vec3 color){
	void (*vtable[])(DrawSurface,int,int,int,int,int,Vec3) = {
		[RENDER_BACKEND_SOFTWARE] = drawSegmentSoft,
#if !defined(__wasm__) && !defined(__linux__)
        [RENDER_BACKEND_GL] = drawSegmentGL,
	    [RENDER_BACKEND_GDI] = drawSegmentGDI,
#endif
	};
    if(surface.backend >= countof(vtable) || !vtable[surface.backend])
        return;
	vtable[surface.backend](surface,x1,y1,x2,y2,thickness,color);
}

void drawSegment3d(DrawSurface surface,Vec3* coordinats,int thickness,Vec3 color){
	void (*vtable[])(DrawSurface,Vec3*,int,Vec3) = {
#if !defined(__wasm__) && !defined(__linux__)
        [RENDER_BACKEND_GL] = drawSegment3dGL,
#endif
	};
    if(surface.backend >= countof(vtable) || !vtable[surface.backend])
        return;
	vtable[surface.backend](surface,coordinats,thickness,color);
}

void drawPolygon(DrawSurface surface,Vec2* coordinats,int n_point,Vec3 color){
    void (*vtable[])(DrawSurface,Vec2*,int,Vec3) = {
		[RENDER_BACKEND_SOFTWARE] = drawPolygonSoft,
#if !defined(__wasm__) && !defined(__linux__)
        [RENDER_BACKEND_GL] = drawPolygonGL,
#endif
	};
    if(surface.backend >= countof(vtable) || !vtable[surface.backend])
        return;
	vtable[surface.backend](surface,coordinats,n_point,color);
}

void drawPolygon3d(DrawSurface surface,Vec3* coordinats,Vec3 color){
    void (*vtable[])(DrawSurface*,Vec3*,Vec3) = {
		[RENDER_BACKEND_SOFTWARE] = spanQuad3dAdd,
#if !defined(__wasm__) && !defined(__linux__)
        [RENDER_BACKEND_GL] = drawPolygon3dGL,
#endif
	};
    if(surface.backend >= countof(vtable) || !vtable[surface.backend])
        return;
	vtable[surface.backend](&surface,coordinats,color);
}

void drawColoredPolygon(DrawSurface surface,Vec2* coordinats,Vec3* color,int n_point){
    void (*vtable[])(DrawSurface,Vec2*,Vec3*,int) = {
		[RENDER_BACKEND_SOFTWARE] = drawColoredPolygonSoft,
#if !defined(__wasm__) && !defined(__linux__)
        [RENDER_BACKEND_GL] = drawColoredPolygonGL,
#endif
	};
    if(surface.backend >= countof(vtable) || !vtable[surface.backend])
        return;
	vtable[surface.backend](surface,coordinats,color,n_point);
}

void drawColoredPolygon3d(DrawSurface surface,Vec3* coordinats,Vec3* color){
    void (*vtable[])(DrawSurface*,Vec3*,Vec3*) = {
		[RENDER_BACKEND_SOFTWARE] = spanQuad3dLightingAdd,
#if !defined(__wasm__) && !defined(__linux__)
        [RENDER_BACKEND_GL] = drawColoredPolygon3dGL,
#endif
	};
    if(surface.backend >= countof(vtable) || !vtable[surface.backend])
        return;
	vtable[surface.backend](&surface,coordinats,color);
}

void drawTexturePolygon(DrawSurface surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3 color,int n_point){
    void (*vtable[])(DrawSurface,Texture*,Vec2*,Vec2*,Vec3,int) = {
		[RENDER_BACKEND_SOFTWARE] = drawTexturePolygonSoft,
#if !defined(__wasm__) && !defined(__linux__)
        [RENDER_BACKEND_GL] = drawTexturePolygonGL,
#endif
	};
    if(surface.backend >= countof(vtable) || !vtable[surface.backend])
        return;
	vtable[surface.backend](surface,texture,texture_coordinats,coordinats,color,n_point);
}

void drawTexturePolygon3d(DrawSurface surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3 color,int n_point){
    void (*vtable[])(DrawSurface,Texture*,Vec2*,Vec3*,Vec3,int) = {
		[RENDER_BACKEND_SOFTWARE] = drawTexturePolygon3dSoft,
#if !defined(__wasm__) && !defined(__linux__)
        [RENDER_BACKEND_GL] = drawTexturePolygon3dGL,
#endif
	};
    if(surface.backend >= countof(vtable) || !vtable[surface.backend])
        return;
	vtable[surface.backend](surface,texture,texture_coordinats,coordinats,color,n_point);
}

void drawColoredTexturePolygon(DrawSurface surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3* color,int n_point){
    void (*vtable[])(DrawSurface,Texture*,Vec2*,Vec2*,Vec3*,int) = {
		[RENDER_BACKEND_SOFTWARE] = drawColoredTexturePolygonSoft,
#if !defined(__wasm__) && !defined(__linux__)
        [RENDER_BACKEND_GL] = drawColoredTexturePolygonGL,
#endif
	};
    if(surface.backend >= countof(vtable) || !vtable[surface.backend])
        return;
	vtable[surface.backend](surface,texture,texture_coordinats,coordinats,color,n_point);
}

void drawColoredTexturePolygon3d(DrawSurface surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color){
    void (*vtable[])(DrawSurface*,Texture*,Vec2*,Vec3*,Vec3*) = {
		[RENDER_BACKEND_SOFTWARE] = spanQuad3dLightingTextureAdd,
#if !defined(__wasm__) && !defined(__linux__)
        [RENDER_BACKEND_GL] = drawColoredTexturePolygon3dGL,
#endif
	};
    if(surface.backend >= countof(vtable) || !vtable[surface.backend])
        return;
	vtable[surface.backend](&surface,texture,texture_coordinats,coordinats,color);
}

void drawSkyboxPolygon3d(DrawSurface surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color){
    void (*vtable[])(DrawSurface*,Texture*,Vec2*,Vec3*,Vec3*) = {
		[RENDER_BACKEND_SOFTWARE] = spanQuad3dLightingTextureAdd,
#if !defined(__wasm__) && !defined(__linux__)
        [RENDER_BACKEND_GL] = drawColoredTextureSkyboxPolygon3dGL,
#endif
	};
    if(surface.backend >= countof(vtable) || !vtable[surface.backend])
        return;
	vtable[surface.backend](&surface,texture,texture_coordinats,coordinats,color);
}

void drawEllipses(DrawSurface surface,int x,int y,int size_x,int size_y,Vec3 color){
	void (*vtable[])(DrawSurface,int,int,int,int,Vec3) = {
		[RENDER_BACKEND_SOFTWARE] = drawEllipsesSoft,
#if !defined(__wasm__) && !defined(__linux__)
        [RENDER_BACKEND_GL] = drawEllipsesGL,
#endif
	};
    if(surface.backend >= countof(vtable) || !vtable[surface.backend])
        return;
	vtable[surface.backend](surface,x,y,size_x,size_y,color);
}

void drawCircle(DrawSurface surface,int x,int y,int radius,Vec3 color){
    drawEllipses(surface,x,y,radius,radius,color);
}

void drawCircle3d(DrawSurface surface,Vec3* coordinates,Vec3 color){
	void (*vtable[])(DrawSurface,Vec3*,Vec3) = {
#if !defined(__wasm__) && !defined(__linux__)
        [RENDER_BACKEND_GL] = drawCircle3dGL,
#endif
	};
    if(surface.backend >= countof(vtable) || !vtable[surface.backend])
        return;
	vtable[surface.backend](surface,coordinates,color);
}

void drawRing(DrawSurface surface,int x,int y,int radius,int thickness,Vec3 color){
	void (*vtable[])(DrawSurface,int,int,int,int,Vec3) = {
		[RENDER_BACKEND_SOFTWARE] = drawRingSoft,
	};
    if(surface.backend >= countof(vtable) || !vtable[surface.backend])
        return;
	vtable[surface.backend](surface,x,y,radius,thickness,color);
}

void drawRectangle(DrawSurface surface,int x,int y,int size_x,int size_y,Vec3 color){
	void (*vtable[])(DrawSurface,int,int,int,int,Vec3) = {
		[RENDER_BACKEND_SOFTWARE] = drawRectangleSoft,
#if !defined(__wasm__) && !defined(__linux__)
	    [RENDER_BACKEND_GDI] = drawRectangleGDI,
        [RENDER_BACKEND_GL] = drawRectangleGL,
#endif
	};
    if(surface.backend >= countof(vtable) || !vtable[surface.backend])
        return;
	vtable[surface.backend](surface,x,y,size_x,size_y,color);
}

void drawString(DrawSurface surface,int x,int y,String string,int scale,Vec3 color,int thickness){
    int down_offset = 0;
    int offset = 0;
    for(int j = 0;j < string.size;j++){
        char string_char = string.data[j];
        if(string_char == '\n'){
            down_offset += FIXED_ONE;
            offset = 0;
        }
        else{
            for(int i = 0;g_vector_font[string_char].position[i][0];i++){
                uint8* coords = &g_vector_font[string_char].position[i][0];
                int offset_transform = fixedMulR(offset,scale);
                int offset_transform_x = fixedMulR(down_offset,scale);
                int coord_transform[] = {
                    (coords[0] * scale >> 8) + x + offset_transform_x,(coords[1] * scale >> 8) + y + offset_transform,
                    (coords[2] * scale >> 8) + x + offset_transform_x,(coords[3] * scale >> 8) + y + offset_transform
                };
                drawSegment(surface,coord_transform[0],coord_transform[1],coord_transform[2],coord_transform[3],thickness,color);
            }
        }
        offset += g_vector_font[string_char].width;
    }
}

void drawNumber(DrawSurface surface,int x,int y,int number,int scale){
	char buffer[0x10];
    String string = intToString(buffer,number);
    drawString(surface,x,y,string,scale,pixelColorToColor(0xFFFFFF),0xC0);
}
