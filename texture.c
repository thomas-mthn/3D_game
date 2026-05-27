#include "texture.h"
#include "draw.h"
#include "draw_soft.h"
#include "main.h"
#include "memory.h"
#include "voxel_menu.h"
#include "texture_markov.h"
#include "opengl.h"

#ifdef __linux__

#include "linux/l_main.h"

#elif defined(__wasm__)

#include "wasm/wasm.h"

#elif defined(_MSC_VER)

#include "win32/w_main.h"

#endif

Vec2 g_texture_coordinates_fill[] = {
	{FIXED_ONE,FIXED_ONE},
	{0,FIXED_ONE},	
	{0,0},
	{FIXED_ONE,0},
};

Texture g_textures[] = {
	[TEXTURE_WALL] = {.size = 0x100},
	[TEXTURE_GRASS] = {.size = 0x400},
	[TEXTURE_STONE] = {.size = 0x400},
	[TEXTURE_PLANKS] = {.size = 0x100},
	[TEXTURE_UNDESTRUCTIBLE] = {.size = 0x100},
	[TEXTURE_STONE_BRICK] = {.size = 0x400},
	[TEXTURE_ENTITY] = {.size = 0x100},
	[TEXTURE_CHEST] = {.size = 0x100},
	[TEXTURE_PICKUP] = {.size = 0x40},
	[TEXTURE_BOLT] = {.size = 0x40},
	[TEXTURE_SMOKE] = {.size = 0x40},
	[TEXTURE_STONE2] = {.size = 0x400},
};

Cubemap g_skybox = {.size = 0x100};

#if !defined(__wasm__) && !defined(__linux__)
void textureResetGL(void){
	for(int i = 0;i < countof(g_textures);i++){
		Texture* texture = g_textures + i;
		deleteTextureGL(texture->gl_id);
		texture->gl_id = 0;
	}
}
#endif

void generateMipmaps(Texture* texture){
	int offset = 0;
	for(int size_i = texture->size;size_i;size_i >>= 1){
		int mipmap_offset = size_i * size_i;
		for(int i = 0;i < (size_i >> 1) * (size_i >> 1);i++){
			int x = i % (size_i >> 1);
			int y = i / (size_i >> 1);
			int index = offset + mipmap_offset + x * (size_i >> 1) + y;

			Vec3 color = {0};

			color = vec3Add(color,pixelColorToColor(texture->pixel_data[offset + (x * 2 + 0) * size_i + (y * 2 + 0)]));
			color = vec3Add(color,pixelColorToColor(texture->pixel_data[offset + (x * 2 + 0) * size_i + (y * 2 + 1)]));
			color = vec3Add(color,pixelColorToColor(texture->pixel_data[offset + (x * 2 + 1) * size_i + (y * 2 + 0)]));
			color = vec3Add(color,pixelColorToColor(texture->pixel_data[offset + (x * 2 + 1) * size_i + (y * 2 + 1)]));

			color = vec3Shr(color,2);

			int alpha = 0;
			alpha += (texture->pixel_data[offset + (x * 2 + 0) * size_i + (y * 2 + 0)] >> 24);
			alpha += (texture->pixel_data[offset + (x * 2 + 0) * size_i + (y * 2 + 1)] >> 24);
			alpha += (texture->pixel_data[offset + (x * 2 + 1) * size_i + (y * 2 + 0)] >> 24);
			alpha += (texture->pixel_data[offset + (x * 2 + 1) * size_i + (y * 2 + 1)] >> 24);
			alpha >>= 2;

			texture->pixel_data[index] = colorToPixelColor(color);
			texture->pixel_data[index] |= alpha << 24;
		}
		offset += mipmap_offset;
	}
}

int textureLookup(Texture* texture,real x,real y,int mipmap){
	int offset = 0;
	for(int i = 0;i < mipmap;i++)
		offset += texture->size * texture->size >> i * 2;
	x = tFract(x);
	y = tFract(y);
	int x_i = realShr(realToInt(x * texture->size),mipmap);
	int y_i = realShr(realToInt(y * texture->size),mipmap);
	return texture->pixel_data[offset + y_i * (texture->size >> mipmap) + x_i];
}

