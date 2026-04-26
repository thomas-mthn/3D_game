#include "draw.h"
#include "draw_soft.h"
#include "font.h"
#include "fixed.h"
#include "main.h"
#include "langext.h"
#include "span.h"
#include "opengl.h"
#include "dda.h"

#ifdef _MSC_VER
#include "win32/w_draw_gdi.h"
#endif

#ifdef __wasm__
#include "wasm/wasm.h"
#endif

DrawSurface g_surface = {
	.width = 640 * 2,
	.height = 480 * 2,
	.window_width = 2560 / 2,
    .window_height = 1440 / 2,
    .backend = RENDER_BACKEND_SOFTWARE,
    .angle = PLAYER_SPAWN_ANGLE,
    .position = PLAYER_SPAWN_POSITION,
};

static void (*destroySurface[])(DrawSurface*) = {
	[RENDER_BACKEND_SOFTWARE] = softSurfaceDestroy,
    [RENDER_BACKEND_GL] = destroySurfaceGL,
#if !defined(__wasm__) && !defined(__linux__)
	[RENDER_BACKEND_GDI] = destroySurfaceGDI,
#endif
};

static bool (*createSurface[])(DrawSurface*) = {
	[RENDER_BACKEND_SOFTWARE] = softSurfaceCreate,
    [RENDER_BACKEND_GL] = createSurfaceGL,
#if !defined(__wasm__) && !defined(__linux__)
	[RENDER_BACKEND_GDI] = createSurfaceGDI,
#endif
};

