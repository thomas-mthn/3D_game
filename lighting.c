#include "octree.h"
#include "main.h"
#include "lighting.h"
#include "memory.h"
#include "vec3.h"
#include "thread.h"

#ifdef __linux__
#include "linux/l_main.h"
#include <pthread.h>
#elif !defined(__wasm__)
#include "win32/w_main.h"
#include "win32/w_kernel.h"
#include <immintrin.h>
#endif

Luxel g_luxel_cache[N_LUXEL_CACHE];
static Luxel luxel_dynamic_cache[0x100000];

//only works on axis aligned squares
Vec3 squarePointClosestPosition(Vec3 square_pos,int square_size,Vec3 normal) {
	int s = square_size / 2;
	int neg_s = -s;
	int pos_s = s;

	Vec3 d = vec3SubR(g_position, square_pos);
	int t = vec3Dot(d, normal);
	Vec3 Q = vec3SubR(g_position, vec3MulRS(normal,t));

	Vec3 QC = vec3SubR(Q, square_pos);

	int x_clamped = tClamp(QC.x, neg_s, pos_s);
	int y_clamped = tClamp(QC.y, neg_s, pos_s);
	int z_clamped = 0;

	Vec3 R = vec3AddR(square_pos,(Vec3){x_clamped,y_clamped,z_clamped});
	return R;
}

Luxel* luxelDynamicGet(unsigned hash){
	return luxel_dynamic_cache + hash % countof(luxel_dynamic_cache);
}

Vec3 skyboxSample(Vec3 direction){
	/*
	if(direction.x < 0 && direction.y < 0)
		return (Vec3){FIXED_ONE * 8,FIXED_ONE * 8,FIXED_ONE * 2};
	return (Vec3){0};
	*/
	return vec3Single(direction.z / 4 + FIXED_ONE / 2);
}

typedef enum{
	RAYLUMINANCE_FULLTRACE = (1 << 0),
	RAYLUMINANCE_NOEMIT    = (1 << 1),
} RayLuminanceFlag;

static Vec3 rayLuminanceRecursive(TraverseInit init,Vec3 position,Vec3 direction,int depth,RayLuminanceFlag flags){
	if(!depth)
		return vec3Single(0);

	int side;
	Voxel* voxel = treeRayTrace(init.voxel,init.pos,direction,&side);

	if(!voxel)
		return skyboxSample(direction);

	VoxelStatic* voxel_s = g_voxel_static + voxel->type;

	if(voxel_s->flags & VOXEL_EMITER)
		return flags & RAYLUMINANCE_NOEMIT ? (Vec3){0} : vec3ShrR(voxel_s->color,4);

	Vec3 end_pos = rayHitPosition(voxel,position,direction,side);

	if(voxel->type == VOXEL_MIRROR){
		Vec3 normal = g_normal_table[side << 1 | (((int*)&direction)[side] < 0)];
		Vec3 relative = vec3SubR(end_pos,position);
		Vec3 offset = vec3Reflect(relative,normal);
		return vec3MulRS(rayLuminanceRecursive(initTraverse(end_pos),end_pos,offset,depth - 1,flags),FIXED_ONE - (FIXED_ONE / 8));
	}

	if(voxel->type == VOXEL_GLASS){
		((int*)&end_pos)[side] -= ((int*)&direction)[side] < 0 ? 0x10 : -0x10;
		return rayLuminanceRecursive(initTraverse(end_pos),end_pos,direction,depth - 1,flags);
	}
	if(voxel->type == VOXEL_WATER){
		((int*)&end_pos)[side] -= ((int*)&direction)[side] < 0 ? 0x10 : -0x10;
		return rayLuminanceRecursive(initTraverse(end_pos),end_pos,direction,depth - 1,flags);
	}

	if(flags & RAYLUMINANCE_FULLTRACE){
		Vec3 normal = g_normal_table[side << 1 | (((int*)&direction)[side] < 0)];
		Vec3 offset = normal;
		Vec3 rnd_vector = vec3Rnd();
		vec3Add(&offset,rnd_vector);
		Vec3 direction_new = vec3Normalize(offset);
		Vec3 color = rayLuminanceRecursive(initTraverse(end_pos),end_pos,direction_new,depth - 1,flags);

		Vec2 uv = voxelLocalPositionGet(voxel,position,direction,side);
		
		uv.x = fixedMulR(fixedMulR(uv.x,voxel_s->texture_size),depthToSize(voxel->depth)) >> 4;
		uv.y = fixedMulR(fixedMulR(uv.y,voxel_s->texture_size),depthToSize(voxel->depth)) >> 4;

		if(!voxel_s->texture)
			return vec3MulR(color,voxel_s->color);
		Vec3 texel = vec3ShrR(pixelColorToColor(textureLookup(voxel_s->texture,uv.x,uv.y,0)),4);
		return vec3MulR(color,texel);
	}
	int mipmap = tClamp(mipmapGet(end_pos,g_normal_table[side << 1],vec3Distance(vec3ShrR(end_pos,4),vec3ShrR(g_position,4)),surfaceAngle(position,g_normal_table[side << 1])),26 - LUXEL_MAX_MIPMAP,31);

	Vec3 luxel_pos = vec3ShrR(end_pos,mipmap);
	unsigned hash = luxelHashGet(luxel_pos,mipmap);
	Luxel* luxel = luxelGet(hash);

	if(luxel->hash == hash){
		Vec3 luminance = vec3ShrR(luxel->luminance,4);

		if(!voxel_s->texture)
			return vec3MulR(luminance,voxel_s->color);

		Vec2 uv = voxelLocalPositionGet(voxel,position,direction,side);
		
		uv.x = fixedMulR(fixedMulR(uv.x,voxel_s->texture_size),depthToSize(voxel->depth)) >> 4;
		uv.y = fixedMulR(fixedMulR(uv.y,voxel_s->texture_size),depthToSize(voxel->depth)) >> 4;

		Vec3 texel = vec3ShrR(pixelColorToColor(textureLookup(voxel_s->texture,uv.x,uv.y,3)),4);
		return vec3MulR(luminance,texel);
	}
	return vec3Single(0);
}

