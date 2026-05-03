#include "span.h"
#include "draw.h"
#include "fixed.h"
#include "geometry.h"
#include "main.h"
#include "texture.h"
#include "memory.h"
#include "lighting.h"

structure(Span){
	Span* next;
    Span* previous;
	int16 begin;
	int16 end;
    union{
        Vec2 color_begin;
        Vec3 color;
    };
    Vec2 color_end;
    bool solid_color : 1;
    bool solid_color_texture : 1;
    Texture* texture;
    int mipmap;
    Vec2 texture_begin;
    Vec2 texture_end;
    int z_begin;
    int z_end;
    int interpolate_begin;
    int interpolate_end;
    LightmapTree* lightmap;
};

structure(Clip){
    int16 begin;
    int16 end;
};

structure(ClipContainer){
    ClipContainer* next;
    int count;
    Clip clip[0x100];
};

int perspective_lerp(int z1,int z2,int t){
    int inv_z1 = fixedDivR(FIXED_ONE << 8,z1) >> 8;
    int inv_z2 = fixedDivR(FIXED_ONE << 8,z2) >> 8;

    int b = fixedMulR(t,inv_z2);
    
    return fixedDivR(b << 8,tMix(inv_z1,inv_z2,t)) >> 8;
}

static void setScanlineColor(ScanlineColor* color,Scanline scanline,ScanlineZ* scanline_z,Vec2 color_1,Vec2 color_2,Vec3 pos_1,Vec3 pos_2,int size){
    if(pos_1.x > pos_2.x){
        Vec2 tmp = color_1;
        color_1 = color_2;
        color_2 = tmp;

        Vec3 tmp_pos = pos_1;
        pos_1 = pos_2;
        pos_2 = tmp_pos;
    }
    
    Vec2 delta_color = vec2Sub(color_2,color_1);
    int p_begin = pos_1.x;
    int p_end = pos_2.x;
    int delta = (pos_2.y - pos_1.y << FIXED_PRECISION) / (pos_2.x - pos_1.x ? pos_2.x - pos_1.x : 1);
    int delta_pos = (pos_1.y << FIXED_PRECISION);
    int delta_t = (FIXED_ONE) / tMax(p_end - p_begin,1);
    int t = 0;

    delta_color.x /= tMax(p_end - p_begin,1);
    delta_color.y /= tMax(p_end - p_begin,1);

	if(p_end < 0)
		return;
    if(p_begin >= size)
        return;
	if(p_begin < 0){
        delta_pos += delta * (-p_begin);
        t += delta_t * (-p_begin);
		p_begin = 0;
    }
	if(p_end >= size){
		p_end = size;
	}
    while(p_begin < p_end){
        int index = p_begin;

        int l = perspective_lerp(pos_1.z,pos_2.z,t);

        Vec2 c = vec2Mix(color_1,color_2,l);

        if(delta_pos >> FIXED_PRECISION < scanline.begin[index]){
            scanline_z[index].begin = tMix(pos_1.z,pos_2.z,l);
            color[index].begin = c;
        }
        if(delta_pos >> FIXED_PRECISION > scanline.end[index]){
            scanline_z[index].end = tMix(pos_1.z,pos_2.z,l);
            color[index].end = c;
        }

        scanline.begin[index] = tMin(scanline.begin[index],delta_pos >> FIXED_PRECISION);
        scanline.end[index] = tMax(scanline.end[index],delta_pos >> FIXED_PRECISION);
        delta_pos += delta;
        
        p_begin += 1;
        t += delta_t;               
    }
}

static void setScanlineTexture(ScanlineTexture texture,Scanline scanline,ScanlineZ* scanline_z,Vec2 texture_1,Vec2 texture_2,Vec3 pos_1,Vec3 pos_2,int size){
    if(pos_1.x > pos_2.x){
        Vec2 tmp_tex = texture_1;
        texture_1 = texture_2;
        texture_2 = tmp_tex;

        Vec3 tmp_pos = pos_1;
        pos_1 = pos_2;
        pos_2 = tmp_pos;
    }

    Vec2 delta_texture = vec2Sub(texture_2,texture_1);
    Vec2 texture_iterator = texture_1;
    int p_begin = pos_1.x;
    int p_end = pos_2.x;
    int delta = (pos_2.y - pos_1.y << FIXED_PRECISION) / (pos_2.x - pos_1.x ? pos_2.x - pos_1.x : 1);
    int delta_pos = (pos_1.y << FIXED_PRECISION);
    int delta_t = (FIXED_ONE << 8) / tMax(p_end - p_begin,1);
    int t = 0;
    
    delta_texture.x /= tMax(p_end - p_begin,1);
    delta_texture.y /= tMax(p_end - p_begin,1);
	if(p_end < 0)
		return;
    if(p_begin >= size)
        return;
	if(p_begin < 0){
        delta_pos += delta * (-p_begin);
        texture_iterator = vec2Add(texture_iterator,(Vec2){delta_texture.x * -p_begin,delta_texture.y * -p_begin});
        t += delta_t * (-p_begin);
		p_begin = 0;
    }
	if(p_end >= size){
		p_end = size;
	}
    while(p_begin < p_end){
        int index = p_begin;

        int l = perspective_lerp(pos_1.z,pos_2.z,t >> 8);

        Vec2 uv = vec2Mix(texture_1,texture_2,l);

        if(delta_pos >> FIXED_PRECISION < scanline.begin[index]){
            texture.begin[index] = uv;
            scanline_z[index].begin = tMix(pos_1.z,pos_2.z,l);
        }
        if(delta_pos >> FIXED_PRECISION > scanline.end[index]){
            texture.end[index] = uv;
            scanline_z[index].end = tMix(pos_1.z,pos_2.z,l);
        }

        scanline.begin[index] = tMin(scanline.begin[index],delta_pos >> FIXED_PRECISION);
        scanline.end[index] = tMax(scanline.end[index],delta_pos >> FIXED_PRECISION);
        delta_pos += delta;
        
        p_begin += 1;
        t += delta_t;               
    }
}

