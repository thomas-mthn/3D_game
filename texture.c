#include "texture.h"
#include "draw.h"
#include "main.h"
#include "memory.h"
#include "voxel_menu.h"
#include "texture_markov.h"

#ifndef __wasm__
#include "win32/w_draw_opengl.h"
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
	[TEXTURE_STONE] = {.size = 0x100},
	[TEXTURE_PLANKS] = {.size = 0x100},
	[TEXTURE_UNDESTRUCTIBLE] = {.size = 0x100},
	[TEXTURE_STONE_BRICK] = {.size = 0x400},
	[TEXTURE_ENTITY] = {.size = 0x100},
	[TEXTURE_CHEST] = {.size = 0x100},
	[TEXTURE_PICKUP] = {.size = 0x40},
	[TEXTURE_BOLT] = {.size = 0x40},
	[TEXTURE_SMOKE] = {.size = 0x40},
	[TEXTURE_SKYBOX_YZ_UP] = {.size = 0x100},
	[TEXTURE_SKYBOX_YZ_DOWN] = {.size = 0x100},
	[TEXTURE_SKYBOX_XZ_UP] = {.size = 0x100},
	[TEXTURE_SKYBOX_XZ_DOWN] = {.size = 0x100},
	[TEXTURE_SKYBOX_XY_UP] = {.size = 0x100},
	[TEXTURE_SKYBOX_XY_DOWN] = {.size = 0x100},
	[TEXTURE_SKYBOX_XY_UP] = {.size = 0x100},
	[TEXTURE_STONE2] = {.size = 0x400},
};

TextureType g_skybox_textures[] = {
	TEXTURE_SKYBOX_YZ_UP,
	TEXTURE_SKYBOX_YZ_DOWN,
	TEXTURE_SKYBOX_XZ_UP,
	TEXTURE_SKYBOX_XZ_DOWN,
	TEXTURE_SKYBOX_XY_UP,
	TEXTURE_SKYBOX_XY_DOWN,
};

#ifndef __wasm__
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

			vec3Add(&color,pixelColorToColor(texture->pixel_data[offset + (x * 2 + 0) * size_i + (y * 2 + 0)]));
			vec3Add(&color,pixelColorToColor(texture->pixel_data[offset + (x * 2 + 0) * size_i + (y * 2 + 1)]));
			vec3Add(&color,pixelColorToColor(texture->pixel_data[offset + (x * 2 + 1) * size_i + (y * 2 + 0)]));
			vec3Add(&color,pixelColorToColor(texture->pixel_data[offset + (x * 2 + 1) * size_i + (y * 2 + 1)]));

			vec3Shr(&color,2);

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

int textureLookup(Texture* texture,int x,int y,int mipmap){
	int offset = 0;
	for(int i = 0;i < mipmap;i++)
		offset += texture->size * texture->size >> i * 2;
	x = fixedFract(x);
	y = fixedFract(y);
	x = x * texture->size / FIXED_ONE >> mipmap;
	y = y * texture->size / FIXED_ONE >> mipmap;
	return texture->pixel_data[offset + y * (texture->size >> mipmap) + x];
}

static void textureAllocate(Texture* texture){
	texture->pixel_data = tMallocZero(texture->size * texture->size * 2 * sizeof(*texture->pixel_data));
}

Texture textureCreate(int size){
	return (Texture){.size = size,.pixel_data = tMallocZero(size * size * 2 * sizeof(int))};
}