static bool voxelTranslucent(Voxel* voxel){
    return
        g_voxel_static[voxel->type].flags & VOXEL_TRANSLUCENT || voxel->type == VOXEL_AIR ||
        voxel->animation || voxel->opened;
}

Vec3 rayLuminance(Vec3 position,Vec3 direction){
	TraverseInit init = initTraverse(position);
	Voxel* voxel = voxelPositionGet(position);
    if(!voxelTranslucent(voxel))
        return (Vec3){0};
	return rayLuminanceRecursive(init,position,direction,8,RAYLUMINANCE_NOEMIT);
}

Vec3 rayLuminanceInit(TraverseInit init,Vec3 position,Vec3 direction){
	return rayLuminanceRecursive(init,position,direction,8,0);
}

Vec3 rayLuminanceTrace(Vec3 position,Vec3 direction){
	return rayLuminanceRecursive(initTraverse(position),position,direction,8,RAYLUMINANCE_FULLTRACE);
}

Vec3 luminanceQuery(Voxel* voxel,Vec3 normal,Vec3 position){
	VoxelStatic* voxel_s = g_voxel_static + voxel->type;
	Vec3 offset;
	if(voxel->type == VOXEL_METALLIC){
		offset = normal;
		offset.x += tRnd() % (FIXED_ONE * 2) - FIXED_ONE;
		offset.y += tRnd() % (FIXED_ONE * 2) - FIXED_ONE;
		offset.z += tRnd() % (FIXED_ONE * 2) - FIXED_ONE;
		offset = vec3Normalize(offset);
		Vec3 relative = vec3SubR(position,g_position);
		Vec3 reflect = vec3Normalize(vec3Reflect(vec3ShrR(relative,8),normal));
		offset = vec3Mix(offset,reflect,FIXED_ONE / 2);
	}
	else{
		offset = normal;
		Vec3 rnd_vector = vec3Rnd();
		vec3Add(&offset,rnd_vector);
		offset = vec3Normalize(offset);
	}
	return rayLuminance(position,offset);
}

structure(LightingWorkData){
	Voxel* voxel;
	Vec3 position;
	Vec3 light_position;
	int side;
	int mipmap;
	int size;
    bool emiter;
};