static void setScanlineColorTexture(ScanlineColor* color,ScanlineTexture texture,ScanlineZ* scanline_z,Scanline scanline,Vec2 texture_1,Vec2 texture_2,Vec2 color_1,Vec2 color_2,Vec3 pos_1,Vec3 pos_2,int size){
    if(pos_1.x > pos_2.x){
        Vec2 tmp_tex = texture_1;
        texture_1 = texture_2;
        texture_2 = tmp_tex;

        Vec2 tmp = color_1;
        color_1 = color_2;
        color_2 = tmp;

        Vec3 tmp_pos = pos_1;
        pos_1 = pos_2;
        pos_2 = tmp_pos;
    }
    
    Vec2 delta_color = vec2Sub(color_2,color_1);
    Vec2 delta_texture = vec2Sub(texture_2,texture_1);
    int p_begin = pos_1.x;
    int p_end = pos_2.x;
    int delta = (pos_2.y - pos_1.y << FIXED_PRECISION) / (pos_2.x - pos_1.x ? pos_2.x - pos_1.x : 1);
    int delta_pos = (pos_1.y << FIXED_PRECISION);
    int delta_t = (FIXED_ONE << 8) / tMax(p_end - p_begin,1);
    int t = 0;

    delta_color.x /= tMax(p_end - p_begin,1);
    delta_color.y /= tMax(p_end - p_begin,1);

    delta_texture.x /= tMax(p_end - p_begin,1);
    delta_texture.y /= tMax(p_end - p_begin,1);

	if(p_end < 0)
		return;
    if(p_begin >= size)
        return;
	if(p_begin < 0){
        delta_pos += delta * (-p_begin);
        t += delta_t * (-p_begin);
		p_begin = 0;
    }
	if(p_end >= size){
		p_end = size;
	}
    while(p_begin < p_end){
        int index = p_begin;

        int l = perspective_lerp(pos_1.z,pos_2.z,t >> 8);

        Vec2 uv = vec2Mix(texture_1,texture_2,l);
        Vec2 c = vec2Mix(color_1,color_2,l);

        if(delta_pos >> FIXED_PRECISION < scanline.begin[index]){
            texture.begin[index] = uv;
            scanline_z[index].begin = tMix(pos_1.z,pos_2.z,l);
            color[index].begin = c;
        }
        if(delta_pos >> FIXED_PRECISION > scanline.end[index]){
            texture.end[index] = uv;
            scanline_z[index].end = tMix(pos_1.z,pos_2.z,l);
            color[index].end = c;
        }

        scanline.begin[index] = tMin(scanline.begin[index],delta_pos >> FIXED_PRECISION);
        scanline.end[index] = tMax(scanline.end[index],delta_pos >> FIXED_PRECISION);
        delta_pos += delta;
        
        p_begin += 1;
        t += delta_t;               
    }
}

static Vec3 lightmapGet(LightmapTree* lightmap,Vec2 uv){
    uv.x = tClamp(uv.x,0,FIXED_ONE - 1);
    uv.y = tClamp(uv.y,0,FIXED_ONE - 1);
    int bit = FIXED_PRECISION - 1;
    while(lightmap->child[0]){
        int index = (uv.x & 1 << bit) >> bit << 1 | (uv.y & 1 << bit) >> bit;
        lightmap = lightmap->child[index];
        bit -= 1;
    }
    return lightmap->luminance;
}

static Vec3 lightmapBilinear(LightmapTree* lightmap,Vec2 uv){
    LightmapTree* node = lightmap;
    int bit = FIXED_PRECISION - 1;
    while(node->child[0]){
        int index = (uv.x & 1 << bit) >> bit << 1 | (uv.y & 1 << bit) >> bit;
        node = node->child[index];
        bit -= 1;
    }
    bit += 1;
    int luxel_size = 1 << bit + 1;

    int mask = luxel_size - 1;
    
    Vec2 lightmap_pos = {uv.x & ~mask,uv.y & ~mask};
    
    Vec2 l_uv = {fixedDivR(uv.x & mask,luxel_size),fixedDivR(uv.y & mask,luxel_size)};
    
    Vec3 ll = lightmapGet(lightmap,(Vec2){lightmap_pos.x,lightmap_pos.y});
    Vec3 lh = lightmapGet(lightmap,(Vec2){lightmap_pos.x,lightmap_pos.y + luxel_size});
    Vec3 hh = lightmapGet(lightmap,(Vec2){lightmap_pos.x + luxel_size,lightmap_pos.y + luxel_size});
    Vec3 hl = lightmapGet(lightmap,(Vec2){lightmap_pos.x + luxel_size,lightmap_pos.y});

    Vec3 lx1 = vec3Mix(ll,hl,l_uv.x);
    Vec3 lx2 = vec3Mix(lh,hh,l_uv.x);

    return vec3Mix(lx1,lx2,l_uv.y); 
}

