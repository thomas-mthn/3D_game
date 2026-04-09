#include "draw_soft.h"
#include "font.h"
#include "fixed.h"
#include "memory.h"
#include "vec2.h"
#include "vec3.h"
#include "main.h"

#ifdef __linux__
#include "linux/l_main.h"
#elif !defined(__wasm__)
#include "win32/w_gdi.h"
#endif

static int numberStringLength(int number){
    int length = 0;
    if(!number)
        return 1;
    if(number < 0)
        length += 1;
    while(number){
        number /= 10;
        length += 1;
    }
    return length;
}

bool softSurfaceCreate(DrawSurface* surface){
    if(!surface->data)
        surface->data = tMalloc(surface->height * surface->width * sizeof(surface->data));
    surface->scanline = tMalloc(surface->height * sizeof(Vec2));
    surface->scanline_color = tMalloc(surface->height * sizeof(ScanlineColor));
    surface->scanline_texture = tMalloc(surface->height * sizeof(ScanlineTexture));
#if !defined(__wasm__) && !defined(__linux__)
    surface->soft_bitmapinfo = (BitmapInfo){
		.bmi_header = {
			.size = sizeof(BitmapInfo),
			.width = surface->width,
			.height = -surface->height,
			.planes = 1,
			.bit_count = 32,
		},
	};
#endif
}

void softSurfaceDestroyMeta(DrawSurface* surface){
    tFree(surface->scanline);
    tFree(surface->scanline_color);
    tFree(surface->scanline_texture);
}

void softSurfaceDestroy(DrawSurface* surface){
    tFree(surface->data);
    softSurfaceDestroyMeta(surface);
}

void softSurfaceClear(DrawSurface* surface){
    drawSquare(surface,-FIXED_ONE,-FIXED_ONE,FIXED_ONE * 2,vec3Single(0));
}

void softSurfaceSizeChange(DrawSurface* surface,int width,int height){
    surface->width = width;
    surface->height = height;
    tFree(surface->data);
    tFree(surface->scanline);
    tFree(surface->scanline_color);
    tFree(surface->scanline_texture);
    surface->scanline = tMalloc(surface->height * sizeof(Vec2));
    surface->scanline_color = tMalloc(surface->height * sizeof(ScanlineColor));
    surface->scanline_texture = tMalloc(surface->height * sizeof(ScanlineTexture));
    surface->data = tMalloc(width * height * sizeof(surface->data));
}

void softSurfaceBlit(DrawSurface* surface){
#ifdef __linux__
	linuxBlit(surface->data,surface->width,surface->height);
#elif !defined(__wasm__)
    StretchDIBits(surface->window_context,0,0,surface->window_width,surface->window_height,0,0,surface->width,surface->height,surface->data,&surface->soft_bitmapinfo,0,0x00CC0020);
#endif
}

static void setScanline(Vec2* scanline,Vec2 pos_1,Vec2 pos_2,int size){
	int p_begin,p_end;
	int delta,delta_pos;
	if(pos_1.x > pos_2.x){
		p_begin = pos_1.x;
		p_end = pos_2.x;
		delta = (pos_2.y - pos_1.y << FIXED_PRECISION) / (pos_2.x - pos_1.x ? pos_2.x - pos_1.x : 1);
		delta_pos = (pos_1.y << FIXED_PRECISION);
	}
	else{
		p_begin = pos_2.x;
		p_end = pos_1.x;
		delta = (pos_1.y - pos_2.y << FIXED_PRECISION) / (pos_1.x - pos_2.x ? pos_1.x - pos_2.x : 1);
		delta_pos = (pos_2.y << FIXED_PRECISION);
	}
	if(p_begin < 0)
		return;
    if(p_end >= size)
        return;
	if(p_end < 0)
		p_end = 0;
	if(p_begin >= size){
		delta_pos -= delta * (p_begin - size);
		p_begin = size;
	}
	while(p_begin-- > p_end){
		scanline[p_begin].x = tMin(scanline[p_begin].x,delta_pos >> FIXED_PRECISION);
		scanline[p_begin].y = tMax(scanline[p_begin].y,delta_pos >> FIXED_PRECISION);
        int v = delta_pos >> FIXED_PRECISION;
		delta_pos -= delta;
	}
}

