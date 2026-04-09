#include "span.h"
#include "draw.h"
#include "geometry.h"
#include "main.h"
#include "texture.h"
#include "memory.h"

structure(Span){
	Span* next;
    Span* previous;
	int16 begin;
	int16 end;
    union{
        Vec3 color_begin;
        Vec3 color;
    };
    Vec3 color_end;
    enum{
        SPAN_SOLIDCOLOR = 1 << 0
    } flags;
    Texture* texture;
    int mipmap;
    Vec2 texture_begin;
    Vec2 texture_end;
};

structure(Clip){
    int16 begin;
    int16 end;
};

static Span* span_list[0x1000];

static MemoryArena span_arena;
static MemoryArena clip_arena;

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

static void spanDraw(Span* span,int height){
    int i = span->begin;

    if(span->flags & SPAN_SOLIDCOLOR){
        for(;i < span->end - 3;i += 4){
            for(int j = 0;j < 4;j++)
                g_surface.data[height * g_surface.width + i + j] = colorToPixelColor(span->color);
        }
	    for(;i < span->end;i++){
            g_surface.data[height * g_surface.width + i] = colorToPixelColor(span->color);
        }
        return;
    }

    Vec3 color_delta = vec3SubR(span->color_end,span->color_begin);
    color_delta.x /= tMax(span->end - span->begin,1);
    color_delta.y /= tMax(span->end - span->begin,1);
    color_delta.z /= tMax(span->end - span->begin,1);

    if(!span->texture){
        for(;i < span->end - 3;i += 4){
            for(int j = 0;j < 4;j++){
                g_surface.data[height * g_surface.width + i + j] = colorToPixelColor(span->color_begin);
                vec3Add(&span->color_begin,color_delta);
            }
        }
	    for(;i < span->end;i++){
            g_surface.data[height * g_surface.width + i] = colorToPixelColor(span->color_begin);
            vec3Add(&span->color_begin,color_delta);
        }
        return;
    }

    int mipmap_size = span->texture->size >> span->mipmap;
    int mipmap_shift = (bitScanReverse(span->texture->size)) - span->mipmap;
    mipmap_shift = tClamp(mipmap_shift,0,16);
    int mipmap_offset = 0;
    for(int i = 0;i < span->mipmap;i++)
        mipmap_offset += (span->texture->size >> i) * (span->texture->size >> i);

    Vec2 texture_delta = vec2ShlR(vec2SubR(span->texture_end,span->texture_begin),mipmap_shift);
    texture_delta.x /= tMax(span->end - span->begin,1);
    texture_delta.y /= tMax(span->end - span->begin,1);

    span->texture_begin.x <<= mipmap_shift;
    span->texture_begin.y <<= mipmap_shift;
    
    for(;i < span->end - 3;i += 4){
        for(int j = 0;j < 4;j++){
            int texture_x = span->texture_begin.x >> FIXED_PRECISION & mipmap_size - 1;
            int texture_y = span->texture_begin.y >> FIXED_PRECISION & mipmap_size - 1;
            unsigned texel = span->texture->pixel_data[mipmap_offset + (texture_x << mipmap_shift) + texture_y];
            Vec3 texture_color = vec3ShrR(pixelColorToColor(texel),4);
            g_surface.data[height * g_surface.width + (i + j)] = colorToPixelColor(vec3MulR(texture_color,span->color_begin));
            vec3Add(&span->color_begin,color_delta);
            vec2Add(&span->texture_begin,texture_delta);
        }
    }

	for(;i < span->end;i++){            
        int texture_x = span->texture_begin.x >> FIXED_PRECISION & mipmap_size - 1;
        int texture_y = span->texture_begin.y >> FIXED_PRECISION & mipmap_size - 1;
        unsigned texel = span->texture->pixel_data[mipmap_offset + (texture_x << mipmap_shift) + texture_y];
        Vec3 texture_color = vec3ShrR(pixelColorToColor(texel),4);
        g_surface.data[height * g_surface.width + i] = colorToPixelColor(vec3MulR(texture_color,span->color_begin));
        vec3Add(&span->color_begin,color_delta);
        vec2Add(&span->texture_begin,texture_delta);
    }
}

