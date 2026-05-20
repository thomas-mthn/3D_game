#include "lighting.h"
#include "octree.h"
#include "main.h"
#include "memory.h"
#include "vec3.h"
#include "platform/thread.h"
#include "entity.h"

#ifdef __linux__
#include "linux/l_main.h"
#include <pthread.h>
#elif !defined(__wasm__)
#include "win32/w_main.h"
#include "win32/w_kernel.h"
#include <immintrin.h>
#endif

#define N_EVICT_TRIES 4

Luxel g_luxel_cache[N_LUXEL_CACHE];
static Luxel luxel_dynamic_cache[0x10000];

//only works on axis aligned squares
Vec3 squarePointClosestPosition(Vec3 square_pos,real square_size,Vec3 normal){
	real s = square_size / 2;
	real neg_s = -s;
	real pos_s = s;

	Vec3 d = vec3Sub(g_surface.position,square_pos);
	real t = vec3Dot(d, normal);
	Vec3 Q = vec3Sub(g_surface.position,vec3MulS(normal,t));

	Vec3 QC = vec3Sub(Q, square_pos);

	real x_clamped = tClamp(QC.x,neg_s,pos_s);
	real y_clamped = tClamp(QC.y,neg_s,pos_s);
	real z_clamped = 0;

	Vec3 R = vec3Add(square_pos,(Vec3){x_clamped,y_clamped,z_clamped});
	return R;
}

static unsigned hash4(unsigned x,unsigned y,unsigned z,unsigned w,unsigned n_x,unsigned n_y,unsigned n_z){
    unsigned values[] = {x,y,z,w,(n_x + 0x80) >> 8,(n_y + 0x80) >> 8,(n_z + 0x80)  >> 8};
    unsigned h = 0x811C9DC5u;
    
    for(int i = 0;i < countof(values);i++){
        h ^= values[i];
        h = tHash(h);
    }

    return h;
}

static unsigned luxelHashGet(Vec3 position,int depth,Vec3 normal){
    Vec3 u = vec3NormalToU(normal);
    Vec3 v = vec3Cross(normal,u);
    position = (Vec3){tAbs(vec3Dot(position,u)),tAbs(vec3Dot(position,v)),tAbs(vec3Dot(position,normal))};
    if(IS_FLOAT(real)){
        position = vec3MulS(position,0x10000);
        normal = vec3MulS(normal,0x10000);
    }
	return hash4(position.x,position.y,position.z,depth,normal.x,normal.y,normal.z);
}

static Luxel* luxelGet(unsigned hash){
    for(int i = 0;i < N_EVICT_TRIES;i++){
        Luxel* luxel = g_luxel_cache + (hash + i) % N_LUXEL_CACHE;
        if(luxel->hash == hash)
            return luxel;
    }
    return 0;
}

Vec3 lightingPositionLuminanceGet(Vec3 position,int depth,Vec3 normal){
    unsigned hash = luxelHashGet(position,depth,normal);
    Luxel* luxel = luxelGet(hash);
    if(luxel)
        return luxel->luminance;
    hash = luxelHashGet(vec3Shr(position,1),depth + 1,normal);
    luxel = luxelGet(hash);

    return luxel ? luxel->luminance : (Vec3){0};
}

Luxel* luxelDynamicGet(unsigned hash){
	return luxel_dynamic_cache + hash % countof(luxel_dynamic_cache);
}

Vec3 skyboxSample(Vec3 direction){
    return vec3Shr(pixelColorToColor(cubemapColorGet(&g_skybox,direction)),4);
}

static bool voxelTranslucent(Voxel* voxel){
    return g_voxel_static[voxel->type].translucent || voxel->animation || voxel->opened;
}
#include "console.h"
Vec3 lightmapGet(LightmapTree* lightmap,Vec2 uv){
    uv.x = tClamp(uv.x + REAL_EPSILON,0,FIXED_ONE - REAL_EPSILON);
    uv.y = tClamp(uv.y + REAL_EPSILON,0,FIXED_ONE - REAL_EPSILON);
    int size = 1;
    while(lightmap->child[0]){
        int index = (tFractU(uv.x * size) >= (FIXED_ONE / 2)) << 1 | (tFractU(uv.y * size) >= (FIXED_ONE / 2));
        lightmap = lightmap->child[index];
        size *= 2;
    }
    return lightmap->luminance;
}

Vec3 lightmapBilinear(LightmapTree* lightmap,Vec2 uv){
    #if 0
    LightmapTree* node = lightmap;
    real size = FIXED_ONE * 2;
    while(lightmap->child[0]){
        int index = tFractU(uv.x * size) < 0 << 1 | tFractU(uv.y * size) < 0;
        lightmap = lightmap->child[index];
        size *= 2;
    }
    real luxel_size = tReciprocal(size);
    
    Vec2 lightmap_pos = {tFractU(uv.x * size),tFractU(uv.y,size)};
    
    Vec2 l_uv = {fixedDivR(uv.x & mask,luxel_size),fixedDivR(uv.y & mask,luxel_size)};
    
    Vec3 ll = lightmapGet(lightmap,(Vec2){lightmap_pos.x,lightmap_pos.y});
    Vec3 lh = lightmapGet(lightmap,(Vec2){lightmap_pos.x,lightmap_pos.y + luxel_size});
    Vec3 hh = lightmapGet(lightmap,(Vec2){lightmap_pos.x + luxel_size,lightmap_pos.y + luxel_size});
    Vec3 hl = lightmapGet(lightmap,(Vec2){lightmap_pos.x + luxel_size,lightmap_pos.y});

    Vec3 lx1 = vec3Mix(ll,hl,l_uv.x);
    Vec3 lx2 = vec3Mix(lh,hh,l_uv.x);

    return vec3Mix(lx1,lx2,l_uv.y);
    #endif
    return (Vec3){0};
}

static Vec3 voxelEmit(Voxel* voxel,Vec3 light_pos,int mipmap,Vec3 normal,bool entity_block){
    if(voxel->type == VOXEL_PARENT){
        Vec3 voxel_position = voxelWorldPosCenter(voxel);
#if 1
        if(sdVoxel(light_pos,voxel_position,depthToSize(voxel->depth)) > voxel->emission)
            return (Vec3){0};
#endif
        Vec3 luminance = {0};
        for(int i = 8;i--;)
            luminance = vec3Add(luminance,voxelEmit(voxel->child_s[i],light_pos,mipmap,normal,entity_block));
        return luminance;
    }
    if(g_voxel_static[voxel->type].emiter){
        Vec3 emiter_position = voxelWorldPosCenter(voxel);
        real distance = vec3Distance(light_pos,emiter_position);
        distance = sdVoxel(light_pos,emiter_position,depthToSize(voxel->depth) / 3);
        
        real intensity = tReciprocal(realMulR(distance,distance));
        Vec3 direction = vec3Direction(emiter_position,light_pos);
        real angle = -vec3Dot(direction,normal);
#if 0
        if(angle < 0)
            return (Vec3){0};

        if(!voxelTranslucent(voxelPositionGet(light_pos)))
            return (Vec3){0};
#endif
#if 1
        if(treeRayTraceAndInit(light_pos,vec3Direction(light_pos,emiter_position),0,(TreeTraceFlags){.entity = false}) != voxel)
            return (Vec3){0};
#endif
        if(voxel->type == VOXEL_CUSTOM_EMIT){
            Vec3 color = vec3MulS(voxel->color,intensity);
            color = vec3MulS(color,intToReal(1 << voxel->emit_pow));
            return vec3MulS(color,angle);
        }
            
        return vec3MulS(vec3MulS(g_voxel_static[voxel->type].color,intensity),angle);
    }
    return (Vec3){0};
}

