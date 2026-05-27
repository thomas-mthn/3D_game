#ifndef MAIN_H
#define MAIN_H

#ifdef __linux__

typedef enum{
    SIDE_YZ_UP,
    SIDE_YZ_DOWN,
    SIDE_XZ_UP,
    SIDE_XZ_DOWN,
    SIDE_XY_UP,
    SIDE_XY_DOWN,
    SIDE_COUNT,
} Side;

typedef enum{
    KEY_TRANSLATE_BACK = 1,
    KEY_TRANSLATE_UP,
    KEY_TRANSLATE_DOWN,
    KEY_TRANSLATE_LEFT,
    KEY_TRANSLATE_RIGHT,
} KeyTranslate;

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
    KEY_NUMLOCK,KEY_SCROLL,KEY_HOME,KEY_UP_OLD,KEY_PRIOR,KEY_SUBTRACT,
    KEY_LEFT_OLD,KEY_CLEAR,KEY_RIGHT_OLD,KEY_ADD,KEY_END,KEY_DOWN_OLD,KEY_NEXT,
    KEY_INSERT,KEY_DELETE,KEY_SNAPSHOT,
    KEY_OEM_10 = (0x56 + 8),KEY_F11,KEY_F12,
} Key;

#define KEY_UP    111
#define KEY_LEFT  113
#define KEY_RIGHT 114
#define KEY_DOWN  116

#define KEY_LBUTTON 191
#define KEY_RBUTTON 192
#define KEY_MBUTTON 193

#else

typedef enum{
    KEY_ESCAPE = 1,
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
    KEY_LEFT_OLD,KEY_CLEAR,KEY_RIGHT_OLD,KEY_ADD,KEY_END,KEY_DOWN,KEY_NEXT,
    KEY_INSERT,KEY_DELETE,KEY_SNAPSHOT,
    KEY_OEM_10 = (0x56 + 8),KEY_F11,KEY_F12,
} Key;

#define KEY_LEFT 75
#define KEY_RIGHT 77

#define KEY_LBUTTON 191
#define KEY_RBUTTON 192
#define KEY_MBUTTON 193

#if 0

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
#define KEY_OEM_3 0xC0
#define KEY_OEM_4 0xDB
#define KEY_OEM_5 0xDC
#define KEY_OEM_6 0xDD
#define KEY_OEM_7 0xDE
#define KEY_OEM_8 0xDF

#endif

#endif

#define ICON_SIZE 64

#define MAX_TICK_LENGTH 0x4000

#define RENDER_DISTANCE (FIXED_ONE * 32)

#define PLAYER_SPAWN_POSITION {FIXED_ONE * 0xF1,FIXED_ONE * 0x106,FIXED_ONE * 0x102}
#define PLAYER_SPAWN_ANGLE {FIXED_ONE / 2 - REAL_EPSILON,FIXED_ONE / 2 - REAL_EPSILON}

#include "vec3.h"
#include "octree.h"
#include "geometry.h"
#include "staff.h"
#include "draw.h"

structure(Entity);

structure(Quaternion){
    Vec3 v;
    real w;
};

structure(VoxelPointed){
	Voxel* voxel;
	int side;
	Vec2 uv;
};

structure(GameOptions){
	bool editor;
	bool fast_startup;
    real rd_fov;
	RenderBackend render_backend;
    int multi_sample;
    bool multi_thread;
    bool lighting_engine;
    bool smooth_lighting;
    bool gl_wireframe;
    bool textures;
    bool rd_octree_wireframe;
    bool rd_occlusion;
    bool audio;
    bool ray_test;
    bool rd_entity_hitbox;
    bool gl_qlightmap;
    bool rd_dshadow;
    real ms_sensitivity;
    bool ov_luminance;
};

structure(Player){
    Entity* entity;
    Entity* weapon;
    VoxelType voxel_select;
    Voxel* voxel_copy;
    int edit_depth;
    bool movement_fly;
};

structure(GameTime){
    real delta;
    unsigned time;
    int tick;
    int frame_tick;
};

structure(World){
    bool skylight;
    Vec3 skylight_luminance;
    Vec2 skylight_angle;
    Voxel voxel;
};

int* iconGenerate(void);

void applicationExit(void);

real bilinearScalar(Vec2 position,real* values);

bool blockOutlinePositionGet(Vec3i* position);

bool pointInScreenSpace(Vec3* frustum,Vec3 point);
bool squareInScreenSpace(Vec3* frustum,Vec3* point);
bool cubeInScreenSpace(Vec3* frustum,Vec3* point);

Vec3 screenRayDirection(real* tri,real x,real y,real fov_x,real fov_y);
Plane getPlane(Voxel* voxel,Vec3 dir,unsigned side);
Vec3 pointToScreen(Vec3 point);
Vec3 pointToScreenRenderer(Vec3 point,real* tri,Vec3 renderer_position,Vec2 fov);
bool keyDown(int key);
int treeRayTraceDistance(Voxel* voxel,Vec3 position,Vec3 dir,int side);

real spriteSize(Vec3 position,real size);

void boxQuadWireframeDraw(Vec3 position,Vec3 size,int color,bool octree);

Vec2 getLookAngle(Vec3 direction);
Vec3 getLookDirection(Vec2 direction);

int bitScanReverse(unsigned value);

void keyPress(Key key);
void lButtonUp(void);
void lButtonDown(void);
void rButtonDown(void);
void mButtonDown(void);
void mouseMove(int delta_x,int delta_y);

Vec2 aspectRatioTransform(Vec2 v);

void mainInit(void);
void frameRender(void);
void tickRun(void);

void worldDestroy(void);
void worldDefaultGenerate(void);
bool worldExist(String name);
bool worldLoad(String name);
void worldSave(String name);

void voxelTickListAdd(Voxel* voxel);

bool inventoryFull(void);

Vec3 fibonnaciSphereSample(int i,int n);

void configSave(void);

Quaternion quaternionCreate(Vec2 angle);
Vec3 quaternionRotate(Quaternion q,Vec3 v);

static int colorToPixelColor(Vec3 color){
    Vec3i color_i = {realToInt(color.x * 0x100),realToInt(color.y * 0x100),realToInt(color.z * 0x100)};
	return tClamp(color_i.x,0,0xFF) | tClamp(color_i.y,0,0xFF) << 8 | tClamp(color_i.z,0,0xFF) << 16;
}

static Vec3 pixelColorToColor(int color){
	return (Vec3){
        intToReal(color >> 0 & 0xFF) / 0x100,
        intToReal(color >> 8 & 0xFF) / 0x100,
        intToReal(color >> 16 & 0xFF) / 0x100
    };
}

#define COLOR_WHITE (Vec3){FIXED_ONE,FIXED_ONE,FIXED_ONE}

extern char g_voxel_lighting_tree[];

extern bool g_test_bool;
extern real g_exposure;
extern uint8 g_key[];
extern Vec2 g_cursor;
extern VoxelPointed g_voxel_pointed;

extern GameOptions g_options;
extern Player g_player;
extern GameTime g_time;
extern World g_world;

extern bool g_voxel_placement;
extern Vec3 g_view_plane[];
extern Vec3 g_view_plane_lighting[];
extern Voxel* g_voxel_link_list;
extern InventorySlot g_inventory[];
extern VoxelSerialized* g_voxel_template;
extern Entity* g_boss;
extern Voxel* g_voxel_interact;

extern bool g_pickup_collected;

#endif