void spanDrawList(void){
    for(int i = 0;i < g_surface.height;i++){
        for(Span* span = span_list[i];span;){
            Span* span_current = span;
            span = span->next;
            bool culled = false;
            bool newclip = true;
            bool changed;
            for(Clip* clip = (void*)clip_arena.data;clip < clip_arena.data + clip_arena.pointer;clip++){
                if(span_current->begin >= clip->begin && span_current->end <= clip->end){
                    culled = true;
                    break;
                }
                if(span_current->begin < clip->begin && span_current->end > clip->end){
                    Span* span_new = memoryArenaAllocate(&span_arena,sizeof(Span));
                    *span_new = *span_current;
                    span_new->begin = clip->end;
                    span_new->next = span;
                    span = span_new;
                    span_current->end = clip->begin;
                    continue;
                }
                if(span_current->begin >= clip->begin && span_current->begin < clip->end){
                    span_current->begin = clip->end;
                }
                else if(span_current->end > clip->begin && span_current->end <= clip->end){
                    span_current->end = clip->begin;
                }
            }
            if(culled)
                continue;
            if(newclip){
                Clip* clip = memoryArenaAllocate(&clip_arena,sizeof(Clip));
                clip->begin = span_current->begin;
                clip->end = span_current->end;
            }
            spanDraw(span_current,i);
        }
        memoryArenaFree(&clip_arena);
        span_list[i] = 0;
    }
    memoryArenaFree(&span_arena);
}

static void spanListAdd(int x,Span* span){
    if(span_list[x])
        span_list[x]->previous = span;
    span->next = span_list[x];
    span->previous = 0;
    span_list[x] = span;
}

static void spanColorTextureAdd(int x,int begin,int end,Vec3 color_begin,Vec3 color_end,Texture* texture,Vec2 texture_begin,Vec2 texture_end,int mipmap,int flags){
    Span* span = memoryArenaAllocate(&span_arena,sizeof(Span));
    *span = (Span){
        .begin = begin,
        .end = end,
        .flags = flags,
        .color_begin = color_begin,
        .color_end = color_end,
        .texture_begin = texture_begin,
        .texture_end = texture_end,
        .mipmap = mipmap,
        .texture = texture,
    };
    spanListAdd(x,span);
}


static void spanAdd(int x,int begin,int end,Vec3 color){
    Span* span = memoryArenaAllocate(&span_arena,sizeof(Span));
    span->begin = begin;
    span->end = end;
    span->flags = SPAN_SOLIDCOLOR;
    span->color = color;

    spanListAdd(x,span);
}

static Vec3 polygonPositionNew(Vec3 inside,Vec3 outside){
    Vec3 direction = vec3Direction(inside,outside);
    Plane plane = {.normal = getLookDirection(g_angle)};
    int distance = tAbs(rayPlaneIntersection(vec3SubR(inside,g_position),direction,plane));
    return vec3AddR(inside,vec3MulRS(direction,distance - 0x2000));
}

void spanEllipsesAdd(DrawSurface* surface,int cx,int cy,int size_x,int size_y,Vec3 color){
    cy = -cy;
    size_x = scaleDraw(surface->height,size_x);
    size_y = scaleDraw(surface->width,size_y);
    cx = transformDraw(surface->height,cx);
    cy = transformDraw(surface->width,cy);
    int min_x = tMax(cx - size_x,0);
    int max_x = tMin(cx + size_x,surface->height - 1);
    for(int x = min_x;x <= max_x;x++){
        int v = (x - (cx)) * FIXED_ONE / size_x;
        int offset_real = tSqrt(FIXED_ONE - fixedMulR(v,v));
        int offset = fixedMulR((size_y * FIXED_ONE),offset) / FIXED_ONE;
        spanAdd(x,tMax(cy - offset,0),tMin(cy + offset,surface->width - 1),color);
    }
}

