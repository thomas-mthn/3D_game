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
	Vec3 local_position = vec3ShrR(vec3ShlR((Vec3){voxel->position_x,voxel->position_y,voxel->position_z},16),voxel->depth);
	vec3Add(&octree_position,(Vec3){local_position.x,local_position.y,local_position.z});

	Vec2 x_direction = vec2AddR(g_holdable.direction,(Vec2){FIXED_ONE / 4,0});
	Vec2 y_direction = vec2AddR(g_holdable.direction,(Vec2){0,FIXED_ONE / 4});
	Vec2 z_direction = vec2AddR(g_holdable.direction,(Vec2){0,0});

	Vec3 pos = vec3AddR(g_holdable.position,octree_position);

	Vec3 vertices[8];
	for(int i = 0;i < countof(vertices);i++){
		int x = i & 1 ? block_size >> 9 : 0;
		int y = i & 2 ? block_size >> 9 : 0;
		int z = i & 4 ? block_size >> 9 : 0;
		Vec3 verticle_pos = vec3AddR(pos,(Vec3){x,y,z});

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

	drawPolygon(g_surface,vertices_2d[0],4,vec3MulR(voxel_s->color,luminance[0]));
	drawPolygon(g_surface,vertices_2d[3],4,vec3MulR(voxel_s->color,luminance[1]));
	drawPolygon(g_surface,vertices_2d[4],4,vec3MulR(voxel_s->color,luminance[2]));
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
		Vec3 pos = vec3ShrR(voxelWorldPos(voxel),8);
		for(int i = 0;i < countof(voxel->child_s);i++){
			int index = order[view][i];
			guiOctreeDrawRecursive(voxel->child_s[index],octree_position,luminance,view);
		}
		return;
	}
	if(voxel->type == VOXEL_AIR || voxel->type == VOXEL_MIRROR || g_voxel_static[voxel->type].flags & VOXEL_EMITER)
		return;
	drawBlock(voxel,octree_position,luminance,block_size,vec3ShrR(pixelColorToColor(0x507090),4));
}

static void guiOctreeDraw(Voxel* voxel,Vec3 octree_position,Vec3* luminance,int view){
	if(!voxel)
		return;
	guiOctreeDrawRecursive(voxel,octree_position,luminance,view);
}

static void drawBlockSelect(Vec3 octree_position,Vec3* luminance,int block_size,int material_id){
	Vec2 x_direction = vec2AddR(g_holdable.direction,(Vec2){FIXED_ONE / 4,0});
	Vec2 y_direction = vec2AddR(g_holdable.direction,(Vec2){0,FIXED_ONE / 4});
	Vec2 z_direction = vec2AddR(g_holdable.direction,(Vec2){0,0});

	Vec3 y_angle = vec3Normalize(getLookDirection(x_direction));
	Vec3 x_angle = vec3Normalize(getLookDirection(y_direction));
	Vec3 z_angle = vec3Cross(x_angle,y_angle);

	Vec3 pos = g_holdable.position;

	Vec3 vertices[8];
	for(int i = 0;i < 8;i++){
		int x = i & 1 ? block_size / 4 : 0;
		int y = i & 2 ? block_size / 4 : 0;
		int z = i & 4 ? block_size / 4 : 0;
		Vec3 verticle_pos = vec3AddR(pos,(Vec3){x,y,z});

		vertices[i] = modelPointToScreen(verticle_pos);
	}
	VoxelStatic* voxel_s = g_voxel_static + g_voxel_select;

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
		if(voxel_s->flags & VOXEL_TEXTUREFILL){
			for(int i = 0;i < countof(frontface);i++){
				if(!frontface[i])
					continue;
 				drawTexturePolygon(g_surface,voxel_s->texture,g_texture_coordinates_fill,vertices_2d[i],luminance[i / 2],4);
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
 				drawTexturePolygon(g_surface,voxel_s->texture,texture_coordinates_fill,vertices_2d[i],luminance[i / 2],4);
			}
		}
	}
	else if(voxel_s->flags & VOXEL_EMITER){
		for(int i = 0;i < countof(frontface);i++){
			if(!frontface[i])
				continue;
			drawPolygon(g_surface,vertices_2d[i],4,voxel_s->color);
		}
	}
	else{
		for(int i = 0;i < countof(frontface);i++){
			if(!frontface[i])
				continue;
			drawPolygon(g_surface,vertices_2d[i],4,vec3MulR(voxel_s->color,luminance[i / 2]));
		}
	}
}