static void setScanlineColor(ScanlineColor* color,Vec2* scanline,Vec3 color_1,Vec3 color_2,Vec2 pos_1,Vec2 pos_2,int size){
	int p_begin,p_end;
	int delta,delta_pos;
    Vec3 delta_color;
    Vec3 color_a;
	if(pos_1.x > pos_2.x){
        delta_color = vec3SubR(color_2,color_1);
        color_a = color_1;
		p_begin = pos_1.x;
		p_end = pos_2.x;
		delta = (pos_2.y - pos_1.y << FIXED_PRECISION) / (pos_2.x - pos_1.x ? pos_2.x - pos_1.x : 1);
		delta_pos = (pos_1.y << FIXED_PRECISION);
	}
	else{
        delta_color = vec3SubR(color_1,color_2);
        color_a = color_2;
		p_begin = pos_2.x;
		p_end = pos_1.x;
		delta = (pos_1.y - pos_2.y << FIXED_PRECISION) / (pos_1.x - pos_2.x ? pos_1.x - pos_2.x : 1);
		delta_pos = (pos_2.y << FIXED_PRECISION);
	}
	if(p_begin < 0)
		return;
    if(p_end >= size)
        return;
	if(p_end < 0)
		p_end = 0;
	if(p_begin >= size){
		delta_pos -= delta * (p_begin - size);
		p_begin = size;
	}
    delta_color.x /= tMax(p_begin - p_end,1);
    delta_color.y /= tMax(p_begin - p_end,1);
    delta_color.z /= tMax(p_begin - p_end,1);
	while(p_begin-- > p_end){
        if(delta_pos >> FIXED_PRECISION < scanline[p_begin].x)
            color[p_begin].begin = color_a;
        if(delta_pos >> FIXED_PRECISION > scanline[p_begin].y)
            color[p_begin].end = color_a;
        vec3Add(&color_a,delta_color);

		scanline[p_begin].x = tMin(scanline[p_begin].x,delta_pos >> FIXED_PRECISION);
		scanline[p_begin].y = tMax(scanline[p_begin].y,delta_pos >> FIXED_PRECISION);
		delta_pos -= delta;
	}
}

static void setScanlineTexture(ScanlineColor* color,ScanlineTexture* texture,Vec2* scanline,Vec2 texture_1,Vec2 texture_2,Vec2 pos_1,Vec2 pos_2,int size){
	int p_begin,p_end;
	int delta,delta_pos;
    Vec2 delta_texture;
    Vec2 texture_iterator;
	if(pos_1.x > pos_2.x){
        delta_texture = vec2SubR(texture_2,texture_1);
        texture_iterator = texture_1;
		p_begin = pos_1.x;
		p_end = pos_2.x;
		delta = (pos_2.y - pos_1.y << FIXED_PRECISION) / (pos_2.x - pos_1.x ? pos_2.x - pos_1.x : 1);
		delta_pos = (pos_1.y << FIXED_PRECISION);
	}
	else{
        delta_texture = vec2SubR(texture_1,texture_2);
        texture_iterator = texture_2;
		p_begin = pos_2.x;
		p_end = pos_1.x;
		delta = (pos_1.y - pos_2.y << FIXED_PRECISION) / (pos_1.x - pos_2.x ? pos_1.x - pos_2.x : 1);
		delta_pos = (pos_2.y << FIXED_PRECISION);
	}
	if(p_begin < 0)
		return;
    if(p_end >= size)
        return;
	if(p_end < 0)
		p_end = 0;
	if(p_begin >= size){
		delta_pos -= delta * (p_begin - size);
		p_begin = size;
	}

    delta_texture.x /= tMax(p_begin - p_end,1);
    delta_texture.y /= tMax(p_begin - p_end,1);

	while(p_begin-- > p_end){
        if(delta_pos >> FIXED_PRECISION < scanline[p_begin].x)
            texture[p_begin].begin = texture_iterator;
        if(delta_pos >> FIXED_PRECISION > scanline[p_begin].y)
            texture[p_begin].end = texture_iterator;

        vec2Add(&texture_iterator,delta_texture);

		scanline[p_begin].x = tMin(scanline[p_begin].x,delta_pos >> FIXED_PRECISION);
		scanline[p_begin].y = tMax(scanline[p_begin].y,delta_pos >> FIXED_PRECISION);
		delta_pos -= delta;
	}
}