void spanQuadAdd(DrawSurface* surface,Vec2* coords_2d,Vec3 color){
    for(int i = 0;i < 4;i++){
        coords_2d[i].x = transformDraw(surface->height,coords_2d[i].x);
        coords_2d[i].y = transformDraw(surface->width ,coords_2d[i].y);
    }

    int x_min = INT_MAX;
    int x_max = INT_MIN;

    for(int i = 0;i < 4;i++){
        x_min = tMin(coords_2d[i].x,x_min);
        x_max = tMax(coords_2d[i].x,x_max);
    }

    x_min = tMax(0,x_min);
    x_max = tMin(surface->height,x_max);

    if(x_min >= surface->height || x_max < 0)    
        return;

    for(int i = x_min;i < x_max;i++){
		surface->scanline[i].x = INT_MAX;
		surface->scanline[i].y = INT_MIN;
	}

    for(int i = 0;i < 4;i++){
        Vec2 p1 = coords_2d[i];
        Vec2 p2 = coords_2d[(i + 1) % 4];
        setScanline(surface->scanline,p1,p2,surface->height);
    }

    for(int x = x_min;x < x_max;x++){
        surface->scanline[x].x = tClamp(surface->scanline[x].x,0,surface->width);
        surface->scanline[x].y = tClamp(surface->scanline[x].y,0,surface->width);
        spanAdd(x,surface->scanline[x].x,surface->scanline[x].y,color);
    }
}

void spanQuad3dAdd(DrawSurface* surface,Vec3* coordinats,Vec3 color){
    Vec3 d_point[] = {
        coordinats[0],
        coordinats[1],
        coordinats[3],
        coordinats[2],
    };
    Vec3 transformed[] = {
        pointToScreenRenderer(d_point[0],g_tri,g_position,g_options.fov),
        pointToScreenRenderer(d_point[1],g_tri,g_position,g_options.fov),
        pointToScreenRenderer(d_point[2],g_tri,g_position,g_options.fov),
        pointToScreenRenderer(d_point[3],g_tri,g_position,g_options.fov),
    };
    
    if(transformed[0].z <= 0 && transformed[1].z <= 0 && transformed[2].z <= 0 && transformed[3].z <= 0)
        return;
    
    Vec3 quad_new[5];
    Vec3* quad_ptr = quad_new;
    for(int i = 0;i < 4;i++){
        if(transformed[i].z > 0){
            *quad_ptr++ = d_point[i];
            if(transformed[(i + 1) % 4].z <= 0)
                *quad_ptr++ = polygonPositionNew(d_point[i],d_point[(i + 1) % 4]);
        }
        else{
            if(transformed[(i + 1) % 4].z > 0)
                *quad_ptr++ = polygonPositionNew(d_point[(i + 1) % 4],d_point[i]);
        }
    }
    int n_vertex = quad_ptr - quad_new;        
    for(int i = n_vertex;i--;)
        transformed[i] = pointToScreenRenderer(quad_new[i],g_tri,g_position,g_options.fov);
    
    Vec2 coords_2d[5];
    for(int i = n_vertex;i--;){
        coords_2d[i] = (Vec2){fixedDivR(transformed[i].x,transformed[i].z),fixedDivR(-transformed[i].y,transformed[i].z)};
        coords_2d[i].x = tClamp(coords_2d[i].x,-FIXED_ONE * 0x10,FIXED_ONE * 0x10);
        coords_2d[i].y = tClamp(coords_2d[i].y,-FIXED_ONE * 0x10,FIXED_ONE * 0x10);
    }

    for(int i = 0;i < n_vertex;i++){
        coords_2d[i].x = transformDraw(surface->height,coords_2d[i].x);
        coords_2d[i].y = transformDraw(surface->width ,coords_2d[i].y);
    }

    int x_min = INT_MAX;
    int x_max = INT_MIN;

    for(int i = 0;i < n_vertex;i++){
        x_min = tMin(coords_2d[i].x,x_min);
        x_max = tMax(coords_2d[i].x,x_max);
    }

    x_min = tMax(0,x_min);
    x_max = tMin(surface->height,x_max);

    if(x_min >= surface->height || x_max < 0)    
        return;

    for(int i = x_min;i < x_max;i++){
		surface->scanline[i].x = INT_MAX;
		surface->scanline[i].y = INT_MIN;
	}

    for(int i = 0;i < n_vertex;i++){
        Vec2 p1 = coords_2d[i];
        Vec2 p2 = coords_2d[(i + 1) % n_vertex];
        setScanline(surface->scanline,p1,p2,surface->height);
    }

    for(int x = x_min;x < x_max;x++){
        surface->scanline[x].x = tClamp(surface->scanline[x].x,0,surface->width);
        surface->scanline[x].y = tClamp(surface->scanline[x].y,0,surface->width);
        spanAdd(x,surface->scanline[x].x,surface->scanline[x].y,color);
    }
}

