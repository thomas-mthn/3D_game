#include "main.h"
#include "draw.h"
#include "vec2.h"
#include "vec3.h"
#include "memory.h"
#include "octree.h"
#include "octree_render.h"
#include "physics.h"
#include "lighting.h"
#include "texture.h"
#include "voxel_gui.h"
#include "entity.h"
#include "staff.h"
#include "voxel_menu.h"
#include "geometry.h"
#include "equib_model.h"
#include "span.h"
#include "libc.h"
#include "console.h"
#include "gui2d.h"
#include "opencl.h"
#include "opengl.h"

#include "platform/thread.h"
#include "platform/storage.h"
#include "platform/audio.h"

#ifdef __linux__

#include "linux/l_main.h"
#include "linux/l_syscall.h"

#elif defined(_MSC_VER)

#include "win32/w_main.h"
#include "win32/w_kernel.h"
#include <immintrin.h>

#elif defined(__wasm__)

#include "wasm/wasm.h"

#endif

//msvc needs this
#if defined(_MSC_VER) && !defined(__POCC__)
int _fltused = 0;
#endif

InventorySlot g_inventory[6 * 5];

GameOptions g_options = {
    .multi_sample = 0,
    .multi_thread = false,
	.editor = true,
	.fast_startup = true,
    .lighting_engine = false,
    .rd_fov = FIXED_ONE,
	.render_backend = RENDER_BACKEND_GL,
    .gl_wireframe = false,
    .textures = true,
    .smooth_lighting = true,
    .rd_occlusion = false,
    .ms_sensitivity = FIXED_ONE / 0x1000,
    .gl_qlightmap = true,
};

bool g_voxel_placement;

real g_exposure = FIXED_ONE;
bool g_luminance_overlay;

uint8 g_key[0x100];
Vec2 g_cursor;

bool g_pickup_collected;

Vec3 g_view_plane[4];
Vec3 g_view_plane_lighting[4];

Player g_player = {
    .voxel_select = VOXEL_CUSTOM_EMIT,
    .edit_depth = 10,
};

World g_world = {
    .skylight = true,
    .skylight_angle = {REAL_UNIT * 0x10,REAL_UNIT * 0x10},
    .skylight_luminance = {FIXED_ONE * 6,FIXED_ONE * 9,FIXED_ONE * 12},
};

real bilinearScalar(Vec2 position,real* values){
	real color_x0 = tMix(values[0],values[1],tFract(position.y));
	real color_x1 = tMix(values[2],values[3],tFract(position.y));

	return tMix(color_x0,color_x1,tFract(position.x));
}

bool keyDown(int key){
    return g_key[key] & 0x80;
}

Vec2 getLookAngle(Vec3 direction){
	return (Vec2){
		tArcTan2(direction.y,direction.x),
		tArcSin(direction.z),
    };
}

Vec3 getLookDirection(Vec2 angle){
    return (Vec3){
    	realMulR(tCos(angle.x),tCos(angle.y)),
        realMulR(tSin(angle.x),tCos(angle.y)),
        tSin(angle.y)
    };
}

Quaternion quaternionCreate(Vec2 angle){
    Quaternion q;
    
    real cy = tCos(angle.y / 2);
    real sy = tSin(angle.y / 2);
    real cr = tCos(0);
    real sr = tSin(0);
    real cp = tCos(angle.x / 2);
    real sp = tSin(angle.x / 2);

    q.w = realMulR(realMulR(cy,cr),cp) + realMulR(realMulR(sy,sr),sp);
    q.v.a[0] = realMulR(realMulR(cy,sr),cp) - realMulR(realMulR(sy,cr),sp);
    q.v.a[1] = realMulR(realMulR(cy,cr),sp) + realMulR(realMulR(sy,sr),cp);
    q.v.a[2] = realMulR(realMulR(sy,cr),cp) - realMulR(realMulR(cy,sr),sp);

    return q;
}

Vec3 quaternionRotate(Quaternion q,Vec3 v){
    Vec3 result;

    real ww = realMulR(q.w,q.w);
    real xx = realMulR(q.v.a[0],q.v.a[0]);
    real yy = realMulR(q.v.a[1],q.v.a[1]);
    real zz = realMulR(q.v.a[2],q.v.a[2]);
    real wx = realMulR(q.w,q.v.a[0]);
    real wy = realMulR(q.w,q.v.a[1]);
    real wz = realMulR(q.w,q.v.a[2]);
    real xy = realMulR(q.v.a[0],q.v.a[1]);
    real xz = realMulR(q.v.a[0],q.v.a[2]);
    real yz = realMulR(q.v.a[1],q.v.a[2]);

    result.a[0] = realMulR(ww,v.a[0]) + realMulR(2 * wy,v.a[2]) - realMulR(2 * wz,v.a[1]) +
                  realMulR(xx,v.a[0]) + realMulR(2 * xy,v.a[1]) + realMulR(2 * xz,v.a[2]) -
                  realMulR(zz,v.a[0]) - realMulR(yy,v.a[0]);
    
    result.a[1] = realMulR(2 * xy,v.a[0]) + realMulR(yy,v.a[1]) + realMulR(2 * yz,v.a[2]) +
                  realMulR(2 * wz,v.a[0]) - realMulR(zz,v.a[1]) + realMulR(ww,v.a[1]) -
                  realMulR(2 * wx,v.a[2]) - realMulR(xx,v.a[1]);
    
    result.a[2] = realMulR(2 * xz,v.a[0]) + realMulR(2 * yz,v.a[1]) + realMulR(zz,v.a[2]) -
                  realMulR(2 * wy,v.a[0]) - realMulR(yy,v.a[2]) + realMulR(2 * wx,v.a[1]) -
                  realMulR(xx,v.a[2]) + realMulR(ww,v.a[2]);

    return result;
}

static void pointInScreenSpaceSide(Vec3* frustum,Vec3 point,int* sides){
	Vec3 transformed = vec3Sub(point,g_surface.position);
	
	if(vec3Dot(transformed,getLookDirection(g_surface.angle)) < 0)
		sides[0] += 1;
	
	for(int i = 0;i < countof(g_view_plane);i++){
		if(vec3Dot(transformed,frustum[i]) > 0)
			sides[i + 1] += 1;
	}
}

bool pointInScreenSpace(Vec3* frustum,Vec3 point){
	Vec3 transformed = vec3Sub(point,g_surface.position);
	
	if(vec3Dot(transformed,getLookDirection(g_surface.angle)) < 0)
		return false;
	
	for(int i = 0;i < countof(g_view_plane);i++){
		if(vec3Dot(transformed,frustum[i]) > 0)
			return false;
	}
	return true;
}

bool squareInScreenSpace(Vec3* frustum,Vec3* point){
	int result[5] = {0};
	for(int i = 0;i < 4;i++)
		pointInScreenSpaceSide(frustum,point[i],result);

	return result[0] != 4 && result[1] != 4 && result[2] != 4 && result[3] != 4 && result[4] != 4;
}

bool cubeInScreenSpace(Vec3* frustum,Vec3* point){
	int result[5] = {0};
	for(int i = 0;i < 8;i++)
		pointInScreenSpaceSide(frustum,point[i],result);

	return result[0] != 8 && result[1] != 8 && result[2] != 8 && result[3] != 8 && result[4] != 8;
}

Vec3 screenRayDirection(real* tri,real x,real y,real fov_x,real fov_y){
	Vec3 ray_ang;
	
	real pixel_offset_x = realMulR(x,tReciprocal(fov_x));
	real pixel_offset_y = realMulR(y,tReciprocal(fov_y));
	ray_ang.x = realMulR(tri[0],tri[2]);
	ray_ang.y = realMulR(tri[1],tri[2]);
	ray_ang.x -= realMulR(realMulR(tri[0],tri[3]),pixel_offset_x);
	ray_ang.y -= realMulR(realMulR(tri[1],tri[3]),pixel_offset_x);
	ray_ang.y += realMulR(tri[0],pixel_offset_y);
	ray_ang.x -= realMulR(tri[1],pixel_offset_y);
	ray_ang.z = tri[3] + realMulR(tri[2],pixel_offset_x);
	return ray_ang;
}

Vec3 pointToScreen(Vec3 point){
	Vec3 screen_point;
	Vec3 pos = vec3Sub(point,g_surface.position);
	real temp;
	temp  = realMulR(pos.y,g_surface.rotation_matrix[0]) - realMulR(pos.x,g_surface.rotation_matrix[1]);
	pos.x = realMulR(pos.y,g_surface.rotation_matrix[1]) + realMulR(pos.x,g_surface.rotation_matrix[0]);
	pos.y = temp;
	temp  = realMulR(pos.z,g_surface.rotation_matrix[2]) - realMulR(pos.x,g_surface.rotation_matrix[3]);
	pos.x = realMulR(pos.z,g_surface.rotation_matrix[3]) + realMulR(pos.x,g_surface.rotation_matrix[2]);
	pos.z = temp;

	if(pos.x <= 0)
		return vec3Single(0);

	screen_point.x = realDivR(realMulR(pos.z,g_surface.fov.x),pos.x);
	screen_point.y = realDivR(realMulR(pos.y,g_surface.fov.y),pos.x);
	if(screen_point.x > FIXED_ONE * 16 || screen_point.x < -FIXED_ONE * 16 || screen_point.y > FIXED_ONE * 16 || screen_point.y < -FIXED_ONE * 16)
		return vec3Single(0);
	screen_point.z = pos.x;
	return screen_point;
}