void textureAllocate(Texture* texture){
	texture->pixel_data = virtualAllocate(texture->size * texture->size * 2 * sizeof(*texture->pixel_data));
}

Texture textureCreate(int size){
	return (Texture){.size = size,.pixel_data = virtualAllocate(size * size * 2 * sizeof(int))};
}

void textureDestroy(Texture texture){
	deleteTextureGL(texture.gl_id);
    virtualFree(texture.pixel_data,texture.size * texture.size * 2 * sizeof(int));
}

static Texture textureDiskLoad(char* path){
    Texture texture;
#ifdef __linux__
    //texture = (Texture){0};
    texture = linuxLoadImage(path);
#elif defined(_MSC_VER)
    texture = win32LoadImage(path);
#elif defined(__wasm__)
    texture = wasmLoadImage(stringMake(path));
    //texture = (Texture){0};
#endif
    if(texture.pixel_data)
        return texture;
    //fallback code

    int texture_size = 0x20;
    
    int* pixel_data = virtualAllocate((texture_size * texture_size) * 2 * sizeof(*pixel_data));
    for(int i = 0;i < texture_size * texture_size;i++){
        int x = i / texture_size;
        int y = i % texture_size;
        pixel_data[i] = (x ^ y) & 1 ? 0xFF00FF : 0x000000;
    }
    return (Texture){.pixel_data = pixel_data,.size = texture_size};
}

static void textureGenerate(char* path,int size,TextureType type){
    Texture texture = textureDiskLoad(path);
    
	generateMipmaps(&texture);

    if(!g_options.fast_startup){
        markovInit();
        markovTextureTrain(texture);
        g_textures[type] = textureCreate(size);
        markovTextureGenerate(g_textures[type],texture);
        markovFree();
    }
    else{
        g_textures[type] = texture;
    }
}

#include "console.h"