static void setScanlineColorTexture(ScanlineColor* color,ScanlineTexture* texture,Vec2* scanline,Vec2 texture_1,Vec2 texture_2,Vec3 color_1,Vec3 color_2,Vec2 pos_1,Vec2 pos_2,int size){
	int p_begin,p_end;
	int delta,delta_pos;
    Vec3 delta_color;
    Vec2 delta_texture;
    Vec3 color_iterator;
    Vec2 texture_iterator;
	if(pos_1.x > pos_2.x){
        delta_color = vec3SubR(color_2,color_1);
        delta_texture = vec2SubR(texture_2,texture_1);
        color_iterator = color_1;
        texture_iterator = texture_1;
		p_begin = pos_1.x;
		p_end = pos_2.x;
		delta = (pos_2.y - pos_1.y << FIXED_PRECISION) / (pos_2.x - pos_1.x ? pos_2.x - pos_1.x : 1);
		delta_pos = (pos_1.y << FIXED_PRECISION);
	}
	else{
        delta_color = vec3SubR(color_1,color_2);
        delta_texture = vec2SubR(texture_1,texture_2);
        color_iterator = color_2;
        texture_iterator = texture_2;
		p_begin = pos_2.x;
		p_end = pos_1.x;
		delta = (pos_1.y - pos_2.y << FIXED_PRECISION) / (pos_1.x - pos_2.x ? pos_1.x - pos_2.x : 1);
		delta_pos = (pos_2.y << FIXED_PRECISION);
	}
	if(p_begin < 0)
		return;
    if(p_end >= size)
        return;
	if(p_end < 0)
		p_end = 0;
	if(p_begin >= size){
		delta_pos -= delta * (p_begin - size);
		p_begin = size;
	}

    delta_color.x /= tMax(p_begin - p_end,1);
    delta_color.y /= tMax(p_begin - p_end,1);
    delta_color.z /= tMax(p_begin - p_end,1);

    delta_texture.x /= tMax(p_begin - p_end,1);
    delta_texture.y /= tMax(p_begin - p_end,1);

	while(p_begin-- > p_end){
        if(delta_pos >> FIXED_PRECISION < scanline[p_begin].x){
            color[p_begin].begin = color_iterator;
            texture[p_begin].begin = texture_iterator;
        }
        if(delta_pos >> FIXED_PRECISION > scanline[p_begin].y){
            color[p_begin].end = color_iterator;
            texture[p_begin].end = texture_iterator;
        }
        vec3Add(&color_iterator,delta_color);
        vec2Add(&texture_iterator,delta_texture);

		scanline[p_begin].x = tMin(scanline[p_begin].x,delta_pos >> FIXED_PRECISION);
		scanline[p_begin].y = tMax(scanline[p_begin].y,delta_pos >> FIXED_PRECISION);
		delta_pos -= delta;
	}
}
/*
void drawSegmentSoft(DrawSurface surface,int x1,int y1,int x2,int y2,int thickness,int color){
    static Vec2 scanline[0x1000];

    int rel_x = x1 - x2;
    int rel_y = y1 - y2;
    int length = tSqrt(rel_x * rel_x + rel_y * rel_y);
    if(!length)
        return;
    rel_x = (rel_x << 8) / length;
    rel_y = (rel_y << 8) / length;

    int perpendicular_x = -rel_y;
    int perpendicular_y = rel_x;

    Vec2 segment_offset = vec2MulRS((Vec2){-rel_y,rel_x},thickness * FIXED_ONE / 2,FIXED_PRECISION);
    segment_offset.x >>= FIXED_PRECISION;
    segment_offset.y >>= FIXED_PRECISION;

    Vec2 v[] = {
        {x1 - segment_offset.x,y1 - segment_offset.y},
        {x1 + segment_offset.x,y1 + segment_offset.y},
        {x2 + segment_offset.x,y2 + segment_offset.y},
        {x2 - segment_offset.x,y2 - segment_offset.y},
    };

    int x_min = tMax(tMin(tMin(v[0].x,v[1].x),tMin(v[2].x,v[3].x)),0);
    int x_max = tMin(tMax(tMax(v[0].x,v[1].x),tMax(v[2].x,v[3].x)),surface.height - 1);

    if(x_min >= surface.height || x_max < 0)
        return;

    for(int i = x_min;i < x_max;i++){
		scanline[i].x = INT_MAX;
		scanline[i].y = INT_MIN;
	}

    setScanline(scanline,v[0],v[1],surface.width);
    setScanline(scanline,v[1],v[2],surface.width);
    setScanline(scanline,v[2],v[3],surface.width);
    setScanline(scanline,v[3],v[0],surface.width);

    for(int x = x_min;x < x_max;x++){
        scanline[x].x = tClamp(scanline[x].x,0,surface.height - 1);
        scanline[x].y = tClamp(scanline[x].y,0,surface.height - 1);
        for(int y = scanline[x].x;y < scanline[x].y;y++)
            surface.data[x * surface.width + y] = color;
    }
}
*/