Vec3 pointToScreenRenderer(Vec3 point,real* tri,Vec3 renderer_position,Vec2 fov){
	Vec3 screen_point;
	Vec3 pos = vec3Sub(point,renderer_position);
	real temp;
	temp  = realMulR(pos.y,tri[0]) - realMulR(pos.x,tri[1]);
	pos.x = realMulR(pos.y,tri[1]) + realMulR(pos.x,tri[0]);
	pos.y = temp;
	temp  = realMulR(pos.z,tri[2]) - realMulR(pos.x,tri[3]);
	pos.x = realMulR(pos.z,tri[3]) + realMulR(pos.x,tri[2]);
	pos.z = temp;

	screen_point.x = realMulR(pos.z,fov.x);
	screen_point.y = realMulR(pos.y,fov.y);

	screen_point.z = pos.x;
	return screen_point;
}

void configSave(void){
    storageFileWrite(&g_options,sizeof g_options,IS_FLOAT(real) ? "configf.bin" : "configi.bin");
}

bool inventoryFull(void){
	for(int i = 0;i < countof(g_inventory);i++){
		if(!g_inventory[i].type)
			return false;
	}
	return true;
}

void voxelTickListAdd(Voxel* voxel){
	if(g_voxel_tick_list)
		voxel->next_voxel_tick = g_voxel_tick_list;
	else
		voxel->next_voxel_tick = 0;
	g_voxel_tick_list = voxel;
}

static void voxelEntityRemove(void){	
	entityVoxelRemove();
}

bool g_gui_clicked;

Vec2 g_recoil;

GameTime g_time;

int* iconGenerate(void){
    DrawSurface surface = {
		.height = ICON_SIZE,
		.width = ICON_SIZE,
		.backend = RENDER_BACKEND_SOFTWARE,
        .angle = {0,0},
        .position = {0,FIXED_ONE,FIXED_ONE},
	};

	surfaceInit(&surface);

	struct{
		Vec3 coordinates[4];
		int color;
	} polygon[] = {
		{
			.coordinates = {
				{FIXED_ONE,FIXED_ONE,FIXED_ONE * 4},
				{FIXED_ONE,FIXED_ONE,FIXED_ONE},
				{FIXED_ONE,-FIXED_ONE,FIXED_ONE * 4},
				{FIXED_ONE,-FIXED_ONE,FIXED_ONE},
			},
			.color = 0x80F080
		},
		{
			.coordinates = {
				{FIXED_ONE,-FIXED_ONE,FIXED_ONE * 4},
				{FIXED_ONE,-FIXED_ONE,FIXED_ONE},
				{-FIXED_ONE,-FIXED_ONE,FIXED_ONE},
				{-FIXED_ONE,-FIXED_ONE,FIXED_ONE * 4},
			},
			.color = 0x60D060
		},
		{
			.coordinates = {
				{-FIXED_ONE,-FIXED_ONE,FIXED_ONE},
				{-FIXED_ONE,-FIXED_ONE,FIXED_ONE * 4},
				{-FIXED_ONE,FIXED_ONE,FIXED_ONE * 4},
				{-FIXED_ONE,FIXED_ONE,FIXED_ONE},
			},
			.color = 0x20A020
		},
		{
			.coordinates = {
				{-FIXED_ONE,FIXED_ONE,FIXED_ONE * 4},
				{-FIXED_ONE,FIXED_ONE,FIXED_ONE},
				{FIXED_ONE,FIXED_ONE,FIXED_ONE},
				{FIXED_ONE,FIXED_ONE,FIXED_ONE * 4},
			},
			.color = 0x40B040
		},
		{
			.coordinates = {
				{FIXED_ONE,FIXED_ONE,FIXED_ONE * 4},
				{-FIXED_ONE,FIXED_ONE,FIXED_ONE * 4},
				{-FIXED_ONE,-FIXED_ONE,FIXED_ONE  * 4},
				{FIXED_ONE,-FIXED_ONE,FIXED_ONE  * 4},
			},
			.color = 0x30C030
		},
	};

	for(int i = 0;i < countof(polygon);i++)
		drawPolygon3d(&surface,polygon[i].coordinates,pixelColorToColor(polygon[i].color));

    spanDrawList(&surface);
    
    return surface.data;
}

void tickRun(void){
	if(g_player.entity->health <= 0){
		entityDestroyAll();
		g_surface.position = (Vec3)PLAYER_SPAWN_POSITION;
		g_surface.angle = (Vec2)PLAYER_SPAWN_ANGLE;
		g_player.entity->health = FIXED_ONE;
		g_player.entity->velocity = (Vec3){0};
	}
    if(keyDown(KEY_LBUTTON) && g_equipped_staff && !g_gui_clicked){
		staffFire();
        g_recoil = vec2Add(g_recoil,vec2Shr(vec2Rnd(),0));
	}
    if(g_equipped_staff){
        g_mana += g_equipped.mana_generation;
        g_mana = tMin(g_mana,g_equipped.mana_max);
    }
#if 0
    g_surface.angle = vec2Add(g_surface.angle,vec2Shr(g_recoil,8));
    g_recoil = vec2MulS(g_recoil,FIXED_ONE - 0x1000);
#endif

    if(g_recoil.x < 0)
        g_recoil.x += 1;
    if(g_recoil.y < 0)
        g_recoil.y += 1;
    
	int n_entity = 0;
	for(Entity* entity = g_entity;entity;entity = entity->next)
		n_entity += 1;
	int change = realDivR(FIXED_ONE,(n_entity + 1) * FIXED_ONE) / 64;
	
	if(realRandom(FIXED_ONE) < change && !g_options.editor)
		entitySpawn();
		
	Voxel* previous = 0;
	for(Voxel* voxel = g_voxel_tick_list;voxel;voxel = voxel->next_voxel_tick){
		switch(voxel->type){
            case VOXEL_PRESSURE_PLATE:{
                voxel->animation -= 1;
                if(voxel->animation <= 0){                    
                    if(previous)
						previous->next_voxel_tick = voxel->next_voxel_tick;
					else
						g_voxel_tick_list = voxel->next_voxel_tick;
					continue;
                }
            } break;
			case VOXEL_WATER:{
				voxel->animation -= 0x40;
				if(voxel->animation <= 0){
					if(previous)
						previous->next_voxel_tick = voxel->next_voxel_tick;
					else
						g_voxel_tick_list = voxel->next_voxel_tick;
					continue;
				}
			} break;
            case VOXEL_DOOR:{
                voxel->animation -= 8 << voxel->depth;
                real block_size = depthToSize(voxel->depth);
                Vec3 block_pos = voxelWorldPos(voxel);
                real door_size = realMulR(block_size,FIXED_ONE - voxel->animation) / 2;
                if(voxel->opened)
                    door_size = block_size / 2 - door_size;
                real door_size_inv = block_size / 2 - door_size;
                bool box_1 = intersectBoxBox(g_surface.position,PLAYER_SIZE,block_pos,(Vec3){block_size,door_size,block_size});

                if(box_1){
                    voxel->animation += 16 << voxel->depth;
                    g_player.entity->velocity.y += 0x100;
                }

                bool box_2 = intersectBoxBox(g_surface.position,PLAYER_SIZE,(Vec3){block_pos.x,block_pos.y + block_size / 2 + door_size_inv,block_pos.z},(Vec3){block_size,door_size,block_size});
                if(box_2){
                    voxel->animation += 16 << voxel->depth;
                    g_player.entity->velocity.y -= 0x100;
                }
                
                if(voxel->animation <= 0){
					if(previous)
						previous->next_voxel_tick = voxel->next_voxel_tick;
					else
						g_voxel_tick_list = voxel->next_voxel_tick;
					continue;
				}
            } break;
			case VOXEL_MOVABLE:{
				voxel->animation -= 8 << voxel->depth;
				if(voxel->animation <= 0){
					if(previous)
						previous->next_voxel_tick = voxel->next_voxel_tick;
					else
						g_voxel_tick_list = voxel->next_voxel_tick;
					continue;
				}
			} break;
			case VOXEL_CHEST: case VOXEL_BOSS:{
				voxel->animation -= 0x1000;
				if(voxel->animation <= 0){
					if(previous)
						previous->next_voxel_tick = voxel->next_voxel_tick;
					else
						g_voxel_tick_list = voxel->next_voxel_tick;
					continue;
				}
			} break;
		}
		previous = voxel;
	}

	if(g_player.movement_fly)
		movementFly();
	else
		movementNormal();

	entityVoxelInsertSimulation();
	entityTick();
	voxelEntityRemove();
	entityDestroy();

	spinningStaffSpin();

    g_time.time += IS_FLOAT(real) ? g_time.delta * 0x10000 : g_time.delta;
	g_time.tick += 1;
}

int bitScanReverse(unsigned value){
	if(!value)
		return 0;
	unsigned index;
#if defined(__GNUC__) || defined(__clang__)
	index = 31 - __builtin_clz(value);
	
#elif defined(_MSC_VER)
	_BitScanReverse(&index,value);
	
#else
	index = 0;
    while(value){
        index += 1;
		value >>= 1;
    }
#endif
	return index;
}

Plane getPlane(Voxel* voxel,Vec3 dir,unsigned side){
    Plane plane;
	real size = depthToSize(voxel->depth);
	Vec3 pos = voxelWorldPos(voxel);
	switch(side){
		case VEC3_X:
			plane.normal = (Vec3){(dir.x < 0) ? FIXED_ONE : -FIXED_ONE,0,0};
		    plane.distance = (dir.x < 0) ? -pos.a[side] - size : pos.a[side];
		    break;
		case VEC3_Y:
			plane.normal = (Vec3){0,(dir.y < 0) ? FIXED_ONE : -FIXED_ONE,0};
		    plane.distance = (dir.y < 0) ? -pos.a[side] - size : pos.a[side];
		    break;
		case VEC3_Z:
			plane.normal = (Vec3){0,0,(dir.z < 0) ? FIXED_ONE : -FIXED_ONE};
		    plane.distance = (dir.z < 0) ? -pos.a[side] - size : pos.a[side];
		    break;
	}
	return plane;
}