int cubemapColorGetBilinear(Cubemap* cubemap,Vec3 direction){
    real abs_x = tAbs(direction.x);
    real abs_y = tAbs(direction.y);
    real abs_z = tAbs(direction.z);

    int side;
    real major;
    real u, v;

    if(abs_x >= abs_y && abs_x >= abs_z){
        side = (direction.a[0] >= 0) ? SIDE_YZ_UP : SIDE_YZ_DOWN; 
        major = direction.a[0];

        u = direction.a[0] >= 0 ? direction.a[1] : -direction.a[1];
        v = direction.a[0] >= 0 ? direction.a[2] : -direction.a[2];
    }
    else if(abs_y >= abs_x && abs_y >= abs_z){
        side = (direction.a[1] >= 0) ? SIDE_XZ_UP : SIDE_XZ_DOWN;
        major = direction.a[1];
        
        u = direction.a[1] >= 0 ? direction.a[0] : -direction.a[0];
        v = direction.a[1] >= 0 ? direction.a[2] : -direction.a[2];
    }
    else{
        side = (direction.a[2] >= 0) ? SIDE_XY_UP : SIDE_XY_DOWN;
        major = direction.a[2];
        
        u = direction.a[2] >= 0 ? direction.a[0] : -direction.a[0];
        v = direction.a[2] >= 0 ? direction.a[1] : -direction.a[1];
    }
    
    if(tAbs(major) < REAL_EPSILON)
        major = major < 0 ? -REAL_EPSILON : REAL_EPSILON;

    real inv_major = tReciprocal(major);  
    u = realMulR(u,inv_major);
    v = realMulR(v,inv_major);
    
    int x = realToInt(realMulR(u + FIXED_ONE,intToReal(cubemap->size)) + FIXED_ONE) / 2;
    int y = realToInt(realMulR(v + FIXED_ONE,intToReal(cubemap->size)) + FIXED_ONE) / 2;

    x -= 1;
    y -= 1;

    Vec2i crd[] = {
        {y,x},
        {y,(x + 1) % cubemap->size},
        {(y + 1) % cubemap->size,(x + 1) % cubemap->size},
        {(y + 1) % cubemap->size,x},
    };

    int sides[] = {
        side,
        side,
        side,
        side,
    };
#if 0
    if(side != 4 && side != 5)
        return 0;
#endif
    if(y == -1){
        crd[0].x = 0;
        crd[1].x = 0;
    }
    if(x == -1){
        crd[0].y = 0;
        crd[3].y = 0;
    }
    
    if(y == -1){
        if(side == 0){
            sides[0] = 2;
            sides[1] = 2;

            crd[0].x = cubemap->size - 1;
            crd[1].x = cubemap->size - 1;
        }
        else if(side == 1){
            sides[0] = 1;
            sides[1] = 1;
        }
        else if(side == 2){
            sides[0] = 5;
            sides[1] = 5;
        }
        else if(side == 3){
            sides[0] = 5;
            sides[1] = 5;
        }
        else if(side == 4){
            sides[0] = 3;
            sides[1] = 3;
            
            crd[0].x = cubemap->size - 1;
            crd[1].x = cubemap->size - 1;
        }
        else if(side == 5){
            sides[0] = 3;
            sides[1] = 3;
        }
    }
    else if(x == -1){
        if(side == 0){
            sides[0] = 3;
            sides[3] = 3;

            crd[0].y = cubemap->size - 1;
            crd[3].y = cubemap->size - 1;
        }
        else if(side == 1){
            sides[0] = 3;
            sides[3] = 3;
        }
        else if(side == 2){
            sides[0] = 1;
            sides[3] = 1;

            crd[0].y = cubemap->size - 1;
            crd[3].y = cubemap->size - 1;
        }
        else if(side == 3){
            sides[0] = 1;
            sides[3] = 1;
        }
        else if(side == 4){
            sides[0] = 1;
            sides[3] = 1;
#if 0
            crd[0].y = cubemap->size - 1;
            crd[3].y = cubemap->size - 1;
#endif
        }
        else if(side == 5){
            sides[0] = 1;
            sides[3] = 1;
        }
    }
    else if(x == cubemap->size - 1){
        if(side == 0){
            sides[1] = 2;
            sides[2] = 2;

            crd[1].y = cubemap->size - 1;
            crd[2].y = cubemap->size - 1;
        }
        else if(side == 2){
            sides[1] = 0;
            sides[2] = 0;

            crd[1].y = cubemap->size - 1;
            crd[2].y = cubemap->size - 1;
        }
        else if(side == 1){
            sides[1] = 2;
            sides[2] = 2;
        }
        else if(side == 3){
            sides[1] = 0;
            sides[2] = 0;
        }
        else if(side == 5){
            sides[1] = 3;
            sides[2] = 3;

            int temp = crd[1].x;
            crd[1].x = crd[1].y;
            crd[1].y = temp;

            temp = crd[2].x;
            crd[2].x = crd[2].y;
            crd[2].y = temp;
        }
        else{
            sides[1] = 0;
            sides[2] = 0;
            
            int temp = crd[1].x;
            crd[1].x = crd[1].y;
            crd[1].y = temp;

            temp = crd[2].x;
            crd[2].x = crd[2].y;
            crd[2].y = temp;
            
            crd[1].y = cubemap->size - 1;
            crd[2].y = cubemap->size - 1;
        }
    }
    else if(y == cubemap->size - 1){
        if(side == 0){
            sides[3] = 4;
            sides[2] = 4;
            crd[3].x = cubemap->size - 1;
            crd[2].x = cubemap->size - 1;
        }
        else if(side == 5){
            sides[3] = 2;
            sides[2] = 2;
        }
        else if(side == 4){
            sides[3] = 2;
            sides[2] = 2;
            crd[3].x = cubemap->size - 1;
            crd[2].x = cubemap->size - 1;
        }
        else if(side == 3){
            sides[3] = 4;
            sides[2] = 4;
        }
        else if(side == 1){
            sides[3] = 4;
            sides[2] = 4;
#if 0
            crd[3].x = cubemap->size - 1;
            crd[2].x = cubemap->size - 1;
#endif
        }
        else{
            sides[3] = 4;
            sides[2] = 4;
            crd[3].x = cubemap->size - 1;
            crd[2].x = cubemap->size - 1;
        }
    }

    Vec3 ll = pixelColorToColor(cubemap->textures[sides[0]].pixel_data[crd[0].x * cubemap->size + crd[0].y]);
    Vec3 lh = pixelColorToColor(cubemap->textures[sides[1]].pixel_data[crd[1].x * cubemap->size + crd[1].y]);
    Vec3 hh = pixelColorToColor(cubemap->textures[sides[2]].pixel_data[crd[2].x * cubemap->size + crd[2].y]);
    Vec3 hl = pixelColorToColor(cubemap->textures[sides[3]].pixel_data[crd[3].x * cubemap->size + crd[3].y]);

    Vec3 uv = {
        tFractU(tAbs(u + FIXED_ONE) / 2 * cubemap->size + FIXED_ONE / 2),
        tFractU(tAbs(v + FIXED_ONE) / 2 * cubemap->size + FIXED_ONE / 2)
    };

    Vec3 lx1 = vec3Mix(ll,lh,uv.x);
    Vec3 lx2 = vec3Mix(hl,hh,uv.x);

    Vec3 luminance = vec3Mix(lx1,lx2,uv.y);
    if(g_test_bool)
        return colorToPixelColor((Vec3){0,uv.x,uv.y});
    
    return colorToPixelColor(luminance);
}