void drawPolygonSoft(DrawSurface* surface,Vec2* coordinats,int n_point,Vec3 color){
    int pixel_color = colorToPixelColor(color);
    for(int i = 0;i < n_point;i++){
        coordinats[i].x = transformDraw(surface->height,coordinats[i].x);
        coordinats[i].y = transformDraw(surface->width ,coordinats[i].y);
    }

    int x_min = INT_MAX;
    int x_max = INT_MIN;

    for(int i = 0;i < n_point;i++){
        x_min = tMin(coordinats[i].x,x_min);
        x_max = tMax(coordinats[i].x,x_max);
    }

    x_min = tMax(0,x_min);
    x_max = tMin(surface->height,x_max);

    if(x_min >= surface->height || x_max < 0)
        return;

    for(int i = x_min;i < x_max;i++){
		surface->scanline[i].x = INT_MAX;
		surface->scanline[i].y = INT_MIN;
	}

    for(int i = 0;i < n_point;i += 1){
        Vec2 p1 = coordinats[i];
        Vec2 p2 = coordinats[(i + 1) % n_point];
        setScanline(surface->scanline,p1,p2,surface->height);
    }

    for(int x = x_min;x < x_max;x++){
        surface->scanline[x].x = tClamp(surface->scanline[x].x,0,surface->width);
        surface->scanline[x].y = tClamp(surface->scanline[x].y,0,surface->width);
        for(int y = surface->scanline[x].x;y < surface->scanline[x].y;y++)
            surface->data[x * surface->width + y] = pixel_color;
    }
}

void drawPolygon3dSoft(DrawSurface* surface,Vec3* coordinats,Vec3 color){
    if(coordinats[0].z <= 0 || coordinats[1].z <= 0 || coordinats[2].z <= 0 || coordinats[3].z <= 0)
        return;
    Vec2 coords_2d[] = {
        fixedDivR(coordinats[0].x,coordinats[0].z),fixedDivR(-coordinats[0].y,coordinats[0].z),
        fixedDivR(coordinats[1].x,coordinats[1].z),fixedDivR(-coordinats[1].y,coordinats[1].z),
        fixedDivR(coordinats[2].x,coordinats[2].z),fixedDivR(-coordinats[2].y,coordinats[2].z),
        fixedDivR(coordinats[3].x,coordinats[3].z),fixedDivR(-coordinats[3].y,coordinats[3].z),
    };
    drawPolygonSoft(surface,coords_2d,4,color);
}

#define MAX_N_POINT 0x10

void drawColoredPolygonSoft(DrawSurface* surface,Vec2* coordinats,Vec3* color,int n_point){
    if(n_point >= MAX_N_POINT)
        return;
    for(int i = 0;i < n_point;i++){
        coordinats[i].x = transformDraw(surface->height,coordinats[i].x);
        coordinats[i].y = transformDraw(surface->width ,coordinats[i].y);
    }

    int x_min = INT_MAX;
    int x_max = INT_MIN;

    for(int i = 0;i < n_point;i++){
        x_min = tMin(coordinats[i].x,x_min);
        x_max = tMax(coordinats[i].x,x_max);
    }

    x_min = tMax(0,x_min);
    x_max = tMin(surface->height,x_max);

    if(x_min >= surface->height || x_max < 0)
        return;

    for(int i = x_min;i < x_max;i++){
		surface->scanline[i].x = INT_MAX;
		surface->scanline[i].y = INT_MIN;
	}

    for(int i = 0;i < n_point;i++){
        Vec2 p1 = coordinats[i];
        Vec2 p2 = coordinats[(i + 1) % n_point];
        Vec3 color_1 = color[i];
        Vec3 color_2 = color[(i + 1) % n_point];
        setScanlineColor(surface->scanline_color,surface->scanline,color_1,color_2,p1,p2,surface->height);
    }

    for(int x = x_min;x < x_max;x++){
        surface->scanline[x].x = tClamp(surface->scanline[x].x,0,surface->width);
        surface->scanline[x].y = tClamp(surface->scanline[x].y,0,surface->width);

        Vec3 color_1 = surface->scanline_color[x].begin;
        Vec3 color_2 = surface->scanline_color[x].end;

        Vec3 delta = vec3SubR(color_2,color_1);
        delta.x /= tMax(surface->scanline[x].y - surface->scanline[x].x,1);
        delta.y /= tMax(surface->scanline[x].y - surface->scanline[x].x,1);
        delta.z /= tMax(surface->scanline[x].y - surface->scanline[x].x,1);
        
        for(int y = surface->scanline[x].x;y < surface->scanline[x].y;y++){
            surface->data[x * surface->width + y] = colorToPixelColor(color_1);
            vec3Add(&color_1,delta);
        }
    }
}