static void spanDraw(DrawSurface* surface,Span* span,int height){
    int i = span->begin;

    if(span->solid_color){
        for(;i < span->end - 3;i += 4){
            for(int j = 0;j < 4;j++)
                surface->data[height * surface->width + i + j] = colorToPixelColor(span->color);
        }
	    for(;i < span->end;i++){
            surface->data[height * surface->width + i] = colorToPixelColor(span->color);
        }
        return;
    }
    if(!span->texture){
#if 0
        for(;i < span->end - 3;i += 4){
            for(int j = 0;j < 4;j++){
                surface->data[height * surface->width + i + j] = colorToPixelColor(span->color_begin);
                vec3Add(&span->color_begin,color_delta);
            }
        }
	    for(;i < span->end;i++){
            surface->data[height * surface->width + i] = colorToPixelColor(span->color_begin);
            vec3Add(&span->color_begin,color_delta);
        }
#endif
        return;
    }

    int mipmap_size = span->texture->size >> span->mipmap;
    int mipmap_shift = (bitScanReverse(span->texture->size)) - span->mipmap;
    mipmap_shift = tClamp(mipmap_shift,0,16);
    int mipmap_offset = 0;
    for(int i = 0;i < span->mipmap;i++)
        mipmap_offset += (span->texture->size >> i) * (span->texture->size >> i);

    Vec2 texture_delta = vec2Shl(vec2Sub(span->texture_end,span->texture_begin),mipmap_shift);
    texture_delta.x /= tMax(span->end - span->begin,1);
    texture_delta.y /= tMax(span->end - span->begin,1);

    int delta = (span->interpolate_end - span->interpolate_begin << 8) / tMax(span->end - span->begin,1);
    int z = span->interpolate_begin << 8;
    
    span->texture_begin.x <<= mipmap_shift;
    span->texture_begin.y <<= mipmap_shift;
    span->texture_end.x <<= mipmap_shift;
    span->texture_end.y <<= mipmap_shift;

    int batch_size = 16;

    int l = perspective_lerp(span->z_begin,span->z_end,z >> 8);
    int l_n = perspective_lerp(span->z_begin,span->z_end,(z >> 8) + (delta >> 8) * batch_size);
    Vec2 uv = vec2Mix(span->texture_begin,span->texture_end,l);
    Vec2 uv_next = vec2Mix(span->texture_begin,span->texture_end,l_n);
    
    if(span->solid_color_texture){
        for(;i < span->end - batch_size + 1;i += batch_size){
            Vec2 uv_delta = vec2Shr(vec2Sub(uv_next,uv),4);
            for(int j = 0;j < batch_size;j++){
                int texture_x = uv.x >> FIXED_PRECISION & mipmap_size - 1;
                int texture_y = uv.y >> FIXED_PRECISION & mipmap_size - 1;
                unsigned texel = span->texture->pixel_data[mipmap_offset + (texture_x << mipmap_shift) + texture_y];
                Vec3 texture_color = vec3Shr(pixelColorToColor(texel),4);
                surface->data[height * surface->width + i + j] = colorToPixelColor(vec3Mul(texture_color,span->color));
                uv = vec2Add(uv,uv_delta);
            }
            z += delta * batch_size;
            l = l_n;
            l_n = perspective_lerp(span->z_begin,span->z_end,(z >> 8) + (delta >> 8) * batch_size);
            uv = uv_next;
            uv_next = vec2Mix(span->texture_begin,span->texture_end,l_n);
        }
        Vec2 uv_delta = vec2Shr(vec2Sub(uv_next,uv),4);
        for(;i < span->end;i++){
            int texture_x = uv.x >> FIXED_PRECISION & mipmap_size - 1;
            int texture_y = uv.y >> FIXED_PRECISION & mipmap_size - 1;
            unsigned texel = span->texture->pixel_data[mipmap_offset + (texture_x << mipmap_shift) + texture_y];
            Vec3 texture_color = vec3Shr(pixelColorToColor(texel),4);
            surface->data[height * surface->width + i] = colorToPixelColor(vec3Mul(texture_color,span->color));
            uv = vec2Add(uv,uv_delta);
        }
        return;
    }

    Vec2 uv_lightmap = vec2Mix(span->color_begin,span->color_end,l);
    Vec2 uv_lightmap_next = vec2Mix(span->color_begin,span->color_end,l_n);
    
    if(!g_options.smooth_lighting){
        for(;i < span->end - batch_size + 1;i += batch_size){
            Vec2 uv_delta = vec2Shr(vec2Sub(uv_next,uv),4);
            Vec2 uv_lightmap_delta = vec2Shr(vec2Sub(uv_lightmap_next,uv_lightmap),4);
            for(int j = 0;j < batch_size;j++){
                int texture_x = uv.x >> FIXED_PRECISION & mipmap_size - 1;
                int texture_y = uv.y >> FIXED_PRECISION & mipmap_size - 1;
                unsigned texel = span->texture->pixel_data[mipmap_offset + (texture_x << mipmap_shift) + texture_y];
                Vec3 texture_color = vec3Shr(pixelColorToColor(texel),4);
                Vec3 color = lightmapGet(span->lightmap,uv_lightmap);
                surface->data[height * surface->width + i + j] = colorToPixelColor(vec3Mul(color,texture_color));
                uv = vec2Add(uv,uv_delta);
                uv_lightmap = vec2Add(uv_lightmap,uv_lightmap_delta);
            }
            z += delta * batch_size;
            l = l_n;
            l_n = perspective_lerp(span->z_begin,span->z_end,(z >> 8) + (delta >> 8) * batch_size);
            
            uv = uv_next;
            uv_next = vec2Mix(span->texture_begin,span->texture_end,l_n);

            uv_lightmap = uv_lightmap_next;
            uv_lightmap_next = vec2Mix(span->color_begin,span->color_end,l_n);
        }
        Vec2 uv_delta = vec2Shr(vec2Sub(uv_next,uv),4);
        for(;i < span->end;i++){
            Vec2 u = vec2Mix(span->color_begin,span->color_end,l_n);
            int texture_x = uv.x >> FIXED_PRECISION & mipmap_size - 1;
            int texture_y = uv.y >> FIXED_PRECISION & mipmap_size - 1;
            unsigned texel = span->texture->pixel_data[mipmap_offset + (texture_x << mipmap_shift) + texture_y];
            Vec3 texture_color = vec3Shr(pixelColorToColor(texel),4);
            Vec3 color = lightmapGet(span->lightmap,uv_lightmap);
            surface->data[height * surface->width + i] = colorToPixelColor(vec3Mul(color,texture_color));
            uv = vec2Add(uv,uv_delta);
        }
        return;
    }

    Vec3 color_next = {0};
    Vec3 color = {0};
    
    color = lightmapBilinear(span->lightmap,uv_lightmap);
    color_next = lightmapBilinear(span->lightmap,uv_lightmap_next);
    
    for(;i < span->end - batch_size + 1;i += batch_size){
        Vec2 uv_delta = vec2Shr(vec2Sub(uv_next,uv),4);
        Vec3 color_delta = vec3Shr(vec3Sub(color_next,color),4);
        for(int j = 0;j < batch_size;j++){
            Vec2 u = vec2Mix(span->color_begin,span->color_end,l_n);
            int texture_x = uv.x >> FIXED_PRECISION & mipmap_size - 1;
            int texture_y = uv.y >> FIXED_PRECISION & mipmap_size - 1;
            unsigned texel = span->texture->pixel_data[mipmap_offset + (texture_x << mipmap_shift) + texture_y];
            Vec3 texture_color = vec3Shr(pixelColorToColor(texel),4);
            surface->data[height * surface->width + i + j] = colorToPixelColor(vec3Mul(color,texture_color));
            uv = vec2Add(uv,uv_delta);
            color = vec3Add(color,color_delta);
        }
        z += delta * batch_size;
        l = l_n;
        l_n = perspective_lerp(span->z_begin,span->z_end,(z >> 8) + (delta >> 8) * batch_size);
        uv = uv_next;
        uv_next = vec2Mix(span->texture_begin,span->texture_end,l_n);
        
        color = color_next;
        
        uv_lightmap = uv_lightmap_next;
        uv_lightmap_next = vec2Mix(span->color_begin,span->color_end,l_n);
        
        color_next = lightmapBilinear(span->lightmap,uv_lightmap_next);
    }
    Vec2 uv_delta = vec2Shr(vec2Sub(uv_next,uv),4);
    Vec3 color_delta = vec3Shr(vec3Sub(color_next,color),4);
    for(;i < span->end;i++){
        Vec2 u = vec2Mix(span->color_begin,span->color_end,l_n);
        int texture_x = uv.x >> FIXED_PRECISION & mipmap_size - 1;
        int texture_y = uv.y >> FIXED_PRECISION & mipmap_size - 1;
        unsigned texel = span->texture->pixel_data[mipmap_offset + (texture_x << mipmap_shift) + texture_y];
        Vec3 texture_color = vec3Shr(pixelColorToColor(texel),4);
        surface->data[height * surface->width + i] = colorToPixelColor(vec3Mul(color,texture_color));
        uv = vec2Add(uv,uv_delta);
        color = vec3Add(color,color_delta);
    }
    /*
    Vec2 uv_delta = vec2ShrR(vec2SubR(uv_next,uv),4);
    for(;i < span->end;i++){
        int texture_x = uv.x >> FIXED_PRECISION & mipmap_size - 1;
        int texture_y = uv.y >> FIXED_PRECISION & mipmap_size - 1;
        unsigned texel = span->texture->pixel_data[mipmap_offset + (texture_x << mipmap_shift) + texture_y];
        Vec3 texture_color = vec3ShrR(pixelColorToColor(texel),4);
        g_surface.data[height * g_surface.width + i] = colorToPixelColor(vec3MulR(texture_color,span->color));
        vec2Add(&uv,uv_delta);
    }
    return;
    */
        /*
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
        */
}

