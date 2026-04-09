#ifndef MAIN_H
#define MAIN_H

#define KEYTRANSLATE_BACK 1

#ifdef __linux__

typedef enum{
    KEY_ESCAPE = 9,
    KEY_1,KEY_2,KEY_3,KEY_4,KEY_5,KEY_6,KEY_7,KEY_8,KEY_9,KEY_0,
    KEY_OEM_4,KEY_OEM_2,KEY_BACK,KEY_TAB,
    KEY_Q,KEY_W,KEY_E,KEY_R,KEY_T,KEY_Y,KEY_U,KEY_I,KEY_O,KEY_P,
    KEY_OEM_6,KEY_OEM_1,KEY_RETURN,KEY_LCONTROL,
    KEY_A,KEY_S,KEY_D,KEY_F,KEY_G,KEY_H,KEY_J,KEY_K,KEY_L,
    KEY_OEM_PLUS,KEY_OEM_3,KEY_OEM_7,KEY_LSHIFT,KEY_OEM_5,
    KEY_Z,KEY_X,KEY_C,KEY_V,KEY_B,KEY_N,KEY_M,
    KEY_OEM_COMMA,KEY_OEM_PERIOD,KEY_OEM_MINUS,KEY_RSHIFT,KEY_MULTIPLY,
    KEY_LMENU,KEY_SPACE,KEY_CAPITAL,
    KEY_F1,KEY_F2,KEY_F3,KEY_F4,KEY_F5,KEY_F6,KEY_F7,KEY_F8,KEY_F9,KEY_F10,
    KEY_NUMLOCK,KEY_SCROLL,KEY_HOME,KEY_UP,KEY_PRIOR,KEY_SUBTRACT,
    KEY_LEFT,KEY_CLEAR,KEY_RIGHT,KEY_ADD,KEY_END,KEY_DOWN,KEY_NEXT,
    KEY_INSERT,KEY_DELETE,KEY_SNAPSHOT,
    KEY_OEM_10 = (0x56 + 8),KEY_F11,KEY_F12,
} KeyType;

#define KEY_LBUTTON 191
#define KEY_RBUTTON 192
#define KEY_MBUTTON 193

#else

enum{
    KEY_BACK = 8,KEY_TAB,KEY_CLEAR = 0x0C,KEY_RETURN,
    KEY_SHIFT = 0x10,KEY_CONTROL,KEY_MENU,KEY_PAUSE,KEY_CAPITAL,
    KEY_ESCAPE = 0x1B,KEY_SPACE = 0x20,KEY_PRIOR,KEY_NEXT,KEY_END,KEY_HOME,
    KEY_LEFT,KEY_UP,KEY_RIGHT,KEY_DOWN,KEY_SELECT,KEY_PRINT,KEY_EXECUTE,
    KEY_SNAPSHOT,KEY_INSERT,KEY_DELETE,KEY_HELP,KEY_0,KEY_1,KEY_2,
    KEY_3,KEY_4,KEY_5,KEY_6,KEY_7,KEY_8,KEY_9,
    KEY_A = 'A',
    KEY_B,KEY_C,KEY_D,KEY_E,KEY_F,KEY_G,
    KEY_H,KEY_I,KEY_J,KEY_K,KEY_L,KEY_M,
    KEY_N,KEY_O,KEY_P,KEY_Q,KEY_R,KEY_S,
    KEY_T,KEY_U,KEY_V,KEY_W,KEY_X,KEY_Y,
    KEY_Z,
};

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

#define KEY_LBUTTON  0x01
#define KEY_RBUTTON  0x02

#define KEY_LCONTROL 0xA2
#define KEY_LALT     0xA4
#define KEY_LSHIFT   0xA0

#define KEY_UP	     0x26	
#define KEY_DOWN	 0x28

#define KEY_OEM_PLUS 0xBB
#define KEY_OEM_COMMA 0xBC
#define KEY_OEM_MINUS 0xBD
#define KEY_OEM_PERIOD 0xBE

#define KEY_ADD 0x6B
#define KEY_SUBTRACT 0x6D

#define KEY_LMENU 0xA4

#define KEY_OEM_1 0xBA	
#define KEY_OEM_2 0xBF
#define KEY_OEM_3    0xC0
#define KEY_OEM_4 0xDB
#define KEY_OEM_5 0xDC
#define KEY_OEM_6 0xDD
#define KEY_OEM_7 0xDE
#define KEY_OEM_8 0xDF

#endif
	
#define N_TICK_SECOND 128
#define RENDER_DISTANCE (FIXED_ONE * 8)

#include "vec3.h"
#include "octree.h"
#include "geometry.h"
#include "inventory.h"
#include "draw.h"

structure(VoxelPointed){
	Voxel* voxel;
	int side;
	Vec2 uv;
};

structure(GameOptions){
	bool editor;
	bool fast_startup;
	Vec2 fov;
	RenderBackend render_backend;
    int multi_sample;
    bool multi_thread;
};

void applicationQuit(void);

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

int bitScanReverse(unsigned value);

void configSave(void);

void keyPress(int key);
void lButtonUp(void);
void lButtonDown(void);
void rButtonDown(void);
void mButtonDown(void);
void mouseMove(int delta_x,int delta_y);

void mainInit(void);
void frameRender(void);
void tickRun(void);

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
extern int g_tri[];
extern Vec2 g_angle;
extern Vec3 g_velocity;
extern int g_health;
extern bool g_smooth_lighting;
extern bool g_luminance_overlay;
extern uint8 g_key[];
extern Vec2 g_cursor;
extern VoxelPointed g_voxel_pointed;
extern GameOptions g_options;
extern bool g_voxel_placement;
extern int g_edit_depth;
extern bool g_movement_fly;
extern Plane g_view_plane[];
extern Voxel* g_voxel_link_list;
extern InventorySlot g_inventory[];
extern VoxelSerialized* g_voxel_template;
extern Entity* g_boss;
extern Voxel* g_voxel_interact;

extern bool g_pickup_collected;

#endif