void drawColoredPolygon3dSoft(DrawSurface* surface,Vec3* coordinats,Vec3* color){
    if(coordinats[0].z <= 0 || coordinats[1].z <= 0 || coordinats[2].z <= 0 || coordinats[3].z <= 0)
        return;
    Vec2 coords_2d[] = {
        fixedDivR(coordinats[0].x,coordinats[0].z),fixedDivR(-coordinats[0].y,coordinats[0].z),
        fixedDivR(coordinats[1].x,coordinats[1].z),fixedDivR(-coordinats[1].y,coordinats[1].z),
        fixedDivR(coordinats[2].x,coordinats[2].z),fixedDivR(-coordinats[2].y,coordinats[2].z),
        fixedDivR(coordinats[3].x,coordinats[3].z),fixedDivR(-coordinats[3].y,coordinats[3].z),
    };
    drawColoredPolygonSoft(surface,coords_2d,color,4);
}

void drawTexturePolygonSoft(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3 color,int n_point){
    if(n_point >= MAX_N_POINT)
        return;
    int pixel_color = colorToPixelColor(color);
    for(int i = 0;i < n_point;i++){
        coordinats[i].x = transformDraw(surface->height,coordinats[i].x);
        coordinats[i].y = transformDraw(surface->width ,-coordinats[i].y);
    }

    int x_min = INT_MAX;
    int x_max = INT_MIN;

    for(int i = 0;i < n_point;i++){
        x_min = tMin(coordinats[i].x,x_min);
        x_max = tMax(coordinats[i].x,x_max);
    }

    x_min = tMax(0,x_min);
    x_max = tMin(surface->height,x_max);

    if(x_min >= surface->height || x_max < 0)    
        return;

    for(int i = x_min;i < x_max;i++){
		surface->scanline[i].x = INT_MAX;
		surface->scanline[i].y = INT_MIN;
	}

    for(int i = 0;i < n_point;i++){
        Vec2 p1 = coordinats[i];
        Vec2 p2 = coordinats[(i + 1) % n_point];
        Vec2 texture_1 = texture_coordinats[i];
        Vec2 texture_2 = texture_coordinats[(i + 1) % n_point];
        setScanlineTexture(surface->scanline_color,surface->scanline_texture,surface->scanline,texture_1,texture_2,p1,p2,surface->height);
    }

    Vec2 texture_begin = surface->scanline_texture[x_min].begin;
    Vec2 texture_end   = surface->scanline_texture[x_min].end;

    Vec2 texture_delta = vec2SubR(texture_end,texture_begin);
    texture_delta.x /= tMax(surface->scanline[x_min].y - surface->scanline[x_min].x,1);
    texture_delta.y /= tMax(surface->scanline[x_min].y - surface->scanline[x_min].x,1);

    int mipmap = tClamp(bitScanReverse((tAbs(texture_delta.x) + tAbs(texture_delta.y)) * texture->size) - 16,0,31);
    int mipmap_size = texture->size >> mipmap;
    int mipmap_offset = 0;
    for(int i = 0;i < mipmap;i++){
        mipmap_offset += (texture->size >> i) * (texture->size >> i);
    }

    for(int x = x_min;x < x_max;x++){
        surface->scanline[x].x = tClamp(surface->scanline[x].x,0,surface->width);
        surface->scanline[x].y = tClamp(surface->scanline[x].y,0,surface->width);

        Vec2 texture_begin = surface->scanline_texture[x].begin;
        Vec2 texture_end = surface->scanline_texture[x].end;

        Vec2 texture_delta = vec2SubR(texture_end,texture_begin);
        texture_delta.x /= tMax(surface->scanline[x].y - surface->scanline[x].x,1);
        texture_delta.y /= tMax(surface->scanline[x].y - surface->scanline[x].x,1);
        
        for(int y = surface->scanline[x].x;y < surface->scanline[x].y;y++){
            unsigned texel = texture->pixel_data[mipmap_offset + (texture_begin.x * mipmap_size >> FIXED_PRECISION & mipmap_size - 1) * mipmap_size + (texture_begin.y * mipmap_size >> FIXED_PRECISION & mipmap_size - 1)];
            
            if(texel >> 24 <= 0x80) 
                surface->data[x * surface->width + y] = colorToPixelColor(vec3MulR(vec3ShrR(pixelColorToColor(texel),4),color)); 
            vec2Add(&texture_begin,texture_delta);
        }
    }
}