static void entityDynamicLightingSide(Voxel* voxel,Entity* entity,Vec3 block_pos,int side,Vec2i coord,int depth,real distance_max,real surface_angle){
	Vec3 normal = g_normal_table[side];
	Vec2i axis = g_axis_table[side];
	Vec3 luxel_position = block_pos;
	real size = depthToSize(voxel->depth) / (1 << depth);
	luxel_position.a[axis.x] += realMulR(intToReal(coord.x),size);
	luxel_position.a[axis.y] += realMulR(intToReal(coord.y),size);
	
	Vec3 square_pos = luxel_position;
	square_pos.a[axis.x] += size / 2;
	square_pos.a[axis.y] += size / 2;

	int mipmap = mipmapGet(squarePointClosestPosition(square_pos,size,normal),normal,distance_max,surface_angle);

	Vec3 luxel_pos = vec3Shr(luxel_position,mipmap);
	unsigned hash = luxelHashGet(luxel_pos,mipmap,normal);
	Luxel* luxel = luxelDynamicGet(hash);

	Vec3 position = square_pos;
	position.a[side >> 1] += side & 1 ? REAL_EPSILON : -REAL_EPSILON;

    if(!g_voxel_static[voxelPositionGet(position)->type].translucent)
        return;

	VoxelStatic* voxel_s = g_voxel_static + voxel->type;

	real distance = vec3Distance(entity->position,position);
	real intensity = realDivR(FIXED_ONE / 4,realMulR(distance,distance));
	realMul(&intensity,tMax(vec3Dot(normal,vec3Direction(position,entity->position)),0));

    luxel->luminance = vec3Add(luxel->luminance,vec3MulS(COLOR_WHITE,intensity));
#if 0
	luxel->luminance = vec3Add(luxel->luminance,vec3MulS(entity->color_emit,intensity));
#endif
	//luxel->luminance = (Vec3){position.x % (FIXED_ONE << 4),position.y % (FIXED_ONE << 4),position.z % (FIXED_ONE << 4)};
	luxel->hash = hash;
}

static void entityDynamicLightingSideRecursive(Voxel* voxel,Entity* entity,Vec3 block_pos,int side,Vec2i coord,int depth,real surface_angle){
	Vec2i axis = g_axis_table[side];
	Vec3 block_pos_t = block_pos;
	real size = depthToSize(voxel->depth) / (1 << depth);
	block_pos_t.a[axis.x] += realMulR(intToReal(coord.x),size);
	block_pos_t.a[axis.y] += realMulR(intToReal(coord.y),size);

	Vec3 positions[4] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

	positions[1].a[axis.y] += size;
	positions[2].a[axis.x] += size;
	positions[3].a[axis.x] += size;
	positions[3].a[axis.y] += size;

	real distance_max = 0;
	int distance_max_index;

	Vec3* position_farthest;

	for(int i = 0;i < countof(positions);i++){
		real distance = vec3Dot(vec3Shr(g_surface.position,4),vec3Shr(positions[i],4));
		if(distance > distance_max){
			distance_max = distance;
			position_farthest = positions + i;
		}
	}

	distance_max = vec3Distance(vec3Shr(g_surface.position,4),vec3Shr(*position_farthest,4));

	Vec3 normal = g_normal_table[side];

	int mipmap = mipmapGet(squarePointClosestPosition(positions[0],size,normal),normal,distance_max,surface_angle);

	int split = 25 + -mipmap - voxel->depth;

	if(depth < split){
		Vec3 v_pos = vec3Shl((Vec3){voxel->position_x,voxel->position_y,voxel->position_z},depth);
		v_pos.a[axis.x] += coord.x;
		v_pos.a[axis.y] += coord.y;
		if(side & 1)
			v_pos.a[side >> 1] += (1 << depth) - 1;
		
		if(sdVoxel(entity->position,positions[0],size) > FIXED_ONE * 4)
			return;

        coord.x <<= 1;
		coord.y <<= 1;
        
		entityDynamicLightingSideRecursive(voxel,entity,block_pos,side,(Vec2i){coord.x + 0,coord.y + 0},depth + 1,surface_angle);
		entityDynamicLightingSideRecursive(voxel,entity,block_pos,side,(Vec2i){coord.x + 0,coord.y + 1},depth + 1,surface_angle);
		entityDynamicLightingSideRecursive(voxel,entity,block_pos,side,(Vec2i){coord.x + 1,coord.y + 0},depth + 1,surface_angle);
		entityDynamicLightingSideRecursive(voxel,entity,block_pos,side,(Vec2i){coord.x + 1,coord.y + 1},depth + 1,surface_angle);
		return;
	}
	entityDynamicLightingSide(voxel,entity,block_pos,side,coord,depth,distance_max,surface_angle);
}

static void entityDynamicLightingSidePre(Voxel* voxel,Entity* entity,Vec3 block_pos,int side){
	entityDynamicLightingSideRecursive(voxel,entity,block_pos,side,(Vec2i){0},0,surfaceAngle(block_pos,g_normal_table[side]));
}

void lightingEntityDynamic(Voxel* voxel,Entity* entity){
	real block_size = depthToSize(voxel->depth);
	Vec3 block_pos = voxelWorldPos(voxel);
	if(voxel->type == VOXEL_PARENT){
		if(sdVoxel(entity->position,block_pos,block_size) > FIXED_ONE * 4)
			return;
		for(int i = 0;i < countof(voxel->child_s);i++)
			lightingEntityDynamic(voxel->child_s[i],entity);
			
		return;
	}
	if(voxel->type == VOXEL_AIR || voxel->type == VOXEL_MIRROR || g_voxel_static[voxel->type].emiter)
		return;
	entityDynamicLightingSidePre(voxel,entity,block_pos,0);
	entityDynamicLightingSidePre(voxel,entity,vec3Add(block_pos,(Vec3){block_size,0,0}),1);
	entityDynamicLightingSidePre(voxel,entity,block_pos,2);
	entityDynamicLightingSidePre(voxel,entity,vec3Add(block_pos,(Vec3){0,block_size,0}),3);
	entityDynamicLightingSidePre(voxel,entity,block_pos,4);
	entityDynamicLightingSidePre(voxel,entity,vec3Add(block_pos,(Vec3){0,0,block_size}),5);
}
#include "console.h"
static void shadowSide(Voxel* voxel,Entity* entity,Vec3 block_pos,int side,Vec2i coord,int depth,real distance_max,real surface_angle){
    Vec3 normal = g_normal_table[side];
	Vec2i axis = g_axis_table[side];
	Vec3 luxel_position = block_pos;
	real size = depthToSize(voxel->depth) / (1 << depth);
	luxel_position.a[axis.x] += realMulR(intToReal(coord.x),size);
	luxel_position.a[axis.y] += realMulR(intToReal(coord.y),size);
	
	Vec3 square_pos = luxel_position;
	square_pos.a[axis.x] += size / 2;
	square_pos.a[axis.y] += size / 2;

	int mipmap = mipmapGet(squarePointClosestPosition(square_pos,size,normal),normal,distance_max,surface_angle);

	Vec3 luxel_pos = vec3Shr(luxel_position,mipmap);
	unsigned hash = luxelHashGet(luxel_pos,mipmap,normal);
	Luxel* luxel = luxelGet(hash);
     
    if(!luxel || luxel->tick_last_updated == g_time.frame_tick)
        return;

    if(!luxel->luminance_direct.x && !luxel->luminance_direct.y && !luxel->luminance_direct.z)
        return;

	Vec3 position = luxel_position;
	position.a[side >> 1] += side & 1 ? REAL_EPSILON : -REAL_EPSILON;

    if(!g_voxel_static[voxelPositionGet(position)->type].translucent)
        return;
    
    luxel->luminance_direct = voxelEmit(&g_world.voxel,position,mipmap,normal,true);
    luxel->tick_last_updated = g_time.frame_tick;
}

