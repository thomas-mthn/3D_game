#include "span.h"
#include "main.h"

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
    bool solid_color;
    Texture* texture;
    int mipmap;
    Vec2 texture_begin;
    Vec2 texture_end;
};

structure(Clip){
    int16 begin;
    int16 end;
};

static Span  g_span_arena[0x1000000];
static Span* g_span_arena_ptr = g_span_arena;
static Span* g_span_list[0x1000];

static Clip  g_clip_list[0x1000];
static Clip* g_clip_ptr = g_clip_list;

static void spanDraw(Span* span,int height){
    int i = span->begin;

    if(span->solid_color){
        for(;i < span->end - 3;i += 4){
            for(int j = 0;j < 4;j++){
                g_surface.data[height * g_surface.width + i + j] = colorToPixelColor(span->color);
            }
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
    int mipmap_shift = leadingZeroCount(span->texture->size) - span->mipmap;
    mipmap_shift = tClamp(mipmap_shift,0,16);
    int mipmap_offset = 0;
    for(int i = 0;i < span->mipmap;i++){
        mipmap_offset += (span->texture->size >> i) * (span->texture->size >> i);
    }

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
		for(Span* span = g_span_list[i];span;span = span->next){
            bool can_cull = false;
            bool create_clip = true;
            Clip* my_clip = 0;
            for(Clip* clip = g_clip_list;clip < g_clip_ptr;clip += 1){
                //TODO: look into this, I want to delete spans if I can
                /*
                if(my_clip){
                    if(my_clip->begin >= clip->begin && my_clip->begin <= clip->end){
                        my_clip->begin = clip->begin;
                        for(Clip* clip2 = clip;clip2 < g_clip_ptr;clip2++)
                            *clip2 = *(clip2 + 1);
                        g_clip_ptr -= 1;
                        clip -= 1;
                    }
                    else if(my_clip->end >= clip->begin && my_clip->end <= clip->end){
                        my_clip->end = clip->end;
                        for(Clip* clip2 = clip;clip2 < g_clip_ptr;clip2++)
                            *clip2 = *(clip2 + 1);
                        g_clip_ptr -= 1;
                        clip -= 1;
                    }
                    continue;
                }
                */
                if(span->begin >= clip->begin && span->end <= clip->end){
                    can_cull = true;
                    break;
                }
                if(span->begin < clip->begin && span->end >= clip->end){
                    Span* span_new = g_span_arena_ptr++;
                    *span_new = *span;
                    span_new->begin = clip->end;
                    span->end = clip->begin;
                    span->next = span_new;
                }
                if(span->begin >= clip->begin && span->begin <= clip->end){
                    span->begin = clip->end;
                    clip->end = span->end;
                    create_clip = false;
                    my_clip = clip;
                }
                else if(span->end >= clip->begin && span->end <= clip->end){
                    span->end = clip->begin;
                    clip->begin = span->begin;
                    create_clip = false;
                    my_clip = clip;
                }
            }
            /*
            for(Clip* clip = g_clip_list;clip < g_clip_ptr;clip += 1){
                if(span->begin >= clip->begin && span->end <= clip->end){
                    can_cull = true;
                    break;
                }
                if(span->begin < clip->begin && span->end >= clip->end){
                    Span* span_new = g_span_arena_ptr++;
                    *span_new = *span;
                    span_new->begin = clip->end;
                    span->end = clip->begin;
                    span->next = span_new;
                }
                if(span->begin >= clip->begin && span->begin <= clip->end){
                    span->begin = clip->end;
                    clip->end = span->end;
                    create_clip = false;
                }
                else if(span->end >= clip->begin && span->end <= clip->end){
                    span->end = clip->begin;
                    clip->begin = span->begin;
                    create_clip = false;
                }
            }
            */
            if(can_cull)
                continue;
            if(create_clip){
                Clip* clip = g_clip_ptr++;
                clip->begin = span->begin;
                clip->end = span->end;
            }
			spanDraw(span,i);
        }
        g_clip_ptr = g_clip_list;
        g_span_list[i] = 0;
	}
	g_span_arena_ptr = g_span_arena;
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

static void spanColorTextureAdd(int x,int begin,int end,Vec3 color_begin,Vec3 color_end,Texture* texture,Vec2 texture_begin,Vec2 texture_end,int mipmap){
    Span* span = g_span_arena_ptr++;
    span->begin = begin;
    span->end = end;
    span->solid_color = false;
    span->color_begin = color_begin;
    span->color_end = color_end;
    span->texture_begin = texture_begin;
    span->texture_end = texture_end;
    span->mipmap = mipmap;
    span->texture = texture;

    if(g_span_list[x])
        g_span_list[x]->previous = span;
    span->next = g_span_list[x];
    span->previous = 0;
    g_span_list[x] = span;
}

static void spanAdd(int x,int begin,int end,Vec3 color){
    Span* span = g_span_arena_ptr++;
    span->begin = begin;
    span->end = end;
    span->solid_color = true;
    span->color = color;

    if(g_span_list[x])
        g_span_list[x]->previous = span;
    span->next = g_span_list[x];
    span->previous = 0;
    g_span_list[x] = span;
}

void spanQuadAdd(DrawSurface* surface,Vec3* coordinats,Vec3 color){
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
        setScanline(surface->scanline,p1,p2,surface->height);
    }

    for(int x = x_min;x < x_max;x++){
        surface->scanline[x].x = tClamp(surface->scanline[x].x,0,surface->width);
        surface->scanline[x].y = tClamp(surface->scanline[x].y,0,surface->width);
        spanAdd(x,surface->scanline[x].x,surface->scanline[x].y,color);
    }
}

void spanQuadLightingAdd(DrawSurface* surface,Vec3* coordinats,Vec3* color){
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
        spanColorTextureAdd(x,surface->scanline[x].x,surface->scanline[x].y,surface->scanline_color[x].begin,surface->scanline_color[x].end,0,surface->scanline_texture[x].begin,surface->scanline_texture[x].end,0);
    }
}

void spanQuadLightingTextureAdd(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color){
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

    int mipmap = tClamp(bitScanReverse((tAbs(texture_delta.x) + tAbs(texture_delta.y)) * texture->size) - 15,0,31);

    for(int x = x_min;x < x_max;x++){
        surface->scanline[x].x = tClamp(surface->scanline[x].x,0,surface->width);
        surface->scanline[x].y = tClamp(surface->scanline[x].y,0,surface->width);
        spanColorTextureAdd(x,surface->scanline[x].x,surface->scanline[x].y,surface->scanline_color[x].begin,surface->scanline_color[x].end,texture,surface->scanline_texture[x].begin,surface->scanline_texture[x].end,mipmap);
    }
}