void drawTexturePolygon3dSoft(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3 color,int n_point){
    if(coordinats[0].z <= 0 || coordinats[1].z <= 0 || coordinats[2].z <= 0 || coordinats[3].z <= 0)
        return;
    Vec2 coords_2d[] = {
        fixedDivR(coordinats[0].x,coordinats[0].z),fixedDivR(coordinats[0].y,coordinats[0].z),
        fixedDivR(coordinats[1].x,coordinats[1].z),fixedDivR(coordinats[1].y,coordinats[1].z),
        fixedDivR(coordinats[2].x,coordinats[2].z),fixedDivR(coordinats[2].y,coordinats[2].z),
        fixedDivR(coordinats[3].x,coordinats[3].z),fixedDivR(coordinats[3].y,coordinats[3].z),
    };
    drawTexturePolygonSoft(surface,texture,texture_coordinats,coords_2d,color,n_point);
}

void drawColoredTexturePolygonSoft(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3* color,int n_point){
    if(n_point >= MAX_N_POINT)
        return;

    for(int i = 0;i < n_point;i++){
        int temp = texture_coordinats[i].x;
        texture_coordinats[i].x = texture_coordinats[i].y;
        texture_coordinats[i].y = temp;
    }

    for(int i = 0;i < n_point;i++){
        coordinats[i].x = transformDraw(surface->height,coordinats[i].x);
        coordinats[i].y = transformDraw(surface->width ,coordinats[i].y);
    }

    int x_min = INT_MAX;
    int x_max = INT_MIN;

    for(int i = 0;i < n_point;i++){
        x_min = tMin(coordinats[i].x,x_min);
        x_max = tMax(coordinats[i].x,x_max);
    }

    x_min = tMax(0,x_min);
    x_max = tMin(surface->height,x_max);

    if(x_min >= surface->height || x_max < 0)    
        return;

    for(int i = x_min;i < x_max;i++){
		surface->scanline[i].x = INT_MAX;
		surface->scanline[i].y = INT_MIN;
	}

    for(int i = 0;i < n_point;i++){
        Vec2 p1 = coordinats[i];
        Vec2 p2 = coordinats[(i + 1) % n_point];
        Vec3 color_1 = color[i];
        Vec3 color_2 = color[(i + 1) % n_point];
        Vec2 texture_1 = texture_coordinats[i];
        Vec2 texture_2 = texture_coordinats[(i + 1) % n_point];
        setScanlineColorTexture(surface->scanline_color,surface->scanline_texture,surface->scanline,texture_1,texture_2,color_1,color_2,p1,p2,surface->height);
    }

    Vec2 texture_begin = surface->scanline_texture[x_min].begin;
    Vec2 texture_end   = surface->scanline_texture[x_min].end;

    Vec2 texture_delta = vec2SubR(texture_end,texture_begin);
    texture_delta.x /= tMax(surface->scanline[x_min].y - surface->scanline[x_min].x,1);
    texture_delta.y /= tMax(surface->scanline[x_min].y - surface->scanline[x_min].x,1);

    int mipmap = tClamp(bitScanReverse((tAbs(texture_delta.x) + tAbs(texture_delta.y)) * texture->size) - 15,0,31);
    int mipmap_size = texture->size >> mipmap;
    int mipmap_offset = 0;
    for(int i = 0;i < mipmap;i++){
        mipmap_offset += (texture->size >> i) * (texture->size >> i);
    }

    for(int x = x_min;x < x_max;x++){
        surface->scanline[x].x = tClamp(surface->scanline[x].x,0,surface->width);
        surface->scanline[x].y = tClamp(surface->scanline[x].y,0,surface->width);

        Vec3 color_begin = surface->scanline_color[x].begin;
        Vec3 color_end = surface->scanline_color[x].end;

        Vec2 texture_begin = surface->scanline_texture[x].begin;
        Vec2 texture_end = surface->scanline_texture[x].end;

        Vec3 color_delta = vec3SubR(color_end,color_begin);
        color_delta.x /= tMax(surface->scanline[x].y - surface->scanline[x].x,1);
        color_delta.y /= tMax(surface->scanline[x].y - surface->scanline[x].x,1);
        color_delta.z /= tMax(surface->scanline[x].y - surface->scanline[x].x,1);

        Vec2 texture_delta = vec2SubR(texture_end,texture_begin);
        texture_delta.x /= tMax(surface->scanline[x].y - surface->scanline[x].x,1);
        texture_delta.y /= tMax(surface->scanline[x].y - surface->scanline[x].x,1);

        int y = surface->scanline[x].x;

        for(;y < surface->scanline[x].y - 3;y += 4){
            for(int j = 0;j < 4;j++){
                unsigned texel = texture->pixel_data[mipmap_offset + (texture_begin.x * mipmap_size >> FIXED_PRECISION & mipmap_size - 1) * mipmap_size + (texture_begin.y * mipmap_size >> FIXED_PRECISION & mipmap_size - 1)];
                Vec3 texture_color = vec3ShrR(pixelColorToColor(texel),4);
                surface->data[x * surface->width + (y + j)] = colorToPixelColor(vec3MulR(color_begin,texture_color)); 
                vec3Add(&color_begin,color_delta);
                vec2Add(&texture_begin,texture_delta);
            }
        }
        for(;y < surface->scanline[x].y;y++){
            unsigned texel = texture->pixel_data[mipmap_offset + (texture_begin.x * mipmap_size >> FIXED_PRECISION & mipmap_size - 1) * mipmap_size + (texture_begin.y * mipmap_size >> FIXED_PRECISION & mipmap_size - 1)];
            Vec3 texture_color = vec3ShrR(pixelColorToColor(texel),4);
            surface->data[x * surface->width + y] = colorToPixelColor(vec3MulR(color_begin,texture_color)); 
            vec3Add(&color_begin,color_delta);
            vec2Add(&texture_begin,texture_delta);
        }
    }
}