static int g_lighting_work_data_ptr;

static void lightingSide(Voxel* voxel,LightingWorkData* lighting_data,Vec3 block_pos,int side,Vec2 coord,int depth,int distance_max,Vec3 cube_c,int angle){
	Vec3 normal = g_normal_table[side];
	Vec2 axis = g_axis_table[side];
	Vec3 light_pos = block_pos;
	int size = depthToSize(voxel->depth) / (1 << depth);
	((int*)&light_pos)[axis.x] += fixedMulR(coord.x << FIXED_PRECISION,size);
	((int*)&light_pos)[axis.y] += fixedMulR(coord.y << FIXED_PRECISION,size);

	if(g_smooth_lighting){
		if(coord.y >= 0 && coord.y < (1 << depth)){
			if(!coord.x)
				lightingSide(voxel,lighting_data,block_pos,side,vec2AddR(coord,(Vec2){-1,0}),depth,distance_max,cube_c,angle);
			if(coord.x == (1 << depth) - 1)
				lightingSide(voxel,lighting_data,block_pos,side,vec2AddR(coord,(Vec2){1,0}),depth,distance_max,cube_c,angle);
		}
		if(coord.x >= 0 && coord.x < (1 << depth)){
			if(!coord.y)
				lightingSide(voxel,lighting_data,block_pos,side,vec2AddR(coord,(Vec2){0,-1}),depth,distance_max,cube_c,angle);
			if(coord.y == (1 << depth) - 1)
				lightingSide(voxel,lighting_data,block_pos,side,vec2AddR(coord,(Vec2){0,1}),depth,distance_max,cube_c,angle);
		}
	}

	int mipmap = tClamp(mipmapGet(light_pos,normal,distance_max,angle),26 - LUXEL_MAX_MIPMAP,31);

	Vec3 position = light_pos;
	((int*)&light_pos)[side >> 1] += side & 1 ? 0x10 : -0x10;

	if(g_lighting_work_data_ptr == 0x100000)
		return;
	LightingWorkData* light_data = lighting_data + g_lighting_work_data_ptr++;

	*light_data = (LightingWorkData){
		.position = position,
		.light_position = light_pos,
		.mipmap = mipmap,
		.side = side,
		.voxel = voxel,
		.size = size
	};
} 

static void lightingSideRecursive(Voxel* voxel,LightingWorkData* lighting_data,Vec3 block_pos,int side,Vec2 coord,int depth,int angle){
	Vec2 axis = g_axis_table[side];
	Vec3 block_pos_t = block_pos;
	int size = depthToSize(voxel->depth) / (1 << depth);
	((int*)&block_pos_t)[axis.x] += fixedMulR(coord.x << FIXED_PRECISION,size);
	((int*)&block_pos_t)[axis.y] += fixedMulR(coord.y << FIXED_PRECISION,size);

	Vec3 pos[4] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

	((int*)&pos[1])[axis.y] += size;
	((int*)&pos[2])[axis.x] += size;
	((int*)&pos[3])[axis.x] += size;
	((int*)&pos[3])[axis.y] += size;

	int distance_max = 0;
	int distance_max_index;

	for(int i = 0;i < 4;i++){
		int distance = vec3Dot(vec3ShrR(g_position,4),vec3ShrR(pos[i],4));
		if(distance > distance_max){
			distance_max = distance;
			distance_max_index = i;
		}
	}

	distance_max = vec3Distance(vec3ShrR(g_position,4),vec3ShrR(pos[distance_max_index],4));

	Vec3 cube_c = pos[0];	

	((int*)&cube_c)[axis.x] = tClamp(((int*)&g_position)[axis.x],((int*)&pos[0])[axis.x],((int*)&pos[3])[axis.x]);
	((int*)&cube_c)[axis.y] = tClamp(((int*)&g_position)[axis.y],((int*)&pos[0])[axis.y],((int*)&pos[3])[axis.y]);

	Vec3 normal = g_normal_table[side];
	
	int mipmap = tClamp(mipmapGet(squarePointClosestPosition(pos[0],size,normal),normal,distance_max,angle),26 - LUXEL_MAX_MIPMAP,31);

	int split = 25 + -mipmap - voxel->depth;
	
	if(depth < split){
		Vec3 v_pos = vec3ShlR((Vec3){voxel->position_x,voxel->position_y,voxel->position_z},depth);
		((int*)&v_pos)[axis.x] += coord.x;
		((int*)&v_pos)[axis.y] += coord.y;
		if(side & 1)
			((int*)&v_pos)[side >> 1] += (1 << depth) - 1;
		
		if(sdSquare(vec3ShrR(g_position,4),vec3ShrR(block_pos_t,4),size >> 4,side) > RENDER_DISTANCE)
			return;

		if(!squareVisible(v_pos,voxel->depth + depth,side,voxel->type))
			return;
		
		coord.x <<= 1;
		coord.y <<= 1;
		lightingSideRecursive(voxel,lighting_data,block_pos,side,vec2AddR(coord,(Vec2){0,0}),depth + 1,angle);
		lightingSideRecursive(voxel,lighting_data,block_pos,side,vec2AddR(coord,(Vec2){0,1}),depth + 1,angle);
		lightingSideRecursive(voxel,lighting_data,block_pos,side,vec2AddR(coord,(Vec2){1,0}),depth + 1,angle);
		lightingSideRecursive(voxel,lighting_data,block_pos,side,vec2AddR(coord,(Vec2){1,1}),depth + 1,angle);
		return;
	}
	lightingSide(voxel,lighting_data,block_pos,side,coord,depth,distance_max,cube_c,angle);
}

