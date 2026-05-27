#ifndef OCTREE_H
#define OCTREE_H

#include "vec2.h"
#include "vec3.h"
#include "langext.h"
#include "voxel_gui.h"
#include "memory.h"
#include "opengl.h"

#define LINK_MAX 0x10

structure(Entity);
structure(Cubemap);

static Vec2i g_axis_table[] = {
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

static Vec3 g_u_table[6] = {
    {0,0,FIXED_ONE},  
    {0,0,FIXED_ONE},   
    {FIXED_ONE,0,0},   
    {FIXED_ONE,0,0},   
    {0,FIXED_ONE,0},   
    {0,FIXED_ONE,0},   
};

static Vec3 g_v_table[6] = {
    {0, FIXED_ONE,0},
    {0,-FIXED_ONE,0},
    {0,0, FIXED_ONE},
    {0,0,-FIXED_ONE},
    { FIXED_ONE,0,0},
    {-FIXED_ONE,0,0},
};

typedef enum{
    VOXEL_AIR,
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
    VOXEL_STRING,
    VOXEL_CONSOLE,
    VOXEL_DOOR,
    VOXEL_PRESSURE_PLATE,
    VOXEL_SLOPE_XPP,
    VOXEL_SLOPE_XPN,
    VOXEL_SLOPE_XNP,
    VOXEL_SLOPE_XNN,
    VOXEL_SLOPE_YNP,
    VOXEL_SLOPE_YNN,
    VOXEL_SLOPE_YPP,
    VOXEL_SLOPE_YPN,
    VOXEL_SLOPE_ZNP,
    VOXEL_SLOPE_ZNN,
    VOXEL_SLOPE_ZPP,
    VOXEL_SLOPE_ZPN,
    VOXEL_PLANE,
    VOXEL_CUSTOM,
    VOXEL_CUSTOM_EMIT,
    VOXEL_SPHERE,
    VOXEL_CYLINDER,
    VOXEL_TORUS,
    VOXEL_ECOUNT,
} VoxelType;

structure(VoxelGPU){
    int32 parent;
    VoxelType type;
    int16 position_x;
	int16 position_y;
	int16 position_z;
    int8 depth;
    uint8 child_mask;
    int32 child_s[8];
};

structure(Voxel){
    Voxel* parent;
	VoxelType type;
	int16 position_x;
	int16 position_y;
	int16 position_z;
    int8 depth;
    uint8 child_mask;
    real emission;
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
            int n_link;
            Voxel** links;
			Voxel* next_voxel_link;
			real animation;

            //chest
			bool chest_open : 1;
			bool opened : 1;
            
            //string
            String string;

            //plane
            Vec2 angle;
            real distance;

            //custom
            Vec3 color;
            bool has_texture : 1;
            bool emiter : 1;
            union{
                int8 texture_id;
                ProcTextType procedural_texture;
            };
            int8 emit_pow;

            Entity* entity_list;

            //model_voxels
            Texture* texture_dynamic;
            Cubemap* cubemap;
            Vec3 render_direction;
            Vec2 cylinder_angle;
            Vec3 primitive_position;
		};
	};
	//TODO: move this in a temporary structure to safe memory
	int index;
    int index_gpu;
};

structure(VoxelStatic){
	Vec3 color;
    
    bool emiter : 1;
    bool texturefill : 1;
    bool no_blockplace : 1;
    bool translucent : 1;
    bool rd_trace : 1;
    bool slope : 1;
    bool interact : 1;
    
	Texture* texture;
	real texture_size;
	int n_gui;
	VoxelGuiElement* gui;
    int n_gui_interact;
	VoxelGuiElement* gui_interact;

    Vec3 normal;
    
    Vec3 slope_u;
    Vec3 slope_v;
    Vec3 slope_offset;
    Vec3Axis slope_axis;
    bool slope_flip_x : 1;
    bool slope_flip_y : 1;
    
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
    int n_link;
	int links[];
};

structure(VoxelSerializedString){
    VoxelSerialized voxel;
    uint8 string_length;
    char string_data[];
};