static void shadowSideRecursive(Voxel* voxel,Entity* entity,Vec3 block_pos,int side,Vec2i coord,int depth,real surface_angle){
	Vec2i axis = g_axis_table[side];
	Vec3 block_pos_t = block_pos;
	real size = depthToSize(voxel->depth) / (1 << depth);
	block_pos_t.a[axis.x] += realMulR(intToReal(coord.x),size);
	block_pos_t.a[axis.y] += realMulR(intToReal(coord.y),size);

	Vec3 positions[4] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

	positions[1].a[axis.y] += size;
	positions[2].a[axis.x] += size;
	positions[3].a[axis.x] += size;
	positions[3].a[axis.y] += size;

	real distance_max = 0;
	int distance_max_index;

	Vec3* position_farthest;

	for(int i = 0;i < countof(positions);i++){
		real distance = vec3Dot(vec3Shr(g_surface.position,4),vec3Shr(positions[i],4));
		if(distance > distance_max){
			distance_max = distance;
			position_farthest = positions + i;
		}
	}

	distance_max = vec3Distance(vec3Shr(g_surface.position,4),vec3Shr(*position_farthest,4));

	Vec3 normal = g_normal_table[side];

	int mipmap = mipmapGet(squarePointClosestPosition(positions[0],size,normal),normal,distance_max,surface_angle);

	int split = 25 + -mipmap - voxel->depth;

	if(depth < split){
		Vec3 v_pos = vec3Shl((Vec3){voxel->position_x,voxel->position_y,voxel->position_z},depth);
		v_pos.a[axis.x] += coord.x;
		v_pos.a[axis.y] += coord.y;
		if(side & 1)
			v_pos.a[side >> 1] += (1 << depth) - 1;
#if 0
		if(sdVoxel(entity->position,positions[0],size) > FIXED_ONE * 4)
			return;
#endif
		coord.x <<= 1;
		coord.y <<= 1;
	    shadowSideRecursive(voxel,entity,block_pos,side,(Vec2i){coord.x + 0,coord.y + 0},depth + 1,surface_angle);
	    shadowSideRecursive(voxel,entity,block_pos,side,(Vec2i){coord.x + 0,coord.y + 1},depth + 1,surface_angle);
	    shadowSideRecursive(voxel,entity,block_pos,side,(Vec2i){coord.x + 1,coord.y + 0},depth + 1,surface_angle);
	    shadowSideRecursive(voxel,entity,block_pos,side,(Vec2i){coord.x + 1,coord.y + 1},depth + 1,surface_angle);
		return;
	}
#if 1
    shadowSide(voxel,entity,block_pos,side,coord,depth,distance_max,surface_angle);
#endif
    //entityDynamicLightingSide(voxel,entity,block_pos,side,coord,depth,distance_max,surface_angle);
}

static void shadowLightingSidePre(Voxel* voxel,Entity* entity,Vec3 block_pos,int side){
	shadowSideRecursive(voxel,entity,block_pos,side,(Vec2i){0},0,surfaceAngle(block_pos,g_normal_table[side]));
}

void lightingEntityShadow(Voxel* voxel,Entity* entity){
#if 0 
	real block_size = depthToSize(voxel->depth);
	Vec3 block_pos = voxelWorldPos(voxel);
	if(voxel->type == VOXEL_PARENT){
     
		if(sdVoxel(entity->position,block_pos,block_size) > FIXED_ONE * 0x4)
			return;
        
		for(int i = 0;i < countof(voxel->child_s);i++)
			lightingEntityShadow(voxel->child_s[i],entity);
		return;
	}
	if(voxel->type == VOXEL_AIR || voxel->type == VOXEL_MIRROR || g_voxel_static[voxel->type].emiter)
		return;
    shadowLightingSidePre(voxel,entity,block_pos,0);
	shadowLightingSidePre(voxel,entity,vec3Add(block_pos,(Vec3){block_size,0,0}),1);
    shadowLightingSidePre(voxel,entity,block_pos,2);
    shadowLightingSidePre(voxel,entity,vec3Add(block_pos,(Vec3){0,block_size,0}),3);
    shadowLightingSidePre(voxel,entity,block_pos,4);
    shadowLightingSidePre(voxel,entity,vec3Add(block_pos,(Vec3){0,0,block_size}),5);
#endif
}

static Vec3 triangleNormal(Vec3 a,Vec3 b,Vec3 c){
    Vec3 u = vec3Sub(b,a);
    Vec3 v = vec3Sub(c,a);
    return vec3Normalize(vec3Cross(u,v));
}

static void lightmapGenerate(LightmapTree* node,Voxel* voxel,Vec3 block_pos,Vec2i coord,int depth,Side side,Vec2 size,real distance_max,real surface_angle){
    Vec2i axis = g_axis_table[side];
    Vec3 block_pos_t = block_pos;
	block_pos_t.a[axis.x] += realMulR(intToReal(coord.x),size.x);
	block_pos_t.a[axis.y] += realMulR(intToReal(coord.y),size.y);

    Vec3 pos[] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

	pos[1].a[axis.y] += size.y;
	pos[2].a[axis.x] += size.x;
	pos[3].a[axis.x] += size.x;
	pos[3].a[axis.y] += size.y;

    Vec3 normal = g_normal_table[side];
    
    int mipmap = mipmapGet(squarePointClosestPosition(pos[0],size.x,normal),normal,distance_max,surface_angle);
    
    Vec3 light_pos = pos[0];
    light_pos.a[axis.x] += size.x / 2;
    light_pos.a[axis.y] += size.y / 2;
    
    switch(voxel->type){
        case VOXEL_WATER:{
            Vec3 quad_normal = triangleNormal(vec3Shl(pos[3],2),vec3Shl(pos[1],2),vec3Shl(pos[0],2));
            Vec3 relative = vec3Sub(light_pos,g_surface.position);
            Vec3 direction = vec3Refract(vec3Normalize(vec3Shr(relative,8)),quad_normal,FIXED_ONE - FIXED_ONE / 4);
            Vec3 position = light_pos;
            position.a[side >> 1] -= side % 2 ? REAL_EPSILON : -REAL_EPSILON;

            Vec3 refraction = rayLuminance(position,direction,(RayLuminanceFlag){0});
            refraction = vec3Shr(refraction,4);

            relative = vec3Sub(light_pos,g_surface.position);
            direction = vec3Normalize(vec3Reflect(relative,quad_normal));
            position = light_pos;
            position.a[side >> 1] += side % 2 ? REAL_EPSILON : -REAL_EPSILON;
            if(tAbs(direction.x) < REAL_EPSILON)
                direction.x = REAL_EPSILON;
            if(tAbs(direction.y) < REAL_EPSILON)
                direction.y = REAL_EPSILON;
            if(tAbs(direction.z) < REAL_EPSILON)
                direction.z = REAL_EPSILON;
            Vec3 reflection = rayLuminance(position,direction,(RayLuminanceFlag){0});
            reflection = vec3Shr(reflection,4);

            node->luminance = vec3Mix(reflection,refraction,tClamp(tAbs(vec3Dot(vec3Direction(g_surface.position,light_pos),quad_normal)),0,FIXED_ONE));
        } break;
        case VOXEL_MIRROR:{
            Vec3 luminance;
            Vec3 relative = vec3Sub(light_pos,g_surface.position);
            Vec3 direction = vec3Normalize(vec3Reflect(vec3Shr(relative,8),normal));
            Vec3 position = light_pos;
            position.a[side >> 1] += side % 2 ? REAL_EPSILON : -REAL_EPSILON;
            if(tAbs(direction.x) < REAL_EPSILON)
                direction.x = REAL_EPSILON;
            if(tAbs(direction.y) < REAL_EPSILON)
                direction.y = REAL_EPSILON;
            if(tAbs(direction.z) < REAL_EPSILON)
                direction.z = REAL_EPSILON;
            luminance = rayLuminance(position,direction,(RayLuminanceFlag){0});
            luminance = vec3Shr(luminance,4);
            node->luminance = luminance;
        } break;
        default:{
            Vec3 luxel_pos = vec3Shr(light_pos,mipmap);
            unsigned hash = luxelHashGet(luxel_pos,mipmap,normal);
            Luxel* luxel = luxelGet(hash);
    
            node->luminance = vec3MulS(lightingPositionLuminanceGet(luxel_pos,mipmap,normal),g_exposure);
        }
    }
}