static void lightingSidePre(Voxel* voxel,LightingWorkData* lighting_data,Vec3 block_pos,int side){
	lightingSideRecursive(voxel,lighting_data,block_pos,side,(Vec2){0,0},0,surfaceAngle(block_pos,g_normal_table[side]));
}

static void lightingCollect(Voxel* voxel,LightingWorkData* lighting_data){
	int block_size = depthToSize(voxel->depth);
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
		if(!cubeInScreenSpace(point))
			return;
		if(sdVoxel(vec3ShrR(g_position,4),vec3ShrR(block_pos,4),block_size >> 4) > RENDER_DISTANCE)
			return;
		for(int i = 0;i < 8;i++)
			lightingCollect(voxel->child_s[i],lighting_data);
		return;
	}
	if(voxel->type == VOXEL_AIR || voxel->type == VOXEL_MIRROR || voxel->type == VOXEL_GLASS)
		return;
    if(g_voxel_static[voxel->type].flags & VOXEL_EMITER){
        LightingWorkData* light_data = lighting_data + g_lighting_work_data_ptr++;

        *light_data = (LightingWorkData){
            .voxel = voxel,
            .emiter = true
        };
        return;
    }   
	if(voxel->type == VOXEL_MOVABLE){
		if(voxel->opened)
			block_pos.z -= fixedMulR(block_size,FIXED_ONE - voxel->animation);
		else
			block_pos.z -= fixedMulR(block_size,voxel->animation);
	}
	if(g_position.x - block_pos.x < 0)
		lightingSidePre(voxel,lighting_data,block_pos,0);
	if(g_position.x - block_pos.x - block_size > 0)
		lightingSidePre(voxel,lighting_data,vec3AddR(block_pos,(Vec3){block_size,0,0}),1);
	if(g_position.y - block_pos.y < 0)
		lightingSidePre(voxel,lighting_data,block_pos,2);
	if(g_position.y - block_pos.y - block_size > 0)
		lightingSidePre(voxel,lighting_data,vec3AddR(block_pos,(Vec3){0,block_size,0}),3);
	if(g_position.z - block_pos.z < 0)
		lightingSidePre(voxel,lighting_data,block_pos,4);
	if(g_position.z - block_pos.z - block_size > 0)
		lightingSidePre(voxel,lighting_data,vec3AddR(block_pos,(Vec3){0,0,block_size}),5);
}

structure(LightingTraceParameters){
	int index;
	int amount;
	LightingWorkData* data;
};