void surfaceInit(DrawSurface* surface){
    int alloc_size = OCCLUSION_BUFFER_SIZE * OCCLUSION_BUFFER_SIZE / CHAR_BIT;
    surface->occlusion_buffer = memoryArenaAllocate(&surface->fb_meta_arena,alloc_size);
    surface->scanline_occlusion.begin = memoryArenaAllocate(&surface->fb_meta_arena,OCCLUSION_BUFFER_SIZE * sizeof(*surface->scanline.begin));
    surface->scanline_occlusion.end = memoryArenaAllocate(&surface->fb_meta_arena,OCCLUSION_BUFFER_SIZE * sizeof(*surface->scanline.end));
    
    surface->rotation_matrix[0] = tCos(surface->angle.x);
    surface->rotation_matrix[1] = tSin(surface->angle.x);
    surface->rotation_matrix[2] = tCos(surface->angle.y);
    surface->rotation_matrix[3] = tSin(surface->angle.y);
    
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
        [RENDER_BACKEND_GL] = surfaceClearGL,
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
        [RENDER_BACKEND_GL] = changeSurfaceSizeGL,
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

void surfaceBlit(DrawSurface* surface){
    void (*blit[])(DrawSurface*) = {
		[RENDER_BACKEND_SOFTWARE] = softSurfaceBlit,
        [RENDER_BACKEND_GL] = blitSurfaceGL,
#if !defined(__wasm__) && !defined(__linux__)
	    [RENDER_BACKEND_GDI] = softSurfaceBlit,
        
#endif
	};
    if(surface->backend >= countof(blit) || !blit[surface->backend])
        return;
    
    blit[surface->backend](surface);
}

void drawLine(DrawSurface* surface,int x1,int y1,int x2,int y2,Vec3 color){
	void (*vtable[])(DrawSurface*,int,int,int,int,Vec3) = {
		[RENDER_BACKEND_SOFTWARE] = drawLineSoft,
        [RENDER_BACKEND_GL] = drawLineGL,
#if !defined(__wasm__) && !defined(__linux__)
	    [RENDER_BACKEND_GDI] = drawLineGDI,
#endif
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,x1,y1,x2,y2,color);
}

void drawSegment(DrawSurface* surface,int x1,int y1,int x2,int y2,int thickness,Vec3 color){
	void (*vtable[])(DrawSurface*,int,int,int,int,int,Vec3) = {
		[RENDER_BACKEND_SOFTWARE] = drawSegmentSoft,
        [RENDER_BACKEND_GL] = drawSegmentGL,
#if !defined(__wasm__) && !defined(__linux__)
	    [RENDER_BACKEND_GDI] = drawSegmentGDI,
#endif
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,x1,y1,x2,y2,thickness,color);
}

void drawSegment3d(DrawSurface* surface,Vec3* coordinats,int thickness,Vec3 color){
	void (*vtable[])(DrawSurface*,Vec3*,int,Vec3) = {
        [RENDER_BACKEND_GL] = drawSegment3dGL,
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,coordinats,thickness,color);
}

void drawPolygon(DrawSurface* surface,Vec2* coordinats,int n_point,Vec3 color){
    void (*vtable[])(DrawSurface*,Vec2*,int,Vec3) = {
		[RENDER_BACKEND_SOFTWARE] = drawPolygonSoft,
        [RENDER_BACKEND_GL] = drawPolygonGL,
#if defined(__wasm__)
        [RENDER_BACKEND_WEBGL] = wasmPolygon,
#endif
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,coordinats,n_point,color);
}

void drawPolygon3d(DrawSurface* surface,Vec3* coordinats,Vec3 color){
    void (*vtable[])(DrawSurface*,Vec3*,Vec3) = {
		[RENDER_BACKEND_SOFTWARE] = spanQuad3dAdd,
        [RENDER_BACKEND_GL] = drawPolygon3dGL,
#if defined(__wasm__)
        [RENDER_BACKEND_WEBGL] = wasmPolygon3d,
#endif
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,coordinats,color);
}

void drawColoredPolygon(DrawSurface* surface,Vec2* coordinats,Vec3* color,int n_point){
    void (*vtable[])(DrawSurface*,Vec2*,Vec3*,int) = {
		[RENDER_BACKEND_SOFTWARE] = drawColoredPolygonSoft,
        [RENDER_BACKEND_GL] = drawColoredPolygonGL,
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,coordinats,color,n_point);
}

void drawColoredPolygon3d(DrawSurface* surface,Vec3* coordinats,Vec3* color,LightmapTree* lightmap){
    void (*vtable[])(DrawSurface*,Vec3*,Vec3*,LightmapTree*) = {
		[RENDER_BACKEND_SOFTWARE] = spanQuad3dLightingAdd,
        [RENDER_BACKEND_GL] = drawColoredPolygon3dGL,
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,coordinats,color,lightmap);
}

void drawTexturePolygon(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3 color,int n_point){
    void (*vtable[])(DrawSurface*,Texture*,Vec2*,Vec2*,Vec3,int) = {
		[RENDER_BACKEND_SOFTWARE] = drawTexturePolygonSoft,
        [RENDER_BACKEND_GL] = drawTexturePolygonGL,
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,texture,texture_coordinats,coordinats,color,n_point);
}

void drawTexturePolygon3d(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3 color,int n_point){
    void (*vtable[])(DrawSurface*,Texture*,Vec2*,Vec3*,Vec3,int) = {
        [RENDER_BACKEND_SOFTWARE] = spanQuad3dTextureAdd,
        [RENDER_BACKEND_GL] = drawTexturePolygon3dGL,
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,texture,texture_coordinats,coordinats,color,n_point);
}

void drawColoredTexturePolygon(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3* color,int n_point){
    void (*vtable[])(DrawSurface*,Texture*,Vec2*,Vec2*,Vec3*,int) = {
		[RENDER_BACKEND_SOFTWARE] = drawColoredTexturePolygonSoft,
        [RENDER_BACKEND_GL] = drawColoredTexturePolygonGL,
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,texture,texture_coordinats,coordinats,color,n_point);
}

void drawColoredTexturePolygon3d(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color,LightmapTree* lightmap){
    void (*vtable[])(DrawSurface*,Texture*,Vec2*,Vec3*,Vec3*,LightmapTree*) = {
		[RENDER_BACKEND_SOFTWARE] = spanQuad3dLightingTextureAdd,
        [RENDER_BACKEND_GL] = drawColoredTexturePolygon3dGL,
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,texture,texture_coordinats,coordinats,color,lightmap);
}

void drawSkyboxPolygon3d(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color,LightmapTree* lightmap){
    void (*vtable[])(DrawSurface*,Texture*,Vec2*,Vec3*,Vec3*,LightmapTree*) = {
		[RENDER_BACKEND_SOFTWARE] = spanQuad3dLightingTextureAdd,
        [RENDER_BACKEND_GL] = drawColoredTextureSkyboxPolygon3dGL,
#if !defined(__wasm__) && !defined(__linux__)
        
#endif
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,texture,texture_coordinats,coordinats,color,lightmap);
}

void drawEllipses(DrawSurface* surface,int x,int y,int size_x,int size_y,Vec3 color){
	void (*vtable[])(DrawSurface*,int,int,int,int,Vec3) = {
		[RENDER_BACKEND_SOFTWARE] = drawEllipsesSoft,
        [RENDER_BACKEND_GL] = drawEllipsesGL,
#if !defined(__wasm__) && !defined(__linux__)
        
#endif
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,x,y,size_x,size_y,color);
}

void drawCircle(DrawSurface* surface,int x,int y,int radius,Vec3 color){
    drawEllipses(surface,x,y,radius,radius,color);
}

void drawCircle3d(DrawSurface* surface,Vec3* coordinates,Vec3 color){
	void (*vtable[])(DrawSurface*,Vec3*,Vec3) = {
        [RENDER_BACKEND_GL] = drawCircle3dGL,
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,coordinates,color);
}

void drawRing(DrawSurface* surface,int x,int y,int radius,int thickness,Vec3 color){
	void (*vtable[])(DrawSurface*,int,int,int,int,Vec3) = {
		[RENDER_BACKEND_SOFTWARE] = drawRingSoft,
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,x,y,radius,thickness,color);
}

void drawRectangle(DrawSurface* surface,int x,int y,int size_x,int size_y,Vec3 color){
	void (*vtable[])(DrawSurface*,int,int,int,int,Vec3) = {
		[RENDER_BACKEND_SOFTWARE] = drawRectangleSoft,
        [RENDER_BACKEND_GL] = drawRectangleGL,
#if !defined(__wasm__) && !defined(__linux__)
	    [RENDER_BACKEND_GDI] = drawRectangleGDI,
#endif
	};
    if(surface->backend >= countof(vtable) || !vtable[surface->backend])
        return;
	vtable[surface->backend](surface,x,y,size_x,size_y,color);
}

void drawStringEx(DrawSurface* surface,int x,int y,String string,int scale,Vec3 color,int thickness){
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
                    (coords[0] * scale >> 8) + x + offset_transform_x,(-coords[1] * scale >> 8) + y + offset_transform,
                    (coords[2] * scale >> 8) + x + offset_transform_x,(-coords[3] * scale >> 8) + y + offset_transform
                };
                drawSegment(surface,coord_transform[0],coord_transform[1],coord_transform[2],coord_transform[3],fixedMulR(thickness,scale),color);
            }
        }
        offset -= g_vector_font[string_char].width;
    }
}

