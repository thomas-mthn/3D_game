#ifndef LIGHTING_H
#define LIGHTING_H

#include "langext.h"
#include "vec3.h"
#include "main.h"

#define LUXEL_MAX_MIPMAP 12

#ifdef __wasm__
#define N_LUXEL_CACHE 0x100000
#else
#define N_LUXEL_CACHE 0x400000
#endif

structure(Luxel){
	uint32 hash;
	Vec3 luminance;
    Vec3 luminance_direct;
	uint16 n_sample;
	uint16 tick_last_updated;
};

static int surfaceAngle(Vec3 position,Vec3 normal){
	return FIXED_ONE;
	int dot = vec3Dot(vec3Normalize(getLookDirection(g_angle)),normal);
	int angle = tAbs(dot) / 2 + FIXED_ONE / 2;
	angle = fixedDivR(FIXED_ONE,angle);

	if(g_test_bool)
		return tSqrt(angle);
	else
		return angle;
}

static int mipmapGet(Vec3 position,Vec3 normal,int distance,int angle){
	int angle_distance = fixedMulR(angle,distance);
	return bitScanReverse(angle_distance);
}

Vec3 skyboxSample(Vec3 direction);
Vec3 rayLuminance(Vec3 position,Vec3 direction);
Vec3 rayLuminanceInit(TraverseInit init,Vec3 position,Vec3 direction);
Vec3 luminanceQuery(Voxel* voxel,Vec3 normal,Vec3 position);
Vec3 squarePointClosestPosition(Vec3 square_pos,int square_size,Vec3 normal);

void lightingOctree(void);
Vec3 rayLuminanceTrace(Vec3 position,Vec3 direction);

Luxel* luxelDynamicGet(unsigned hash);

extern Luxel g_luxel_cache[];

static unsigned hash4(unsigned x,unsigned y,unsigned z,unsigned w){
    unsigned h = 0x811C9DC5;
    
    h ^= x; 
	h *= 0x27d4eb2d;
    h ^= y; 
	h *= 0x165667b1;
    h ^= z; 
	h *= 0x1b873593;
    h ^= w; 
	h *= 0x85ebca6b;
    
    h ^= h >> 16;
    return h;
}

static unsigned luxelHashGet(Vec3 position,int depth){
	return hash4(position.x,position.y,position.z,depth);
}

static Luxel* luxelGet(unsigned hash){
	return g_luxel_cache + hash % N_LUXEL_CACHE;
}

#endif