int treeRayTraceDistance(Voxel* voxel,Vec3 position,Vec3 dir,int side){
	if(!voxel)
		return INT_MAX;
    real block_size = depthToSize(voxel->depth);
	Vec3 pos = voxelWorldPos(voxel);
    Vec2i axis = g_axis_table[side << 1];
    Plane plane = getPlane(voxel,dir,side);
    return rayPlaneIntersection(position,dir,plane);
}

static void addSubVoxel(Vec3i vpos,Vec3i pos,int depth,int remove_depth,VoxelType voxel_type){
	if(--depth == -1)
		return;
	bool offset_x = pos.x >> depth & 1;
	bool offset_y = pos.y >> depth & 1;
	bool offset_z = pos.z >> depth & 1;
	for(int i = 0;i < 8;i++){
		Vec3i lpos = {(i & 1 << 2) >> 2,(i & 1 << 1) >> 1,(i & 1 << 0) >> 0};
		if(offset_x == lpos.x && offset_y == lpos.y && offset_z == lpos.z){
			Vec3i pos2 = {vpos.x + offset_x << 1,vpos.y + offset_y << 1,vpos.z + offset_z << 1};
			addSubVoxel(pos2,pos,depth,remove_depth,voxel_type);
			continue;
		}
		voxelSet(&g_world.voxel,(Vec3i){vpos.x + lpos.x,vpos.y + lpos.y,vpos.z + lpos.z},remove_depth - depth,voxel_type);
	}
}

bool g_test_bool;
static bool in_settings;

Voxel* g_voxel_interact;

void applicationExit(void){
#ifdef __linux__
    systemProcessExit(0);
#elif _MSC_VER
    ExitProcess(0);
#endif
}

static KeyTranslate keyTranslate(Key key){
    KeyTranslate key_char_table[] = {
        [KEY_A] = 'A',[KEY_B] = 'B',[KEY_C] = 'C',[KEY_D] = 'D',[KEY_E] = 'E',[KEY_F] = 'F',
        [KEY_G] = 'G',[KEY_H] = 'H',[KEY_I] = 'I',[KEY_J] = 'J',[KEY_K] = 'K',[KEY_L] = 'L',
        [KEY_M] = 'M',[KEY_N] = 'N',[KEY_O] = 'O',[KEY_P] = 'P',[KEY_Q] = 'Q',[KEY_R] = 'R',
        [KEY_S] = 'S',[KEY_T] = 'T',[KEY_U] = 'U',[KEY_V] = 'V',[KEY_W] = 'W',[KEY_X] = 'X',
        [KEY_Y] = 'Y',[KEY_Z] = 'Z',
        [KEY_1] = '1',[KEY_2] = '2',[KEY_3] = '3',[KEY_4] = '4',[KEY_5] = '5',
        [KEY_6] = '6',[KEY_7] = '7',[KEY_8] = '8',[KEY_9] = '9',[KEY_0] = '0',
        [KEY_SPACE] = ' ',[KEY_RETURN] = '\n',
        [KEY_OEM_7] = '`',[KEY_OEM_4] = '-',[KEY_OEM_2] = '=',[KEY_OEM_6] = '[',[KEY_OEM_1] = ']',
        [KEY_OEM_PLUS] = ';',[KEY_OEM_5] = '\\',[KEY_OEM_3] = '\'',[KEY_OEM_COMMA] = ',',
        [KEY_OEM_PERIOD] = '.',[KEY_OEM_MINUS] = '/',[KEY_RETURN] = '\n',[KEY_BACK] = KEY_TRANSLATE_BACK,
        [KEY_TAB] = '\t',[KEY_UP] = KEY_TRANSLATE_UP,[KEY_DOWN] = KEY_TRANSLATE_DOWN,
        [KEY_LEFT] = KEY_TRANSLATE_LEFT,[KEY_RIGHT] = KEY_TRANSLATE_RIGHT,
    };
    KeyTranslate key_char_shift_table[] = {
        [KEY_1] = '!',[KEY_2] = '@',[KEY_3] = '#',[KEY_4] = '$',[KEY_5] = '%',
        [KEY_6] = '^',[KEY_7] = '&',[KEY_8] = '*',[KEY_9] = '(',[KEY_0] = ')',
        [KEY_OEM_7] = '~',[KEY_OEM_4] = '_',[KEY_OEM_2] = '+',[KEY_OEM_6] = '{',[KEY_OEM_1] = '}',
        [KEY_OEM_PLUS] = ':',[KEY_OEM_5] = '|',[KEY_OEM_3] = '"',[KEY_OEM_COMMA] = '<',
        [KEY_OEM_PERIOD] = '>',[KEY_OEM_MINUS] = '?'
    };
    if(key >= countof(key_char_table) || !key_char_table[key])
        return 0;
                
    bool shift = keyDown(KEY_LSHIFT) && key < countof(key_char_shift_table); 
    return shift ? key_char_shift_table[key] : key_char_table[key];
}