void drawNumber(DrawSurface* surface,int x,int y,int number,int scale){
	char buffer[0x10];
    String string = numberToString(buffer,number);
    drawStringEx(surface,x,y,string,scale,pixelColorToColor(0xFFFFFF),0x2000);
}

void setScanline(Scanline scanline,Vec2 pos_1,Vec2 pos_2,int size){
    if(pos_1.x > pos_2.x){
        Vec2 tmp_pos = pos_1;
        pos_1 = pos_2;
        pos_2 = tmp_pos;
    }
    
    int p_begin = pos_1.x;
    int p_end = pos_2.x;
    int delta = (pos_2.y - pos_1.y << FIXED_PRECISION) / (pos_2.x - pos_1.x ? pos_2.x - pos_1.x : 1);
    int delta_pos = (pos_1.y << FIXED_PRECISION) + FIXED_ONE / 2;

    if(p_end < 0)
		return;
    if(p_begin >= size)
        return;
	if(p_begin < 0){
        delta_pos += delta * (-p_begin);
		p_begin = 0;
    }
	if(p_end >= size){
		p_end = size;
	}

    Ray2 ray = initRay2(vec2Shl(pos_1,FIXED_PRECISION),vec2Direction(vec2Shl(pos_1,FIXED_PRECISION),vec2Shl(pos_2,FIXED_PRECISION)));

    while(p_begin < p_end){
        scanline.begin[p_begin] = tMin(scanline.begin[p_begin],delta_pos >> FIXED_PRECISION);
        scanline.end[p_begin] = tMax(scanline.end[p_begin],delta_pos >> FIXED_PRECISION);
        delta_pos += delta;
        p_begin += 1;
    }
}