void spanDrawList(DrawSurface* surface){
    static Clip clip_list[0x400];
    int clip_n = 0;
    for(int i = 0;i < surface->height;i++){
        int cover = 0;
        for(Span* span = surface->span_list[i];span;span = span->next){
            Clip* neighbour[2] = {0};
            for(int i = 0;i < clip_n;i++){
                Clip* clip = clip_list + i;
                if(span->begin >= clip->begin && span->end <= clip->end)
                    goto end;
                if(span->begin < clip->begin && span->end > clip->end){
                    Span* span_new = memoryArenaAllocate(&g_arena_frame,sizeof(Span));
                    *span_new = *span;
                    int mix = fixedDivR(clip->end - span->begin << FIXED_PRECISION,span->end - span->begin << FIXED_PRECISION);

                    span_new->interpolate_begin = tMix(span->interpolate_begin,span->interpolate_end,mix);

                    mix = fixedDivR(clip->begin - span->begin << FIXED_PRECISION,span->end - span->begin << FIXED_PRECISION);

                    span->interpolate_end = tMix(span->interpolate_begin,span->interpolate_end,mix);
                    
                    span_new->begin = clip->end;
                    span_new->next = span->next;
                    span->next = span_new;
                    span->end = clip->begin;
                    neighbour[1] = clip;
                }
                else if(span->begin >= clip->begin && span->begin < clip->end){
                    span->begin = clip->end;
                    neighbour[0] = clip;
                }
                else if(span->end > clip->begin && span->end <= clip->end){
                    span->end = clip->begin;
                    neighbour[1] = clip;
                }
            }
            if(neighbour[0]){
                neighbour[0]->end = span->end;
            }
            else if(neighbour[1]){
                neighbour[1]->begin = span->begin;
            }
            else if(clip_n < countof(clip_list)){
                Clip* clip_new = clip_list + clip_n++;
                clip_new->begin = span->begin;
                clip_new->end = span->end;
                cover += span->end - span->begin;
            }
            spanDraw(surface,span,i);
            if(cover >= surface->width)
                break;
        end:;
        }
        clip_n = 0;
        surface->span_list[i] = 0;
    }
}

