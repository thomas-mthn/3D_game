#include "fixed.h"
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
Vec3 squarePointClosestPosition(Vec3 square_pos,int square_size,Vec3 normal){
	int s = square_size / 2;
	int neg_s = -s;
	int pos_s = s;

	Vec3 d = vec3Sub(g_position, square_pos);
	int t = vec3Dot(d, normal);
	Vec3 Q = vec3Sub(g_position, vec3MulS(normal,t));

	Vec3 QC = vec3Sub(Q, square_pos);

	int x_clamped = tClamp(QC.x, neg_s, pos_s);
	int y_clamped = tClamp(QC.y, neg_s, pos_s);
	int z_clamped = 0;

	Vec3 R = vec3Add(square_pos,(Vec3){x_clamped,y_clamped,z_clamped});
	return R;
}

Luxel* luxelDynamicGet(unsigned hash){
	return luxel_dynamic_cache + hash % countof(luxel_dynamic_cache);
}

Vec3 skyboxSample(Vec3 direction){
	return vec3Single(direction.z / 4 + FIXED_ONE / 2);
}

structure(RayLuminanceFlag){
    bool fulltrace : 1;
    bool no_emit : 1;
};

Vec3 luxelVoxelGet(Vec3 position,int mipmap,Vec2 axis){
	unsigned hash_main = luxelHashGet(position,mipmap);
	Luxel* luxel_main = luxelGet(hash_main); 

	if(luxel_main->hash == hash_main && luxel_main->n_sample > 0x100)
		return vec3Add(luxel_main->luminance,luxel_main->luminance_direct);

	unsigned hash = luxelHashGet(vec3Shr(position,1),mipmap + 1);
	Luxel* luxel = luxelGet(hash);

	if(luxel->hash == hash && luxel->n_sample > 0x100)
		return vec3Add(luxel->luminance,luxel->luminance_direct);

	Vec3 neighbour = position;
	((int*)&neighbour)[axis.x] += 1;

	hash = luxelHashGet(neighbour,mipmap);
	luxel = luxelGet(hash);

	if(luxel->hash == hash && luxel->n_sample > 0x100)
		return vec3Add(luxel->luminance,luxel->luminance_direct);

	return vec3Add(luxel_main->luminance,luxel_main->luminance_direct);
}