void setScanlineDDA(Scanline scanline,Vec2 pos_1,Vec2 pos_2,int size){
    if(pos_1.x > pos_2.x){
        Vec2 tmp_pos = pos_1;
        pos_1 = pos_2;
        pos_2 = tmp_pos;
    }
    
    int p_begin = pos_1.x;
    int p_end = pos_2.x;
    int delta = (pos_2.y - pos_1.y << FIXED_PRECISION) / (pos_2.x - pos_1.x ? pos_2.x - pos_1.x : 1);
    int delta_pos = (pos_1.y << FIXED_PRECISION);

    if(p_end < 0)
		return;
    if(p_begin >= size)
        return;
	if(p_begin < 0){
        delta_pos += delta * (-p_begin);
		p_begin = 0;
    }
	if(p_end >= size){
		p_end = size;
	}

    Ray2 ray = initRay2(vec2Shl(pos_1,FIXED_PRECISION),vec2Direction(vec2Shl(pos_1,FIXED_PRECISION),vec2Shl(pos_2,FIXED_PRECISION)));

    for(int i = tAbs(pos_1.x - pos_2.x) + tAbs(pos_1.y - pos_2.y);i--;){
        int x = tClamp(ray.square_pos.x,0,size);
        int y = tClamp(ray.square_pos.y,0,size);
        scanline.begin[x] = tMin(scanline.begin[x],y);
        scanline.end[x] = tMax(scanline.end[x],y);
        iterateRay2(&ray);
    }
}

void occlusionBufferFill(DrawSurface* surface,Vec3* coordinats){
    int offset = 0x2000;
    Vec3 coordinats_copy[4];
    for(int i = 4;i--;)
        coordinats_copy[i] = pointToScreenRenderer(coordinats[i],surface->rotation_matrix,surface->position,g_options.fov);
    if(coordinats_copy[0].z <= offset || coordinats_copy[1].z <= offset || coordinats_copy[2].z <= offset || coordinats_copy[3].z <= offset)
        return;
    Vec2 coords_2d[] = {
        {fixedDivR(coordinats_copy[0].x,coordinats_copy[0].z),fixedDivR(-coordinats_copy[0].y,coordinats_copy[0].z)},
        {fixedDivR(coordinats_copy[1].x,coordinats_copy[1].z),fixedDivR(-coordinats_copy[1].y,coordinats_copy[1].z)},
        {fixedDivR(coordinats_copy[3].x,coordinats_copy[3].z),fixedDivR(-coordinats_copy[3].y,coordinats_copy[3].z)},
        {fixedDivR(coordinats_copy[2].x,coordinats_copy[2].z),fixedDivR(-coordinats_copy[2].y,coordinats_copy[2].z)},
    };

    for(int i = 4;i--;){
        coords_2d[i].x = tClamp(coords_2d[i].x,-FIXED_ONE * 0x10,FIXED_ONE * 0x10);
        coords_2d[i].y = tClamp(coords_2d[i].y,-FIXED_ONE * 0x10,FIXED_ONE * 0x10);
    }
    
    for(int i = 0;i < 4;i++){
        coords_2d[i].x = transformDraw(OCCLUSION_BUFFER_SIZE,coords_2d[i].x);
        coords_2d[i].y = transformDraw(OCCLUSION_BUFFER_SIZE,coords_2d[i].y);
    }

    int x_min = INT_MAX;
    int x_max = INT_MIN;

    for(int i = 0;i < 4;i++){
        x_min = tMin(coords_2d[i].x,x_min);
        x_max = tMax(coords_2d[i].x,x_max);
    }

    x_min = tMax(0,x_min);
    x_max = tMin(OCCLUSION_BUFFER_SIZE,x_max);
    
    if(x_min >= OCCLUSION_BUFFER_SIZE || x_max < 0)
        return;
    
    for(int i = x_min;i < x_max;i++){
		surface->scanline_occlusion.begin[i] = INT16_MAX;
		surface->scanline_occlusion.end[i] = INT16_MIN;
	}

    for(int i = 0;i < 4;i += 1){
        Vec2 p1 = coords_2d[i];
        Vec2 p2 = coords_2d[(i + 1) % 4];

        setScanline(surface->scanline_occlusion,p1,p2,OCCLUSION_BUFFER_SIZE);
    }
    
    int16 begin[OCCLUSION_BUFFER_SIZE];
    int16 end[OCCLUSION_BUFFER_SIZE];
    Scanline s = {.begin = begin,.end = end};
    
    for(int x = x_min + 1;x < x_max - 1;x++){
        int n_trim = 0;
        for(int y = surface->scanline_occlusion.begin[x];y < surface->scanline_occlusion.end[x];y++){
            if(surface->scanline_occlusion.begin[x - 1] > y || surface->scanline_occlusion.begin[x + 1] > y)
                n_trim += 1;
            else
                break;
        }
        n_trim = tMax(1,n_trim);
        s.begin[x] = surface->scanline_occlusion.begin[x] + n_trim;

        n_trim = 0;
        for(int y = surface->scanline_occlusion.end[x];y >= surface->scanline_occlusion.begin[x];y--){
            if(surface->scanline_occlusion.end[x - 1] < y || surface->scanline_occlusion.end[x + 1] < y)
                n_trim += 1;
            else
                break;
        }
        n_trim = tMax(1,n_trim);
        s.end[x] = surface->scanline_occlusion.end[x] - n_trim;
    }
    for(int x = x_min + 1;x < x_max - 1;x++){
        surface->scanline_occlusion.begin[x] = s.begin[x];
        surface->scanline_occlusion.end[x] = s.end[x];
    }
    
    for(int x = x_min + 1;x < x_max - 1;x++){
        surface->scanline_occlusion.begin[x] = tClamp(surface->scanline_occlusion.begin[x],0,OCCLUSION_BUFFER_SIZE - 1);
        surface->scanline_occlusion.end[x] = tClamp(surface->scanline_occlusion.end[x],0,OCCLUSION_BUFFER_SIZE - 1);
        int n_bit = sizeof(*surface->occlusion_buffer) * CHAR_BIT;
        for(int y = surface->scanline_occlusion.begin[x];y < surface->scanline_occlusion.end[x];y++)
            surface->occlusion_buffer[(x * OCCLUSION_BUFFER_SIZE + y) / n_bit] |= (size_t)1 << y % n_bit;
    }
}

