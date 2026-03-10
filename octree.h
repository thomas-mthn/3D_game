#ifndef OCTREE_H
#define OCTREE_H

#include "vec2.h"
#include "vec3.h"
#include "draw.h"
#include "entity.h"
#include "langext.h"
#include "voxel_gui.h"

static Vec2 g_axis_table[] = {
	{VEC3_Y,VEC3_Z},
	{VEC3_Y,VEC3_Z},
	{VEC3_X,VEC3_Z},
	{VEC3_X,VEC3_Z},
	{VEC3_X,VEC3_Y},
	{VEC3_X,VEC3_Y}
};

static Vec3 g_normal_table[] = {
	{-FIXED_ONE,0,0},
	{FIXED_ONE,0,0},
	{0,-FIXED_ONE,0},
	{0,FIXED_ONE,0},
	{0,0,-FIXED_ONE},
	{0,0,FIXED_ONE}
};

enumeration(VoxelType){
	VOXEL_AIR = -2,
	VOXEL_PARENT,
	VOXEL_BLOCK,
	VOXEL_BLOCK_RED,
	VOXEL_BLOCK_GREEN,
	VOXEL_BLOCK_BLUE,
	VOXEL_LIGHT,
	VOXEL_HDR_TEST,
	VOXEL_LIGHT_RED,
	VOXEL_LIGHT_BLUE,
	VOXEL_LIGHT_GREEN,
	VOXEL_MIRROR,
	VOXEL_METALLIC,
	VOXEL_WALL,
	VOXEL_GRASS,
	VOXEL_MENU,
	VOXEL_STONE,
	VOXEL_STONE_BRICK,
	VOXEL_UNDESTRUCTIBLE,
	VOXEL_BLOCK_ORANGE,
	VOXEL_GLASS,
	VOXEL_CHEST,
	VOXEL_MOVABLE,
	VOXEL_BUTTON,
	VOXEL_PLANKS,
	VOXEL_STAFF_INSPECTOR,
	VOXEL_INVENTORY,
	VOXEL_LADDER,
	VOXEL_LIGHT_YELLOW,
	VOXEL_WATER,
	VOXEL_STONE2,
	VOXEL_BOSS,
	VOXEL_ECOUNT,
};

structure(Voxel){
	VoxelType type;
	Voxel* parent;
	int16 position_x;
	int16 position_y;
	int16 position_z;
    int8 depth;
	union{
		//parent
		struct{
			union{
				Voxel* child[2][2][2];
				Voxel* child_s[8];
			};
		};
		//voxel
		struct{	
			Voxel* next_voxel_tick;
			Voxel* linked;
			Voxel* next_voxel_link;
			Entity* entity_list;
			int animation;
			//water 
			Vec2 splash_position;
			unsigned splash_tick;
			unsigned collision_tick;

			bool chest_open;
			bool opened;
		};
	};
	//TODO: move this in a temporary structure to safe memory
	int index;
};

structure(VoxelStatic){
	Vec3 color;
	enum{
		VOXEL_FLAG_EMITER         = 1 << 0,
		VOXEL_FLAG_TEXTURE_FILL   = 1 << 1,
		VOXEL_FLAG_NO_BLOCK_PLACE = 1 << 2,
	} flags;
	Texture* texture;
	int texture_size;
	int n_gui;
	VoxelGuiElement* gui;
	struct{
		bool custom;
		Vec3 color;
		Texture* texture;
		int n_gui;
		VoxelGuiElement* gui;
	} side[6];
};

structure(TraverseInit){
	Vec3 pos;
	Voxel* voxel;
};

structure(VoxelSerialized){
	VoxelType type;
};

structure(VoxelSerializedButton){
	VoxelSerialized voxel;
	int linked;
};

structure(VoxelSerializedParent){
	VoxelSerialized voxel;
	int child_s[8];
};

structure(TraceEntityResult){
	bool hitted_entity;
	union{
		Entity* entity;
		Voxel*  voxel;
	};
};

structure(VoxelTrace){
	int8  type;
	int8  depth;
	int16 x;
	int16 y;
	int16 z;
	int32 parent;
};

structure(VoxelTraceParent){
	VoxelTrace data;
	union{
		int child_s[8];
		int child[2][2][2];
	};
};

extern Voxel g_voxel;
extern Voxel* g_voxel_tick_list;
extern VoxelStatic g_voxel_static[];
extern VoxelGuiElement g_inventory_gui[];

void octreeSerialize(VoxelSerialized* voxel_serial_array,Voxel* voxel);
void octreeSerializeRecursive2(void* voxel_serial_array,int* voxel_serial_index,Voxel* voxel,Vec3 position,int depth,int parent);
Voxel* octreeDeserializeRecursive(VoxelSerializedParent* voxel_serial_array,int index,Voxel* parent,int depth,Vec3 position,Voxel** voxel_array,int* index_i);
void octreeDeserializeLink(VoxelSerializedParent* voxel_serial_array,int index,int depth,Vec3 position,Voxel** voxel_array,int* index_i);
int depthToSize(int depth);
Vec3 posWorldPos(Vec3 position,int depth);
Vec3 voxelWorldPos(Voxel* voxel);
TraverseInit initTraverse(Vec3 pos);
Voxel* treeRayTrace(Voxel* voxel,Vec3 position,Vec3 direction,int* side);
Voxel* treeRayTraceAndInit(Vec3 position,Vec3 direction,int* side);
int treeRayTraceIntersectCountAndInit(Vec3 position,Vec3 direction);
Vec3 rayHitPosition(Voxel* voxel,Vec3 ray_position,Vec3 ray_direction,int side);
void voxelSet(Voxel* voxel,Vec3 pos,int depth,VoxelType type);
Voxel* voxelGet(Vec3 position,int depth);
Voxel* voxelPositionGet(Vec3 pos);
bool squareVisible(Vec3 position,int depth,int side,VoxelType voxel_type);
int voxelChildCountRecursive(Voxel* voxel);
int voxelMemoryCountRecursive(Voxel* voxel);
Entity* entityRayCollisionRecursive(Voxel* voxel,Vec3 position,Vec3 direction);

void voxelMenuMainSet(void);
void voxelMenuStaffEditorDefault(void);

bool lineOfSight(Vec3 position_1,Vec3 position_2);

#endif