void lightmapTreeGenerate(LightmapTree* node,Voxel* voxel,Vec3 block_pos,int side,Vec2i coord,int depth,real surface_angle,Vec2 size){
    Vec2i axis = g_axis_table[side];
    Vec3 block_pos_t = block_pos;
	block_pos_t.a[axis.x] += realMulR(intToReal(coord.x),size.x);
	block_pos_t.a[axis.y] += realMulR(intToReal(coord.y),size.y);

    Vec3 pos[4] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

	pos[1].a[axis.y] += size.y;
	pos[2].a[axis.x] += size.x;
	pos[3].a[axis.x] += size.x;
	pos[3].a[axis.y] += size.y;
    
	real distance_max = 0;
	int distance_max_index;

	for(int i = 0;i < 4;i++){
		real distance = vec3Distance(vec3Shr(g_surface.position,4),vec3Shr(pos[i],4));
		if(distance > distance_max){
			distance_max = distance;
			distance_max_index = i;
		}
	}
    
    Vec3 normal = g_normal_table[side];
    int mipmap = mipmapGet(squarePointClosestPosition(block_pos_t,size.x,normal),normal,distance_max,surface_angle);

	int split = 25 + -mipmap - voxel->depth;

    for(int i = countof(node->child);i--;)
        node->child[i] = memoryArenaAllocateZero(&g_arena_frame,sizeof *node->child[i]);

    coord.x <<= 1;
    coord.y <<= 1;
    
    if(depth < split){
        lightmapTreeGenerate(node->child[0],voxel,block_pos,side,(Vec2i){coord.x + 0,coord.y + 0},depth + 1,surface_angle,vec2Shr(size,1));
        lightmapTreeGenerate(node->child[1],voxel,block_pos,side,(Vec2i){coord.x + 0,coord.y + 1},depth + 1,surface_angle,vec2Shr(size,1));
        lightmapTreeGenerate(node->child[2],voxel,block_pos,side,(Vec2i){coord.x + 1,coord.y + 0},depth + 1,surface_angle,vec2Shr(size,1));
        lightmapTreeGenerate(node->child[3],voxel,block_pos,side,(Vec2i){coord.x + 1,coord.y + 1},depth + 1,surface_angle,vec2Shr(size,1));
        return;
    }
    lightmapGenerate(node->child[0],voxel,block_pos,(Vec2i){coord.x + 0,coord.y + 0},depth + 1,side,vec2Shr(size,1),distance_max,surface_angle);
    lightmapGenerate(node->child[1],voxel,block_pos,(Vec2i){coord.x + 0,coord.y + 1},depth + 1,side,vec2Shr(size,1),distance_max,surface_angle);
    lightmapGenerate(node->child[2],voxel,block_pos,(Vec2i){coord.x + 1,coord.y + 0},depth + 1,side,vec2Shr(size,1),distance_max,surface_angle);
    lightmapGenerate(node->child[3],voxel,block_pos,(Vec2i){coord.x + 1,coord.y + 1},depth + 1,side,vec2Shr(size,1),distance_max,surface_angle);
}

static Vec3 water_color = (Vec3){REAL_UNIT * 0xC00,REAL_UNIT * 0x400,REAL_UNIT * 0x100};

static Vec3 rayLuminanceRecursive(TraverseInit init,Vec3 position,Vec3 direction,int depth,RayLuminanceFlag flags){
    if(!depth)
	    return (Vec3){0};

	Vec3Axis side;
	Voxel* voxel = treeRayTrace(init.voxel,init.pos,position,direction,&side,(TreeTraceFlags){0});

	if(!voxel)
		return skyboxSample(direction);

	VoxelStatic* voxel_s = g_voxel_static + voxel->type;

	if(voxel_s->emiter)
		return flags.no_emit ? (Vec3){0} : vec3Shl(voxel_s->color,4);

    if(voxel->type == VOXEL_CUSTOM_EMIT)
		return flags.no_emit ? (Vec3){0} : vec3Shl(voxel->color,4);

	Vec3 end_pos = rayVoxelHitPosition(voxel,position,direction,side);

    switch(voxel->type){
        case VOXEL_MIRROR:{
            Vec3 normal = g_normal_table[side << 1 | (direction.a[side] < 0)];
            Vec3 relative = vec3Sub(end_pos,position);
            Vec3 offset = vec3Reflect(relative,normal);
            return vec3MulS(rayLuminanceRecursive(initTraverse(end_pos),end_pos,offset,depth - 1,flags),FIXED_ONE - (FIXED_ONE / 8));
        }
        case VOXEL_GLASS:{
            end_pos.a[side] -= direction.a[side] < 0 ? REAL_EPSILON : -REAL_EPSILON;
            return rayLuminanceRecursive(initTraverse(end_pos),end_pos,direction,depth - 1,flags);
        }
        case VOXEL_WATER:{
            real distance_o = rayCubeIntersection(voxelWorldPosCenter(voxel),depthToSize(voxel->depth) / 2,position,direction);
            real distance_i = rayCubeIntersectionInside(voxelWorldPosCenter(voxel),depthToSize(voxel->depth) / 2,position,direction);
            real distance = distance_i - distance_o;
            distance /= 4;
            distance = tReciprocal(realMulR(distance,distance) + FIXED_ONE);
            end_pos.a[side] -= direction.a[side] < 0 ? REAL_EPSILON : -REAL_EPSILON;
            return vec3Mix(water_color,rayLuminanceRecursive(initTraverse(end_pos),end_pos,direction,depth - 1,flags),distance);
        }
    }

	if(flags.fulltrace){
		Vec3 normal = g_normal_table[side << 1 | (((int*)&direction)[side] < 0)];
		Vec3 offset = normal;
		Vec3 rnd_vector = vec3Rnd();
		offset = vec3Add(offset,rnd_vector);
		Vec3 direction_new = vec3Normalize(offset);
		Vec3 color = rayLuminanceRecursive(initTraverse(end_pos),end_pos,direction_new,depth - 1,flags);

		Vec2 uv = voxelGuiPositionGet(voxel,position,direction,side);
		
		uv.x = realShr(realMulR(realMulR(uv.x,voxel_s->texture_size),depthToSize(voxel->depth)),4);
		uv.y = realShr(realMulR(realMulR(uv.y,voxel_s->texture_size),depthToSize(voxel->depth)),4);

		if(!voxel_s->texture)
			return vec3Mul(color,voxel_s->color);
		Vec3 texel = vec3Shr(pixelColorToColor(textureLookup(voxel_s->texture,uv.x,uv.y,0)),4);
		return vec3Mul(color,texel);
	}

    if(!g_options.lighting_engine){
        if(!voxel_s->texture)
            return voxel_s->color;
        Vec2 uv = voxelGuiPositionGet(voxel,position,direction,side);
		
		uv.x = realShr(realMulR(realMulR(uv.x,voxel_s->texture_size),depthToSize(voxel->depth)),4);
		uv.y = realShr(realMulR(realMulR(uv.y,voxel_s->texture_size),depthToSize(voxel->depth)),4);

		Vec3 texel = vec3Shr(pixelColorToColor(textureLookup(voxel_s->texture,uv.x,uv.y,3)),4);
		return texel;
    }
    Vec3 normal = g_normal_table[side << 1 | direction.a[side] < 0];
	int mipmap = mipmapGet(end_pos,normal,vec3Distance(vec3Shr(end_pos,4),vec3Shr(g_surface.position,4)),FIXED_ONE);

	Vec3 luxel_pos = vec3Shr(end_pos,mipmap);
	unsigned hash = luxelHashGet(luxel_pos,mipmap,normal);
	Luxel* luxel = luxelGet(hash);

    if(!luxel)
        return (Vec3){0,0,0};
    
	if(luxel->hash == hash){
		Vec3 luminance = vec3Add(vec3Shr(luxel->luminance,4),vec3Shr(luxel->luminance_direct,4));
        Texture* texture = voxel->type == VOXEL_CUSTOM ? g_textures + voxel->texture_id : voxel_s->texture;
		if(!texture){
            if(voxel->type == VOXEL_CUSTOM)
                return vec3Mul(luminance,voxel->color);
			return vec3Mul(luminance,voxel_s->color);
        }
        if(!g_options.textures)
            return luminance;

		Vec2 uv = voxelGuiPositionGet(voxel,position,direction,side);

		uv.x = realShr(realMulR(realMulR(uv.x,voxel_s->texture_size),depthToSize(voxel->depth)),5);
		uv.y = realShr(realMulR(realMulR(uv.y,voxel_s->texture_size),depthToSize(voxel->depth)),5);

		Vec3 texel = vec3Shr(pixelColorToColor(textureLookup(texture,uv.x,uv.y,2)),4);
        texel = vec3Mul(texel,luminance);
        if(voxel->type == VOXEL_CUSTOM)
            texel = vec3Mul(texel,voxel->color);
        
		return texel;
	}
	return vec3Single(0);
}