static Span* spanListAdd(DrawSurface* surface,int x){
    Span* span = memoryArenaAllocate(&g_arena_frame,sizeof(Span));

    *span = (Span){0};
    
    if(surface->span_list[x])
        surface->span_list[x]->previous = span;
    span->next = surface->span_list[x];
    span->previous = 0;
    surface->span_list[x] = span;

    return span;
}

structure(SpanFlag){
    bool solid_color : 1;
    bool solid_color_texture : 1;
};

#if 0

static void spanColorTextureAdd(int x,int begin,int end,Vec2 color_begin,Vec2 color_end,Texture* texture,Vec2 texture_begin,Vec2 texture_end,int z_begin,int z_end,int interpolate_begin,int interpolate_end,int mipmap,SpanFlag flags,LightmapTree* lightmap){
    Span* span = memoryArenaAllocate(&g_arena_frame,sizeof(Span));
    span->begin = begin;
    span->end = end;
    span->solid_color = flags.solid_color;
    span->color_begin = color_begin;
    span->color_end = color_end;
    span->texture_begin = texture_begin;
    span->texture_end = texture_end;
    span->mipmap = mipmap;
    span->texture = texture;
    span->z_begin = z_begin;
    span->z_end = z_end;
    span->interpolate_begin = interpolate_begin;
    span->interpolate_end = interpolate_end;
    span->lightmap = lightmap;
    spanListAdd(x,span);
}

static void spanTextureAdd(int x,int begin,int end,Texture* texture,Vec2 texture_begin,Vec2 texture_end,Vec3 color,int z_begin,int z_end,int interpolate_begin,int interpolate_end,int mipmap){
    Span* span = memoryArenaAllocate(&g_arena_frame,sizeof(Span));
    span->begin = begin;
    span->end = end;
    span->texture_begin = texture_begin;
    span->texture_end = texture_end;
    span->mipmap = mipmap;
    span->texture = texture;
    span->color = color;
    span->solid_color_texture = true;
    span->z_begin = z_begin;
    span->z_end = z_end;
    span->interpolate_begin = interpolate_begin;
    span->interpolate_end = interpolate_end;
    spanListAdd(x,span);
}

static void spanAdd(int x,int begin,int end,Vec3 color){
    Span* span = memoryArenaAllocate(&span_arena,sizeof(Span));
    span->begin = begin;
    span->end = end;
    span->solid_color = true;
    span->color = color;

    spanListAdd(x,span);
}

#endif

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

        Span* span = spanListAdd(surface,x);

        span->begin = tMax(cy - offset,0);
        span->end = tMin(cy + offset,0);
        span->solid_color = true;
        span->color = color;
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
		surface->scanline.begin[i] = INT_MAX;
		surface->scanline.end[i] = INT_MIN;
	}

    for(int i = 0;i < 4;i++){
        Vec2 p1 = coords_2d[i];
        Vec2 p2 = coords_2d[(i + 1) % 4];
        setScanline(surface->scanline,p1,p2,surface->height);
    }

    for(int x = x_min;x < x_max;x++){
        surface->scanline.begin[x] = tClamp(surface->scanline.begin[x],0,surface->width);
        surface->scanline.end[x] = tClamp(surface->scanline.end[x],0,surface->width);

        Span* span = spanListAdd(surface,x);
        span->begin = surface->scanline.begin[x];
        span->end = surface->scanline.end[x];
        span->solid_color = true;
        span->color = color;
    }
}

static Vec3 polygonPositionNew(Vec3 inside,Vec3 outside,int* between,int z_offset){
    Vec3 direction = vec3Direction(outside,inside);
    Plane plane = {.normal = getLookDirection(g_surface.angle),.distance = -z_offset};
    int distance = rayPlaneIntersection(vec3Sub(outside,g_surface.position),direction,plane);
    if(between)
        *between = FIXED_ONE - fixedDivR(distance,vec3Distance(inside,outside));
    return vec3Add(outside,vec3MulS(direction,distance));
}