void spanQuad3dLightingAdd(DrawSurface* surface,Vec3* coordinats,Vec3* color){
    if(coordinats[0].z <= 0 || coordinats[1].z <= 0 || coordinats[2].z <= 0 || coordinats[3].z <= 0)
        return;
    Vec2 coords_2d[] = {
        {fixedDivR(coordinats[0].x,coordinats[0].z),fixedDivR(-coordinats[0].y,coordinats[0].z)},
        {fixedDivR(coordinats[1].x,coordinats[1].z),fixedDivR(-coordinats[1].y,coordinats[1].z)},
        {fixedDivR(coordinats[2].x,coordinats[2].z),fixedDivR(-coordinats[2].y,coordinats[2].z)},
        {fixedDivR(coordinats[3].x,coordinats[3].z),fixedDivR(-coordinats[3].y,coordinats[3].z)},
    };

    for(int i = 0;i < 4;i++){
        coords_2d[i].x = transformDraw(surface->height,coords_2d[i].x);
        coords_2d[i].y = transformDraw(surface->width ,coords_2d[i].y);
    }

    int x_min = INT_MAX;
    int x_max = INT_MIN;

    for(int i = 0;i < 4;i++){
        x_min = tMin(coords_2d[i].x,x_min);
        x_max = tMax(coords_2d[i].x,x_max);
    }

    x_min = tMax(0,x_min);
    x_max = tMin(surface->height,x_max);

    if(x_min >= surface->height || x_max < 0)    
        return;

    for(int i = x_min;i < x_max;i++){
		surface->scanline[i].x = INT_MAX;
		surface->scanline[i].y = INT_MIN;
	}

    for(int i = 0;i < 4;i++){
        Vec2 p1 = coords_2d[i];
        Vec2 p2 = coords_2d[(i + 1) % 4];
        Vec3 color_1 = color[i];
        Vec3 color_2 = color[(i + 1) % 4];
        setScanlineColor(surface->scanline_color,surface->scanline,color_1,color_2,p1,p2,surface->height);
    }

    for(int x = x_min;x < x_max;x++){
        surface->scanline[x].x = tClamp(surface->scanline[x].x,0,surface->width);
        surface->scanline[x].y = tClamp(surface->scanline[x].y,0,surface->width);
        spanColorTextureAdd(x,surface->scanline[x].x,surface->scanline[x].y,surface->scanline_color[x].begin,surface->scanline_color[x].end,0,surface->scanline_texture[x].begin,surface->scanline_texture[x].end,0,0);
    }
}