#define N_LUXEL_SAMPLE 0x1000
#define EMIT_RANGE 0x10000
/*
static void voxelEmitSideRecursive(Voxel* voxel,Vec3 block_pos,int side,Vec2 coord,int depth,int surface_angle,Vec3 emiter_position,Voxel* emiter){
	Vec2 axis = g_axis_table[side];
	Vec3 block_pos_t = block_pos;
	int size = depthToSize(voxel->depth) / (1 << depth);
	((int*)&block_pos_t)[axis.x] += fixedMulR(coord.x << FIXED_PRECISION,size);
	((int*)&block_pos_t)[axis.y] += fixedMulR(coord.y << FIXED_PRECISION,size);

	Vec3 pos[4] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

	((int*)&pos[1])[axis.y] += size;
	((int*)&pos[2])[axis.x] += size;
	((int*)&pos[3])[axis.x] += size;
	((int*)&pos[3])[axis.y] += size;

	int distance_max = 0;
	int distance_max_index;

	for(int i = 0;i < 4;i++){
		int distance = vec3Dot(vec3ShrR(g_position,4),vec3ShrR(pos[i],4));
		if(distance > distance_max){
			distance_max = distance;
			distance_max_index = i;
		}
	}

	distance_max = vec3Distance(vec3ShrR(g_position,4),vec3ShrR(pos[distance_max_index],4));

	Vec3 cube_c = pos[0];	

	((int*)&cube_c)[axis.x] = tClamp(((int*)&g_position)[axis.x],((int*)&pos[0])[axis.x],((int*)&pos[3])[axis.x]);
	((int*)&cube_c)[axis.y] = tClamp(((int*)&g_position)[axis.y],((int*)&pos[0])[axis.y],((int*)&pos[3])[axis.y]);

	Vec3 normal = g_normal_table[side];
	
	int mipmap = tClamp(mipmapGet(squarePointClosestPosition(pos[0],size,normal),normal,distance_max,surface_angle),26 - LUXEL_MAX_MIPMAP,31);

	int split = 25 + -mipmap - voxel->depth;

    if(depth < split){
		Vec3 v_pos = vec3ShlR((Vec3){voxel->position_x,voxel->position_y,voxel->position_z},depth);
		((int*)&v_pos)[axis.x] += coord.x;
		((int*)&v_pos)[axis.y] += coord.y;
		if(side & 1)
			((int*)&v_pos)[side >> 1] += (1 << depth) - 1;
		
		if(!voxel->opened && !voxel->animation && !squareVisible(v_pos,voxel->depth + depth,side,voxel->type))
			return;
		
		if(sdSquare(vec3ShrR(emiter_position,4),vec3ShrR(block_pos_t,4),size >> 4,side) > EMIT_RANGE)
			return;
        
		if(!squareInScreenSpace(pos))
			return;
		
		coord.x <<= 1;
		coord.y <<= 1;
		voxelEmitSideRecursive(voxel,block_pos,side,vec2AddR(coord,(Vec2){0,0}),depth + 1,surface_angle,emiter_position,emiter);
		voxelEmitSideRecursive(voxel,block_pos,side,vec2AddR(coord,(Vec2){0,1}),depth + 1,surface_angle,emiter_position,emiter);
		voxelEmitSideRecursive(voxel,block_pos,side,vec2AddR(coord,(Vec2){1,0}),depth + 1,surface_angle,emiter_position,emiter);
		voxelEmitSideRecursive(voxel,block_pos,side,vec2AddR(coord,(Vec2){1,1}),depth + 1,surface_angle,emiter_position,emiter);
		return;
	}
    Vec3 light_pos = block_pos;
	size = depthToSize(voxel->depth) / (1 << depth);
	((int*)&light_pos)[axis.x] += fixedMulR(coord.x << FIXED_PRECISION,size) + size / 2;
	((int*)&light_pos)[axis.y] += fixedMulR(coord.y << FIXED_PRECISION,size) + size / 2;

    tClamp(mipmapGet(light_pos,normal,distance_max,surface_angle),26 - LUXEL_MAX_MIPMAP,31);
    
    Vec3 luxel_pos = vec3ShrR(light_pos,mipmap);
    unsigned hash = luxelHashGet(luxel_pos,mipmap);
    Luxel* luxel = luxelGet(hash);

    if(luxel->tick_last_updated == g_tick)
        return;

    if(luxel->hash != hash){
        Vec3 luxel_pos_up = vec3ShrR(light_pos,mipmap + 1);
        unsigned hash_up = luxelHashGet(luxel_pos_up,mipmap + 1);
        Luxel* luxel_up = luxelGet(hash_up);

        if(luxel_up->hash == hash_up){
            luxel->luminance = luxel_up->luminance;
            luxel->n_sample = luxel_up->n_sample / 4;
        }
        else{
            luxel->n_sample = 0;
        }
        luxel->hash = hash;
    }

    ((int*)&light_pos)[side >> 1] += side & 1 ? 0x10 : -0x10;
    int emiter_size = depthToSize(emiter->depth);
    int distance = sdVoxel(light_pos,voxelWorldPos(emiter),emiter_size);
    
    int intensity = fixedDivR(1 << tMax(16 - emiter->depth,0),fixedMulR(distance,distance));

    Vec3 direction = vec3Direction(emiter_position,light_pos);
    int angle = -vec3Dot(direction,normal);
    
    if(angle < 0)
        return;

    if(!voxelTranslucent(voxelPositionGet(light_pos)))
        return;
    
    if(treeRayTraceAndInit(light_pos,vec3Direction(light_pos,emiter_position),0) != emiter)
        return;
    
    int n = luxel->n_sample < 0x100 ? 16 : 1;
    for(int i = n;i--;){
        Vec3 luminance = vec3MulRS(vec3MulRS(g_voxel_static[emiter->type].color,intensity),angle);
        luminance = vec3ShlR(direction,4);
        luminance.x = tAbs(luminance.x);
        luminance.y = tAbs(luminance.y);
        luminance.z = tAbs(luminance.z);
        //luxel->n_sample = tMin(++luxel->n_sample,N_LUXEL_SAMPLE);
        vec3Add(&luxel->luminance,luminance);
        //luxel->luminance = vec3Mix(luxel->luminance,luminance,FIXED_ONE / luxel->n_sample);	
    }
}
*/
static Vec3 voxelEmit(Voxel* voxel,Vec3 light_pos,int mipmap,Vec3 normal){
    if(voxel->type == VOXEL_PARENT){
        Vec3 voxel_position = voxelWorldPos(voxel);
        
        if(sdVoxel(light_pos,voxel_position,depthToSize(voxel->depth)) / 0x40000 > voxel->emission)
            return (Vec3){0};
        
        Vec3 luminance = {0};
        for(int i = 8;i--;)
            vec3Add(&luminance,voxelEmit(voxel->child_s[i],light_pos,mipmap,normal));
        return luminance;
    }
    if(g_voxel_static[voxel->type].flags & VOXEL_EMITER){
        int emiter_size = depthToSize(voxel->depth);
        Vec3 emiter_position = vec3AddRS(voxelWorldPos(voxel),emiter_size / 2);
        int distance = vec3Distance(light_pos,emiter_position);
    
        int intensity = fixedDivR(1 << tMax(26 - voxel->depth,0),fixedMulR(distance,distance));
    
        Vec3 direction = vec3Direction(emiter_position,light_pos);
        int angle = -vec3Dot(direction,normal);
    
        if(angle < 0)
            return (Vec3){0};

        if(!voxelTranslucent(voxelPositionGet(light_pos)))
            return (Vec3){0};
    
        if(treeRayTraceAndInit(light_pos,vec3Direction(light_pos,emiter_position),0) != voxel)
            return (Vec3){0};
    
        return vec3MulRS(vec3MulRS(g_voxel_static[voxel->type].color,intensity),angle);
        //luxel->n_sample = tMin(++luxel->n_sample,N_LUXEL_SAMPLE);

        //luxel->luminance = vec3Mix(luxel->luminance,luminance,FIXED_ONE / luxel->n_sample);

    }
    return (Vec3){0};
}