void keyPress(Key key){
    if(g_voxel_interact){
        switch(g_voxel_interact->type){
            case VOXEL_PLANE:{
                switch(key){
                    case KEY_DOWN:{
                        g_voxel_interact->angle.y += REAL_UNIT;
                        octreeRefresh();
                    } break;
                    case KEY_UP:{
                        g_voxel_interact->angle.y -= REAL_UNIT;
                        octreeRefresh();
                    } break;
                    case KEY_LEFT:{
                        g_voxel_interact->angle.x += REAL_UNIT * 4;
                        octreeRefresh();
                    } break;
                    case KEY_RIGHT:{
                        g_voxel_interact->angle.x -= REAL_UNIT * 4;
                        octreeRefresh();
                    } break;
                    case KEY_SPACE:{
                        g_voxel_interact->distance += REAL_UNIT * 16;
                        octreeRefresh();
                    } break;
                    case KEY_LSHIFT:{
                        g_voxel_interact->distance -= REAL_UNIT * 16;
                        octreeRefresh();
                    } break;
                }
            } break;
            case VOXEL_CONSOLE:{
                consoleInput(keyTranslate(key));
            } break;
            case VOXEL_STRING:{
                key = keyTranslate(key);
                switch(key){
                    case KEY_BACK:{ 
                        if(g_voxel_interact->string.size > 0)
                            g_voxel_interact->string.size -= 1;
                    } break;
                    default:{
                        char* str = tMalloc(g_voxel_interact->string.size + 1);
                        tMemcpy(str,g_voxel_interact->string.data,g_voxel_interact->string.size);
                        str[g_voxel_interact->string.size] = key;
                        g_voxel_interact->string.size += 1;
                        tFree(g_voxel_interact->string.data);
                        g_voxel_interact->string.data = str;
                    } break;
                }
            } break;
        }
        return;
    }
	switch(key){
		case KEY_G:{
			if(!g_equipped_staff)
				break;
            
			Entity* staff = entityCreate(g_surface.position,ENTITY_STAFF);
			staff->velocity = vec3Shr(getLookDirection(g_surface.angle),4);
			staff->staff = g_equipped;
			voxelMenuStaffEditorDefault();
			g_equipped = (Staff){0};
			g_equipped_staff = false;
		} break;
		case KEY_F8:{
			if(!g_options.editor)
				break;
            RayHit hit = rayHitPosition(g_surface.position,getLookDirection(g_surface.angle));
            if(hit.voxel){
                Vec3 spawn = vec3Direction(g_surface.position,hit.position);
                spawn = vec3Sub(hit.position,vec3MulS(spawn,FIXED_ONE * 2));
                entityCreate(spawn,ENTITY_SLIME);
            }
		} break;
		case KEY_M:{
			if(!g_options.editor)
				break;
			g_player.movement_fly ^= true;
		} break;
		case KEY_V:{
			if(!g_options.editor)
				break;
            
			g_voxel_placement ^= true;

            if(g_voxel_placement)
                g_player.weapon->health = 0;
            else
                g_player.weapon = entityCreate(g_player.entity->position,ENTITY_WEAPON);
            
		} break;
		case KEY_O:{
			static Vec3 pre_settings_position;
			if(in_settings){
				voxelSet(&g_world.voxel,(Vec3i){0,0,(1 << 4) - 1},4,VOXEL_AIR);
				
				g_surface.position = pre_settings_position;

                storageFileReadStatic(&g_options,sizeof g_options,"config.bin");
			}
			else{
				pre_settings_position = g_surface.position;
                
				voxelSet(&g_world.voxel,(Vec3i){0,0,(1 << 5) - 2},5,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_world.voxel,(Vec3i){0,0,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_world.voxel,(Vec3i){0,1,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_world.voxel,(Vec3i){0,2,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_world.voxel,(Vec3i){0,3,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_world.voxel,(Vec3i){0,4,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_world.voxel,(Vec3i){1,4,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_world.voxel,(Vec3i){2,4,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_world.voxel,(Vec3i){3,4,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_world.voxel,(Vec3i){4,4,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_world.voxel,(Vec3i){4,3,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_world.voxel,(Vec3i){4,2,(1 << 7) - 4},7,VOXEL_MENU);
				voxelSet(&g_world.voxel,(Vec3i){4,1,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_world.voxel,(Vec3i){4,0,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_world.voxel,(Vec3i){3,0,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_world.voxel,(Vec3i){2,0,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_world.voxel,(Vec3i){1,0,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);

				g_surface.position = (Vec3){0xA0000,0xA0000,(1 << 25) - 0x80000};
			}
			in_settings ^= true;
		} break;
		case KEY_F3:{
			g_test_bool ^= true;
            if(g_test_bool)
                print((String)STRING_LITERAL("true"));
            else
                print((String)STRING_LITERAL("false"));
		} break;
		case KEY_LEFT:{
			g_player.voxel_select -= 1;
			if(g_player.voxel_select == -1)
				g_player.voxel_select = VOXEL_ECOUNT - 1;
		} break;
		case KEY_RIGHT:{
			g_player.voxel_select += 1;
			if(g_player.voxel_select == VOXEL_ECOUNT)
				g_player.voxel_select = 0;
		} break;
		case KEY_ADD:{
			g_player.edit_depth += 1;
		} break;
		case KEY_SUBTRACT:{
			g_player.edit_depth -= 1;
		} break;
	}
}

static Vec2 voxelPosition2D(Voxel* voxel,Vec3 position,Vec3 dir,int side){
    return uvMirror(voxelGuiPositionGet(voxel,position,dir,side),side << 1 | dir.a[side] < 0);
}

bool blockOutlinePositionGet(Vec3i* position){
	Vec3Axis side;
	Vec3 dir = getLookDirection(g_surface.angle);
	Voxel* voxel = treeRayTraceAndInit(g_surface.position,dir,&side,(TreeTraceFlags){.everything_solid = true});
	if(!voxel)
		return false;
	Vec3i block_pos_i = {voxel->position_x,voxel->position_y,voxel->position_z};
	block_pos_i.a[side] += (dir.a[side] < 0) * 2 - 1;
			
	if(voxel->depth > g_player.edit_depth){
		int shift = voxel->depth - g_player.edit_depth;
		*position = (Vec3i){block_pos_i.x >> shift,block_pos_i.y >> shift,block_pos_i.z >> shift};
	}
	else{
		int shift = g_player.edit_depth - voxel->depth;
		*position = (Vec3i){block_pos_i.x << shift,block_pos_i.y << shift,block_pos_i.z << shift};
	}
	if(voxel->depth < g_player.edit_depth){
		int depht_difference = g_player.edit_depth - voxel->depth;
		Vec2 uv = voxelPosition2D(voxel,g_surface.position,dir,side);
		Vec2i axis = g_axis_table[side << 1];
		uv.x *= 1 << depht_difference;
		uv.y *= 1 << depht_difference;
		position->a[side] += ((dir.a[side] > 0) << depht_difference) - (dir.a[side] > 0);
		position->a[axis.x] += realToInt(uv.x);
		position->a[axis.y] += realToInt(uv.y);
	}
	return true;
}

VoxelSerialized* g_voxel_template;

static void voxelTemplatePlace(VoxelSerialized* voxel_array,int voxel_index,Vec3i position,int depth){
	if(voxel_index == -1)
		return;
	VoxelSerialized* voxel = (VoxelSerialized*)((char*)voxel_array + voxel_index);
	if(voxel->type == VOXEL_PARENT){
		VoxelSerializedParent* voxel_parent = (VoxelSerializedParent*)voxel;
		for(int i = 0;i < 8;i++){
			Vec3i child_position = {
				position.x << 1 | (i >> 0 & 1),
				position.y << 1 | (i >> 1 & 1),
				position.z << 1 | (i >> 2 & 1)
			};
			voxelTemplatePlace(voxel_array,voxel_parent->child_s[i],child_position,depth + 1);
		}
		return;
	}
	voxelSet(&g_world.voxel,position,depth,voxel->type);
}

void lButtonUp(void){
	g_gui_clicked = false;
	voxelGuiOnRelease(g_voxel_pointed.voxel,g_voxel_pointed.side);
	if(g_spell_hold){
		Entity* entity = entityCreate(g_surface.position,ENTITY_PICKUP);
		entity->velocity = vec3Shr(getLookDirection(g_surface.angle),2);
		entity->pickup_type = g_spell_hold;
		g_spell_hold = 0;
	}
}

static Voxel* button_link;

Entity* g_boss;

void lButtonDown(void){
	Voxel* voxel = g_voxel_pointed.voxel;
	if(!g_voxel_placement){
		if(voxel){
            int side = g_voxel_pointed.side;
            VoxelStatic* voxel_s = g_voxel_static + voxel->type;
            int n_gui = voxel_s->side[side].custom ? voxel_s->side[side].n_gui : voxel_s->n_gui;
            VoxelGuiElement* gui = voxel_s->side[side].custom ? voxel_s->side[side].gui : voxel_s->gui;
            if(voxelGuiOnClick(voxel,g_voxel_pointed.side,gui,n_gui)){
                g_gui_clicked = true;
                return;
            }
		}
		if(!g_player.weapon->attack_cooldown && !g_equipped_staff){
			Vec3 direction = getLookDirection(g_surface.angle);
			Vec3Axis side;
			Voxel* voxel = treeRayTraceAndInit(g_surface.position,direction,&side,(TreeTraceFlags){0});
			real distance;
			if(voxel)
				distance = treeRayTraceDistance(voxel,g_surface.position,direction,side);
			else
				distance = INT_MAX;
#if 0
			Entity* entity = entityRayCollisionRecursive(&g_world.voxel,g_surface.position,direction);
			voxelEntityRemove();
			if(entity && vec3Distance(g_surface.position,entity->position) < FIXED_ONE * 6 && vec3Distance(g_surface.position,entity->position) < distance){
				entityHit(entity);
			}

			audioPlay(g_surface.position,AUDIO_PUNCH);
#endif
		    g_player.weapon->attack_cooldown = FIXED_ONE;
		}
        if(!voxel)
            return;
        Voxel* voxel = g_voxel_pointed.voxel;
        switch(voxel->type){
            case VOXEL_BOSS:{
                if(g_boss)
                    break;
                Vec3 block_position = voxelWorldPos(voxel);
                block_position.x += depthToSize(voxel->depth) / 2;
                block_position.y += depthToSize(voxel->depth) / 2;
                block_position.z += depthToSize(voxel->depth) + FIXED_ONE;
                g_boss = entityCreate(block_position,ENTITY_BOSS);
                g_gui_clicked = true;
                voxel->animation = FIXED_ONE;
                voxelTickListAdd(voxel);
            } return;
            case VOXEL_CHEST:{
                if(voxel->chest_open)
                    break;
                Vec3 block_position = voxelWorldPos(voxel);
                block_position.x += depthToSize(voxel->depth) / 2;
                block_position.y += depthToSize(voxel->depth) / 2;
                block_position.z += depthToSize(voxel->depth) + FIXED_ONE;

                static bool dropped_staff;
                if(!dropped_staff || tRndChance(8)){
                    staffGenerate(block_position);
                    dropped_staff = true;
                }
                else{
                    for(int i = 0x10;i--;){
                        Entity* pickup = entityCreate(block_position,ENTITY_PICKUP);
                        pickup->velocity.x += realRandom(FIXED_ONE / 8) - FIXED_ONE / 16;
                        pickup->velocity.y += realRandom(FIXED_ONE / 8) - FIXED_ONE / 16;
                        pickup->velocity.z += realRandom(FIXED_ONE / 8) + FIXED_ONE / 8;
                        pickup->color_emit = (Vec3){realRandom(FIXED_ONE),realRandom(FIXED_ONE),realRandom(FIXED_ONE)};
                    }
                }
                voxel->chest_open = true;
                voxel->animation = FIXED_ONE;
                audioPlay(block_position,AUDIO_CHEST_OPEN);
                voxelTickListAdd(voxel);
                g_gui_clicked = true;
            } return;
		}
	}
    	
    if(!voxel)
		return;
    VoxelStatic* voxel_s = g_voxel_static + voxel->type;
    if(keyDown(KEY_LMENU)){
        if(voxel_s->interact){
            g_voxel_interact = voxel;
            if(voxel->type == VOXEL_CUSTOM)
                g_voxel_custom_gui[0].colorpicker.color = &voxel->color;
            
            if(voxel->type == VOXEL_CUSTOM_EMIT)
                g_voxel_custom_emit_gui[0].colorpicker.color = &voxel->color;
            
        }
        return;
    }
    if(g_voxel_interact){
        if(g_voxel_interact != voxel){
            g_voxel_interact = 0;
        }
        else{
            if(voxelGuiOnClick(voxel,g_voxel_pointed.side,voxel_s->gui_interact,voxel_s->n_gui_interact)){
                g_gui_clicked = true;
                return;
            }   
        }
        return;
    }

	if(g_voxel_placement){
		if(button_link){
			if(voxel->type == VOXEL_MOVABLE || voxel->type == VOXEL_DOOR){
                if(!button_link->links){
                    button_link->links = tMalloc(LINK_MAX * sizeof(int));
                    button_link->next_voxel_link = g_voxel_link_list;
                    g_voxel_link_list = button_link;
                }
                button_link->links[button_link->n_link++] = voxel;
                if(!voxel->links)
                    voxel->links = tMalloc(LINK_MAX * sizeof(int));
                voxel->links[voxel->n_link++] = button_link;
            }

			button_link = 0;
			return;
		}
		else if(voxel->type == VOXEL_BUTTON || voxel->type == VOXEL_PRESSURE_PLATE){
			button_link = voxel;
			return;
		}
	}
	
	if(!g_voxel_placement)
		return;

	if(voxel_s->no_blockplace)
		return;
	
	Vec3i octree_position;
	if(!blockOutlinePositionGet(&octree_position))
		return;
	if(keyDown(KEY_LCONTROL)){
		static struct{
			Vec3i position;
			int depth;
			bool setted;
		} voxel_fill_begin;
		if(!voxel_fill_begin.setted || voxel_fill_begin.depth != g_player.edit_depth){
			voxel_fill_begin.position = octree_position;
			voxel_fill_begin.depth = g_player.edit_depth;
			voxel_fill_begin.setted = true;
		}
		else{
			Vec3i begin = voxel_fill_begin.position;
			VoxelType type = g_player.voxel_select;
			for(int x = tMin(begin.x,octree_position.x);x <= tMax(begin.x,octree_position.x);x++){
				for(int y = tMin(begin.y,octree_position.y);y <= tMax(begin.y,octree_position.y);y++){
					for(int z = tMin(begin.z,octree_position.z);z <= tMax(begin.z,octree_position.z);z++)
						voxelSet(&g_world.voxel,(Vec3i){x,y,z},g_player.edit_depth,type);
				}
			}
			voxel_fill_begin.setted = false;
		}
		return;
	}
	if(g_voxel_template){
		voxelTemplatePlace(g_voxel_template,0,octree_position,g_player.edit_depth);
		return;
	}
    VoxelType type = g_player.voxel_copy ? g_player.voxel_copy->type : g_player.voxel_select;
	Voxel* voxel_new = voxelEditorSet(octree_position,g_player.edit_depth,type);
    
    if(!g_player.voxel_copy)
        return;
    
    switch(type){
        case VOXEL_CUSTOM:{
            voxel_new->color = g_player.voxel_copy->color;
            voxel_new->texture_id = g_player.voxel_copy->texture_id;
            voxel_new->has_texture = g_player.voxel_copy->has_texture;
        } break;
        case VOXEL_PLANE:{
            voxel_new->angle = g_player.voxel_copy->angle;
            voxel_new->distance = g_player.voxel_copy->distance;
        } break;
        case VOXEL_CUSTOM_EMIT:{
            voxel_new->color = g_player.voxel_copy->color;
#if 0
            voxel_new->texture_id = g_player.voxel_copy->texture_id;
            voxel_new->has_texture = g_player.voxel_copy->has_texture;
#endif
            voxel_new->emit_pow = g_player.voxel_copy->emit_pow;
        } break;
    }
}

void rButtonDown(void){
	if(!g_options.editor){
		if(g_equipped_staff)
			staffSkip();
		return;
	}
	Vec3Axis side;
	Vec3 direction = getLookDirection(g_surface.angle);
	Voxel* voxel = treeRayTraceAndInit(g_surface.position,direction,&side,(TreeTraceFlags){0});
	if(!voxel)
		return;
	Vec3i block_pos_i = {voxel->position_x,voxel->position_y,voxel->position_z};

	Voxel voxel_cpy = *voxel;

	voxelSet(&g_world.voxel,(Vec3i){voxel->position_x,voxel->position_y,voxel->position_z},voxel->depth,VOXEL_AIR);

	if(voxel->depth >= g_player.edit_depth){
        octreeRefresh();
		return;
    }
			
	int depht_difference = g_player.edit_depth - voxel_cpy.depth;
	Vec3i octree_position = (Vec3i){block_pos_i.x << depht_difference,block_pos_i.y << depht_difference,block_pos_i.z << depht_difference};
	Vec2i axis = g_axis_table[side << 1];
	Vec2 uv = voxelGuiPositionGet(&voxel_cpy,g_surface.position,direction,side);
    uv = uvMirror(uv,side << 1 | direction.a[side] < 0);
	uv.x *= 1 << depht_difference;
	uv.y *= 1 << depht_difference;
	octree_position.a[side] += ((direction.a[side] < 0) << depht_difference) - (direction.a[side] < 0);
	octree_position.a[axis.x] += realToInt(uv.x);
	octree_position.a[axis.y] += realToInt(uv.y);
	Vec3i pos_i;
	int depht_difference_size = 1 << depht_difference;
	pos_i.x = octree_position.x & depht_difference_size - 1;
	pos_i.y = octree_position.y & depht_difference_size - 1;
	pos_i.z = octree_position.z & depht_difference_size - 1;

	addSubVoxel((Vec3i){block_pos_i.x << 1,block_pos_i.y << 1,block_pos_i.z << 1},pos_i,depht_difference,g_player.edit_depth,voxel_cpy.type);
    octreeRefresh();
}

void mButtonDown(void){
	if(keyDown(KEY_LCONTROL)){
		Vec3i octree_position;
		if(!blockOutlinePositionGet(&octree_position))
			return;
		Voxel* voxel = voxelGet(octree_position,g_player.edit_depth);
		int sub_voxel_count = voxelMemoryCountRecursive(voxel);
		if(g_voxel_template)
			tFree(g_voxel_template);
		g_voxel_template = tMallocZero(sizeof(*g_voxel_template) * sub_voxel_count);
		octreeSerialize(g_voxel_template,voxel);
		return;
	}
	if(g_voxel_template){
		tFree(g_voxel_template);
		g_voxel_template = 0;
	}
	Vec3Axis side;
	Vec3 direction = getLookDirection(g_surface.angle);
	Voxel* voxel = treeRayTraceAndInit(g_surface.position,direction,&side,(TreeTraceFlags){.everything_solid = true});
	if(!voxel)
		return;
	g_player.voxel_copy = voxel;
}

void mouseMove(int delta_x,int delta_y){
	g_surface.angle.x += realMulR(intToReal(delta_x) / 0x100,g_options.ms_sensitivity);
	g_surface.angle.y += realMulR(intToReal(delta_y) / 0x100,g_options.ms_sensitivity);

    g_surface.angle.y = tClamp(g_surface.angle.y,FIXED_ONE / 4,FIXED_ONE - FIXED_ONE / 4);
}

VoxelPointed g_voxel_pointed;

void boxQuadWireframeDraw(Vec3 position,Vec3 size,int color,bool octree){
	Vec3 points[] = {
		vec3Add(position,(Vec3){0,0,0}),
		vec3Add(position,(Vec3){0,0,size.z}),
		vec3Add(position,(Vec3){0,size.y,0}),
		vec3Add(position,(Vec3){0,size.y,size.z}),
		vec3Add(position,(Vec3){size.x,0,0}),
		vec3Add(position,(Vec3){size.x,0,size.z}),
		vec3Add(position,(Vec3){size.x,size.y,0}),
		vec3Add(position,(Vec3){size.x,size.y,size.z}),
	};
	for(int i = 0;i < countof(points);i++)
		points[i] = pointToScreen(points[i]);

	int table[][4] = {
		{0,1,3,2},
		{4,5,7,6},
		{0,1,5,4},
		{2,3,7,6},
		{0,2,6,4},
		{1,3,7,5},
	};
	for(int i = 0;i < 6;i++){
		for(int j = 0;j < (4);j++){
			if(points[table[i][j]].z && points[table[i][(j + 1) % 4]].z){
				if(octree){
					DrawPrimitive* polygon = primitiveToDraw();
					polygon->luminance = pixelColorToColor(color);
					polygon->position[0] = points[table[i][j]];
					polygon->position[1] = points[table[i][(j + 1) % 4]];
					polygon->type = PRIMITIVE_LINE;
					polygon->texture = 0;
					polygon->has_lighting = false;
				}
				else{
					drawLine(&g_surface,points[table[i][j]].x,points[table[i][j]].y,points[table[i][(j + 1) % 4]].x,points[table[i][(j + 1) % 4]].y,pixelColorToColor(color));	
				}
			}
		}
	}
}

Vec3 fibonnaciSphereSample(int i,int n){
    real golden_angle = REAL_UNIT * 0x62;

    real z = FIXED_ONE - ((i * 2 + 1) * FIXED_ONE) / n;

    real zz = realMulR(z,z);
    real r = tSqrt(FIXED_ONE - zz);

    real phi = tFract(i * golden_angle);

    real x = realMulR(r,tCos(phi));
    real y = realMulR(r,tSin(phi));

    return (Vec3){x,y,z};
}

static void generateBlockOutline(Vec3 sub_block_pos,real sub_block_size){
	Vec3 look_direction = getLookDirection(g_surface.angle);
	Vec3Axis side;

    Voxel* voxel = treeRayTraceAndInit(g_surface.position,look_direction,&side,(TreeTraceFlags){.everything_solid = true});
    
	g_voxel_pointed.voxel = voxel;
	g_voxel_pointed.side = side;

	if(!voxel)
		return;

	Vec2 uv = voxelGuiPositionGet(voxel,g_surface.position,look_direction,side);

	g_voxel_pointed.uv = uv;

    uv = uvMirror(uv,side << 1 | look_direction.a[side] < 0);

	if(g_voxel_static[g_voxel_pointed.voxel->type].no_blockplace || !g_voxel_placement)
		return;

	real edit_size = depthToSize(g_player.edit_depth);

	Vec3i block_pos_i = {voxel->position_x,voxel->position_y,voxel->position_z};

	block_pos_i.a[side] += (look_direction.a[side] < 0) * 2 - 1;
	Vec3i octree_position;
	if(voxel->depth > g_player.edit_depth){
		int shift = voxel->depth - g_player.edit_depth;
		octree_position = (Vec3i){block_pos_i.x >> shift,block_pos_i.y >> shift,block_pos_i.z >> shift};
	}
	else{
		int shift = g_player.edit_depth - voxel->depth;
		octree_position = (Vec3i){block_pos_i.x << shift,block_pos_i.y << shift,block_pos_i.z << shift};;
	}
	Vec3 point[8];
	Vec2 screen_point[8];
    
	if(voxel->depth < g_player.edit_depth){
		Vec2i axis = g_axis_table[side << 1];
        uv = vec2MulS(uv,intToReal(1 << g_player.edit_depth - voxel->depth));
		octree_position.a[side] += ((look_direction.a[side] > 0) << (g_player.edit_depth - voxel->depth)) - (look_direction.a[side] > 0);
		octree_position.a[axis.x] += realToInt(uv.x);
		octree_position.a[axis.y] += realToInt(uv.y);
	}
	
	Vec3 octree_position_r = {octree_position.x * edit_size,octree_position.y * edit_size,octree_position.z * edit_size};
	realMul(&sub_block_size,edit_size);
	sub_block_pos = (Vec3){sub_block_pos.x * edit_size,sub_block_pos.y * edit_size,sub_block_pos.z * edit_size};

	boxQuadWireframeDraw(vec3Add(octree_position_r,sub_block_pos),vec3Single(sub_block_size),0xFFFFFF,false);
}

#define GEN_SIZE (1 << 4)

void worldDestroy(void){
    allocatorFreeListFreeAll(&g_allocator_world);
    g_voxel_link_list = 0;
    g_voxel_tick_list = 0;
    g_world.voxel = (Voxel){0};
}

void worldDefaultGenerate(void){
    voxelSet(&g_world.voxel,(Vec3i){64,64,64},7,VOXEL_CONSOLE);
    voxelSet(&g_world.voxel,(Vec3i){0,0,0},1,VOXEL_STONE);
	voxelSet(&g_world.voxel,(Vec3i){0,1,0},1,VOXEL_STONE);
	voxelSet(&g_world.voxel,(Vec3i){1,1,0},1,VOXEL_STONE);
	voxelSet(&g_world.voxel,(Vec3i){1,0,0},1,VOXEL_STONE);
}

static String worldNameToPath(String name){
    String path = STRING_LITERAL("world/");
    String path_name = stringConcat(&g_arena_frame,path,name);
    String path_name_ext = stringConcat(&g_arena_frame,path_name,(String)STRING_LITERAL(".octvxl\0"));
    return path_name_ext;
}

bool worldExist(String name){
    return storageFileExist(worldNameToPath(name).data);
}

bool worldLoad(String name){
    FileContent file = storageFileRead(&g_arena_frame,worldNameToPath(name).data);

	if(!file.content)
		return false;

    struct{
        int32 size;
        char* content;
    } world = {.size = *(int32*)file.content,.content = (char*)(file.content + sizeof world.size)};
    
    if(!world.content)
        return false;
    Voxel** voxel_array = virtualAllocate(sizeof(Voxel*) * world.size);
	int index_i = 0;
	g_world.voxel = *octreeDeserializeRecursive((void*)world.content,0,0,0,(Vec3i){0,0,0},voxel_array,&index_i);
	index_i = 0;
	octreeDeserializeLink((void*)world.content,0,0,(Vec3i){0,0,0},voxel_array,&index_i);
	virtualFree(voxel_array,sizeof(Voxel*) * world.size);
    return true;
}

void worldSave(String name){
#ifdef __linux__
    linuxOctreeSerialize(&g_world.voxel,worldNameToPath(name).data);
#elif defined(_MSC_VER)
	win32OctreeSerialize(&g_world.voxel,worldNameToPath(name).data);
#endif
}

char g_voxel_lighting_tree[0x100000];

Vec2 aspectRatioTransform(Vec2 v){
    real ratio = realDivR(tMax(g_surface.window_width,g_surface.window_height),tMin(g_surface.window_width,g_surface.window_height));
    
    if(g_surface.window_width < g_surface.window_height)
        v.x = realDivR(v.x,ratio);
    else
        v.y = realDivR(v.y,ratio);
    
    return v;
}

void mainInit(void){
    printNumberNL(mipmapGet((Vec3){0},(Vec3){0},intToReal(0x10),FIXED_ONE));
    printNumberNL(mipmapGet((Vec3){0},(Vec3){0},intToReal(0x20),FIXED_ONE));
    printNumberNL(mipmapGet((Vec3){0},(Vec3){0},intToReal(0x40),FIXED_ONE));
    printNumberNL(mipmapGet((Vec3){0},(Vec3){0},intToReal(0x80),FIXED_ONE));
    if(!worldLoad((String)STRING_LITERAL("world_1")))
        worldDefaultGenerate();

    voxelChildMaskSet(&g_world.voxel);
    
    threadInit();
    openclInit();

	if(g_options.editor){
		g_player.movement_fly = true;
		g_voxel_placement = true;
	}

    g_player.entity = entityCreate((Vec3)PLAYER_SPAWN_POSITION,ENTITY_PLAYER);
    
    if(!g_voxel_placement)
        g_player.weapon = entityCreate(g_player.entity->position,ENTITY_WEAPON);

	for(int i = 0;i < 5 * 6;i++){
		real offset_x = i / 5 * (REAL_UNIT * 0x28);
		real offset_y = i % 5 * (REAL_UNIT * 0x28);
		g_inventory_gui[i] = (VoxelGuiElement){
			.type = VOXEL_GUI_INVENTORY_SLOT,
			.position = (Vec2){offset_x + REAL_UNIT * 0x08,REAL_UNIT * 0x08 + offset_y},
			.inventory_slot = g_inventory + i,
		};
	}
	g_surface.backend = g_options.render_backend;
	entityInit();
	surfaceInit(&g_surface);
    g_surface.fov = aspectRatioTransform(vec2Single(g_options.rd_fov));
	texturesGenerate();
}

#define SKYBOX_POLYGON_SIZE 16
#define SKYBOX_POLYGON_RSIZE (FIXED_ONE * SKYBOX_POLYGON_SIZE)

static void setViewPlanes(void){
    Vec3 corner_angle[] = {
		screenRayDirection(g_surface.rotation_matrix,-FIXED_ONE,-FIXED_ONE,g_surface.fov.x,g_surface.fov.y),
		screenRayDirection(g_surface.rotation_matrix,FIXED_ONE,-FIXED_ONE,g_surface.fov.x,g_surface.fov.y),
		screenRayDirection(g_surface.rotation_matrix,FIXED_ONE,FIXED_ONE,g_surface.fov.x,g_surface.fov.y),
		screenRayDirection(g_surface.rotation_matrix,-FIXED_ONE,FIXED_ONE,g_surface.fov.x,g_surface.fov.y),
	};
	g_view_plane[0] = vec3Cross(corner_angle[0],corner_angle[1]);
	g_view_plane[1] = vec3Cross(corner_angle[1],corner_angle[2]);
	g_view_plane[2] = vec3Cross(corner_angle[2],corner_angle[3]);
	g_view_plane[3] = vec3Cross(corner_angle[3],corner_angle[0]);

    real wideness = REAL_UNIT * 0x100;
            
    Vec3 corner_angle_l[] = {
		screenRayDirection(g_surface.rotation_matrix,-wideness,-wideness,g_surface.fov.x,g_surface.fov.y),
		screenRayDirection(g_surface.rotation_matrix,wideness,-wideness,g_surface.fov.x,g_surface.fov.y),
		screenRayDirection(g_surface.rotation_matrix,wideness,wideness,g_surface.fov.x,g_surface.fov.y),
		screenRayDirection(g_surface.rotation_matrix,-wideness,wideness,g_surface.fov.x,g_surface.fov.y),
	};
#if 1
	g_view_plane_lighting[0] = vec3Cross(corner_angle_l[0],corner_angle_l[1]);
	g_view_plane_lighting[1] = vec3Cross(corner_angle_l[1],corner_angle_l[2]);
	g_view_plane_lighting[2] = vec3Cross(corner_angle_l[2],corner_angle_l[3]);
	g_view_plane_lighting[3] = vec3Cross(corner_angle_l[3],corner_angle_l[0]);
#endif
}

Voxel* g_voxel_link_list;

structure(RayTracePixelsArg){
	Vec3 trace_position;
	Vec2 trace_angle;
	int offset;
	int amount;
	Vec3* buffer;
	int trace_tick;
	int trace_size;
};

static void nextSpellDraw(void){
#if 0
    Gui2dFlags bottom_left = (Gui2dFlags){.invert_x = true,.invert_y = true};
    gui2dStringDraw(0x4000,0x1000,(String)STRING_LITERAL("next spell"),0x1000,0xFFFFFF,0x1000,bottom_left);
	int i = 0;
		
	InventorySlot* spell_slot;
	do
		spell_slot = g_equipped.spell_array + (g_spell_index + i) % g_equipped.capacity;
	while(i++ < 0x10 && (spell_slot->type != INVENTORY_SPELL || g_spell_static[spell_slot->spell_type].adjective));

	i = 0;

	if(g_spell_static[spell_slot->spell_type].adjective)
		return;

	for(int offset = 0;;offset++){
		InventorySlot* spell_slot;
		do
			spell_slot = g_equipped.spell_array + (g_spell_index + i) % g_equipped.capacity;
		while(i++ < 0x10 && spell_slot->type != INVENTORY_SPELL);

		if(spell_slot->type != INVENTORY_SPELL)
			break;

		real x = 0xA00 * 2;
		real y = offset * 0x2000 + 0x1000;

        int frame_color = 0x202020;
		int color = 0x8080800;
		if(g_spell_static[spell_slot->spell_type].adjective){
			color = 0xA02020;
            frame_color = 0x200000;
        }

        int thickness = 0x100;
        
        real size_x = FIXED_ONE / 8 - thickness * 6;
        real size_y = FIXED_ONE / 8 - thickness * 6;

        gui2dFrameDraw(x,y,size_x,size_y,color,thickness,bottom_left);
        gui2dRectangleDraw(x + thickness,y + thickness,size_x - thickness * 2,size_y - thickness * 2,frame_color,bottom_left);
        
		switch(spell_slot->spell_type){
			case SPELL_BOLT:{
                gui2dEllipsesDraw(x + 0x300,y + 0x300,0x0A00,0x0A00,0xFF0000,bottom_left);
			} break;
			case SPELL_BOMB:{
                return;
				real size = 0x300;
                real fuse = 0x800;

                gui2dEllipsesDraw(x,y,size * 4,size * 4,0x202020,bottom_left);
                gui2dEllipsesDraw(x - size / 2,y + size / 2,size,size,0x606060,bottom_left);

                gui2dRectangleDraw(x - size * 2 - size / 4,y - size / 8 - size / 2,size,size * 4,0x404040,bottom_left);
                gui2dRectangleDraw(x - size * 2 - size / 4 - fuse / 2,y,fuse,size,0x404040,bottom_left);
                gui2dRectangleDraw(x - size * 2 - size / 4 - fuse / 2 - size / 2,y,size,size,0x404040,bottom_left);
#if 0
				drawEllipses(&g_surface,x,y,fixedMulR(size * 4,g_surface.fov.x),fixedMulR(size * 4,g_surface.fov.y),vec3Single(1 << 14));
				drawEllipses(&g_surface,x - size / 2,y + size / 2,fixedMulR(size,g_surface.fov.x),fixedMulR(size,g_surface.fov.y),vec3Single(1 << 18));
				drawRectangle(&g_surface,x - size * 2 - size / 4,y - size / 8 - size / 2,fixedMulR(size,g_surface.fov.x),fixedMulR(size,g_surface.fov.y) * 4,vec3Single(1 << 16));
				
				drawRectangle(&g_surface,x - size * 2 - size / 4 - fuse / 2,y,fixedMulR(fuse,g_surface.fov.x),fixedMulR(size,g_surface.fov.y),pixelColorToColor(0x83B2EB));
				drawRectangle(&g_surface,x - size * 2 - size / 4 - fuse / 2 - size / 2,y,fixedMulR(size,g_surface.fov.x),fixedMulR(size,g_surface.fov.y),pixelColorToColor(0x1050FF));
#endif
            } break;
			case SPELL_ORB:{
                gui2dEllipsesDraw(x + 0x300,y + 0x300,0x0A00,0x0A00,0x00FF00,bottom_left);
			} break;
			case SPELL_ADJ_SPEED:{
				Vec2 string_uv = vec2Add((Vec2){x,y},(Vec2){0x1200,0x600});
                gui2dStringDraw(string_uv.x,string_uv.y,(String)STRING_LITERAL(">>"),0xA00,0xFFFFFF,0x1400,bottom_left);
			} break;
			case SPELL_ADJ_DAMAGE:{
                Vec2 string_uv = vec2Add((Vec2){x,y},(Vec2){0x1200,0x600});
                gui2dStringDraw(string_uv.x,string_uv.y,(String)STRING_LITERAL("#+"),0xA00,0xFFFFFF,0x1400,bottom_left);
			} break;
			case SPELL_ADJ_DOUBLER:{
                Vec2 string_uv = vec2Add((Vec2){x,y},(Vec2){0x1200,0x600});
                gui2dStringDraw(string_uv.x,string_uv.y,(String)STRING_LITERAL("x2"),0xA00,0xFFFFFF,0x1400,bottom_left);
			} break;
			default:{
                gui2dEllipsesDraw(x + 0x300,y + 0x300,0x0A00,0x0A00,0xFF00FF,bottom_left);
			} break;
		}

		if(g_spell_static[spell_slot->spell_type].adjective)
			continue;

		break;
	}
#endif
}
#include "console.h"

void frameRender(void){
    if(g_options.gl_wireframe)
		surfaceClear(&g_surface);
    
    g_surface.position = g_player.entity->position;
    g_surface.position.z += FIXED_ONE / 2 + FIXED_ONE / 4;

    g_surface.rotation_matrix[0] = tCos(g_surface.angle.x);
    g_surface.rotation_matrix[1] = tSin(g_surface.angle.x);
    g_surface.rotation_matrix[2] = tCos(g_surface.angle.y);
    g_surface.rotation_matrix[3] = tSin(g_surface.angle.y);

    g_options.ms_sensitivity = FIXED_ONE / 0x40;

    g_options.lighting_engine = true;
    
	setViewPlanes();

    entityVoxelInsertSimulation();
	lightingOctree();
    entityVoxelRemove();
    
	if(g_surface.backend == RENDER_BACKEND_GL)
		antiAliasingEnableGL(false);

    if(!g_options.gl_wireframe){
        for(int k = 0;k < countof(g_axis_table);k++){
            Vec3 coordinates[] = {
                g_surface.position,
                g_surface.position,
                g_surface.position,
                g_surface.position,
            };

            coordinates[1].a[g_axis_table[k].x] += FIXED_ONE * 2;
            coordinates[3].a[g_axis_table[k].x] += FIXED_ONE * 2;
            coordinates[3].a[g_axis_table[k].y] += FIXED_ONE * 2;
            coordinates[2].a[g_axis_table[k].y] += FIXED_ONE * 2;

            Vec3 color[4];

            for(int j = 0;j < countof(coordinates);j++){
                coordinates[j].a[k >> 1] += k % 2 ? -FIXED_ONE : FIXED_ONE;
                coordinates[j].a[g_axis_table[k].x] -= FIXED_ONE;
                coordinates[j].a[g_axis_table[k].y] -= FIXED_ONE;
            }
 
            if(
               !pointInScreenSpace(g_view_plane,coordinates[0]) && 
               !pointInScreenSpace(g_view_plane,coordinates[1]) && 
               !pointInScreenSpace(g_view_plane,coordinates[2]) && 
               !pointInScreenSpace(g_view_plane,coordinates[3])
            )
                continue;
            
            for(int j = 0;j < countof(coordinates);j += 1)
                color[j] = vec3MulS(COLOR_WHITE,g_exposure);
           
            LightmapTree* lightmap = memoryArenaAllocateZero(&g_arena_frame,sizeof *lightmap);
            if(g_surface.backend == RENDER_BACKEND_SOFTWARE){
                for(int i = countof(lightmap->child);i--;){
                    lightmap->child[i] = memoryArenaAllocateZero(&g_arena_frame,sizeof *lightmap->child[i]);
                    lightmap->child[i]->luminance = color[i];
                }
            }
            
            Vec2 text_crd[] = {
                g_texture_coordinates_fill[2],
                g_texture_coordinates_fill[3],
                g_texture_coordinates_fill[0],
                g_texture_coordinates_fill[1],
            };
            drawSkyboxPolygon3d(&g_surface,g_skybox.textures + k,text_crd,coordinates,color,lightmap);
        }
    }

	if(g_surface.backend == RENDER_BACKEND_GL)
		antiAliasingEnableGL(true);

    entityVoxelInsertSimulation();
	entityDynamicLighting();
    entityVoxelRemove();
    
	entityVoxelInsertRender();

	if(voxelPositionGet(g_surface.position)->type == VOXEL_WATER){
        int WATER_RES = 256;
		for(int i = 0;i < WATER_RES * WATER_RES;i += 1){
			int x = i / WATER_RES;
			int y = i % WATER_RES;

			real n_x = x * FIXED_ONE / (WATER_RES / 2) - FIXED_ONE;
			real n_y = y * FIXED_ONE / (WATER_RES / 2) - FIXED_ONE;

			real offset_x = realShr(tCos(intToReal(g_time.time / 0x4000 % 0x100) / 0x100 + n_x),6);
			real offset_y = realShr(tCos(intToReal(g_time.time / 0x4000 % 0x100) / 0x100 + n_x),6);
			Vec3 direction = screenRayDirection(g_surface.rotation_matrix,n_x + offset_x,n_y + offset_y,g_surface.fov.x,g_surface.fov.y);

			Vec3 color = vec3Shr(rayLuminance(g_surface.position,vec3Normalize(direction),(RayLuminanceFlag){0}),4);

			drawRectangle(&g_surface,n_x,n_y,FIXED_ONE / (WATER_RES / 8),FIXED_ONE / (WATER_RES / 8),color);
		}
	}
	else{
		octreeDraw(&g_world.voxel);
        octreeDrawList();
        if(g_surface.backend == RENDER_BACKEND_SOFTWARE)
            spanDrawList(&g_surface);
	}

	voxelEntityRemove();
    //return;
#if 0
    if(g_options.lighting_engine){
        int brightness_acc = 0;

        TraverseInit init = initTraverse(g_surface.position);

        int exposure_samle_row_size = 32;

        for(int i = 0;i < exposure_samle_row_size * exposure_samle_row_size;i++){
            int x = i / exposure_samle_row_size;
            int y = i % exposure_samle_row_size;

            real n_x = x * FIXED_ONE / (exposure_samle_row_size / 2) - FIXED_ONE;
            real n_y = y * FIXED_ONE / (exposure_samle_row_size / 2) - FIXED_ONE;

            Vec3 luminance = rayLuminanceInit(init,g_surface.position,screenRayDirection(g_surface.rotation_matrix,n_x,n_y,g_surface.fov.x,g_surface.fov.y));

            brightness_acc += realShr(tMax(luminance.x,tMax(luminance.y,luminance.z)),4);
        }

        if(brightness_acc){
            g_exposure = tMix(g_exposure,realDivR(FIXED_ONE,brightness_acc >> 4),FIXED_ONE / 0x100);
            g_exposure = tMix(g_exposure,FIXED_ONE,FIXED_ONE / 0x40);
        }
    }
#endif
	if(g_luminance_overlay){
		for(int i = 0;i < 64 * 64;i++){
			int x = i / 64;
			int y = i % 64;

			real n_x = x * FIXED_ONE / 32 - FIXED_ONE;
			real n_y = y * FIXED_ONE / 32 - FIXED_ONE;

            Vec3 direction = screenRayDirection(g_surface.rotation_matrix,n_x,n_y,g_surface.fov.x,g_surface.fov.y);
            
			Vec3 color = vec3Shr(rayLuminance(g_surface.position,direction,(RayLuminanceFlag){0}),4);

			drawRectangle(&g_surface,n_x / 4 - FIXED_ONE + FIXED_ONE / 4,n_y / 4 - FIXED_ONE + FIXED_ONE / 4,FIXED_ONE / 128,FIXED_ONE / 128,color);
		}
    }
    if(g_options.rd_occlusion){    
        for(int i = 0;i < OCCLUSION_BUFFER_SIZE * OCCLUSION_BUFFER_SIZE;i++){
			int x = i / OCCLUSION_BUFFER_SIZE;
			int y = i % OCCLUSION_BUFFER_SIZE;

			real n_x = x * FIXED_ONE / (OCCLUSION_BUFFER_SIZE / 2);
			real n_y = y * FIXED_ONE / (OCCLUSION_BUFFER_SIZE / 2);

            int n_bit = sizeof(*g_surface.occlusion_buffer) * CHAR_BIT;
            
			Vec3 color = g_surface.occlusion_buffer[i / n_bit] >> i % n_bit & 1 ? COLOR_WHITE : (Vec3){0};

			drawRectangle(&g_surface,n_x / 4 - FIXED_ONE,-n_y / 4 + FIXED_ONE,FIXED_ONE / (OCCLUSION_BUFFER_SIZE * 2),FIXED_ONE / (OCCLUSION_BUFFER_SIZE * 2),color);
		}
        for(int i = 0;i < OCCLUSION_BUFFER_SIZE * OCCLUSION_BUFFER_SIZE;i++){
			int x = i / OCCLUSION_BUFFER_SIZE;
			int y = i % OCCLUSION_BUFFER_SIZE;

			real n_x = x * FIXED_ONE / (OCCLUSION_BUFFER_SIZE / 2);
			real n_y = y * FIXED_ONE / (OCCLUSION_BUFFER_SIZE / 2);

            int n_bit = sizeof(*g_surface.occlusion_buffer) * CHAR_BIT;
            
			Vec3 color = g_surface.occlusion_debug[i / n_bit] >> i % n_bit & 1 ? COLOR_WHITE : (Vec3){0};

			drawRectangle(&g_surface,n_x / 4 - FIXED_ONE + FIXED_ONE,-n_y / 4 + FIXED_ONE,FIXED_ONE / (OCCLUSION_BUFFER_SIZE * 2),FIXED_ONE / (OCCLUSION_BUFFER_SIZE * 2),color);
		}
    }
	if(g_options.editor){
		for(Voxel* voxel = g_voxel_link_list;voxel;voxel = voxel->next_voxel_link){
            for(int i = 0;i < voxel->n_link;i++){
                Vec3 p1 = vec3AddS(voxelWorldPos(voxel),depthToSize(voxel->depth) / 2);
                Vec3 p2 = vec3AddS(voxelWorldPos(voxel->links[i]),depthToSize(voxel->links[i]->depth) / 2);
                drawLine3d(&g_surface,p1,p2,0xFF00FF);    
            }
		}
        if(g_options.rd_entity_hitbox)
            entityDrawHitbox();
	}
    if(g_voxel_interact){
        boxQuadWireframeDraw(voxelWorldPos(g_voxel_interact),vec3Single(depthToSize(g_voxel_interact->depth)),0x00FF00,false);
    }
    if(g_options.ray_test){
        static struct{
            Vec3 begin;
            Vec3 end;
        } rays[0x100];
        static uint8 counter;
        Vec3 ray_start = vec3Add(g_player.entity->position,(Vec3){FIXED_ONE,FIXED_ONE,-FIXED_ONE});
        RayHit hit = rayHitPosition(ray_start,getLookDirection(g_surface.angle));
        if(hit.voxel){
            rays[counter].begin = ray_start;
            rays[counter].end = hit.position;
            counter += 1;
        }
        for(int i = countof(rays);i--;){
            drawLine3d(&g_surface,rays[i].begin,rays[i].end,tHash(i) & 0xFFFFFF);
        }
    }

	generateBlockOutline(vec3Single(0),FIXED_ONE);

	genBlockSelect();
#if 0
	if(g_equipped_staff){
        int percentage = fixedDivR(g_mana,g_equipped.mana_max);
        gui2dFrameDraw(0x2800,0x1000,0x1000,0x8000,0x808080,0x100,(Gui2dFlags){0});
        gui2dRectangleDraw(0x2900,0x1100 + fixedMulR(0x7E00,FIXED_ONE - percentage),0x0E00,fixedMulR(0x7E00,percentage),0xA060FF,(Gui2dFlags){0});
        gui2dRectangleDraw(0x2900,0x1100,0x0E00,fixedMulR(0x7E00,FIXED_ONE - percentage),0x503080,(Gui2dFlags){0});
        gui2dStringDraw(0x2900,0x8A00,(String)STRING_LITERAL("mana"),0xE00,0x281840,0x1400,(Gui2dFlags){0});
	}
#endif  
    gui2dFrameDraw(REAL_UNIT * 0x18,REAL_UNIT * 0x10,REAL_UNIT * 0x10,REAL_UNIT * 0x80,0x808080,REAL_UNIT,(Gui2dFlags){0});
    real healt_offset = g_player.entity->health;
    gui2dRectangleDraw((Vec2){REAL_UNIT * 0x19,REAL_UNIT * 0x11 + realMulR(REAL_UNIT * 0x7E,FIXED_ONE - healt_offset)},(Vec2){REAL_UNIT * 0x0E,realMulR(REAL_UNIT * 0x7E,healt_offset)},0xFF3030,(Gui2dFlags){0});
    gui2dRectangleDraw((Vec2){REAL_UNIT * 0x19,REAL_UNIT * 0x11},(Vec2){REAL_UNIT * 0x0E,realMulR(REAL_UNIT * 0x7E,FIXED_ONE - healt_offset)},0x801818,(Gui2dFlags){0});
    gui2dStringDraw(REAL_UNIT * 0x19,REAL_UNIT * 0x8A,(String)STRING_LITERAL("health"),REAL_UNIT * 0x10,0x401010,REAL_UNIT * 0x10,(Gui2dFlags){0});

	if(button_link){
		Vec3 p1 = (Vec3){0};
		Vec3 p2 = pointToScreen(vec3AddS(voxelWorldPos(button_link),depthToSize(button_link->depth) / 2));
		if(p2.z > 0)
			drawLine(&g_surface,p1.x,p1.y,p2.x,p2.y,pixelColorToColor(0xFF00FF));
	}
    
	if(g_spell_hold){
		switch(g_spell_hold){
			case SPELL_BOLT:{
				drawCircle(&g_surface,0,0,0x800,pixelColorToColor(0xFF0000));
			} break;
			case SPELL_BOMB:{
				drawCircle(&g_surface,0,0,0x800,pixelColorToColor(0x0000FF));
			} break;
			case SPELL_ORB:{
				drawCircle(&g_surface,0,0,0x800,pixelColorToColor(0x00FF00));
			} break;
		}
	}
	//draw next spell in line
	if(g_equipped.capacity)
        nextSpellDraw();
#if 0
	if(g_boss){
		int progress = (fixedDivR(g_boss->health,g_boss->health));
        int bar_width = 0x10000;
        progress = fixedMulR(progress,bar_width);
        gui2dRectangleDraw(0x1000,-0x8000,0x1000,progress,0xFF3030,(Gui2dFlags){.middle_y = true});
        gui2dRectangleDraw(0x1000,-0x8000 + progress,0x1000,bar_width - progress,0x801818,(Gui2dFlags){.middle_y = true});
        gui2dFrameDraw(0x1000,-0x8000,0x1000,bar_width,0x808080,0x100,(Gui2dFlags){.middle_y = true});

        gui2dStringDraw(0x1100,0x1000,(String)STRING_LITERAL("boss"),0xE00,0x401010,0x1400,(Gui2dFlags){.middle_y = true});
    }
#endif  
    tMemset(g_surface.occlusion_buffer,0,OCCLUSION_BUFFER_SIZE * OCCLUSION_BUFFER_SIZE / CHAR_BIT);
    tMemset(g_surface.occlusion_debug,0,OCCLUSION_BUFFER_SIZE * OCCLUSION_BUFFER_SIZE / CHAR_BIT);

    //crosshair
    gui2dRectangleDraw((Vec2){-FIXED_ONE / 64,-FIXED_ONE / 512},(Vec2){FIXED_ONE / 32,FIXED_ONE / 256},0x00FF00,(Gui2dFlags){.middle_x = true,.middle_y = true});
    gui2dRectangleDraw((Vec2){-FIXED_ONE / 512,-FIXED_ONE / 64},(Vec2){FIXED_ONE / 256,FIXED_ONE / 32},0x00FF00,(Gui2dFlags){.middle_x = true,.middle_y = true});

    memoryArenaFree(&g_arena_frame);

    g_time.frame_tick += 1;
}