Vec3 rayLuminance(Vec3 position,Vec3 direction,RayLuminanceFlag flags){
	TraverseInit init = initTraverse(position);
	Voxel* voxel = voxelPositionGet(position);
    direction = vec3Epsilon(direction);
    if(!voxelTranslucent(voxel))
        return (Vec3){0};
    if(voxel->type == VOXEL_WATER){
        real distance = rayCubeIntersectionInside(voxelWorldPosCenter(voxel),depthToSize(voxel->depth) / 2,position,direction);
        if(distance > REAL_EPSILON * 0x40){
            position = vec3Add(position,vec3MulS(direction,distance - REAL_EPSILON * 0x10));
            distance /= 4;
            distance = tReciprocal(realMulR(distance,distance) + FIXED_ONE);
            
            return vec3Mix(water_color,rayLuminance(position,direction,flags),distance);
        }
    }
	return rayLuminanceRecursive(init,position,direction,8,flags);
}

Vec3 rayLuminanceInit(TraverseInit init,Vec3 position,Vec3 direction){
    direction = vec3Epsilon(direction);
	return rayLuminanceRecursive(init,position,direction,8,(RayLuminanceFlag){0});
}

Vec3 rayLuminanceTrace(Vec3 position,Vec3 direction){
    direction = vec3Epsilon(direction);
	return rayLuminanceRecursive(initTraverse(position),position,direction,8,(RayLuminanceFlag){.fulltrace = true});
}

structure(LightingWorkData){
	Voxel* voxel;
	Vec3 position;
	Vec3 u;
    Vec3 v;
    int side;
	int mipmap;
	real size;
    bool emiter : 1;
    bool use_side : 1;
};

static LightingWorkData lighting_work_data[0x8000];
static int lighting_work_data_ptr;

structure(LightingTraceParameters){
	int index;
	int amount;
	LightingWorkData* data;
};

#define N_LUXEL_SAMPLE 0x1000
#define EMIT_RANGE FIXED_ONE

Vec3 luminanceQuery(Voxel* voxel,Vec3 normal,Vec3 position,int n_sample){
	VoxelStatic* voxel_s = g_voxel_static + voxel->type;
	Vec3 offset;
	if(voxel->type == VOXEL_METALLIC){
		offset = normal;
		offset.x += realRandom(FIXED_ONE * 2) - FIXED_ONE;
		offset.y += realRandom(FIXED_ONE * 2) - FIXED_ONE;
		offset.z += realRandom(FIXED_ONE * 2) - FIXED_ONE;
		offset = vec3Normalize(offset);
		Vec3 relative = vec3Sub(position,g_surface.position);
		Vec3 reflect = vec3Normalize(vec3Reflect(vec3Shr(relative,8),normal));
		offset = vec3Mix(offset,reflect,FIXED_ONE / 2);
	}
	else{
        int l = bitScanReverse(n_sample + 2);
        int n = (n_sample + 2) - (1 << l);
        Vec3 rnd_vector = fibonnaciSphereSample(n,1 << l);
		offset = normal;
		offset = vec3Add(offset,rnd_vector);
		offset = vec3Normalize(offset);
	}
	return rayLuminance(position,offset,(RayLuminanceFlag){.no_emit = true});
}