/*
static void voxelEmit(Vec3 emiter_position,Voxel* emiter,Voxel* voxel){
    int block_size = depthToSize(voxel->depth);
    Vec3 block_pos = voxelWorldPos(voxel);
    if(voxel->type == VOXEL_PARENT){
        
        if(sdVoxel(vec3ShrR(emiter_position,4),vec3ShrR(block_pos,4),block_size >> 4) > EMIT_RANGE)
            return;
        
        for(int i = 8;i--;)
            voxelEmit(emiter_position,emiter,voxel->child_s[i]);
    }
    if(voxel->type == VOXEL_GLASS || voxel->type == VOXEL_MIRROR || voxel->type == VOXEL_WATER)
        return;
    if(g_position.x - block_pos.x < 0)
		voxelEmitSideRecursive(voxel,block_pos,0,(Vec2){0},0,surfaceAngle(block_pos,g_normal_table[0]),emiter_position,emiter);
	if(g_position.x - block_pos.x - block_size > 0)
		voxelEmitSideRecursive(voxel,vec3AddR(block_pos,(Vec3){block_size,0,0}),1,(Vec2){0},0,surfaceAngle(block_pos,g_normal_table[1]),emiter_position,emiter);
	if(g_position.y - block_pos.y < 0)
		voxelEmitSideRecursive(voxel,block_pos,2,(Vec2){0},0,surfaceAngle(block_pos,g_normal_table[2]),emiter_position,emiter);
	if(g_position.y - block_pos.y - block_size > 0)
		voxelEmitSideRecursive(voxel,vec3AddR(block_pos,(Vec3){0,block_size,0}),3,(Vec2){0},0,surfaceAngle(block_pos,g_normal_table[3]),emiter_position,emiter);
	if(g_position.z - block_pos.z < 0)
		voxelEmitSideRecursive(voxel,block_pos,4,(Vec2){0},0,surfaceAngle(block_pos,g_normal_table[4]),emiter_position,emiter);
	if(g_position.z - block_pos.z - block_size > 0)
		voxelEmitSideRecursive(voxel,vec3AddR(block_pos,(Vec3){0,0,block_size}),5,(Vec2){0},0,surfaceAngle(block_pos,g_normal_table[5]),emiter_position,emiter);
}
*/