int cubemapColorGet(Cubemap* cubemap,Vec3 direction){
    real abs_x = tAbs(direction.x);
    real abs_y = tAbs(direction.y);
    real abs_z = tAbs(direction.z);

    int side;
    real major;
    real u, v;

    if(abs_x >= abs_y && abs_x >= abs_z){
        side = (direction.a[0] >= 0) ? SIDE_YZ_UP : SIDE_YZ_DOWN; 
        major = direction.a[0];

        u = direction.a[0] >= 0 ? direction.a[1] : -direction.a[1];
        v = direction.a[0] >= 0 ? direction.a[2] : -direction.a[2];
    }
    else if(abs_y >= abs_x && abs_y >= abs_z){
        side = (direction.a[1] >= 0) ? SIDE_XZ_UP : SIDE_XZ_DOWN;
        major = direction.a[1];
        
        u = direction.a[1] >= 0 ? direction.a[0] : -direction.a[0];
        v = direction.a[1] >= 0 ? direction.a[2] : -direction.a[2];
    }
    else{
        side = (direction.a[2] >= 0) ? SIDE_XY_UP : SIDE_XY_DOWN;
        major = direction.a[2];
        
        u = direction.a[2] >= 0 ? direction.a[0] : -direction.a[0];
        v = direction.a[2] >= 0 ? direction.a[1] : -direction.a[1];
    }
    
    if(tAbs(major) < REAL_EPSILON)
        major = major < 0 ? -REAL_EPSILON : REAL_EPSILON;

    real inv_major = tReciprocal(major);  
    u = realMulR(u,inv_major);
    v = realMulR(v,inv_major);

    int x = realToInt(realMulR(u + FIXED_ONE,intToReal(cubemap->size))) / 2;
    int y = realToInt(realMulR(v + FIXED_ONE,intToReal(cubemap->size))) / 2;
    
    return cubemap->textures[side].pixel_data[y * cubemap->size + x];
}