void textureDestroy(Texture texture){
#ifndef __wasm__
	deleteTextureGL(texture.gl_id);
#endif
	tFree(texture.pixel_data);
}
#include "win32/w_console.h"
void texturesGenerate(void){
	int size = 0x100;

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
	drawSquare(surface,-FIXED_ONE,-FIXED_ONE,FIXED_ONE * 2,pixelColorToColor(0x3090F0));
	drawString(surface,-FIXED_ONE + 0x2000,-FIXED_ONE + 0x2000,"wall",STRING_LENGTH_CSTRING,0x2000,pixelColorToColor(0xFFFFFF),0x800);

	texture = g_textures + TEXTURE_GRASS;

	Texture grass_markov = {.size = 256};
	textureAllocate(&grass_markov);

	for(int i = 0;i < grass_markov.size * grass_markov.size;i++){ 
        int x = i / grass_markov.size * FIXED_ONE * 2 / grass_markov.size - FIXED_ONE;
        int y = i % grass_markov.size * FIXED_ONE * 2 / grass_markov.size - FIXED_ONE;
        Vec3 color_dirt = pixelColorToColor(0x30A830);
        Vec3 color_grass = pixelColorToColor(0x40C040);

        int w = tClamp(tCos((fixedMulR(x,x) + fixedMulR(y,y)) * 2) + FIXED_ONE,0,FIXED_ONE);
        Vec3 luminance = vec3Mix(color_dirt,color_grass,w);
        grass_markov.pixel_data[i] = colorToPixelColor(luminance);
    }

	Texture grass = {.size = 128,.pixel_data = win32LoadImage("img/grass.bmp")};

	generateMipmaps(&grass);
	
	markovInit();
	markovTextureTrain(grass);
	markovTextureGenerate(g_textures[TEXTURE_GRASS],grass_markov);
	markovFree();
	

	texture = g_textures + TEXTURE_STONE;

	for(int i = 0;i < texture->size * texture->size;i++){
		int r = tRnd() & 0x7F | 0x80;
		texture->pixel_data[i] = r | r << 8 | r << 16;
	}

	texture = g_textures + TEXTURE_UNDESTRUCTIBLE;

	for(int i = 0;i < texture->size * texture->size;i++){
		int color;
		if((i / texture->size + i % texture->size) % 0x10 < 0x0A)
			color = 0x20A0A0;
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
	drawCircle(surface,0,0,FIXED_ONE,pixelColorToColor(0x606060));

	drawCircle(surface,-0x8000,0x4000,0x4000,pixelColorToColor(0x20FF20));
	drawCircle(surface,0x8000,0x4000,0x4000,pixelColorToColor(0x20FF20));
	drawCircle(surface,-0x8000,0x4000,0x2000,pixelColorToColor(0xA0A0A0));
	drawCircle(surface,0x8000,0x4000,0x2000,pixelColorToColor(0xA0A0A0));
	drawSegment(surface,-0x8000,-0x6000,0x8000,-0x6000,0x800,pixelColorToColor(0x20FF20));

	texture = g_textures + TEXTURE_PLANKS;
	surface = (DrawSurface){
		.data = texture->pixel_data,
		.width = texture->size,
		.height = texture->size,
	};
		
	for(int i = 0;i < texture->size * texture->size;i++){
		int x = i / texture->size * FIXED_ONE / 2;
		int y = i % texture->size * FIXED_ONE / 16;

		int v00 = tHash(tHash(fixedToInt(x)) ^ fixedToInt(y)) % FIXED_ONE;
		int v01 = tHash(tHash(fixedToInt(x)) ^ fixedToInt(y) + 1) % FIXED_ONE;

		int v10 = tHash(tHash(fixedToInt(x) + 1) ^ fixedToInt(y)) % FIXED_ONE;
		int v11 = tHash(tHash(fixedToInt(x) + 1) ^ fixedToInt(y) + 1) % FIXED_ONE;

		int luminance = bilinearScalar((Vec2){x,y},(int[]){v00,v01,v10,v11}) / 2 + FIXED_ONE / 2;

		Vec3 color = vec3MulRS((Vec3){FIXED_ONE / 3 + FIXED_ONE / 12 << 4,FIXED_ONE / 4 << 4,FIXED_ONE / 12 << 4},luminance);
		vec3Add(&color,vec3Single(tRnd() % (FIXED_ONE / 2) - FIXED_ONE / 4));
		texture->pixel_data[i] = colorToPixelColor(color);
	}

	for(int i = 0;i < texture->size;i++){
		for(int j = 0;j < 5;j++){
			for(int k = 0;k < 8;k++){
				Vec3 color = pixelColorToColor(texture->pixel_data[((texture->size / 8) * k + j) * texture->size + i]);

				vec3MulS(&color,FIXED_ONE / 8 * tAbs(j - 2) + FIXED_ONE / 2);

				texture->pixel_data[((texture->size / 8) * k + j) * texture->size + i] = colorToPixelColor(color);
			}
		}
	}

	int random_x = tRnd() % (FIXED_ONE * 0x100);
	int random_y = tRnd() % (FIXED_ONE * 0x100);

	for(int j = 0;j < countof(g_skybox_textures);j++){
		Vec2 axis = g_axis_table[j];
		texture = g_textures + g_skybox_textures[j];
		surface = (DrawSurface){
			.data = texture->pixel_data,
			.width = texture->size,
			.height = texture->size,
		};
		
		for(int i = 0;i < texture->size * texture->size;i++){
			Vec3 color = {0};
			int x = i / texture->size * FIXED_ONE * 2 / texture->size - FIXED_ONE;
			int y = i % texture->size * FIXED_ONE * 2 / texture->size - FIXED_ONE;

			Vec3 ray_direction;
			((int*)&ray_direction)[axis.y] = x;
			((int*)&ray_direction)[axis.x] = y;
			((int*)&ray_direction)[j >> 1] = j & 1 ? -FIXED_ONE : FIXED_ONE;
			ray_direction = vec3Normalize(ray_direction);

			int distance = rayPlaneIntersection((Vec3){0},ray_direction,(Plane){.normal = {0,0,FIXED_ONE},.distance = -FIXED_ONE});
			if(distance < 0 || distance > FIXED_ONE * 8){
				texture->pixel_data[i] = colorToPixelColor(vec3Single(FIXED_ONE << 4));
				continue;
			}
			Vec3 position = vec3MulRS(ray_direction,distance);

			int luminance = 0;

			for(int j = 0;j < 4;j++){
				Vec3 position_copy = position;
				position_copy.x = fixedMulR(position_copy.x + random_x,(2 << j) * 0x10000);
				position_copy.y = fixedMulR(position_copy.y + random_y,(2 << j) * 0x10000);

				int v00 = tHash(tHash(fixedToInt(position_copy.x)) ^ fixedToInt(position_copy.y)) % FIXED_ONE;
				int v01 = tHash(tHash(fixedToInt(position_copy.x)) ^ fixedToInt(position_copy.y) + 1) % FIXED_ONE;

				int v10 = tHash(tHash(fixedToInt(position_copy.x) + 1) ^ fixedToInt(position_copy.y)) % FIXED_ONE;
				int v11 = tHash(tHash(fixedToInt(position_copy.x) + 1) ^ fixedToInt(position_copy.y) + 1) % FIXED_ONE;

				luminance += bilinearScalar((Vec2){position_copy.x,position_copy.y},(int[]){v00,v01,v10,v11});

				//luminance = FIXED_ONE - tClamp(luminance,0,FIXED_ONE / 2);

				//vec3Add(&color,vec3Single(tRnd() % (FIXED_ONE / 2) - FIXED_ONE / 4));
			}

			luminance >>= 2;

			luminance = FIXED_ONE - tClamp(luminance,FIXED_ONE / 2,FIXED_ONE);

			luminance <<= 4;

			texture->pixel_data[i] = colorToPixelColor(vec3Mix(vec3Single(luminance),vec3Single(FIXED_ONE << 4),distance / 8));
		}
	}
	Texture planks = {.size = 128,.pixel_data = win32LoadImage("img/planks.bmp")};
	print("planks");
	generateMipmaps(&planks);
	
	markovInit();
	markovTextureTrain(planks);
	markovTextureGenerate(g_textures[TEXTURE_PLANKS],g_textures[TEXTURE_PLANKS]);
	markovFree();
	
	texture = g_textures + TEXTURE_STONE_BRICK;
	surface = (DrawSurface){
		.data = texture->pixel_data,
		.width = texture->size,
		.height = texture->size,
	};

	for(int i = 0;i < texture->size * texture->size;i++){
		int r = tRnd() & 0x1F | 0xC0;
		texture->pixel_data[i] = r | r << 8 | r << 16;
	}

	for(int i = 0;i < texture->size;i++){
		for(int j = 0;j < 5;j++){
			for(int k = 0;k < 4;k++){
				Vec3 color = pixelColorToColor(texture->pixel_data[((texture->size / 4) * k + j) * texture->size + i]);

				vec3MulS(&color,FIXED_ONE / 8 * tAbs(j - 2) + FIXED_ONE / 2);

				texture->pixel_data[((texture->size / 4) * k + j) * texture->size + i] = colorToPixelColor(color);
			}
		}
	}

	for(int i = 0;i < texture->size / 4;i++){
		for(int j = 0;j < 5;j++){
			for(int k = 0;k < 2;k++){
				for(int l = 0;l < 4;l++){
					int offset = l % 2 * texture->size / 4;

					Vec3 color = pixelColorToColor(texture->pixel_data[texture->size * (i + l * texture->size / 4) + (texture->size / 2 * k + j) + offset]);

					vec3MulS(&color,FIXED_ONE / 8 * tAbs(j - 2) + FIXED_ONE / 2);

					texture->pixel_data[texture->size * (i + l * texture->size / 4) + (texture->size / 2 * k + j) + offset] = colorToPixelColor(color);
				}
			}
		}
	}

	Texture stone_brick = {.size = 256,.pixel_data = win32LoadImage("img/brick_alt2.bmp")};

	generateMipmaps(&stone_brick);

	markovInit();
	markovTextureTrain(stone_brick);
	markovTextureGenerate(g_textures[TEXTURE_STONE_BRICK],g_textures[TEXTURE_STONE_BRICK]);
	markovFree();

	texture = g_textures + TEXTURE_CHEST;
	surface = (DrawSurface){
		.data = texture->pixel_data,
		.width = texture->size,
		.height = texture->size,
	};
	drawSquare(surface,-FIXED_ONE,-FIXED_ONE,FIXED_ONE * 2,pixelColorToColor(0xC0C0C0));
	drawRectangle(surface,FIXED_ONE / 3,-FIXED_ONE,0x2000,FIXED_ONE * 2,pixelColorToColor(0x808080));
	drawRectangle(surface,FIXED_ONE / 3 - 0x3000,-0x3000,0xA800,FIXED_ONE / 3 + 0xA00,pixelColorToColor(0xA0F0F0));
	drawRectangle(surface,FIXED_ONE / 3 - 0x2800,-0x2800,0x9800,FIXED_ONE / 3 - 0x400,pixelColorToColor(0x505050));
	
	drawRectangle(surface,FIXED_ONE - 0x9800,-FIXED_ONE + 0x2000,0x9800,FIXED_ONE / 3 - 0x400,pixelColorToColor(0x3070D0));
	drawRectangle(surface,FIXED_ONE - 0x9800,FIXED_ONE - (FIXED_ONE / 3 - 0x400) - 0x2000,0x9800,FIXED_ONE / 3 - 0x400,pixelColorToColor(0x3070D0));

	texture = g_textures + TEXTURE_PICKUP;
	surface = (DrawSurface){
		.data = texture->pixel_data,
		.width = texture->size,
		.height = texture->size,
	};
	for(int i = 0;i < texture->size * texture->size;i++)
		texture->pixel_data[i] = 0xFF000000;
	drawFrame(surface,0,0,FIXED_ONE,FIXED_ONE,pixelColorToColor(0xC0C0C0),0x2000);
	drawString(surface,FIXED_ONE / 2 - FIXED_ONE / 8,FIXED_ONE / 2 - FIXED_ONE / 4,"x2",STRING_LENGTH_CSTRING,FIXED_ONE / 4,pixelColorToColor(0xFFFFFF),0x800);

	texture = g_textures + TEXTURE_BOLT;
	surface = (DrawSurface){
		.data = texture->pixel_data,
		.width = texture->size,
		.height = texture->size,
	};
	for(int i = 0;i < texture->size * texture->size;i++)
		texture->pixel_data[i] = 0xFF000000;
	drawCircle(surface,0,0,FIXED_ONE,pixelColorToColor(0xF08080));

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
        int x = i / circle.size * FIXED_ONE * 2 / circle.size - FIXED_ONE;
        int y = i % circle.size * FIXED_ONE * 2 / circle.size - FIXED_ONE;
        int luminance = tClamp(FIXED_ONE - (fixedMulR(x,x) + fixedMulR(y,y)),FIXED_ONE / 2,FIXED_ONE);
        circle.pixel_data[i] = colorToPixelColor((Vec3){luminance << 4,luminance << 4,luminance << 4});
    }
	generateMipmaps(&circle);
	
	markovInit();
	markovTextureTrain(circle);
	markovTextureGenerate(*texture,circle);
	markovFree();

	for(int i = 0;i < countof(g_textures);i++)
		generateMipmaps(g_textures + i);
}