void lightmapTreeGenerate(LightmapTree* node,Voxel* voxel,Vec3 block_pos,int side,Vec2 coord,int depth,int surface_angle,Vec2 size){
    Vec2 axis = g_axis_table[side];
    Vec3 block_pos_t = block_pos;
	block_pos_t.a[axis.x] += fixedMulR(coord.x << FIXED_PRECISION,size.x);
	block_pos_t.a[axis.y] += fixedMulR(coord.y << FIXED_PRECISION,size.y);

    Vec3 pos[4] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

	pos[1].a[axis.y] += size.y;
	pos[2].a[axis.x] += size.x;
	pos[3].a[axis.x] += size.x;
	pos[3].a[axis.y] += size.y;
    
	int distance_max = 0;
	int distance_max_index;

	for(int i = 0;i < 4;i++){
		int distance = vec3Distance(vec3Shr(g_position,4),vec3Shr(pos[i],4));
		if(distance > distance_max){
			distance_max = distance;
			distance_max_index = i;
		}
	}
    
    Vec3 normal = g_normal_table[side];
    int mipmap = mipmapGet(squarePointClosestPosition(block_pos_t,size.x,normal),normal,distance_max,surface_angle);

	int split = 25 + -tClamp(mipmap,26 - LUXEL_MAX_MIPMAP,31) - voxel->depth;
    
    if(depth < split){
        coord.x <<= 1;
        coord.y <<= 1;
        
        for(int i = countof(node->child);i--;)
            node->child[i] = memoryArenaAllocateZero(&g_arena_frame,sizeof(*node->child[i]));
        
        lightmapTreeGenerate(node->child[0],voxel,block_pos,side,vec2Add(coord,(Vec2){0,0}),depth + 1,surface_angle,vec2Shr(size,1));
        lightmapTreeGenerate(node->child[1],voxel,block_pos,side,vec2Add(coord,(Vec2){0,1}),depth + 1,surface_angle,vec2Shr(size,1));
        lightmapTreeGenerate(node->child[2],voxel,block_pos,side,vec2Add(coord,(Vec2){1,0}),depth + 1,surface_angle,vec2Shr(size,1));
        lightmapTreeGenerate(node->child[3],voxel,block_pos,side,vec2Add(coord,(Vec2){1,1}),depth + 1,surface_angle,vec2Shr(size,1));

        return;
    }
    mipmap = tClamp(mipmapGet(squarePointClosestPosition(pos[0],size.x,normal),normal,distance_max,surface_angle),26 - LUXEL_MAX_MIPMAP,31);
    
    Vec3 light_pos = pos[0];
    light_pos.a[axis.x] += size.x / 2;
    light_pos.a[axis.y] += size.y / 2;

    Vec3 luxel_pos = vec3Shr(light_pos,mipmap);
	unsigned hash = luxelHashGet(luxel_pos,mipmap);
	Luxel* luxel = luxelGet(hash);
    
    node->luminance = vec3MulS(luxelVoxelGet(luxel_pos,mipmap,axis),g_exposure);
}
#include "console.h"
static Vec3 rayLuminanceRecursive(TraverseInit init,Vec3 position,Vec3 direction,int depth,RayLuminanceFlag flags){
    if(!depth)
		return vec3Single(0);

	int side;
	Voxel* voxel = treeRayTrace(init.voxel,init.pos,direction,&side);

	if(!voxel)
		return skyboxSample(direction);

	VoxelStatic* voxel_s = g_voxel_static + voxel->type;

	if(voxel_s->emiter)
		return flags.no_emit ? (Vec3){0} : vec3Shr(voxel_s->color,4);

	Vec3 end_pos = rayHitPosition(voxel,position,direction,side);

	if(voxel->type == VOXEL_MIRROR){
		Vec3 normal = g_normal_table[side << 1 | (((int*)&direction)[side] < 0)];
		Vec3 relative = vec3Sub(end_pos,position);
		Vec3 offset = vec3Reflect(relative,normal);
		return vec3MulS(rayLuminanceRecursive(initTraverse(end_pos),end_pos,offset,depth - 1,flags),FIXED_ONE - (FIXED_ONE / 8));
	}

	if(voxel->type == VOXEL_GLASS){
		((int*)&end_pos)[side] -= ((int*)&direction)[side] < 0 ? 0x10 : -0x10;
		return rayLuminanceRecursive(initTraverse(end_pos),end_pos,direction,depth - 1,flags);
	}
	if(voxel->type == VOXEL_WATER){
		((int*)&end_pos)[side] -= ((int*)&direction)[side] < 0 ? 0x10 : -0x10;
		return rayLuminanceRecursive(initTraverse(end_pos),end_pos,direction,depth - 1,flags);
	}

	if(flags.fulltrace){
		Vec3 normal = g_normal_table[side << 1 | (((int*)&direction)[side] < 0)];
		Vec3 offset = normal;
		Vec3 rnd_vector = vec3Rnd();
		offset = vec3Add(offset,rnd_vector);
		Vec3 direction_new = vec3Normalize(offset);
		Vec3 color = rayLuminanceRecursive(initTraverse(end_pos),end_pos,direction_new,depth - 1,flags);

		Vec2 uv = voxelGuiPositionGet(voxel,position,direction,side);
		
		uv.x = fixedMulR(fixedMulR(uv.x,voxel_s->texture_size),depthToSize(voxel->depth)) >> 4;
		uv.y = fixedMulR(fixedMulR(uv.y,voxel_s->texture_size),depthToSize(voxel->depth)) >> 4;

		if(!voxel_s->texture)
			return vec3Mul(color,voxel_s->color);
		Vec3 texel = vec3Shr(pixelColorToColor(textureLookup(voxel_s->texture,uv.x,uv.y,0)),4);
		return vec3Mul(color,texel);
	}

    if(!g_options.lighting_engine){
        if(!voxel_s->texture)
            return voxel_s->color;
        Vec2 uv = voxelGuiPositionGet(voxel,position,direction,side);
		
		uv.x = fixedMulR(fixedMulR(uv.x,voxel_s->texture_size),depthToSize(voxel->depth)) >> 4;
		uv.y = fixedMulR(fixedMulR(uv.y,voxel_s->texture_size),depthToSize(voxel->depth)) >> 4;

		Vec3 texel = vec3Shr(pixelColorToColor(textureLookup(voxel_s->texture,uv.x,uv.y,3)),4);
		return texel;
    }
    
	int mipmap = tClamp(mipmapGet(end_pos,g_normal_table[side << 1],vec3Distance(vec3Shr(end_pos,4),vec3Shr(g_position,4)),surfaceAngle(position,g_normal_table[side << 1])),26 - LUXEL_MAX_MIPMAP,31);
    
	Vec3 luxel_pos = vec3Shr(end_pos,mipmap);
	unsigned hash = luxelHashGet(luxel_pos,mipmap);
	Luxel* luxel = luxelGet(hash);
    
	if(luxel->hash == hash){
		Vec3 luminance = vec3Add(vec3Shr(luxel->luminance,4),vec3Shr(luxel->luminance_direct,4));
		if(!voxel_s->texture)
			return vec3Mul(luminance,voxel_s->color);
        if(!g_options.textures)
            return luminance;

		Vec2 uv = voxelGuiPositionGet(voxel,position,direction,side);
		
		uv.x = fixedMulR(fixedMulR(uv.x,voxel_s->texture_size),depthToSize(voxel->depth)) >> 4;
		uv.y = fixedMulR(fixedMulR(uv.y,voxel_s->texture_size),depthToSize(voxel->depth)) >> 4;

		Vec3 texel = vec3Shr(pixelColorToColor(textureLookup(voxel_s->texture,uv.x,uv.y,3)),4);
		return vec3Mul(luminance,texel);
	}
	return vec3Single(0);
}