Vec3 cubemapDirectionGet(Cubemap* cubemap,Side side,int x,int y){
    real r_x = intToReal(x) * 2 / cubemap->size - FIXED_ONE;
    real r_y = intToReal(y) * 2 / cubemap->size - FIXED_ONE;
    
    Vec2i axis = g_axis_table[side];
    
    Vec3 ray_direction;
    ray_direction.a[axis.y] = r_x;
    ray_direction.a[axis.x] = r_y;
    ray_direction.a[side >> 1] = side & 1 ? -FIXED_ONE : FIXED_ONE;
    return ray_direction;
}

static unsigned hash(unsigned x){
    x += x << 10;
    x ^= x >>  6;
    x += x <<  3;
    x ^= x >> 11;
    x += x << 15;
    return x;
}

void texturesGenerate(void){
	for(int i = 0;i < countof(g_textures);i++)
		textureAllocate(g_textures + i);
	textureAllocate(&g_spinning_staff);

	Texture* texture;
	texture = g_textures + TEXTURE_WALL;

	DrawSurface surface;
	surface = (DrawSurface){
		.data = texture->pixel_data,
		.width = texture->size,
		.height = texture->size,
		.backend = RENDER_BACKEND_SOFTWARE,
	};
	surfaceInit(&surface);
	drawSquare(&surface,-FIXED_ONE,-FIXED_ONE,FIXED_ONE * 2,pixelColorToColor(0xF09030));
	drawString(&surface,-FIXED_ONE + 0x4000,-FIXED_ONE + 0x4000,(String)STRING_LITERAL("wall"),0x1000,COLOR_WHITE);

	texture = g_textures + TEXTURE_GRASS;
	softSurfaceDestroyMeta(&surface);
	Texture grass_markov = {.size = 256};
	textureAllocate(&grass_markov);

	for(int i = 0;i < grass_markov.size * grass_markov.size;i++){ 
        real x = i / grass_markov.size * FIXED_ONE * 2 / grass_markov.size - FIXED_ONE;
        real y = i % grass_markov.size * FIXED_ONE * 2 / grass_markov.size - FIXED_ONE;
        Vec3 color_dirt = pixelColorToColor(0x30A830);
        Vec3 color_grass = pixelColorToColor(0x40C040);

        int w = tClamp(tCos((realMulR(x,x) + realMulR(y,y)) * 2) + FIXED_ONE,0,FIXED_ONE);
        Vec3 luminance = vec3Mix(color_dirt,color_grass,w);
        grass_markov.pixel_data[i] = colorToPixelColor(luminance);
    }

    textureGenerate("img/grass.bmp",1024,TEXTURE_GRASS);
    softSurfaceDestroyMeta(&surface);
	texture = g_textures + TEXTURE_STONE;
    
	for(int i = 0;i < texture->size * texture->size;i++){
        int cell_size = 0x20;
        
        int x = i / texture->size;
        int y = i % texture->size;

        int pos_x = x / cell_size;
        int pos_y = y / cell_size;

        Vec2 uv = {
            intToReal(x % cell_size) / cell_size,
            intToReal(y % cell_size) / cell_size
        };

        real min_distance = REAL_MAX;
        int min_index;

        for(int j = 9;j--;){
            int cell_x = j / 3 - 1;
            int cell_y = j % 3 - 1;

            int cx = (pos_x + cell_x) % (texture->size / cell_size);
            int cy = (pos_y + cell_y) % (texture->size / cell_size);

            int c_x = cx + 0x400;
            int c_y = cy + 0x4000;
            
            Vec2 cell = {
                intToReal(cell_x) + intToReal(tHash(tHash(c_x) ^ c_y) % 0x10) / 0x10,
                intToReal(cell_y) + intToReal(tHash(tHash(c_y) ^ c_x) % 0x10) / 0x10,
            };
 
            int index = cx * (texture->size / cell_size) + cy;
            real distance = vec2Distance(cell,uv);

            if(min_distance > distance){
                min_distance = distance;
                min_index = index; 
            }
        }
        
		int r = tHash(min_index) & 0x7F | 0x80;
		texture->pixel_data[i] = r | r << 8 | r << 16;
	}

	texture = g_textures + TEXTURE_UNDESTRUCTIBLE;

	for(int i = 0;i < texture->size * texture->size;i++){
		int color;
		if((i / texture->size + i % texture->size) % 0x10 < 0x0A)
			color = 0xA0A020;
		else
			color = 0x606060;
		texture->pixel_data[i] = color;
	}

	texture = g_textures + TEXTURE_ENTITY;
	for(int i = 0;i < texture->size * texture->size;i++){
		int color = 0xFF000000;
		
		texture->pixel_data[i] = color;
	}

	surface = (DrawSurface){
		.data = texture->pixel_data,
		.width = texture->size,
		.height = texture->size,
	};
	surfaceInit(&surface);
	drawCircle(&surface,0,0,FIXED_ONE,pixelColorToColor(0x606060));

	drawCircle(&surface,-0x8000,0x4000,0x4000,pixelColorToColor(0x20FF20));
	drawCircle(&surface,0x8000,0x4000,0x4000,pixelColorToColor(0x20FF20));
	drawCircle(&surface,-0x8000,0x4000,0x2000,pixelColorToColor(0xA0A0A0));
	drawCircle(&surface,0x8000,0x4000,0x2000,pixelColorToColor(0xA0A0A0));
	drawSegment(&surface,-0x8000,-0x6000,0x8000,-0x6000,0x800,pixelColorToColor(0x20FF20));
	softSurfaceDestroyMeta(&surface);
	texture = g_textures + TEXTURE_PLANKS;
	surface = (DrawSurface){
		.data = texture->pixel_data,
		.width = texture->size,
		.height = texture->size,
	};
		
	for(int i = 0;i < texture->size * texture->size;i++){
		real x = i / texture->size * FIXED_ONE / 2;
		real y = i % texture->size * FIXED_ONE / 16;

		real v00 = tFract(tHash(tHash(realToInt(x)) ^ realToInt(y)));
		real v01 = tFract(tHash(tHash(realToInt(x)) ^ realToInt(y) + 1));

		real v10 = tFract(tHash(tHash(realToInt(x) + 1) ^ realToInt(y)));
		real v11 = tFract(tHash(tHash(realToInt(x) + 1) ^ realToInt(y) + 1));

		real luminance = bilinearScalar((Vec2){x,y},(real[]){v00,v01,v10,v11}) / 2 + FIXED_ONE / 2;

		Vec3 color = vec3MulS((Vec3){(FIXED_ONE / 3 + FIXED_ONE / 12) * 16,FIXED_ONE / 4 * 16,FIXED_ONE / 12 * 16},luminance);
		color = vec3Add(color,vec3Single(realRandom(FIXED_ONE / 2) - FIXED_ONE / 4));
		texture->pixel_data[i] = colorToPixelColor(color);
	}

	for(int i = 0;i < texture->size;i++){
		for(int j = 0;j < 5;j++){
			for(int k = 0;k < 8;k++){
				Vec3 color = pixelColorToColor(texture->pixel_data[((texture->size / 8) * k + j) * texture->size + i]);

				color = vec3MulS(color,FIXED_ONE / 8 * tAbs(j - 2) + FIXED_ONE / 2);

				texture->pixel_data[((texture->size / 8) * k + j) * texture->size + i] = colorToPixelColor(color);
			}
		}
	}

	real random_x = realRandom(FIXED_ONE * 0x100);
	real random_y = realRandom(FIXED_ONE * 0x100);
    
	for(Side j = 0;j < SIDE_COUNT;j++){
		Vec2i axis = g_axis_table[j];
		texture = g_skybox.textures + j;
        *texture = textureCreate(g_skybox.size);
		surface = (DrawSurface){
			.data = texture->pixel_data,
			.width = texture->size,
			.height = texture->size,
		};
		
		for(int i = 0;i < texture->size * texture->size;i++){
			Vec3 color = {0};
			int x = i / texture->size;
			int y = i % texture->size;
            
			Vec3 ray_direction = vec3Normalize(cubemapDirectionGet(&g_skybox,j,x,y));

			real distance = rayPlaneIntersection((Vec3){0},ray_direction,(Plane){.normal = {0,0,FIXED_ONE},.distance = -FIXED_ONE});
            
			Vec3 position = vec3MulS(ray_direction,distance);

            Vec3 lum_up   = {REAL_UNIT * 0x10,REAL_UNIT * 0x40,REAL_UNIT * 0x80};
            Vec3 lum_down = {REAL_UNIT * 0x40,REAL_UNIT * 0x20,REAL_UNIT * 0x08};

            Vec3 luminance = vec3Mix(lum_up,lum_down,tSqrt(tAbs(ray_direction.z)));

            if(g_world.skylight){
                Vec3 skylight_direction = getLookDirection(g_world.skylight_angle);
                real intensity = realMulR(tReciprocal(vec3Distance(ray_direction,skylight_direction)),REAL_UNIT * 4);
                Vec3 sky_lum = vec3MulS(g_world.skylight_luminance,intensity);
                luminance = vec3Add(luminance,sky_lum);
            }
            
            texture->pixel_data[i] = colorToPixelColor(luminance);
		}
	}
    
    textureGenerate("img/planks.bmp",1024,TEXTURE_PLANKS);
	softSurfaceDestroyMeta(&surface);
	texture = g_textures + TEXTURE_STONE_BRICK;

    int brick_size = 4;
    
	for(int i = 0;i < texture->size * texture->size;i++){
        int x = i / texture->size;
        int y = i % texture->size;

        int cell_x = x / (texture->size / (brick_size * 2));
        int cell_y = y / (texture->size / brick_size);

        if(cell_x & 1)
            cell_y = (y + (texture->size / (brick_size * 2))) / (texture->size / brick_size) % brick_size;
        
        unsigned cell_id = hash(hash(cell_x + 1) ^ cell_y);

        Vec3 color_b = {REAL_UNIT * 0xB0,REAL_UNIT * 0xB0,REAL_UNIT * 0xB0};
        Vec3 color_e = {REAL_UNIT * 0x100,REAL_UNIT * 0x100,REAL_UNIT * 0x100};

        Vec3 color = vec3Mix(color_b,color_e,(real)(cell_id & 0xFFFF) / 0x10000);

		texture->pixel_data[i] = colorToPixelColor(color);
	}
#if 1
	for(int i = 0;i < texture->size;i++){
		for(int j = 0;j < brick_size * 2 + 1;j++){
			for(int k = 0;k < brick_size * 2;k++){
				Vec3 color = pixelColorToColor(texture->pixel_data[((texture->size / (brick_size * 2)) * k + j) * texture->size + i]);

				color = vec3MulS(color,FIXED_ONE / 8 * tAbs(j - 2) + FIXED_ONE / 2);

				texture->pixel_data[((texture->size / (brick_size * 2)) * k + j) * texture->size + i] = colorToPixelColor(color);
			}
		}
	}
#if 1
	for(int i = 0;i < texture->size / (brick_size * 2);i++){
		for(int j = 0;j < brick_size * 2 + 1;j++){
			for(int k = 0;k < brick_size;k++){
				for(int l = 0;l < brick_size * 2;l++){
					int offset = l % brick_size * texture->size / (brick_size * 2);

					Vec3 color = pixelColorToColor(texture->pixel_data[texture->size * (i + l * texture->size / (brick_size * 2)) + (texture->size / brick_size * k + j) + offset]);

					color = vec3MulS(color,FIXED_ONE / 8 * tAbs(j - brick_size) + FIXED_ONE / brick_size);

					texture->pixel_data[texture->size * (i + l * texture->size / (brick_size * 2)) + (texture->size / brick_size * k + j) + offset] = colorToPixelColor(color);
				}
			}
		}
	}
#endif
#endif
	texture = g_textures + TEXTURE_CHEST;
	surface = (DrawSurface){
		.data = texture->pixel_data,
		.width = texture->size,
		.height = texture->size,
	};
	surfaceInit(&surface);
	drawSquare(&surface,-FIXED_ONE,-FIXED_ONE,FIXED_ONE * 2,pixelColorToColor(0xC0C0C0));
	drawRectangle(&surface,FIXED_ONE / 3,-FIXED_ONE,0x2000,FIXED_ONE * 2,pixelColorToColor(0x808080));
	drawRectangle(&surface,FIXED_ONE / 3 - 0x3000,-0x3000,0xA800,FIXED_ONE / 3 + 0xA00,pixelColorToColor(0xF0F0A0));
	drawRectangle(&surface,FIXED_ONE / 3 - 0x2800,-0x2800,0x9800,FIXED_ONE / 3 - 0x400,pixelColorToColor(0x505050));
	
	drawRectangle(&surface,FIXED_ONE - 0x9800,-FIXED_ONE + 0x2000,0x9800,FIXED_ONE / 3 - 0x400,pixelColorToColor(0xD07030));
	drawRectangle(&surface,FIXED_ONE - 0x9800,FIXED_ONE - (FIXED_ONE / 3 - 0x400) - 0x2000,0x9800,FIXED_ONE / 3 - 0x400,pixelColorToColor(0xD07030));
	softSurfaceDestroyMeta(&surface);
	texture = g_textures + TEXTURE_PICKUP;
	surface = (DrawSurface){
		.data = texture->pixel_data,
		.width = texture->size,
		.height = texture->size,
	};
	surfaceInit(&surface);
	for(int i = 0;i < texture->size * texture->size;i++)
		texture->pixel_data[i] = 0xFF000000;
	drawFrame(&surface,0,0,FIXED_ONE,FIXED_ONE,pixelColorToColor(0xC0C0C0),0x2000);
	drawString(&surface,FIXED_ONE / 2 - FIXED_ONE / 8,FIXED_ONE / 2 - FIXED_ONE / 4,(String)STRING_LITERAL("x2"),0x1000,COLOR_WHITE);
	softSurfaceDestroyMeta(&surface);
	texture = g_textures + TEXTURE_BOLT;
	surface = (DrawSurface){
		.data = texture->pixel_data,
		.width = texture->size,
		.height = texture->size,
	};
	surfaceInit(&surface);
	for(int i = 0;i < texture->size * texture->size;i++)
		texture->pixel_data[i] = 0xFF000000;
	drawCircle(&surface,0,0,FIXED_ONE,pixelColorToColor(0xF08080));
 	softSurfaceDestroyMeta(&surface);
	texture = g_textures + TEXTURE_SMOKE;
	surface = (DrawSurface){
		.data = texture->pixel_data,
		.width = texture->size,
		.height = texture->size,
	};
	for(int i = 0;i < texture->size * texture->size;i++){
		if(tRndChance(2))
			texture->pixel_data[i] = 0xFF000000;
		else
			texture->pixel_data[i] = 0x00FFFFFF;
	}
	texture = g_textures + TEXTURE_STONE2;
	Texture circle = {.size = 0x80};
	textureAllocate(&circle);
	for(int i = 0;i < circle.size * circle.size;i++){ 
        real x = i / circle.size * FIXED_ONE * 2 / circle.size - FIXED_ONE;
        real y = i % circle.size * FIXED_ONE * 2 / circle.size - FIXED_ONE;
        real luminance = tClamp(FIXED_ONE - (realMulR(x,x) + realMulR(y,y)),FIXED_ONE / 2,FIXED_ONE);
        circle.pixel_data[i] = colorToPixelColor(vec3Shl(vec3Single(luminance),4));
    }
	generateMipmaps(&circle);
	
	markovInit();
	markovTextureTrain(circle);
	markovTextureGenerate(*texture,circle);
	markovFree();

	for(int i = 0;i < countof(g_textures);i++)
		generateMipmaps(g_textures + i);
}