static THREAD_ENTRY lightingTrace(void* arg_void){
	LightingTraceParameters* arg = arg_void;
	for(int i = arg->index;i < arg->index + arg->amount;i++){
		LightingWorkData* light_data = arg->data + i;

		Vec3 luxel_pos = vec3ShrR(light_data->position,light_data->mipmap);
		unsigned hash = luxelHashGet(luxel_pos,light_data->mipmap);
		Luxel* luxel = luxelGet(hash);
		int n = 1;

		if(luxel->tick_last_updated == g_tick)
			continue;

		if(luxel->hash != hash){
			Vec3 luxel_pos_up = vec3ShrR(light_data->position,light_data->mipmap + 1);
			unsigned hash_up = luxelHashGet(luxel_pos_up,light_data->mipmap + 1);
			Luxel* luxel_up = luxelGet(hash_up);

			if(luxel_up->hash == hash_up){
				luxel->luminance = luxel_up->luminance;
				luxel->n_sample = luxel_up->n_sample / 4;
			}
			else{
				luxel->n_sample = 0;
			}
			luxel->hash = hash;
		}

		Vec2 axis   = g_axis_table[light_data->side];
		Vec3 normal = g_normal_table[light_data->side];
        
		if(luxel->n_sample < 0x100)
			n = 16;
		else
#ifdef __wasm__
			n = 0;
#else
        n = 1;
#endif
        while(n--){
            if(luxel->n_sample % 0x40 == 0x20){
                Vec3 direct_lighting = voxelEmit(&g_voxel,light_data->light_position,light_data->mipmap,g_normal_table[light_data->side]);
                Vec3 luxel_pos = vec3ShrR(light_data->position,light_data->mipmap);
                unsigned hash = luxelHashGet(luxel_pos,light_data->mipmap);
                Luxel* luxel = luxelGet(hash);

                int n = 1;

                if(luxel->tick_last_updated == g_tick)
                    continue;

                if(luxel->hash != hash){
                    Vec3 luxel_pos_up = vec3ShrR(light_data->position,light_data->mipmap + 1);
                    unsigned hash_up = luxelHashGet(luxel_pos_up,light_data->mipmap + 1);
                    Luxel* luxel_up = luxelGet(hash_up);

                    if(luxel_up->hash == hash_up){
                        luxel->luminance = luxel_up->luminance;
                        luxel->n_sample = luxel_up->n_sample / 4;
                    }
                    else{
                        luxel->n_sample = 0;
                    }
                    luxel->hash = hash;
                }
                luxel->n_sample = tMin(++luxel->n_sample,N_LUXEL_SAMPLE);
                luxel->luminance = vec3Mix(luxel->luminance,vec3ShlR(direct_lighting,6),FIXED_ONE / luxel->n_sample);
                continue;
            }

            //SUN
            /*
            if(tRnd() % 0x10 == 0){
                Vec3 luminance = {0};
                if(!treeRayTraceAndInit(light_data->light_position,vec3Normalize((Vec3){FIXED_ONE,FIXED_ONE,g_tick}),0))
                    luminance = vec3ShlR(pixelColorToColor(0xC0A080),6);
                luxel->n_sample = tMin(++luxel->n_sample,N_LUXEL_SAMPLE);
                luxel->luminance = vec3Mix(luxel->luminance,luminance,FIXED_ONE / luxel->n_sample);
                continue;
            }
            */
			Vec3 luminance;
  			/*
			if(luxel->n_sample % 0x40 == 0x20){
				if(!treeRayTraceAndInit(light_data->light_position,vec3Normalize((Vec3){FIXED_ONE,FIXED_ONE * 4,FIXED_ONE}),0)){
					luminance = (Vec3){FIXED_ONE << 6,FIXED_ONE << 5,FIXED_ONE};
				}
				else{
					luminance = luxel->luminance;
				}
				vec3Shl(&luminance,4);
				luxel->n_sample = tMin(++luxel->n_sample,0x1000);
				luxel->luminance = vec3Mix(luxel->luminance,luminance,FIXED_ONE / luxel->n_sample);
				continue;
			}
			*/
            
			Vec3 position = light_data->light_position;
			((int*)&position)[axis.x] += fixedMulR(tRnd() % FIXED_ONE,light_data->size) - light_data->size / 2;
			((int*)&position)[axis.y] += fixedMulR(tRnd() % FIXED_ONE,light_data->size) - light_data->size / 2;
			luminance = luminanceQuery(light_data->voxel,normal,position);
			vec3Shl(&luminance,4);
			luxel->n_sample = tMin(++luxel->n_sample,N_LUXEL_SAMPLE);
			luxel->luminance = vec3Mix(luxel->luminance,luminance,FIXED_ONE / luxel->n_sample);
            
    
        }
    //luxel->tick_last_updated = g_tick;
	}
}