void spanSpriteAdd(DrawSurface* surface,Texture* texture,Vec2* coords_2d){
    Vec2* texture_coordinats = g_texture_coordinates_fill;
    for(int i = 0;i < 4;i++){
        int temp = texture_coordinats[i].x;
        texture_coordinats[i].x = texture_coordinats[i].y;
        texture_coordinats[i].y = temp;
    }
    
    for(int i = 0;i < 4;i++){
        coords_2d[i].x = transformDraw(surface->height,coords_2d[i].x);
        coords_2d[i].y = transformDraw(surface->width ,-coords_2d[i].y);
    }
    
    int x_min = INT_MAX;
    int x_max = INT_MIN;

    for(int i = 0;i < 4;i++){
        x_min = tMin(coords_2d[i].x,x_min);
        x_max = tMax(coords_2d[i].x,x_max);
    }

    x_min = tMax(0,x_min);
    x_max = tMin(surface->height,x_max);

    if(x_min >= surface->height || x_max < 0)    
        return;

    int delta_x = FIXED_ONE / tMax((x_max - x_min),1);
    int i_e_x = 0;

    int mipmap = tClamp(bitScanReverse((tAbs(delta_x) + tAbs(delta_x)) * texture->size) - 14,0,31);

    unsigned polygon_id = tRnd();
    
    for(int x = x_min;x < x_max;x++){
        int begin = 0;
        int end = 0;
        int texture_b,texture_e;
        
        int delta = FIXED_ONE / tMax((coords_2d[0].y - coords_2d[1].y),1);
        int i_e = 0;
        for(int y = coords_2d[1].y;y < coords_2d[0].y;y++){
            if(!begin && !(textureLookup(texture,i_e_x,i_e,0) >> 24)){
                begin = y;
                texture_b = i_e;
            }
            else if(begin && !end && (textureLookup(texture,i_e_x,i_e,0) >> 24)){
                end = y;
                texture_e = i_e;
            }
            i_e += delta;
        }
        i_e_x += delta_x;

        if(!begin || !end)
            continue;
        
        spanColorTextureAdd(x,begin,end,vec3Single(FIXED_ONE << 4),vec3Single(FIXED_ONE << 4),texture,(Vec2){i_e_x,texture_b},(Vec2){i_e_x,texture_e},mipmap,0);
    }
}

void spanQuad3dLightingTextureAdd(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color){
    if(coordinats[0].z <= 0 || coordinats[1].z <= 0 || coordinats[2].z <= 0 || coordinats[3].z <= 0)
        return;
    Vec2 coords_2d[] = {
        {fixedDivR(coordinats[0].x,coordinats[0].z),fixedDivR(-coordinats[0].y,coordinats[0].z)},
        {fixedDivR(coordinats[1].x,coordinats[1].z),fixedDivR(-coordinats[1].y,coordinats[1].z)},
        {fixedDivR(coordinats[2].x,coordinats[2].z),fixedDivR(-coordinats[2].y,coordinats[2].z)},
        {fixedDivR(coordinats[3].x,coordinats[3].z),fixedDivR(-coordinats[3].y,coordinats[3].z)},
    };
    for(int i = 0;i < 4;i++){
        int temp = texture_coordinats[i].x;
        texture_coordinats[i].x = texture_coordinats[i].y;
        texture_coordinats[i].y = temp;
    }

    for(int i = 0;i < 4;i++){
        coords_2d[i].x = transformDraw(surface->height,coords_2d[i].x);
        coords_2d[i].y = transformDraw(surface->width ,coords_2d[i].y);
    }

    int x_min = INT_MAX;
    int x_max = INT_MIN;

    for(int i = 0;i < 4;i++){
        x_min = tMin(coords_2d[i].x,x_min);
        x_max = tMax(coords_2d[i].x,x_max);
    }

    x_min = tMax(0,x_min);
    x_max = tMin(surface->height,x_max);

    if(x_min >= surface->height || x_max < 0)    
        return;

    for(int i = x_min;i < x_max;i++){
		surface->scanline[i].x = INT_MAX;
		surface->scanline[i].y = INT_MIN;
	}

    for(int i = 0;i < 4;i++){
        Vec2 p1 = coords_2d[i];
        Vec2 p2 = coords_2d[(i + 1) % 4];
        Vec3 color_1 = color[i];
        Vec3 color_2 = color[(i + 1) % 4];
        Vec2 texture_1 = texture_coordinats[i];
        Vec2 texture_2 = texture_coordinats[(i + 1) % 4];
        setScanlineColorTexture(surface->scanline_color,surface->scanline_texture,surface->scanline,texture_1,texture_2,color_1,color_2,p1,p2,surface->height);
    }

    Vec2 texture_begin = surface->scanline_texture[x_min].begin;
    Vec2 texture_end   = surface->scanline_texture[x_min].end;

    Vec2 texture_delta = vec2SubR(texture_end,texture_begin);
    texture_delta.x /= tMax(surface->scanline[x_min].y - surface->scanline[x_min].x,1);
    texture_delta.y /= tMax(surface->scanline[x_min].y - surface->scanline[x_min].x,1);

    int mipmap = tClamp(bitScanReverse((tAbs(texture_delta.x) + tAbs(texture_delta.y)) * texture->size) - 14,0,31);

    for(int x = x_min;x < x_max;x++){
        surface->scanline[x].x = tClamp(surface->scanline[x].x,0,surface->width);
        surface->scanline[x].y = tClamp(surface->scanline[x].y,0,surface->width);
        spanColorTextureAdd(x,surface->scanline[x].x,surface->scanline[x].y,surface->scanline_color[x].begin,surface->scanline_color[x].end,texture,surface->scanline_texture[x].begin,surface->scanline_texture[x].end,mipmap,0);
    }
}