void drawColoredTexturePolygon3dSoft(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color){
    if(coordinats[0].z <= 0 || coordinats[1].z <= 0 || coordinats[2].z <= 0 || coordinats[3].z <= 0)
        return;
    Vec2 coords_2d[] = {
        fixedDivR(coordinats[0].x,coordinats[0].z),fixedDivR(-coordinats[0].y,coordinats[0].z),
        fixedDivR(coordinats[1].x,coordinats[1].z),fixedDivR(-coordinats[1].y,coordinats[1].z),
        fixedDivR(coordinats[2].x,coordinats[2].z),fixedDivR(-coordinats[2].y,coordinats[2].z),
        fixedDivR(coordinats[3].x,coordinats[3].z),fixedDivR(-coordinats[3].y,coordinats[3].z),
    };
    drawColoredTexturePolygonSoft(surface,texture,texture_coordinats,coords_2d,color,4);
}

void drawSegmentSoft(DrawSurface* surface,int x1,int y1,int x2,int y2,int thickness,Vec3 color){
    Vec2 p1 = {x1,y1};
    Vec2 p2 = {x2,y2};

    Vec2 direction = vec2MulRS(vec2Direction((Vec2){x1,y1},(Vec2){x2,y2}),thickness);
    Vec2 quad[] = {
        vec2AddR((Vec2){x1,y1},vec2Rotate(direction,FIXED_ONE / 8 * 3)),
        vec2AddR((Vec2){x2,y2},vec2Rotate(direction,FIXED_ONE / 8 * 1)),
        vec2AddR((Vec2){x2,y2},vec2Rotate(direction,FIXED_ONE / 8 * 7)),
        vec2AddR((Vec2){x1,y1},vec2Rotate(direction,FIXED_ONE / 8 * 5)),
    };

    drawPolygonSoft(surface,quad,4,color);
}