structure(VoxelSerializedPlane){
    VoxelSerialized voxel;
    Vec2 angle;
    real distance;
    Vec3 color;
    int texture_id;
};

structure(VoxelSerializedCustom){
    VoxelSerialized voxel;
    Vec3 color;
    int16 texture_id;
    int16 has_texture;
};

structure(VoxelSerializedCustomEmit){
    VoxelSerialized voxel;
    Vec3 color;
    int emit_pow;
};

structure(VoxelSerializedCylinder){
    VoxelSerialized voxel;
    Vec2 angle;
};

structure(VoxelSerializedTorus){
    VoxelSerialized voxel;
    Vec3 position;
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

extern Voxel* g_voxel_tick_list;
extern VoxelStatic g_voxel_static[];
extern VoxelGuiElement g_voxel_custom_gui[];
extern VoxelGuiElement g_voxel_custom_emit_gui[];
extern VoxelGuiElement g_voxel_plane_gui[];
extern VoxelGuiElement g_inventory_gui[];
extern AllocatorFreeList g_allocator_world;

structure(RayHit){
    Voxel* voxel;
    Vec3 position;
};

structure(TreeTraceFlags){
    bool entity : 1;
    bool everything_solid : 1;
    bool luminance : 1;
    bool skip_first : 1;
};

Vec3 rayVoxelHitPosition(Voxel* voxel,Vec3 ray_position,Vec3 ray_direction,Vec3Axis side);
RayHit rayHitPosition(Vec3 position,Vec3 direction);

void voxelLinkSignal(Voxel* voxel);
void octreeSerialize(VoxelSerialized* voxel_serial_array,Voxel* voxel);
void octreeSerializeRecursive2(void* voxel_serial_array,int* voxel_serial_index,Voxel* voxel,Vec3i position,int depth,int parent);
Voxel* octreeDeserializeRecursive(VoxelSerializedParent* voxel_serial_array,int index,Voxel* parent,int depth,Vec3i position,Voxel** voxel_array,int* index_i);
void octreeDeserializeLink(VoxelSerializedParent* voxel_serial_array,int index,int depth,Vec3i position,Voxel** voxel_array,int* index_i);

Vec3 posWorldPos(Vec3 position,int depth);
TraverseInit initTraverse(Vec3 pos);
Voxel* treeRayTrace(Voxel* voxel,Vec3 position,Vec3 ray_position,Vec3 direction,Vec3Axis* side,TreeTraceFlags flags);
Voxel* treeRayTraceAndInit(Vec3 position,Vec3 direction,Vec3Axis* side,TreeTraceFlags flags);
int treeRayTraceIntersectCountAndInit(Vec3 position,Vec3 direction);
void voxelChildMaskSet(Voxel* voxel);
void octreeRefresh(void);

void voxelFreeRecursive(Voxel* voxel);
Voxel* voxelSet(Voxel* voxel,Vec3i pos,int depth,VoxelType type);
Voxel* voxelEditorSet(Vec3i pos,int depth,VoxelType type);
Voxel* voxelGet(Vec3i position,int depth);
Voxel* voxelPositionGet(Vec3 pos);
bool squareVisible(Vec3i position,int depth,int side,VoxelType voxel_type);
int voxelChildCountRecursive(Voxel* voxel);
int voxelMemoryCountRecursive(Voxel* voxel);
Entity* entityRayCollisionRecursive(Voxel* voxel,Vec3 position,Vec3 direction);

void voxelMenuMainSet(void);
void voxelMenuStaffEditorDefault(void);

bool lineOfSight(Vec3 position_1,Vec3 position_2);

void voxelSetGPU(void);

static real depthToSize(int depth){
	return realShr((FIXED_ONE * 256) * 2,depth);
}

static Vec3 voxelWorldPos(Voxel* voxel){
	real size = depthToSize(voxel->depth);
	return (Vec3){voxel->position_x * size,voxel->position_y * size,voxel->position_z * size};
}

static Vec3 voxelWorldPosCenter(Voxel* voxel){
	real size = depthToSize(voxel->depth);
    Vec3 world_pos = voxelWorldPos(voxel);
    
	return vec3AddS(world_pos,size / 2);
}

#endif