static void lightingPlaneRecursive(Voxel* voxel,LightingWorkData* lighting_data,Vec3 block_pos,Vec3i coord,Plane plane,int depth){
    real cube_size = depthToSize(voxel->depth + depth);
    Vec3 cube_pos = {
        realMulR(intToReal(coord.x),cube_size),
        realMulR(intToReal(coord.y),cube_size),
        realMulR(intToReal(coord.z),cube_size),
    };
#if 1
    if(!intersectCubePlane(vec3SubS(cube_pos,depthToSize(voxel->depth) / 2),cube_size,plane))
        return;
#endif
    real dist = sdVoxel(g_surface.position,vec3Add(block_pos,vec3AddS(cube_pos,cube_size)),cube_size / 2) / 0x10;

    real angle = surfaceAngle(vec3Add(block_pos,vec3AddS(cube_pos,cube_size)),plane.normal);
    
    int mipmap = mipmapGet(vec3Add(block_pos,vec3AddS(cube_pos,cube_size / 2)),plane.normal,dist,angle);
    
    int split = 25 + -mipmap - voxel->depth;

	VoxelStatic* voxel_s = g_voxel_static + voxel->type;
    if(depth < split){
#if 0
        if(sdSquareSquare(vec3Shr(g_surface.position,4),vec3Shr(block_pos_t,4),size >> 4,side) > RENDER_DISTANCE)
            return;
#endif 
        coord.x <<= 1;
        coord.y <<= 1;
        coord.z <<= 1;
        
        lightingPlaneRecursive(voxel,lighting_data,block_pos,(Vec3i){coord.x + 0,coord.y + 0,coord.z + 0},plane,depth + 1);
        lightingPlaneRecursive(voxel,lighting_data,block_pos,(Vec3i){coord.x + 0,coord.y + 1,coord.z + 0},plane,depth + 1);
        lightingPlaneRecursive(voxel,lighting_data,block_pos,(Vec3i){coord.x + 1,coord.y + 0,coord.z + 0},plane,depth + 1);
        lightingPlaneRecursive(voxel,lighting_data,block_pos,(Vec3i){coord.x + 1,coord.y + 1,coord.z + 0},plane,depth + 1);

        lightingPlaneRecursive(voxel,lighting_data,block_pos,(Vec3i){coord.x + 0,coord.y + 0,coord.z + 1},plane,depth + 1);
        lightingPlaneRecursive(voxel,lighting_data,block_pos,(Vec3i){coord.x + 0,coord.y + 1,coord.z + 1},plane,depth + 1);
        lightingPlaneRecursive(voxel,lighting_data,block_pos,(Vec3i){coord.x + 1,coord.y + 0,coord.z + 1},plane,depth + 1);
        lightingPlaneRecursive(voxel,lighting_data,block_pos,(Vec3i){coord.x + 1,coord.y + 1,coord.z + 1},plane,depth + 1);
        return;
    }
    //PRINT_VAR(split);
	Vec3 light_pos = vec3Add(block_pos,vec3AddS(cube_pos,cube_size / 2));
	real size = cube_size / 2;

    if(lighting_work_data_ptr >= countof(lighting_work_data) - 1)
		return;

	Vec3 position = light_pos;
    
	LightingWorkData* light_data = lighting_data + lighting_work_data_ptr++;

    Vec3 normal = getLookDirection(voxel->angle);
    Vec3 tangent = vec3Normalize(vec3Cross(normal,(Vec3){FIXED_ONE,0,0}));
	*light_data = (LightingWorkData){
		.position = position,
		.mipmap = mipmap,
        .u = tangent,
        .v = vec3Cross(normal,tangent),
		.voxel = voxel,
		.size = size
	};
}

static void lightingSlopeRecursive(Voxel* voxel,LightingWorkData* lighting_data,Vec3 block_pos,Vec3 u,Vec3 v,Vec2i coord,int depth){
    Vec3 normal = vec3Cross(u,v);
    
	Vec3 block_pos_t = block_pos;

    real size_u = realShr(depthToSize(voxel->depth),depth);
    real size_v = realShr(realMulR(depthToSize(voxel->depth),tSqrt(FIXED_ONE * 2)),depth);

    block_pos_t = vec3Add(block_pos_t,vec3MulS(u,realMulR(coord.x << FIXED_PRECISION,size_u)));
    block_pos_t = vec3Add(block_pos_t,vec3MulS(v,realMulR(coord.y << FIXED_PRECISION,size_v)));

	Vec3 pos[4] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

    pos[1] = vec3Add(pos[1],vec3MulS(v,size_v));
    pos[2] = vec3Add(pos[2],vec3MulS(u,size_u));
    pos[3] = vec3Add(pos[3],vec3MulS(u,size_u));
    pos[3] = vec3Add(pos[3],vec3MulS(v,size_v));

	Vec3 voxel_pos = voxelWorldPos(voxel);

	Vec3 point[4] = {pointToScreen(pos[0]),pointToScreen(pos[1]),pointToScreen(pos[2]),pointToScreen(pos[3])};

	real distance_max = 0;
	int distance_max_index;

	for(int i = 0;i < 4;i++){
		int distance = vec3Distance(vec3Shr(g_surface.position,4),vec3Shr(pos[i],4));

		if(distance > distance_max){
			distance_max = distance;
			distance_max_index = i;
		}
	}
    int mipmap = mipmapGet(squarePointClosestPosition(pos[0],size_u,normal),normal,distance_max,FIXED_ONE);
    mipmap = tClamp(mipmap,0,31);
    
    int split = 25 + -mipmap - voxel->depth;

	VoxelStatic* voxel_s = g_voxel_static + voxel->type;
    if(depth < split && !voxel_s->emiter){
#if 0
        if(sdSquareSquare(vec3Shr(g_surface.position,4),vec3Shr(block_pos_t,4),size >> 4,side) > RENDER_DISTANCE)
            return;
#endif 
        coord.x <<= 1;
        coord.y <<= 1;
        
        lightingSlopeRecursive(voxel,lighting_data,block_pos,u,v,(Vec2i){coord.x + 0,coord.y + 0},depth + 1);
        lightingSlopeRecursive(voxel,lighting_data,block_pos,u,v,(Vec2i){coord.x + 0,coord.y + 1},depth + 1);
        lightingSlopeRecursive(voxel,lighting_data,block_pos,u,v,(Vec2i){coord.x + 1,coord.y + 0},depth + 1);
        lightingSlopeRecursive(voxel,lighting_data,block_pos,u,v,(Vec2i){coord.x + 1,coord.y + 1},depth + 1);
        
        return;
    }

	Vec3 light_pos = block_pos_t;
	real size = depthToSize(voxel->depth) / (1 << depth);

    if(lighting_work_data_ptr >= countof(lighting_work_data) - 1)
		return;

	mipmap = mipmapGet(light_pos,normal,distance_max,FIXED_ONE);

	Vec3 position = light_pos;
    
	LightingWorkData* light_data = lighting_data + lighting_work_data_ptr++;
    
	*light_data = (LightingWorkData){
		.position = position,
		.mipmap = mipmap,
	    .u = u,
        .v = v,
		.voxel = voxel,
		.size = size
	};
}

static void lightingSide(Voxel* voxel,LightingWorkData* lighting_data,Vec3 block_pos,int side,Vec2i coord,int depth,real distance_max,Vec3 cube_c,real angle){
	Vec3 normal = g_normal_table[side];
	Vec2i axis = g_axis_table[side];
	Vec3 light_pos = block_pos;
	real size = depthToSize(voxel->depth) / (1 << depth);
	light_pos.a[axis.x] += realMulR(intToReal(coord.x),size);
	light_pos.a[axis.y] += realMulR(intToReal(coord.y),size);

    if(lighting_work_data_ptr >= countof(lighting_work_data) - 1)
		return;
    
	if(g_options.smooth_lighting){
		if(coord.y >= 0 && coord.y < (1 << depth)){
			if(!coord.x)
				lightingSide(voxel,lighting_data,block_pos,side,(Vec2i){coord.x - 1,coord.y + 0},depth,distance_max,cube_c,angle);
			if(coord.x == (1 << depth) - 1)
				lightingSide(voxel,lighting_data,block_pos,side,(Vec2i){coord.x + 1,coord.y + 0},depth,distance_max,cube_c,angle);
		}
		if(coord.x >= 0 && coord.x < (1 << depth)){
			if(!coord.y)
				lightingSide(voxel,lighting_data,block_pos,side,(Vec2i){coord.x + 0,coord.y - 1},depth,distance_max,cube_c,angle);
			if(coord.y == (1 << depth) - 1)
				lightingSide(voxel,lighting_data,block_pos,side,(Vec2i){coord.x + 0,coord.y + 1},depth,distance_max,cube_c,angle);
		}
	}

	int mipmap = mipmapGet(light_pos,normal,distance_max,angle);

	Vec3 position = light_pos;
	position.a[side >> 1] += side & 1 ? REAL_EPSILON : -REAL_EPSILON;
    
	LightingWorkData* light_data = lighting_data + lighting_work_data_ptr++;

	*light_data = (LightingWorkData){
		.position = position,
		.mipmap = mipmap,
		.voxel = voxel,
		.size = size,
        .u = g_u_table[side],
        .v = g_v_table[side],
        .side = side,
        .use_side = true,
	};
} 