bool occlusionBufferHidden(DrawSurface* surface,Vec3* coordinats){
    int offset = 0x2000;
    Vec3 coordinats_copy[4];
    for(int i = 4;i--;)
        coordinats_copy[i] = pointToScreenRenderer(coordinats[i],surface->rotation_matrix,surface->position,g_options.fov);
    if(coordinats_copy[0].z <= offset || coordinats_copy[1].z <= offset || coordinats_copy[2].z <= offset || coordinats_copy[3].z <= offset)
        return false;
    Vec2 coords_2d[] = {
        {fixedDivR(coordinats_copy[0].x,coordinats_copy[0].z),fixedDivR(-coordinats_copy[0].y,coordinats_copy[0].z)},
        {fixedDivR(coordinats_copy[1].x,coordinats_copy[1].z),fixedDivR(-coordinats_copy[1].y,coordinats_copy[1].z)},
        {fixedDivR(coordinats_copy[3].x,coordinats_copy[3].z),fixedDivR(-coordinats_copy[3].y,coordinats_copy[3].z)},
        {fixedDivR(coordinats_copy[2].x,coordinats_copy[2].z),fixedDivR(-coordinats_copy[2].y,coordinats_copy[2].z)},
    };
    
    for(int i = 0;i < 4;i++){
        coords_2d[i].x = transformDraw(OCCLUSION_BUFFER_SIZE,coords_2d[i].x);
        coords_2d[i].y = transformDraw(OCCLUSION_BUFFER_SIZE,coords_2d[i].y);
    }

    int x_min = INT_MAX;
    int x_max = INT_MIN;

    for(int i = 0;i < 4;i++){
        x_min = tMin(coords_2d[i].x,x_min);
        x_max = tMax(coords_2d[i].x,x_max);
    }

    x_min = tMax(0,x_min);
    x_max = tMin(OCCLUSION_BUFFER_SIZE,x_max);
    
    if(x_min >= OCCLUSION_BUFFER_SIZE || x_max < 0)
        return false;
    
    for(int i = x_min;i < x_max;i++){
		surface->scanline_occlusion.begin[i] = INT16_MAX;
		surface->scanline_occlusion.end[i] = INT16_MIN;
	}

    for(int i = 0;i < 4;i += 1){
        Vec2 p1 = coords_2d[i];
        Vec2 p2 = coords_2d[(i + 1) % 4];

        setScanlineDDA(surface->scanline_occlusion,p1,p2,OCCLUSION_BUFFER_SIZE);
    }

    bool checked = false;
    
    for(int x = x_min;x < x_max;x++){
        surface->scanline_occlusion.begin[x] = tClamp(surface->scanline_occlusion.begin[x],0,OCCLUSION_BUFFER_SIZE - 1);
        surface->scanline_occlusion.end[x] = tClamp(surface->scanline_occlusion.end[x],0,OCCLUSION_BUFFER_SIZE - 1);
        int n_bit = sizeof(*surface->occlusion_buffer) * CHAR_BIT; 
        for(int y = surface->scanline_occlusion.begin[x];y < surface->scanline_occlusion.end[x];y++){
            checked = true;
            if(!(surface->occlusion_buffer[(x * OCCLUSION_BUFFER_SIZE + y) / n_bit] >> y % n_bit & 1))
                return false;
            
        }
    }
    return checked;
}
