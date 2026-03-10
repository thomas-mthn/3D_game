#ifndef TEXTURE_H
#define TEXTURE_H

#include "vec2.h"

enumeration(TextureType){
	TEXTURE_WALL,
	TEXTURE_GRASS,
	TEXTURE_STONE,
	TEXTURE_UNDESTRUCTIBLE,
	TEXTURE_STONE_BRICK,
	TEXTURE_ENTITY,
	TEXTURE_CHEST,
	TEXTURE_PICKUP,
	TEXTURE_BOLT,
	TEXTURE_PLANKS,
	TEXTURE_SMOKE,
	TEXTURE_SKYBOX_YZ_UP,
	TEXTURE_SKYBOX_YZ_DOWN,
	TEXTURE_SKYBOX_XZ_UP,
	TEXTURE_SKYBOX_XZ_DOWN,
	TEXTURE_SKYBOX_XY_UP,
	TEXTURE_SKYBOX_XY_DOWN,
	TEXTURE_STONE2,
};

structure(Texture){
	unsigned* pixel_data;
	int size;
	unsigned gl_id;
};

extern Texture g_textures[];
extern TextureType g_skybox_textures[];
extern Vec2 g_texture_coordinates_fill[];

void texturesGenerate(void);
int textureLookup(Texture* texture,int x,int y,int mipmap);
void generateMipmaps(Texture* texture);

Texture textureCreate(int size);
void textureDestroy(Texture texture);

#ifndef __wasm__
void textureResetGL(void);
#endif

#endif