static void lightingSideRecursive(Voxel* voxel,LightingWorkData* lighting_data,Vec3 block_pos,int side,Vec2i coord,int depth,real angle){
	Vec2i axis = g_axis_table[side];
	Vec3 block_pos_t = block_pos;
	real size = depthToSize(voxel->depth) / (1 << depth);
	block_pos_t.a[axis.x] += realMulR(intToReal(coord.x),size);
	block_pos_t.a[axis.y] += realMulR(intToReal(coord.y),size);

	Vec3 pos[4] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

	pos[1].a[axis.y] += size;
	pos[2].a[axis.x] += size;
	pos[3].a[axis.x] += size;
	pos[3].a[axis.y] += size;

	real distance_max = 0;
	int distance_max_index;

	for(int i = 0;i < 4;i++){
		real distance = vec3Dot(vec3Shr(g_surface.position,4),vec3Shr(pos[i],4));
		if(distance > distance_max){
			distance_max = distance;
			distance_max_index = i;
		}
	}

	distance_max = vec3Distance(vec3Shr(g_surface.position,4),vec3Shr(pos[distance_max_index],4));

	Vec3 cube_c = pos[0];

	cube_c.a[axis.x] = tClamp(g_surface.position.a[axis.x],pos[0].a[axis.x],pos[3].a[axis.x]);
	cube_c.a[axis.y] = tClamp(g_surface.position.a[axis.y],pos[0].a[axis.y],pos[3].a[axis.y]);

	Vec3 normal = g_normal_table[side];
	
	int mipmap = mipmapGet(squarePointClosestPosition(pos[0],size,normal),normal,distance_max,angle);

	int split = 25 + -mipmap - voxel->depth;
	
	if(depth < split){
		Vec3i v_pos = {voxel->position_x << depth,voxel->position_y << depth,voxel->position_z << depth};
		v_pos.a[axis.x] += coord.x;
		v_pos.a[axis.y] += coord.y;
		if(side & 1)
			v_pos.a[side >> 1] += (1 << depth) - 1;
#if 0
		if(!squareVisible(v_pos,voxel->depth + depth,side,voxel->type))
			return;
#endif
        if(!squareInScreenSpace(g_view_plane_lighting,pos))
			return;
        
		coord.x <<= 1;
		coord.y <<= 1;
		lightingSideRecursive(voxel,lighting_data,block_pos,side,(Vec2i){coord.x + 0,coord.y + 0},depth + 1,angle);
		lightingSideRecursive(voxel,lighting_data,block_pos,side,(Vec2i){coord.x + 0,coord.y + 1},depth + 1,angle);
		lightingSideRecursive(voxel,lighting_data,block_pos,side,(Vec2i){coord.x + 1,coord.y + 0},depth + 1,angle);
		lightingSideRecursive(voxel,lighting_data,block_pos,side,(Vec2i){coord.x + 1,coord.y + 1},depth + 1,angle);
		return;
	}
	lightingSide(voxel,lighting_data,block_pos,side,coord,depth,distance_max,cube_c,angle);
}

static void lightingSidePre(Voxel* voxel,LightingWorkData* lighting_data,Vec3 block_pos,int side){
	lightingSideRecursive(voxel,lighting_data,block_pos,side,(Vec2i){0},0,surfaceAngle(block_pos,g_normal_table[side]));
}

static void lightingSlope(Voxel* voxel,LightingWorkData* lighting_data,Vec3 block_pos,real block_size){
    VoxelStatic* voxel_s = g_voxel_static + voxel->type;

    Vec3Axis axis = voxel_s->slope_axis;
    bool flip_x = voxel_s->slope_flip_x;
    bool flip_y = voxel_s->slope_flip_y;
    
    Vec3Axis table[][3] = {{VEC3_Y,VEC3_Z},{VEC3_X,VEC3_Z},{VEC3_X,VEC3_Y}};

    Vec3 u = voxel_s->slope_u;
    Vec3 v = voxel_s->slope_v;
    
    Vec3 position = block_pos;
    position = vec3Add(position,vec3MulS(voxel_s->slope_offset,depthToSize(voxel->depth)));
    Vec3 normal = vec3Cross(u,v);
    
    Plane plane = {.normal = normal,vec3Dot(normal,position)};
    
    lightingSlopeRecursive(voxel,lighting_data,position,u,v,(Vec2i){0},0);

    if(g_surface.position.x - block_pos.x < 0)
        lightingSidePre(voxel,lighting_data,block_pos,0);
    if(g_surface.position.x - block_pos.x - block_size > 0)
        lightingSidePre(voxel,lighting_data,vec3Add(block_pos,(Vec3){block_size,0,0}),1);
    if(g_surface.position.y - block_pos.y < 0)
        lightingSidePre(voxel,lighting_data,block_pos,2);
    if(g_surface.position.y - block_pos.y - block_size > 0)
        lightingSidePre(voxel,lighting_data,vec3Add(block_pos,(Vec3){0,block_size,0}),3);
    if(g_surface.position.z - block_pos.z < 0)
        lightingSidePre(voxel,lighting_data,block_pos,4);
    if(g_surface.position.z - block_pos.z - block_size > 0)
        lightingSidePre(voxel,lighting_data,vec3Add(block_pos,(Vec3){0,0,block_size}),5);
}