static int punchAnimationOffset(void){
	int value = fixedMulR(fixedMulR(g_attack_animation,g_attack_animation),g_attack_animation);
	return tCos(tAbs(FIXED_ONE / 2 - value)) + FIXED_ONE;
}

void genBlockSelect(void){
	if(g_voxel_template)
		return;
	g_holdable.direction = vec2AddR(vec2MulRS(g_holdable.direction,FIXED_ONE / 2),vec2MulRS(g_angle,FIXED_ONE / 2));

	for(int i = 0;i < 16;i++)
		g_holdable.position = vec3Mix(g_holdable.position,vec3AddR(g_position,getLookDirection(g_angle)),FIXED_ONE >> 4);

	do 
		g_holdable.position = vec3Mix(g_holdable.position,vec3AddR(g_position,getLookDirection(g_angle)),FIXED_ONE >> 4);
	while(vec3Distance(g_holdable.position,vec3AddR(g_position,getLookDirection(g_angle))) > FIXED_ONE);
	
	g_holdable.position = (Vec3){FIXED_ONE,-FIXED_ONE,FIXED_ONE};

	static Vec3 luminance[3];

	Vec3 normals[] = {
		getLookDirection((Vec2){g_holdable.direction.x + FIXED_ONE / 2,g_holdable.direction.y}),
		getLookDirection((Vec2){g_holdable.direction.x - FIXED_ONE / 4,g_holdable.direction.y}),
		getLookDirection((Vec2){g_holdable.direction.x,g_holdable.direction.y + FIXED_ONE / 4}),
	};
	TraverseInit init = initTraverse(g_position);
	Vec3 block_color = vec3Single(FIXED_ONE);
	
	for(int j = 0;j < 3;j++){
		Vec3 luminance_acc = {0};
		for(int i = 0;i < 64;i++){
			Vec3 rnd_dir = normals[j];
			rnd_dir.x += tRnd() % (FIXED_ONE * 2) - FIXED_ONE;
			rnd_dir.y += tRnd() % (FIXED_ONE * 2) - FIXED_ONE;
			rnd_dir.z += tRnd() % (FIXED_ONE * 2) - FIXED_ONE;
			rnd_dir = vec3Normalize(rnd_dir);
			Vec3 luminance = rayLuminance(g_position,rnd_dir);
			vec3Add(&luminance_acc,luminance);
        }
		vec3Add(luminance + j,vec3ShrR(luminance_acc,6));
		vec3MulS(luminance + j,FIXED_ONE - (FIXED_ONE >> 4));
	}
	
	int ani = punchAnimationOffset();

	if(g_voxel_placement){
		Vec3 position = {FIXED_ONE * 2 + FIXED_ONE / 2,FIXED_ONE + FIXED_ONE / 2,FIXED_ONE * 2 + FIXED_ONE / 2};
		drawBlockSelect(position,luminance,FIXED_ONE,g_voxel_select);
	}
	else{
		if(g_equipped_staff){
			Vec3 staff = {(ani << 2),0,0};
			guiOctreeDraw(g_equipped.model,staff,luminance,5);
		}
		else{
			Vec3 fist = {(ani << 2),0,0};
			guiOctreeDraw(g_hand_model,fist,luminance,5);
		}
	}
}
