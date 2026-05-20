#ifndef LIGHTING_H
#define LIGHTING_H

#include "langext.h"
#include "vec3.h"
#include "main.h"

#ifdef __wasm__
#define N_LUXEL_CACHE 0x40000
#else
#define N_LUXEL_CACHE 0x40000
#endif

#define LUXEL_DIRECTSAMPLED (1 << 15)

structure(Luxel){
    uint32 hash;
	Vec3 luminance;
    Vec3 luminance_direct;
    Vec3 pre_refresh;
    bool refresh;
	uint16 tick_last_updated;
    union{
        uint16 n_sample;
        uint16 flags;
    };
};

structure(LightmapTree){
    LightmapTree* child[4];
    Vec3 luminance;
};

structure(LightmapGPU){
    int child[4];
    float color[3];
    int reserved;
};

structure(RayLuminanceFlag){
    bool fulltrace : 1;
    bool no_emit : 1;
};

static real surfaceAngle(Vec3 position,Vec3 normal){
	real dot = vec3Dot(vec3Direction(g_surface.position,position),normal);
	real angle = tAbs(dot) / 2 + FIXED_ONE / 2;
	angle = tReciprocal(angle);

    return angle;
}

static int mipmapGet(Vec3 position,Vec3 normal,real distance,real angle){
	real angle_distance = realMulR(angle,distance);
    angle_distance /= 2;
    if(IS_FLOAT(real)){
        union{
            float f;
            int i;
        } bits = {.f = angle_distance};
        return tMax(((bits.i >> 23) & 0xFF) - 127 + 16,14);
    }
	return tMax(bitScanReverse(angle_distance),12);
}

Vec3 skyboxSample(Vec3 direction);
Vec3 rayLuminance(Vec3 position,Vec3 direction,RayLuminanceFlag flags);
Vec3 rayLuminanceInit(TraverseInit init,Vec3 position,Vec3 direction);
Vec3 luminanceQuery(Voxel* voxel,Vec3 normal,Vec3 position,int n_sample);
Vec3 squarePointClosestPosition(Vec3 square_pos,real square_size,Vec3 normal);

void lightingOctree(void);
Vec3 rayLuminanceTrace(Vec3 position,Vec3 direction);

Luxel* luxelDynamicGet(unsigned hash);

Vec3 lightmapGet(LightmapTree* lightmap,Vec2 uv);
Vec3 lightmapBilinear(LightmapTree* lightmap,Vec2 uv);
void lightmapTreeGenerate(LightmapTree* node,Voxel* voxel,Vec3 block_pos,int side,Vec2i coord,int depth,real surface_angle,Vec2 size);

void lightingEntityDynamic(Voxel* voxel,Entity* entity);
void lightingEntityShadow(Voxel* voxel,Entity* entity);

Vec3 lightingPositionLuminanceGet(Vec3 position,int depth,Vec3 normal);

extern Luxel g_luxel_cache[];

#endif