void drawLineSoft(DrawSurface* surface,int x1,int y1,int x2,int y2,Vec3 color){
    int pixel_color = colorToPixelColor(color);

    x1 = transformDraw(surface->height,x1);
    y1 = transformDraw(surface->width,-y1);
    x2 = transformDraw(surface->height,x2);
    y2 = transformDraw(surface->width,-y2);

    int dx = tAbs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = tAbs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = (dx > dy ? dx : -dy)/2, e2;

    if(
        x1 >= 0 && x1 < surface->height && x2 >= 0 && x2 < surface->height &&
        y1 >= 0 && y1 < surface->width && y2 >= 0 && y2 < surface->width
    ){
        for(;;){
            surface->data[x1 * surface->width + y1] = pixel_color;
            if(x1 == x2 && y1 == y2)
                return;
            e2 = err;
            if(e2 >- dx){
                err -= dy;
                x1 += sx;
            }
            if(e2 < dy){
                err += dx;
                y1 += sy;
            }
        }
    }

    for(;;){
        if(x1 >= 0 && x1 < surface->height && y1 >= 0 && y1 < surface->width)
            surface->data[x1 * surface->width + y1] = pixel_color;
        if(x1 == x2 && y1 == y2)
            break;
        e2 = err;
        if(e2 >- dx){
            err -= dy;
            x1 += sx;
        }
        if(e2 < dy){
            err += dx;
            y1 += sy;
        }
    }
}

void drawEllipsesSoft(DrawSurface* surface,int x,int y,int size_x,int size_y,Vec3 color){
    int pixel_color = colorToPixelColor(color);

    x = transformDraw(surface->height,x);
    y = transformDraw(surface->width,y);
    int min_x = tMax(x - scaleDraw(surface->height,size_x),0);
    int min_y = tMax(y - scaleDraw(surface->width,size_y),0);
    int max_x = tMin(x + scaleDraw(surface->height,size_x) + 1,surface->height - 1);
    int max_y = tMin(y + scaleDraw(surface->width,size_y) + 1,surface->width  - 1);
    for(int i = min_x;i < max_x;i++){
        for(int j = min_y;j < max_y;j++){
            int rel_x = (i - x << FIXED_PRECISION) / surface->height;
            int rel_y = (j - y << FIXED_PRECISION) / surface->width;
            int distance = rel_x * rel_x + rel_y * rel_y;

            if(distance > FIXED_ONE){
                continue;
            }
            else{
                surface->data[i * surface->width + j] = pixel_color;
                continue;
            }
        }
    }
}

void drawRingSoft(DrawSurface* surface,int x,int y,int radius,int thickness,Vec3 color){
    int pixel_color = colorToPixelColor(color);

    int outer_radius = radius + thickness;
    int min_x = tMax(x - outer_radius,0);
    int min_y = tMax(y - outer_radius,0);
    int max_x = tMin(x + outer_radius,surface->height - 1);
    int max_y = tMin(y + outer_radius,surface->width  - 1);
    for(int i = min_x;i < max_x;i++){
        for(int j = min_y;j < max_y;j++){
            int rel_x = i - x;
            int rel_y = j - y;
            int distance = rel_x * rel_x + rel_y * rel_y;
            if(distance > outer_radius * outer_radius)
                continue;
            if(distance < radius * radius)
                continue;
            surface->data[i * surface->width + j] = pixel_color;
        }
    }
}

void drawRectangleSoft(DrawSurface* surface,int x,int y,int size_x,int size_y,Vec3 color){
    int pixel_color = colorToPixelColor(color);
    x = transformDraw(surface->height,x);
    y = transformDraw(surface->width ,y);
    size_x = scaleDraw(surface->height,size_x);
    size_y = scaleDraw(surface->width ,size_y);
    int min_x = tMax(x,0);
    int min_y = tMax(y,0);
    int max_x = tMin(x + size_x,surface->height - 1);
    int max_y = tMin(y + size_y,surface->width  - 1);
    for(int i = min_x;i < max_x;i++){
        for(int j = min_y;j < max_y;j++)
            surface->data[i * surface->width + j] = pixel_color;
    }
}

