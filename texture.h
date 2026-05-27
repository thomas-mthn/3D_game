#ifndef TEXTURE_H
#define TEXTURE_H

#include "vec2.h"
#include "vec3.h"
#include "main.h"

typedef enum {
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
    TEXTURE_ECOUNT,
} TextureType;

structure(Texture){
	int* pixel_data;
	int size;
	unsigned gl_id;
};

structure(Cubemap){
    int size;
    unsigned gl_id;
    Texture textures[6];
};

extern Texture g_textures[];
extern Vec2 g_texture_coordinates_fill[];
extern Cubemap g_skybox;

int cubemapColorGet(Cubemap* cubemap,Vec3 direction);
Vec3 cubemapDirectionGet(Cubemap* cubemap,Side side,int x,int y);
int cubemapColorGetBilinear(Cubemap* cubemap,Vec3 direction);

void texturesGenerate(void);
int textureLookup(Texture* texture,real x,real y,int mipmap);
void generateMipmaps(Texture* texture);

Texture textureCreate(int size);
void textureDestroy(Texture texture);

#if !defined(__wasm__) && !defined(__linux__)
void textureResetGL(void);
#endif

#endif