static int viewPlaneZ(Vec3 point,int* tri,Vec3 renderer_position){
	Vec3 pos = vec3Sub(point,renderer_position);
    
	pos.x = fixedMulR(pos.y,tri[1]) + fixedMulR(pos.x,tri[0]);
	pos.x = fixedMulR(pos.z,tri[3]) + fixedMulR(pos.x,tri[2]);

	return pos.x;
}

static int polygonClipProject(DrawSurface* surface,Vec3* polygon,Vec3* polygon_2d,Vec2* texture_coordinates,Vec2* texture_coordinates_2d,Vec2* lighting_coordinates_2d){
    Vec2 lighting_coordinates[] = {{0,0},{0,FIXED_ONE},{FIXED_ONE,FIXED_ONE},{FIXED_ONE,0},{0,0}};
    Vec3 d_point[] = {
        polygon[0],
        polygon[1],
        polygon[3],
        polygon[2],
    };
    int view_z[5] = {
        viewPlaneZ(d_point[0],surface->rotation_matrix,surface->position),
        viewPlaneZ(d_point[1],surface->rotation_matrix,surface->position),
        viewPlaneZ(d_point[2],surface->rotation_matrix,surface->position),
        viewPlaneZ(d_point[3],surface->rotation_matrix,surface->position),
    };
    if(texture_coordinates){
        for(int i = 4;i--;){
            int temp = texture_coordinates[i].x;
            texture_coordinates[i].x = texture_coordinates[i].y;
            texture_coordinates[i].y = temp;
        }
    }
    
    if(view_z[0] <= 0 && view_z[1] <= 0 && view_z[2] <= 0 && view_z[3] <= 0)
        return 0;

    Vec3 quad_new[5];
    int quad_ptr = 0;

    int z_offset = 0x2000;
    
    for(int i = 0;i < 4;i++){
        int mix_value;
        int i_next = (i + 1) % 4;
        if(view_z[i] > z_offset){
            quad_new[quad_ptr] = d_point[i];
            if(texture_coordinates_2d)
                texture_coordinates_2d[quad_ptr] = texture_coordinates[i];
            if(lighting_coordinates_2d)
                lighting_coordinates_2d[quad_ptr] = lighting_coordinates[i];
            quad_ptr += 1;
            //bandage fix
            if(quad_ptr == countof(quad_new))
                break;
            if(view_z[(i + 1) % 4] <= z_offset){
                quad_new[quad_ptr] = polygonPositionNew(d_point[i],d_point[i_next],texture_coordinates ? &mix_value : 0,z_offset);
                if(texture_coordinates)
                    texture_coordinates_2d[quad_ptr] = vec2Mix(texture_coordinates[i],texture_coordinates[i_next],mix_value);
                if(lighting_coordinates_2d)
                    lighting_coordinates_2d[quad_ptr] = vec2Mix(lighting_coordinates[i],lighting_coordinates[i_next],mix_value);
                quad_ptr += 1;
                //bandage fix
                if(quad_ptr == countof(quad_new))
                    break;
            }
        }
        else{
            if(view_z[(i + 1) % 4] > z_offset){
                quad_new[quad_ptr] = polygonPositionNew(d_point[i_next],d_point[i],texture_coordinates ? &mix_value : 0,z_offset);
                if(texture_coordinates)
                    texture_coordinates_2d[quad_ptr] = vec2Mix(texture_coordinates[i_next],texture_coordinates[i],mix_value);
                if(lighting_coordinates_2d)
                    lighting_coordinates_2d[quad_ptr] = vec2Mix(lighting_coordinates[i_next],lighting_coordinates[i],mix_value);
                quad_ptr += 1;
                //bandage fix
                if(quad_ptr == countof(quad_new))
                    break;
            }
        }
    }
    int n_vertex = quad_ptr;
    Vec3 transformed[5];
    for(int i = n_vertex;i--;)
        transformed[i] = pointToScreenRenderer(quad_new[i],surface->rotation_matrix,surface->position,g_options.fov);
    
    for(int i = n_vertex;i--;){
        polygon_2d[i] = (Vec3){fixedDivR(transformed[i].x,transformed[i].z),fixedDivR(-transformed[i].y,transformed[i].z),transformed[i].z};
        polygon_2d[i].x = tClamp(polygon_2d[i].x,-FIXED_ONE * 0x10,FIXED_ONE * 0x10);
        polygon_2d[i].y = tClamp(polygon_2d[i].y,-FIXED_ONE * 0x10,FIXED_ONE * 0x10);
    }

    for(int i = 0;i < n_vertex;i++){
        polygon_2d[i].x = transformDraw(surface->height,polygon_2d[i].x);
        polygon_2d[i].y = transformDraw(surface->width,polygon_2d[i].y);
    }
    return n_vertex;
}

void spanQuad3dAdd(DrawSurface* surface,Vec3* coordinats,Vec3 color){
    Vec3 coords_2d[5];
    int n_vertex = polygonClipProject(surface,coordinats,coords_2d,0,0,0);
    if(!n_vertex)
        return;
    
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
		surface->scanline.begin[i] = INT16_MAX;
		surface->scanline.end[i] = INT16_MIN;
	}

    for(int i = 0;i < n_vertex;i++){
        Vec2 p1 = (Vec2){coords_2d[i].x,coords_2d[i].y};
        Vec2 p2 = (Vec2){coords_2d[(i + 1) % n_vertex].x,coords_2d[(i + 1) % n_vertex].y};
        setScanline(surface->scanline,p1,p2,surface->height);
    }

    for(int x = x_min;x < x_max;x++){
        surface->scanline.begin[x] = tClamp(surface->scanline.begin[x],0,surface->width);
        surface->scanline.end[x] = tClamp(surface->scanline.end[x],0,surface->width);

        Span* span = spanListAdd(surface,x);
        
        span->begin = surface->scanline.begin[x];
        span->end = surface->scanline.end[x];
        span->solid_color = true;
        span->color = color;
    }
}