void spanSegmentAdd(DrawSurface* surface,int x1,int y1,int x2,int y2,int thickness,Vec3 color){
    /*
    Vec2 p1 = {x1,y1};
    Vec2 p2 = {x2,y2};

    Vec2 direction = vec2MulRS(vec2Direction((Vec2){x1,y1},(Vec2){x2,y2}),thickness);
    Vec2 quad[] = {
        vec2AddR((Vec2){x1,y1},vec2Rotate(direction,FIXED_ONE / 8 * 3)),
        vec2AddR((Vec2){x2,y2},vec2Rotate(direction,FIXED_ONE / 8 * 1)),
        vec2AddR((Vec2){x2,y2},vec2Rotate(direction,FIXED_ONE / 8 * 7)),
        vec2AddR((Vec2){x1,y1},vec2Rotate(direction,FIXED_ONE / 8 * 5)),
    };

    for(int i = 0;i < 4;i++){
        quad[i].x = transformDraw(surface->height,quad[i].x);
        quad[i].y = transformDraw(surface->width ,quad[i].y);
    }
    
    int x_min = INT_MAX;
    int x_max = INT_MIN;

    for(int i = 0;i < 4;i++){
        x_min = tMin(quad[i].x,x_min);
        x_max = tMax(quad[i].x,x_max);
    }

    x_min = tMax(0,x_min);
    x_max = tMin(surface->height,x_max);

    if(x_min >= surface->height || x_max < 0)    
        return;

    for(int i = x_min;i < x_max;i++){
		surface->scanline[i].x = INT_MAX;
		surface->scanline[i].y = INT_MIN;
	}

    for(int i = 0;i < 4;i++){
        Vec2 p1 = quad[i];
        Vec2 p2 = quad[(i + 1) % 4];
        setScanline(surface->scanline,p1,p2,surface->height);
    }

    for(int x = x_min;x < x_max;x++){
        surface->scanline[x].x = tClamp(surface->scanline[x].x,0,surface->width);
        surface->scanline[x].y = tClamp(surface->scanline[x].y,0,surface->width);
        spanAdd(x,surface->scanline[x].x,surface->scanline[x].y,color);
    }
    */
}
