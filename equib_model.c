#include "equib_model.h"
#include "octree.h"
#include "main.h"
#include "lighting.h"
#include "draw.h"

static struct{
	Vec3 position;
	Vec2 direction;
} g_holdable;

Voxel* g_hand_model;

static Vec3 modelPointToScreen(Vec3 point){
	Vec3 screen_point;
	Vec3 pos = point;

	if(pos.x <= 0)	
		return vec3Single(0);

	screen_point.x = fixedDivR(fixedMulR(pos.z,g_options.fov.x),pos.x);
	screen_point.y = fixedDivR(fixedMulR(pos.y,g_options.fov.y),pos.x);
	if(screen_point.x > FIXED_ONE * 16 || screen_point.x < -FIXED_ONE * 16 || screen_point.y > FIXED_ONE * 16 || screen_point.y < -FIXED_ONE * 16)
		return vec3Single(0);
	screen_point.z = pos.x;
	return screen_point;
}

static void drawBlock(Voxel* voxel,Vec3 octree_position,Vec3* luminance,int block_size,Vec3 color){
	Vec3 local_position = vec3Shr(vec3Shl((Vec3){voxel->position_x,voxel->position_y,voxel->position_z},16),voxel->depth);
	octree_position = vec3Add(octree_position,(Vec3){local_position.x,local_position.y,local_position.z});

	Vec2 x_direction = vec2Add(g_holdable.direction,(Vec2){FIXED_ONE / 4,0});
	Vec2 y_direction = vec2Add(g_holdable.direction,(Vec2){0,FIXED_ONE / 4});
	Vec2 z_direction = vec2Add(g_holdable.direction,(Vec2){0,0});

	Vec3 pos = vec3Add(g_holdable.position,octree_position);

	Vec3 vertices[8];
	for(int i = 0;i < countof(vertices);i++){
		int x = i & 1 ? block_size >> 9 : 0;
		int y = i & 2 ? block_size >> 9 : 0;
		int z = i & 4 ? block_size >> 9 : 0;
		Vec3 verticle_pos = vec3Add(pos,(Vec3){x,y,z});

		vertices[i] = modelPointToScreen(verticle_pos);
	}

	Vec2 vertices_2d[][4] = {
		{{vertices[0].x,vertices[0].y},{vertices[1].x,vertices[1].y},{vertices[3].x,vertices[3].y},{vertices[2].x,vertices[2].y}},
		{{vertices[4].x,vertices[4].y},{vertices[5].x,vertices[5].y},{vertices[7].x,vertices[7].y},{vertices[6].x,vertices[6].y}},

		{{vertices[0].x,vertices[0].y},{vertices[1].x,vertices[1].y},{vertices[5].x,vertices[5].y},{vertices[4].x,vertices[4].y}},
		{{vertices[2].x,vertices[2].y},{vertices[3].x,vertices[3].y},{vertices[7].x,vertices[7].y},{vertices[6].x,vertices[6].y}},

		{{vertices[0].x,vertices[0].y},{vertices[2].x,vertices[2].y},{vertices[6].x,vertices[6].y},{vertices[4].x,vertices[4].y}},
		{{vertices[1].x,vertices[1].y},{vertices[3].x,vertices[3].y},{vertices[7].x,vertices[7].y},{vertices[5].x,vertices[5].y}},
	};

	VoxelStatic* voxel_s = g_voxel_static + voxel->type;

	drawPolygon(&g_surface,vertices_2d[0],4,vec3Mul(voxel_s->color,luminance[0]));
	drawPolygon(&g_surface,vertices_2d[3],4,vec3Mul(voxel_s->color,luminance[1]));
	drawPolygon(&g_surface,vertices_2d[4],4,vec3Mul(voxel_s->color,luminance[2]));
}

static void guiOctreeDrawRecursive(Voxel* voxel,Vec3 octree_position,Vec3* luminance,int view){
	int block_size = depthToSize(voxel->depth);
	Vec3 position = voxelWorldPos(voxel);
	if(voxel->type == VOXEL_PARENT){
		Vec3 point[] = {
			{position.x + 0,position.y + 0,position.z + 0},
			{position.x + 0,position.y + 0,position.z + block_size},
			{position.x + 0,position.y + block_size,position.z + 0},
			{position.x + 0,position.y + block_size,position.z + block_size},
			{position.x + block_size,position.y + 0,position.z + 0},
			{position.x + block_size,position.y + 0,position.z + block_size},
			{position.x + block_size,position.y + block_size,position.z + 0},
			{position.x + block_size,position.y + block_size,position.z + block_size},
		};
		int order[][8] = {
			{0,1,2,4,3,5,6,7},
			{1,0,3,5,2,7,4,6},
			{2,0,3,6,1,4,7,5},
			{3,1,2,7,0,5,6,4},
			{4,5,6,0,7,1,2,3},
			{5,7,4,1,6,3,0,2},
			{6,4,7,2,5,0,3,1},
			{7,5,6,3,4,1,2,0},
		};
		Vec3 pos = vec3Shr(voxelWorldPos(voxel),8);
		for(int i = 0;i < countof(voxel->child_s);i++){
			int index = order[view][i];
			guiOctreeDrawRecursive(voxel->child_s[index],octree_position,luminance,view);
		}
		return;
	}
	if(voxel->type == VOXEL_AIR || voxel->type == VOXEL_MIRROR || g_voxel_static[voxel->type].emiter)
		return;
	drawBlock(voxel,octree_position,luminance,block_size,vec3Shr(pixelColorToColor(0x507090),4));
}