static void lightingCollect(LightingWorkData* lighting_data){
    static int stack_depth;
    static struct{
        Voxel* voxel;
        int child_index;
    } stack[0x100] = {{.voxel = &g_world.voxel}};
    while(lighting_work_data_ptr < countof(lighting_work_data) - 1){
        Voxel* voxel = stack[stack_depth].voxel;
        real block_size = depthToSize(voxel->depth);
        Vec3 block_pos = voxelWorldPos(voxel);
        if(voxel->type == VOXEL_PARENT){
            Vec3 point[] = {
                {block_pos.x + 0,block_pos.y + 0,block_pos.z + 0},
                {block_pos.x + 0,block_pos.y + 0,block_pos.z + block_size},
                {block_pos.x + 0.,block_pos.y + block_size,block_pos.z + 0},
                {block_pos.x + 0,block_pos.y + block_size,block_pos.z + block_size},
                {block_pos.x + block_size,block_pos.y + 0,block_pos.z + 0},
                {block_pos.x + block_size,block_pos.y + 0,block_pos.z + block_size},
                {block_pos.x + block_size,block_pos.y + block_size,block_pos.z + 0},
                {block_pos.x + block_size,block_pos.y + block_size,block_pos.z + block_size},
            };
            if(!cubeInScreenSpace(g_view_plane_lighting,point))
                goto next;
            
            if(sdVoxelSquare(vec3Shr(g_surface.position,4),vec3Shr(block_pos,4),block_size / 16) > RENDER_DISTANCE)
                goto next;
            
            if(stack[stack_depth].child_index < 8){
                stack[stack_depth + 1].voxel = voxel->child_s[stack[stack_depth].child_index];
                stack[stack_depth + 1].child_index = 0;
                stack[stack_depth].child_index += 1;
                stack_depth += 1;
            }
            else{
                if(!stack_depth)
                    stack[stack_depth].child_index = 0;
                else
                    stack_depth -= 1;
            }
            continue;
        }
        VoxelStatic* voxel_s = g_voxel_static + voxel->type;
        if(voxel_s->slope){
            lightingSlope(voxel,lighting_data,block_pos,block_size);
        }
        else if(voxel->type == VOXEL_PLANE){
            lightingPlaneRecursive(voxel,lighting_data,block_pos,(Vec3i){0},(Plane){.normal = getLookDirection(voxel->angle)},voxel->distance);
            real preload_offset = FIXED_ONE;
            if(g_surface.position.x - block_pos.x < preload_offset)
                lightingSidePre(voxel,lighting_data,block_pos,0);
            if(g_surface.position.x - block_pos.x - block_size > -preload_offset)
                lightingSidePre(voxel,lighting_data,vec3Add(block_pos,(Vec3){block_size,0,0}),1);
            if(g_surface.position.y - block_pos.y < preload_offset)
                lightingSidePre(voxel,lighting_data,block_pos,2);
            if(g_surface.position.y - block_pos.y - block_size > -preload_offset)
                lightingSidePre(voxel,lighting_data,vec3Add(block_pos,(Vec3){0,block_size,0}),3);
            if(g_surface.position.z - block_pos.z < preload_offset)
                lightingSidePre(voxel,lighting_data,block_pos,4);
            if(g_surface.position.z - block_pos.z - block_size > -preload_offset)
                lightingSidePre(voxel,lighting_data,vec3Add(block_pos,(Vec3){0,0,block_size}),5);
        }
        else if(voxel_s->emiter){
            LightingWorkData* light_data = lighting_data + lighting_work_data_ptr++;

            *light_data = (LightingWorkData){
                .voxel = voxel,
                .emiter = true
            };
        }
        else if(!voxel_s->translucent){
            if(voxel->type == VOXEL_MOVABLE){
                if(voxel->opened)
                    block_pos.z -= realMulR(block_size,FIXED_ONE - voxel->animation);
                else
                    block_pos.z -= realMulR(block_size,voxel->animation);
            }
            real preload_offset = FIXED_ONE;
            if(g_surface.position.x - block_pos.x < preload_offset)
                lightingSidePre(voxel,lighting_data,block_pos,0);
            if(g_surface.position.x - block_pos.x - block_size > -preload_offset)
                lightingSidePre(voxel,lighting_data,vec3Add(block_pos,(Vec3){block_size,0,0}),1);
            if(g_surface.position.y - block_pos.y < preload_offset)
                lightingSidePre(voxel,lighting_data,block_pos,2);
            if(g_surface.position.y - block_pos.y - block_size > -preload_offset)
                lightingSidePre(voxel,lighting_data,vec3Add(block_pos,(Vec3){0,block_size,0}),3);
            if(g_surface.position.z - block_pos.z < preload_offset)
                lightingSidePre(voxel,lighting_data,block_pos,4);
            if(g_surface.position.z - block_pos.z - block_size > -preload_offset)
                lightingSidePre(voxel,lighting_data,vec3Add(block_pos,(Vec3){0,0,block_size}),5);
        }
    next:
        stack_depth -= 1;
    }
}

static void lightingTrace(void* arg_void){
	LightingTraceParameters* arg = arg_void;
    
	for(int i = arg->index;i < arg->index + arg->amount;i++){
		LightingWorkData* light_data = arg->data + i;

		Vec3 normal = vec3Cross(light_data->u,light_data->v);
#if 0
        PRINT_VAR(normal.x * 0x10000);
        PRINT_VAR(normal.y * 0x10000);
        PRINT_VAR(normal.z * 0x10000);
#endif   
		Vec3 luxel_pos = vec3Shr(light_data->position,light_data->mipmap);
		unsigned hash = luxelHashGet(luxel_pos,light_data->mipmap,normal);
		Luxel* luxel = luxelGet(hash);
        
		if(!luxel){
            luxel = g_luxel_cache + hash % N_LUXEL_CACHE;
            for(int i = 1;i < N_EVICT_TRIES;i++){
                Luxel* candidate = g_luxel_cache + (hash + i) % N_LUXEL_CACHE;
                int delta = g_time.frame_tick - luxel->tick_last_updated;
                int delta_candidate = g_time.frame_tick - candidate->tick_last_updated;
                if(delta < delta_candidate)
                    luxel = candidate;
            }
            *luxel = (Luxel){0};
			luxel->hash = hash;
		}

        if(luxel->tick_last_updated == g_time.frame_tick)
			continue;

        if(!(luxel->flags & LUXEL_DIRECTSAMPLED)){
            Vec3 direct_lighting = voxelEmit(&g_world.voxel,light_data->position,light_data->mipmap,normal,false);

            Voxel* voxel = voxelPositionGet(light_data->position);
            if(voxelTranslucent(voxel)){
                if(g_world.skylight){
                    Vec3 direction = getLookDirection(g_world.skylight_angle);
                    if(!treeRayTraceAndInit(light_data->position,direction,0,(TreeTraceFlags){.entity = false})){
                        Vec3 luminance = g_world.skylight_luminance;
                        luminance = vec3MulS(luminance,vec3Dot(normal,direction));
                        direct_lighting = vec3Add(direct_lighting,luminance);
                    }
                }
            }
            luxel->n_sample = tMin(luxel->n_sample + 1 & ~LUXEL_DIRECTSAMPLED,N_LUXEL_SAMPLE) | (luxel->n_sample & LUXEL_DIRECTSAMPLED);
            luxel->flags |= LUXEL_DIRECTSAMPLED;
            luxel->luminance_direct = direct_lighting;
        }
        else{
            Vec3 position = light_data->position;
            position = vec3Add(position,vec3MulS(light_data->u,realMulR(realRandom(FIXED_ONE),light_data->size) - light_data->size / 2));
            position = vec3Add(position,vec3MulS(light_data->v,realMulR(realRandom(FIXED_ONE),light_data->size) - light_data->size / 2));
            if(light_data->use_side){
                Vec2i axis = g_axis_table[light_data->side];
                position.a[axis.x] += realMulR(realRandom(FIXED_ONE),light_data->size) - light_data->size / 2;
                position.a[axis.y] += realMulR(realRandom(FIXED_ONE),light_data->size) - light_data->size / 2;
            }
            Vec3 luminance = luminanceQuery(light_data->voxel,normal,position,luxel->n_sample & ~LUXEL_DIRECTSAMPLED);
            luminance = vec3Shr(luminance,4);
            luxel->n_sample = tMin(luxel->n_sample + 1 & ~LUXEL_DIRECTSAMPLED,N_LUXEL_SAMPLE) | (luxel->n_sample & LUXEL_DIRECTSAMPLED);
            luxel->luminance = vec3Mix(luxel->luminance,luminance,FIXED_ONE / (luxel->n_sample & ~LUXEL_DIRECTSAMPLED));
            luxel->tick_last_updated = g_time.frame_tick;
        }
	}
}

void lightingOctree(void){
    if(!g_options.lighting_engine)
        return;
	for(int i = 0;i < countof(luxel_dynamic_cache);i++){
		luxel_dynamic_cache[i].hash = 0;
		luxel_dynamic_cache[i].luminance = (Vec3){0};
	}
	
	lightingCollect(lighting_work_data);

	LightingTraceParameters thread_arguments[MAX_THREAD];

	for(int i = 0;i < g_n_threads;i++){
		thread_arguments[i] = (LightingTraceParameters){
			.amount = lighting_work_data_ptr / g_n_threads,
			.index = lighting_work_data_ptr / g_n_threads * i,
			.data = lighting_work_data,
		};
		if(i == g_n_threads - 1)
			thread_arguments[i].amount += lighting_work_data_ptr % g_n_threads;
	}
    threadWork(lightingTrace,thread_arguments,sizeof *thread_arguments);

	lighting_work_data_ptr = 0;
}