static bool voxelTranslucent(Voxel* voxel){
    return g_voxel_static[voxel->type].translucent || voxel->animation || voxel->opened;
}

Vec3 rayLuminance(Vec3 position,Vec3 direction){
	TraverseInit init = initTraverse(position);
	Voxel* voxel = voxelPositionGet(position);
    if(!voxelTranslucent(voxel))
        return (Vec3){0};
	return rayLuminanceRecursive(init,position,direction,8,(RayLuminanceFlag){.no_emit = true});
}

Vec3 rayLuminanceInit(TraverseInit init,Vec3 position,Vec3 direction){
	return rayLuminanceRecursive(init,position,direction,8,(RayLuminanceFlag){0});
}

Vec3 rayLuminanceTrace(Vec3 position,Vec3 direction){
	return rayLuminanceRecursive(initTraverse(position),position,direction,8,(RayLuminanceFlag){.fulltrace = true});
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
		Vec3 relative = vec3Sub(position,g_position);
		Vec3 reflect = vec3Normalize(vec3Reflect(vec3Shr(relative,8),normal));
		offset = vec3Mix(offset,reflect,FIXED_ONE / 2);
	}
	else{
		offset = normal;
		Vec3 rnd_vector = vec3Rnd();
		offset = vec3Add(offset,rnd_vector);
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

static LightingWorkData lighting_work_data[0x8000];
static int lighting_work_data_ptr;

static void lightingSide(Voxel* voxel,LightingWorkData* lighting_data,Vec3 block_pos,int side,Vec2 coord,int depth,int distance_max,Vec3 cube_c,int angle){
	Vec3 normal = g_normal_table[side];
	Vec2 axis = g_axis_table[side];
	Vec3 light_pos = block_pos;
	int size = depthToSize(voxel->depth) / (1 << depth);
	light_pos.a[axis.x] += fixedMulR(coord.x << FIXED_PRECISION,size);
	light_pos.a[axis.y] += fixedMulR(coord.y << FIXED_PRECISION,size);

    if(lighting_work_data_ptr >= countof(lighting_work_data) - 1)
		return;
    
	if(g_options.smooth_lighting){
		if(coord.y >= 0 && coord.y < (1 << depth)){
			if(!coord.x)
				lightingSide(voxel,lighting_data,block_pos,side,vec2Add(coord,(Vec2){-1,0}),depth,distance_max,cube_c,angle);
			if(coord.x == (1 << depth) - 1)
				lightingSide(voxel,lighting_data,block_pos,side,vec2Add(coord,(Vec2){1,0}),depth,distance_max,cube_c,angle);
		}
		if(coord.x >= 0 && coord.x < (1 << depth)){
			if(!coord.y)
				lightingSide(voxel,lighting_data,block_pos,side,vec2Add(coord,(Vec2){0,-1}),depth,distance_max,cube_c,angle);
			if(coord.y == (1 << depth) - 1)
				lightingSide(voxel,lighting_data,block_pos,side,vec2Add(coord,(Vec2){0,1}),depth,distance_max,cube_c,angle);
		}
	}

	int mipmap = tClamp(mipmapGet(light_pos,normal,distance_max,angle),26 - LUXEL_MAX_MIPMAP,31);

	Vec3 position = light_pos;
	light_pos.a[side >> 1] += side & 1 ? 0x10 : -0x10;
    
	LightingWorkData* light_data = lighting_data + lighting_work_data_ptr++;

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
	block_pos_t.a[axis.x] += fixedMulR(coord.x << FIXED_PRECISION,size);
	block_pos_t.a[axis.y] += fixedMulR(coord.y << FIXED_PRECISION,size);

	Vec3 pos[4] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

	pos[1].a[axis.y] += size;
	pos[2].a[axis.x] += size;
	pos[3].a[axis.x] += size;
	pos[3].a[axis.y] += size;

	int distance_max = 0;
	int distance_max_index;

	for(int i = 0;i < 4;i++){
		int distance = vec3Dot(vec3Shr(g_position,4),vec3Shr(pos[i],4));
		if(distance > distance_max){
			distance_max = distance;
			distance_max_index = i;
		}
	}

	distance_max = vec3Distance(vec3Shr(g_position,4),vec3Shr(pos[distance_max_index],4));

	Vec3 cube_c = pos[0];	

	cube_c.a[axis.x] = tClamp(g_position.a[axis.x],pos[0].a[axis.x],pos[3].a[axis.x]);
	cube_c.a[axis.y] = tClamp(g_position.a[axis.y],pos[0].a[axis.y],pos[3].a[axis.y]);

	Vec3 normal = g_normal_table[side];
	
	int mipmap = tClamp(mipmapGet(squarePointClosestPosition(pos[0],size,normal),normal,distance_max,angle),26 - LUXEL_MAX_MIPMAP,31);

	int split = 25 + -mipmap - voxel->depth;
	
	if(depth < split){
		Vec3 v_pos = vec3Shl((Vec3){voxel->position_x,voxel->position_y,voxel->position_z},depth);
		v_pos.a[axis.x] += coord.x;
		v_pos.a[axis.y] += coord.y;
		if(side & 1)
			v_pos.a[side >> 1] += (1 << depth) - 1;
		
		if(sdSquare(vec3Shr(g_position,4),vec3Shr(block_pos_t,4),size >> 4,side) > RENDER_DISTANCE)
			return;

		if(!squareVisible(v_pos,voxel->depth + depth,side,voxel->type))
			return;
		
		coord.x <<= 1;
		coord.y <<= 1;
		lightingSideRecursive(voxel,lighting_data,block_pos,side,vec2Add(coord,(Vec2){0,0}),depth + 1,angle);
		lightingSideRecursive(voxel,lighting_data,block_pos,side,vec2Add(coord,(Vec2){0,1}),depth + 1,angle);
		lightingSideRecursive(voxel,lighting_data,block_pos,side,vec2Add(coord,(Vec2){1,0}),depth + 1,angle);
		lightingSideRecursive(voxel,lighting_data,block_pos,side,vec2Add(coord,(Vec2){1,1}),depth + 1,angle);
		return;
	}
	lightingSide(voxel,lighting_data,block_pos,side,coord,depth,distance_max,cube_c,angle);
}

static void lightingSidePre(Voxel* voxel,LightingWorkData* lighting_data,Vec3 block_pos,int side){
	lightingSideRecursive(voxel,lighting_data,block_pos,side,(Vec2){0,0},0,surfaceAngle(block_pos,g_normal_table[side]));
}

static void lightingCollect(LightingWorkData* lighting_data){
    static int stack_depth;
    static struct{
        Voxel* voxel;
        int child_index;
    } stack[0x100] = {{.voxel = &g_voxel}};
    while(lighting_work_data_ptr < countof(lighting_work_data)){
        Voxel* voxel = stack[stack_depth].voxel;
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
            if(sdVoxel(vec3Shr(g_position,4),vec3Shr(block_pos,4),block_size >> 4) > RENDER_DISTANCE)
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
        if(voxel->type == VOXEL_AIR || voxel->type == VOXEL_MIRROR || voxel->type == VOXEL_GLASS)
            goto next;
        if(g_voxel_static[voxel->type].emiter){
            LightingWorkData* light_data = lighting_data + lighting_work_data_ptr++;

            *light_data = (LightingWorkData){
                .voxel = voxel,
                .emiter = true
            };
            goto next;
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
            lightingSidePre(voxel,lighting_data,vec3Add(block_pos,(Vec3){block_size,0,0}),1);
        if(g_position.y - block_pos.y < 0)
            lightingSidePre(voxel,lighting_data,block_pos,2);
        if(g_position.y - block_pos.y - block_size > 0)
            lightingSidePre(voxel,lighting_data,vec3Add(block_pos,(Vec3){0,block_size,0}),3);
        if(g_position.z - block_pos.z < 0)
            lightingSidePre(voxel,lighting_data,block_pos,4);
        if(g_position.z - block_pos.z - block_size > 0)
            lightingSidePre(voxel,lighting_data,vec3Add(block_pos,(Vec3){0,0,block_size}),5);
    next:
        stack_depth -= 1;
    }
}

structure(LightingTraceParameters){
	int index;
	int amount;
	LightingWorkData* data;
};

#define N_LUXEL_SAMPLE 0x1000
#define EMIT_RANGE 0x10000

static Vec3 voxelEmit(Voxel* voxel,Vec3 light_pos,int mipmap,Vec3 normal){
    if(voxel->type == VOXEL_PARENT){
        Vec3 voxel_position = voxelWorldPos(voxel);
        
        if(sdVoxel(light_pos,voxel_position,depthToSize(voxel->depth)) / 0x40000 > voxel->emission)
            return (Vec3){0};
        
        Vec3 luminance = {0};
        for(int i = 8;i--;)
            luminance = vec3Add(luminance,voxelEmit(voxel->child_s[i],light_pos,mipmap,normal));
        return luminance;
    }
    if(g_voxel_static[voxel->type].emiter){
        int emiter_size = depthToSize(voxel->depth);
        Vec3 emiter_position = vec3AddS(voxelWorldPos(voxel),emiter_size / 2);
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
    
        return vec3MulS(vec3MulS(g_voxel_static[voxel->type].color,intensity),angle);
    }
    return (Vec3){0};
}

static void lightingTrace(void* arg_void){
	LightingTraceParameters* arg = arg_void;
	for(int i = arg->index;i < arg->index + arg->amount;i++){
		LightingWorkData* light_data = arg->data + i;

		Vec3 luxel_pos = vec3Shr(light_data->position,light_data->mipmap);
		unsigned hash = luxelHashGet(luxel_pos,light_data->mipmap);
		Luxel* luxel = luxelGet(hash);

		if(luxel->tick_last_updated == g_tick)
			continue;

		if(luxel->hash != hash){
			Vec3 luxel_pos_up = vec3Shr(light_data->position,light_data->mipmap + 1);
			unsigned hash_up = luxelHashGet(luxel_pos_up,light_data->mipmap + 1);
			Luxel* luxel_up = luxelGet(hash_up);

			if(luxel_up->hash == hash_up){
				luxel->luminance = luxel_up->luminance;
				luxel->n_sample = luxel_up->n_sample / 4;
                luxel->luminance_direct_sampled = false;
			}
			else{
				luxel->n_sample = 0;
			}
			luxel->hash = hash;
		}

		Vec2 axis   = g_axis_table[light_data->side];
		Vec3 normal = g_normal_table[light_data->side];
        
	    int n;
        if(luxel->n_sample < 0x10)
            n = 0x10;
        else if(luxel->n_sample < 0x100)
            n = 4;
        else
            n = 1;
        
        while(n--){
            if(!luxel->luminance_direct_sampled){
                Vec3 direct_lighting = voxelEmit(&g_voxel,light_data->light_position,light_data->mipmap,g_normal_table[light_data->side]);
                
                luxel->n_sample = tMin(++luxel->n_sample,N_LUXEL_SAMPLE);
                
                luxel->luminance_direct = direct_lighting;
                luxel->luminance_direct_sampled = true;
                break;
            }
			Vec3 position = light_data->light_position;
			position.a[axis.x] += fixedMulR(tRnd() % FIXED_ONE,light_data->size) - light_data->size / 2;
			position.a[axis.y] += fixedMulR(tRnd() % FIXED_ONE,light_data->size) - light_data->size / 2;
			Vec3 luminance = luminanceQuery(light_data->voxel,normal,position);
			luminance = vec3Shl(luminance,4);
			luxel->n_sample = tMin(++luxel->n_sample,N_LUXEL_SAMPLE);
			luxel->luminance = vec3Mix(luxel->luminance,luminance,FIXED_ONE / luxel->n_sample);
            luxel->tick_last_updated = g_tick;
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