static void guiOctreeDraw(Voxel* voxel,Vec3 octree_position,Vec3* luminance,int view){
	if(!voxel)
		return;
	guiOctreeDrawRecursive(voxel,octree_position,luminance,view);
}

static void drawBlockSelect(Vec3 octree_position,Vec3* luminance,int block_size,VoxelType material_id){
    if(material_id == VOXEL_AIR)
        return;
	Vec2 x_direction = vec2Add(g_holdable.direction,(Vec2){FIXED_ONE / 4,0});
	Vec2 y_direction = vec2Add(g_holdable.direction,(Vec2){0,FIXED_ONE / 4});
	Vec2 z_direction = vec2Add(g_holdable.direction,(Vec2){0,0});

	Vec3 y_angle = vec3Normalize(getLookDirection(x_direction));
	Vec3 x_angle = vec3Normalize(getLookDirection(y_direction));
	Vec3 z_angle = vec3Cross(x_angle,y_angle);

	Vec3 pos = vec3Add(g_holdable.position,octree_position);

	Vec3 vertices[8];
	for(int i = 0;i < 8;i++){
		int x = i & 1 ? block_size : 0;
		int y = i & 2 ? block_size : 0;
		int z = i & 4 ? block_size : 0;
		Vec3 verticle_pos = vec3Add(pos,(Vec3){x,y,z});

		vertices[i] = modelPointToScreen(verticle_pos);
	}
	VoxelStatic* voxel_s = g_voxel_static + material_id;

	Vec2 vertices_2d[][4] = {
		{{vertices[0].x,vertices[0].y},{vertices[1].x,vertices[1].y},{vertices[3].x,vertices[3].y},{vertices[2].x,vertices[2].y}},
		{{vertices[4].x,vertices[4].y},{vertices[5].x,vertices[5].y},{vertices[7].x,vertices[7].y},{vertices[6].x,vertices[6].y}},

		{{vertices[0].x,vertices[0].y},{vertices[1].x,vertices[1].y},{vertices[5].x,vertices[5].y},{vertices[4].x,vertices[4].y}},
		{{vertices[2].x,vertices[2].y},{vertices[3].x,vertices[3].y},{vertices[7].x,vertices[7].y},{vertices[6].x,vertices[6].y}},

		{{vertices[0].x,vertices[0].y},{vertices[2].x,vertices[2].y},{vertices[6].x,vertices[6].y},{vertices[4].x,vertices[4].y}},
		{{vertices[1].x,vertices[1].y},{vertices[3].x,vertices[3].y},{vertices[7].x,vertices[7].y},{vertices[5].x,vertices[5].y}},
	};

	bool frontface[] = {
		true,
		false,
		false,
		true,
		true,
		false,
	};

	if(voxel_s->texture){
		if(voxel_s->texturefill){
			for(int i = 0;i < countof(frontface);i++){
				if(!frontface[i])
					continue;
 				drawTexturePolygon(&g_surface,voxel_s->texture,g_texture_coordinates_fill,vertices_2d[i],luminance[i / 2],4);
			}
		}
		else{
			Vec2 texture_coordinates_fill[] = {
				(Vec2){0,0},
				(Vec2){0,128 * FIXED_ONE / voxel_s->texture->size},
				(Vec2){128 * FIXED_ONE / voxel_s->texture->size,128 * FIXED_ONE / voxel_s->texture->size},
				(Vec2){128 * FIXED_ONE / voxel_s->texture->size,0},
			};
			for(int i = 0;i < countof(frontface);i++){
				if(!frontface[i])
					continue;
 				drawTexturePolygon(&g_surface,voxel_s->texture,texture_coordinates_fill,vertices_2d[i],luminance[i / 2],4);
			}
		}
	}
	else if(voxel_s->emiter){
		for(int i = 0;i < countof(frontface);i++){
			if(!frontface[i])
				continue;
			drawPolygon(&g_surface,vertices_2d[i],4,voxel_s->color);
		}
	}
	else{
		for(int i = 0;i < countof(frontface);i++){
			if(!frontface[i])
				continue;
			drawPolygon(&g_surface,vertices_2d[i],4,vec3Mul(voxel_s->color,luminance[i / 2]));
		}
	}
}