void spanQuad3dLightingAdd(DrawSurface* surface,Vec3* coordinats,Vec3* color,LightmapTree* lightmap){
    Vec3 coords_2d[5];
    Vec2 texture_coordinats_2d[5];
    int n_vertex = polygonClipProject(surface,coordinats,coords_2d,0,0,0);
    if(!n_vertex)
        return;
    if(coordinats[0].z <= 0 || coordinats[1].z <= 0 || coordinats[2].z <= 0 || coordinats[3].z <= 0)
        return;
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
		surface->scanline.begin[i] = INT16_MAX;
		surface->scanline.end[i] = INT16_MIN;
	}

    Vec2 color_coord[] = {{0,0},{0,FIXED_ONE},{FIXED_ONE,FIXED_ONE},{FIXED_ONE,0},{0,0}};

    for(int i = 0;i < 4;i++){
        Vec3 p1 = coords_2d[i];
        Vec3 p2 = coords_2d[(i + 1) % 4];
        Vec2 color_1 = color_coord[i];
        Vec2 color_2 = color_coord[(i + 1) % 4];
        setScanlineColor(surface->scanline_color,surface->scanline,surface->scanline_z,color_1,color_2,p1,p2,surface->height);
    }

    for(int x = x_min;x < x_max;x++){
        int begin = tClamp(surface->scanline.begin[x],0,surface->width);
        int end = tClamp(surface->scanline.end[x],0,surface->width);

        int length = surface->scanline.end[x] - surface->scanline.begin[x];
        int b_length_clip = surface->scanline.end[x] - begin;
        int e_length_clip = end - surface->scanline.begin[x];
        int begin_clip = length - b_length_clip;
        int end_clip = length - e_length_clip;

        int interpolate_begin = ((FIXED_ONE << 8) / tMax(length,1)) * begin_clip >> 8;
        int interpolate_end = FIXED_ONE - (((FIXED_ONE << 8) / tMax(length,1)) * end_clip >> 8);

        Span* span = spanListAdd(surface,x);
        
        span->begin = begin;
        span->end = end;
        span->solid_color = true;
        span->color_begin = surface->scanline_color[x].begin;
        span->color_end = surface->scanline_color[x].end;
        span->texture_begin = surface->scanline_texture.begin[x];
        span->texture_end = surface->scanline_texture.end[x];
        span->mipmap = 0;
        span->texture = 0;
        span->z_begin = surface->scanline_z[x].begin;
        span->z_end = surface->scanline_z[x].end;
        span->interpolate_begin = interpolate_begin;
        span->interpolate_end = interpolate_end;
        span->lightmap = lightmap;
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
        
        //spanColorTextureAdd(x,begin,end,vec3Single(FIXED_ONE << 4),vec3Single(FIXED_ONE << 4),texture,(Vec2){i_e_x,texture_b},(Vec2){i_e_x,texture_e},mipmap,(SpanFlag){0});
    }
}

void spanQuad3dTextureAdd(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3 color,int n_point){
    Vec3 coords_2d[5];
    Vec2 texture_coordinats_2d[5];
    int n_vertex = polygonClipProject(surface,coordinats,coords_2d,texture_coordinats,texture_coordinats_2d,0);
    if(!n_vertex)
        return;
    /*
    for(int i = 0;i < n_vertex;i++){
        int temp = texture_coordinats[i].x;
        texture_coordinats[i].x = texture_coordinats[i].y;
        texture_coordinats[i].y = temp;
    }
    */

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
		surface->scanline.begin[i] = INT16_MAX;
		surface->scanline.end[i] = INT16_MIN;
	}

    for(int i = 0;i < n_vertex;i++){
        Vec3 p1 = coords_2d[i];
        Vec3 p2 = coords_2d[(i + 1) % n_vertex];
        Vec2 texture_1 = texture_coordinats_2d[i];
        Vec2 texture_2 = texture_coordinats_2d[(i + 1) % n_vertex];
        setScanlineTexture(surface->scanline_texture,surface->scanline,surface->scanline_z,texture_1,texture_2,p1,p2,surface->height);
    }

    Vec2 texture_begin = surface->scanline_texture.begin[x_min];
    Vec2 texture_end   = surface->scanline_texture.end[x_min];

    Vec2 texture_delta = vec2Sub(texture_end,texture_begin);
    texture_delta.x /= tMax(surface->scanline.end[x_min] - surface->scanline.begin[x_min],1);
    texture_delta.y /= tMax(surface->scanline.end[x_min] - surface->scanline.begin[x_min],1);

    int mipmap = tClamp(bitScanReverse((tAbs(texture_delta.x) + tAbs(texture_delta.y)) * texture->size) - 14,0,31);
    
    for(int x = x_min;x < x_max;x++){
        int begin = tClamp(surface->scanline.begin[x],0,surface->width);
        int end   = tClamp(surface->scanline.end[x],0,surface->width);

        Vec2 texture_delta = vec2Sub(surface->scanline_texture.end[x],surface->scanline_texture.begin[x]);
        texture_delta.x /= tMax(surface->scanline.end[x] - surface->scanline.begin[x],1);
        texture_delta.y /= tMax(surface->scanline.end[x] - surface->scanline.begin[x],1);

        int z_delta = (surface->scanline_z[x].end - surface->scanline_z[x].begin) / tMax(surface->scanline.end[x] - surface->scanline.begin[x],1);
        
        int length = surface->scanline.end[x] - surface->scanline.begin[x];
        int b_length_clip = surface->scanline.end[x] - begin;
        int e_length_clip = end - surface->scanline.begin[x];
        int begin_clip = length - b_length_clip;
        int end_clip = length - e_length_clip;

        int interpolate_begin = ((FIXED_ONE << 8) / tMax(length,1)) * begin_clip >> 8;
        int interpolate_end = FIXED_ONE - (((FIXED_ONE << 8) / tMax(length,1)) * end_clip >> 8);

        Span* span = spanListAdd(surface,x);
        
        span->begin = begin;
        span->end = end;
        span->texture_begin = surface->scanline_texture.begin[x];
        span->texture_end = surface->scanline_texture.end[x];
        span->mipmap = mipmap;
        span->texture = texture;
        span->color = color;
        span->solid_color_texture = true;
        span->z_begin = surface->scanline_z[x].begin;
        span->z_end = surface->scanline_z[x].end;
        span->interpolate_begin = 0;
        span->interpolate_end = FIXED_ONE;
    }
}