void lightingOctree(void){
	for(int i = 0;i < countof(luxel_dynamic_cache);i++){
		luxel_dynamic_cache[i].hash = 0;
		luxel_dynamic_cache[i].luminance = (Vec3){0};
	}
		
	LightingWorkData* lighting_work_data = memoryScratchGet(MEMORY_SCRATCH_MAX_SIZE);
	lightingCollect(&g_voxel,lighting_work_data);
#if defined(__linux__) || defined(_MSC_VER)
	thread_t threads[16];

	LightingTraceParameters thread_arguments[countof(threads)];
	int n_thread = tClamp(g_n_threads - 1,1,countof(threads));

	for(int i = 0;i < n_thread;i++){
		thread_arguments[i] = (LightingTraceParameters){
			.amount = g_lighting_work_data_ptr / n_thread,
			.index = g_lighting_work_data_ptr / n_thread * i,
			.data = lighting_work_data,
		};
		if(i == n_thread - 1)
			thread_arguments[i].amount += g_lighting_work_data_ptr % n_thread;
		threads[i] = threadCreate(lightingTrace,thread_arguments + i);
	}
	threadWait(threads,n_thread);
    
    /*
	  WaitForMultipleObjects(n_thread,threads,true,INFINITE);

	for(int i = 0;i < n_thread;i++)
		CloseHandle(threads[i]);
    */
#else
	LightingTraceParameters arg = {
		.amount = g_lighting_work_data_ptr,
		.data = lighting_work_data,
	};
	lightingTrace(&arg);
#endif
	g_lighting_work_data_ptr = 0;
}