static int punchAnimationOffset(void){
	int value = fixedMulR(fixedMulR(g_attack_animation,g_attack_animation),g_attack_animation);
	return tCos(tAbs(FIXED_ONE / 2 - value)) + FIXED_ONE;
}

static Texture equib_texture;

static Vec3 sampleFibonacciSphere(int i,int n){
    int golden_angle = 157286;

    int z = FIXED_ONE - ((i * 2 + 1) * FIXED_ONE) / n;

    int zz = fixedMulR(z,z);
    int r = tSqrt(FIXED_ONE - zz);

    int phi = fixedFract(i * golden_angle);

    int x = fixedMulR(r,tCos(phi));
    int y = fixedMulR(r,tSin(phi));

    return (Vec3){x,y,z};
}

static void voxelTemplateDraw(intptr_t base,VoxelSerialized* voxel,Vec3 position,int depth){
    if(voxel->type == VOXEL_PARENT){
        VoxelSerializedParent* parent = (void*)voxel;
        Vec3 position_child = vec3Shl(position,1);
        for(int i = countof(parent->child_s);i--;)
            voxelTemplateDraw(base,(void*)(base + (intptr_t)parent->child_s[i]),vec3Add(position_child,(Vec3){i >> 0 & 1,i >> 1 & 1,i >> 2 & 1}),depth + 1);
        return;
    }
    int size = FIXED_ONE >> depth;
    Vec3 luminance[] = {{FIXED_ONE << 4,FIXED_ONE << 4,FIXED_ONE << 4},{FIXED_ONE << 4,FIXED_ONE << 4,FIXED_ONE << 4},{FIXED_ONE << 4,FIXED_ONE << 4,FIXED_ONE << 4}};
    drawBlockSelect(vec3MulS(vec3Shl(position,16),size),luminance,size,voxel->type);
}

void genBlockSelect(void){
	if(g_voxel_template){
        voxelTemplateDraw((intptr_t)g_voxel_template,g_voxel_template,(Vec3){0},0);
		return;
    }
	g_holdable.direction = vec2Add(vec2MulS(g_holdable.direction,FIXED_ONE / 2),vec2MulS(g_surface.angle,FIXED_ONE / 2));

	for(int i = 0;i < 16;i++)
		g_holdable.position = vec3Mix(g_holdable.position,vec3Add(g_surface.position,getLookDirection(g_surface.angle)),FIXED_ONE >> 4);

	do 
		g_holdable.position = vec3Mix(g_holdable.position,vec3Add(g_surface.position,getLookDirection(g_surface.angle)),FIXED_ONE >> 4);
	while(vec3Distance(g_holdable.position,vec3Add(g_surface.position,getLookDirection(g_surface.angle))) > FIXED_ONE);
	
	g_holdable.position = (Vec3){FIXED_ONE,-FIXED_ONE,FIXED_ONE};

	static Vec3 luminance[3] = {
        {FIXED_ONE << 4,FIXED_ONE << 4,FIXED_ONE << 4},
        {FIXED_ONE << 4,FIXED_ONE << 4,FIXED_ONE << 4},
        {FIXED_ONE << 4,FIXED_ONE << 4,FIXED_ONE << 4},
    };

	Vec3 normals[] = {
		getLookDirection((Vec2){g_holdable.direction.x + FIXED_ONE / 2,g_holdable.direction.y}),
		getLookDirection((Vec2){g_holdable.direction.x - FIXED_ONE / 4,g_holdable.direction.y}),
		getLookDirection((Vec2){g_holdable.direction.x,g_holdable.direction.y + FIXED_ONE / 4}),
	};
	TraverseInit init = initTraverse(g_surface.position);
	Vec3 block_color = vec3Single(FIXED_ONE);
#if 0
	for(int j = 0;j < 3;j++){
		Vec3 luminance_acc = {0};
		for(int i = 0;i < 64;i++){
			Vec3 rnd_dir = normals[j];
			rnd_dir.x += tRnd() % (FIXED_ONE * 2) - FIXED_ONE;
			rnd_dir.y += tRnd() % (FIXED_ONE * 2) - FIXED_ONE;
			rnd_dir.z += tRnd() % (FIXED_ONE * 2) - FIXED_ONE;
			rnd_dir = vec3Normalize(rnd_dir);
			Vec3 luminance = rayLuminance(g_surface.position,rnd_dir);
			luminance_acc = vec3Add(luminance_acc,luminance);
        }
		luminance[j] =  vec3Add(luminance[j],vec3Shr(luminance_acc,6));
        luminance[j] = vec3MulS(luminance[j],FIXED_ONE - (FIXED_ONE >> 4));
	}
#endif
	int ani = punchAnimationOffset();

	if(g_voxel_placement){
		Vec3 position = {FIXED_ONE * 2 + FIXED_ONE / 2,FIXED_ONE + FIXED_ONE / 2,FIXED_ONE * 2 + FIXED_ONE / 2};
		drawBlockSelect((Vec3){0},luminance,FIXED_ONE,g_voxel_select);
	}
}