void spanQuad3dLightingTextureAdd(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color,LightmapTree* lightmap,int n_vertex){
    if(!lightmap){
        spanQuad3dTextureAdd(surface,texture,texture_coordinats,coordinats,vec3Single(FIXED_ONE << 4),4);
        return;
    }
    Vec3 coords_2d[5];
    Vec2 texture_coordinats_2d[5];
    Vec2 color_coordinats_2d[5];
    n_vertex = polygonClipProject(surface,coordinats,coords_2d,texture_coordinats,texture_coordinats_2d,color_coordinats_2d);
    if(!n_vertex)
        return;

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
		surface->scanline.begin[i] = INT16_MAX;
		surface->scanline.end[i] = INT16_MIN;
	}
    
    for(int i = 0;i < n_vertex;i++){
        Vec3 p1 = coords_2d[i];
        Vec3 p2 = coords_2d[(i + 1) % n_vertex];
        Vec2 color_1 = color_coordinats_2d[i];
        Vec2 color_2 = color_coordinats_2d[(i + 1) % n_vertex];
        Vec2 texture_1 = texture_coordinats_2d[i];
        Vec2 texture_2 = texture_coordinats_2d[(i + 1) % n_vertex];
        setScanlineColorTexture(surface->scanline_color,surface->scanline_texture,surface->scanline_z,surface->scanline,texture_1,texture_2,color_1,color_2,p1,p2,surface->height);
    }

    Vec2 texture_begin = surface->scanline_texture.begin[x_min];
    Vec2 texture_end   = surface->scanline_texture.end[x_min];

    Vec2 texture_delta = vec2Sub(texture_end,texture_begin);
    texture_delta.x /= tMax(surface->scanline.end[x_min] - surface->scanline.begin[x_min],1);
    texture_delta.y /= tMax(surface->scanline.end[x_min] - surface->scanline.begin[x_min],1);

    int mipmap = tClamp(bitScanReverse((tAbs(texture_delta.x) + tAbs(texture_delta.y)) * texture->size) - 14,0,31);

    for(int x = x_min;x < x_max;x++){
        int begin = tClamp(surface->scanline.begin[x],0,surface->width);
        int end   = tClamp(surface->scanline.end[x],0,surface->width);

        Vec2 texture_delta = vec2Sub(surface->scanline_texture.end[x],surface->scanline_texture.begin[x]);
        texture_delta.x /= tMax(surface->scanline.end[x] - surface->scanline.begin[x],1);
        texture_delta.y /= tMax(surface->scanline.end[x] - surface->scanline.begin[x],1);

        int z_delta = (surface->scanline_z[x].end - surface->scanline_z[x].begin) / tMax(surface->scanline.end[x] - surface->scanline.begin[x],1);
        
        int length = surface->scanline.end[x] - surface->scanline.begin[x];
        int b_length_clip = surface->scanline.end[x] - begin;
        int e_length_clip = end - surface->scanline.begin[x];
        int begin_clip = length - b_length_clip;
        int end_clip = length - e_length_clip;

        int interpolate_begin = ((FIXED_ONE << 8) / tMax(length,1)) * begin_clip >> 8;
        int interpolate_end = FIXED_ONE - (((FIXED_ONE << 8) / tMax(length,1)) * end_clip >> 8);

        Span* span = spanListAdd(surface,x);
        
        span->begin = begin;
        span->end = end;
        span->color_begin = surface->scanline_color[x].begin;
        span->color_end = surface->scanline_color[x].end;
        span->texture_begin = surface->scanline_texture.begin[x]; 
        span->texture_end = surface->scanline_texture.end[x];
        span->mipmap = mipmap;
        span->texture = texture;
        span->z_begin = surface->scanline_z[x].begin;
        span->z_end = surface->scanline_z[x].end;
        span->interpolate_begin = interpolate_begin;
        span->interpolate_end = interpolate_end;
        span->lightmap = lightmap;
    }
}
