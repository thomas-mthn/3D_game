#ifndef MAIN_H
#define MAIN_H

#define KEY_LBUTTON  0x01
#define KEY_RBUTTON  0x02
#define KEY_ADD      0x6B
#define KEY_SUBTRACT 0x6D
#define KEY_ESCAPE   0x1B
#define KEY_SPACE    0x20
#define KEY_LSHIFT   0xA0
#define KEY_LEFT	 0x25	
#define KEY_UP	     0x26	
#define KEY_RIGHT    0x27	
#define KEY_DOWN	 0x28	
#define KEY_OEM_3    0xC0
#define KEY_F1	     0x70	
#define KEY_F2	     0x71	
#define KEY_F3	     0x72	
#define KEY_F4	     0x73	
#define KEY_F5	     0x74	
#define KEY_F6	     0x75	
#define KEY_F7	     0x76	
#define KEY_F8	     0x77	
#define KEY_F9	     0x78	
#define KEY_F10	     0x79	
#define KEY_F11	     0x7A	
#define KEY_F12	     0x7B	
#define KEY_LCONTROL 0xA2
#define KEY_LALT     0xA4

#define N_TICK_SECOND 128
#define RENDER_DISTANCE (FIXED_ONE * 8)

#include "vec3.h"
#include "octree.h"
#include "geometry.h"
#include "inventory.h"

structure(VoxelPointed){
	Voxel* voxel;
	int side;
	Vec2 uv;
};

int bilinearScalar(Vec2 position,int* values);

bool blockOutlinePositionGet(Vec3* position);

bool pointInScreenSpace(Vec3 point);
bool squareInScreenSpace(Vec3* point);
bool cubeInScreenSpace(Vec3* point);

Vec3 screenRayDirection(int* tri,int x,int y,int fov_x,int fov_y);
Plane getPlane(Voxel* voxel,Vec3 dir,unsigned side);
Vec3 pointToScreen(Vec3 point);
Vec3 pointToScreenRenderer(Vec3 point,int* tri,Vec3 renderer_position,Vec2 fov);
bool keyDown(int key);
Vec2 voxelLocalPositionGet(Voxel* voxel,Vec3 position,Vec3 dir,int side);
int treeRayTraceDistance(Voxel* voxel,Vec3 position,Vec3 dir,int side);

void boxQuadWireframeDraw(Vec3 position,Vec3 size,int color);

Vec2 getLookAngle(Vec3 direction);
Vec3 getLookDirection(Vec2 direction);

int leadingZeroCount(int value);
int bitScanReverse(int value);

void keyPress(int key);
void lButtonUp(void);
void lButtonDown(void);
void rButtonDown(void);
void mButtonDown(void);
void mouseMove(int delta_x,int delta_y);

void mainInit(void);
void frameRender(void);
void tickRun(void);

void intToString(char* buffer,int number);

void worldDefaultGenerate(void);

void voxelTickListAdd(Voxel* voxel);

bool inventoryFull(void);

static int colorToPixelColor(Vec3 color){
	return tClamp(color.x >> 12,0,0xFF) | tClamp(color.y >> 12,0,0xFF) << 8 | tClamp(color.z >> 12,0,0xFF) << 16;
}

static Vec3 pixelColorToColor(int color){
	return (Vec3){(color >> 0 & 0xFF) << 12,(color >> 8 & 0xFF) << 12,(color >> 16 & 0xFF) << 12};
}

extern char g_voxel_lighting_tree[];

extern bool g_test_bool;
extern Vec3 g_position;
extern VoxelType g_voxel_select;
extern int g_exposure;
extern unsigned g_tick;
extern Vec2 g_fov;	
extern int g_tri[];
extern Vec2 g_angle;
extern Vec3 g_velocity;
extern int g_health;
extern bool g_smooth_lighting;
extern bool g_luminance_overlay;
extern uint8 g_key[];
extern Vec2 g_cursor;
extern VoxelPointed g_voxel_pointed;
extern bool g_editor;
extern bool g_voxel_placement;
extern int g_edit_depth;
extern bool g_movement_fly;
extern Plane g_view_plane[];
extern Voxel* g_voxel_link_list;
extern InventorySlot g_inventory[];
extern VoxelSerialized* g_voxel_template;
extern Entity* g_boss;

extern bool g_pickup_collected;

#endif