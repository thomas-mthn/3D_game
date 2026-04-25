#include "main.h"
#include "draw.h"
#include "fixed.h"
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
#include "audio.h"
#include "span.h"
#include "libc.h"
#include "thread.h"
#include "console.h"
#include "gui2d.h"
#include "opencl.h"

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
    .multi_thread = true,
	.editor = true,
	.fast_startup = false,
    .lighting_engine = false,
	.fov = {FIXED_ONE / 2,FIXED_ONE / 3},
	.render_backend = RENDER_BACKEND_GL,
    .gl_wireframe = false,
    .textures = true,
    .smooth_lighting = true,
};

bool g_movement_fly;
bool g_voxel_placement;
int g_edit_depth = 10;

VoxelType g_voxel_select = VOXEL_AIR;

Vec3 g_velocity;
int g_exposure = FIXED_ONE;

int g_health = FIXED_ONE / 8;
bool g_luminance_overlay;

uint8 g_key[0x100];
Vec2 g_cursor;

bool g_pickup_collected;

Plane g_view_plane[4];

int bilinearScalar(Vec2 position,int* values){
	int color_x0 = tMix(values[0],values[1],fixedFract(position.y));
	int color_x1 = tMix(values[2],values[3],fixedFract(position.y));

	return tMix(color_x0,color_x1,fixedFract(position.x));
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
    	fixedMulR(tCos(angle.x),tCos(angle.y)),
        fixedMulR(tSin(angle.x),tCos(angle.y)),
        tSin(angle.y)
    };
}

static void pointInScreenSpaceSide(Vec3 point,int* sides){
	Vec3 transformed = vec3Shr(vec3Sub(point,g_surface.position),0);
	
	if(vec3Dot(transformed,getLookDirection(g_surface.angle)) < 0)
		sides[0] += 1;
	
	for(int i = 0;i < countof(g_view_plane);i++){
		if(vec3Dot(transformed,g_view_plane[i].normal) > 0)
			sides[i + 1] += 1;
	}
}

bool pointInScreenSpace(Vec3 point){
	Vec3 transformed = vec3Shr(vec3Sub(point,g_surface.position),0);
	
	if(vec3Dot(transformed,getLookDirection(g_surface.angle)) < 0)
		return false;
	
	for(int i = 0;i < countof(g_view_plane);i++){
		if(vec3Dot(transformed,g_view_plane[i].normal) > 0)
			return false;
	}
	return true;
}

bool squareInScreenSpace(Vec3* point){
	int result[5] = {0};
	for(int i = 0;i < 4;i++)
		pointInScreenSpaceSide(point[i],result);

	return result[0] != 4 && result[1] != 4 && result[2] != 4 && result[3] != 4 && result[4] != 4;
}

bool cubeInScreenSpace(Vec3* point){
	int result[5] = {0};
	for(int i = 0;i < 8;i++)
		pointInScreenSpaceSide(point[i],result);

	return result[0] != 8 && result[1] != 8 && result[2] != 8 && result[3] != 8 && result[4] != 8;
}

Vec3 screenRayDirection(int* tri,int x,int y,int fov_x,int fov_y){
	Vec3 ray_ang;
	
	int pixel_offset_x = fixedMulR(x,fixedDivR(FIXED_ONE,fov_x));
	int pixel_offset_y = fixedMulR(y,fixedDivR(FIXED_ONE,fov_y));
	ray_ang.x = fixedMulR(tri[0],tri[2]);
	ray_ang.y = fixedMulR(tri[1],tri[2]);
	ray_ang.x -= fixedMulR(fixedMulR(tri[0],tri[3]),pixel_offset_x);
	ray_ang.y -= fixedMulR(fixedMulR(tri[1],tri[3]),pixel_offset_x);
	ray_ang.y += fixedMulR(tri[0],pixel_offset_y);
	ray_ang.x -= fixedMulR(tri[1],pixel_offset_y);
	ray_ang.z = tri[3] + fixedMulR(tri[2],pixel_offset_x);
	return ray_ang;
}

Vec3 pointToScreen(Vec3 point){
	Vec3 screen_point;
	Vec3 pos = vec3Sub(point,g_surface.position);
	int temp;
	temp  = fixedMulR(pos.y,g_surface.rotation_matrix[0]) - fixedMulR(pos.x,g_surface.rotation_matrix[1]);
	pos.x = fixedMulR(pos.y,g_surface.rotation_matrix[1]) + fixedMulR(pos.x,g_surface.rotation_matrix[0]);
	pos.y = temp;
	temp  = fixedMulR(pos.z,g_surface.rotation_matrix[2]) - fixedMulR(pos.x,g_surface.rotation_matrix[3]);
	pos.x = fixedMulR(pos.z,g_surface.rotation_matrix[3]) + fixedMulR(pos.x,g_surface.rotation_matrix[2]);
	pos.z = temp;

	if(pos.x <= 0)
		return vec3Single(0);

	screen_point.x = fixedDivR(fixedMulR(pos.z,g_options.fov.x),pos.x);
	screen_point.y = fixedDivR(fixedMulR(pos.y,g_options.fov.y),pos.x);
	if(screen_point.x > FIXED_ONE * 16 || screen_point.x < -FIXED_ONE * 16 || screen_point.y > FIXED_ONE * 16 || screen_point.y < -FIXED_ONE * 16)
		return vec3Single(0);
	screen_point.z = pos.x;
	return screen_point;
}

Vec3 pointToScreenRenderer(Vec3 point,int* tri,Vec3 renderer_position,Vec2 fov){
	Vec3 screen_point;
	Vec3 pos = vec3Sub(point,renderer_position);
	int temp;
	temp  = fixedMulR(pos.y,tri[0]) - fixedMulR(pos.x,tri[1]);
	pos.x = fixedMulR(pos.y,tri[1]) + fixedMulR(pos.x,tri[0]);
	pos.y = temp;
	temp  = fixedMulR(pos.z,tri[2]) - fixedMulR(pos.x,tri[3]);
	pos.x = fixedMulR(pos.z,tri[3]) + fixedMulR(pos.x,tri[2]);
	pos.z = temp;

	screen_point.x = fixedMulR(pos.z,fov.x);
	screen_point.y = fixedMulR(pos.y,fov.y);

	screen_point.z = pos.x;
	return screen_point;
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
	Voxel* previous = 0;
	for(Voxel* voxel = g_voxel_tick_list;voxel;voxel = voxel->next_voxel_tick){
		switch(voxel->type){
			case VOXEL_AIR:{
				voxel->animation -= 0x1000;
				voxel->entity_list = 0;
				if(previous)
					previous->next_voxel_tick = voxel->next_voxel_tick;
				else
					g_voxel_tick_list = voxel->next_voxel_tick;
			} continue;
		}
		previous = voxel;
	}
	entityVoxelRemove();
}

bool g_gui_clicked;

Vec2 g_recoil;

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
	if(g_health <= 0){
		entityDestroyAll();
		g_surface.position = (Vec3)PLAYER_SPAWN_POSITION;
		g_surface.angle = (Vec2)PLAYER_SPAWN_ANGLE;
		g_health = FIXED_ONE;
		g_velocity = (Vec3){0};
	}
	if(g_attack_animation){
		if(g_equipped_staff){
			g_attack_animation -= fixedDivR(FIXED_ONE,g_equipped.delay) >> 16;
			g_attack_animation = tMax(g_attack_animation,0);
		}
		else{
			g_attack_animation -= 0x600;
			g_attack_animation = tMax(g_attack_animation,0);
		}
	}
    else if(keyDown(KEY_LBUTTON) && g_equipped_staff && !g_gui_clicked){
		staffFire();
        g_recoil = vec2Add(g_recoil,vec2Shr(vec2Rnd(),0));
	}
    if(g_equipped_staff){
        g_mana += g_equipped.mana_generation;
        g_mana = tMin(g_mana,g_equipped.mana_max);
    }
    g_surface.angle = vec2Add(g_surface.angle,vec2Shr(g_recoil,8));
    g_recoil = vec2MulS(g_recoil,FIXED_ONE - 0x1000);

    if(g_recoil.x < 0)
        g_recoil.x += 1;
    if(g_recoil.y < 0)
        g_recoil.y += 1;
    
	int n_entity = 0;
	for(Entity* entity = g_entity;entity;entity = entity->next)
		n_entity += 1;
	int change = fixedDivR(FIXED_ONE,(n_entity + 1) * FIXED_ONE) / 64;
	
	if(tRnd() % FIXED_ONE < change && !g_options.editor)
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
                int block_size = depthToSize(voxel->depth);
                Vec3 block_pos = voxelWorldPos(voxel);
                int door_size = fixedMulR(block_size,FIXED_ONE - voxel->animation) / 2;
                if(voxel->opened)
                    door_size = block_size / 2 - door_size;
                int door_size_inv = block_size / 2 - door_size;
                bool box_1 = boxBoxIntersect(g_surface.position,PLAYER_SIZE,block_pos,(Vec3){block_size,door_size,block_size});

                if(box_1){
                    voxel->animation += 16 << voxel->depth;
                    g_velocity.y += 0x100;
                }

                bool box_2 = boxBoxIntersect(g_surface.position,PLAYER_SIZE,(Vec3){block_pos.x,block_pos.y + block_size / 2 + door_size_inv,block_pos.z},(Vec3){block_size,door_size,block_size});
                if(box_2){
                    voxel->animation += 16 << voxel->depth;
                    g_velocity.y -= 0x100;
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

	if(g_movement_fly)
		movementFly();
	else
		movementNormal();
    
	entityVoxelInsert();
	entityTick();
	voxelEntityRemove();
	entityDestroy();

	spinningStaffSpin();
	
	g_tick += 1;
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

unsigned g_tick;
int g_delta;

Plane getPlane(Voxel* voxel,Vec3 dir,unsigned side){
    Plane plane;
	int size = depthToSize(voxel->depth);
	Vec3 pos = voxelWorldPos(voxel);
	switch(side){
		case VEC3_X:
			plane.normal = (Vec3){(dir.x < 0) ? FIXED_ONE : -FIXED_ONE,0,0};
		    plane.distance = (dir.x < 0) ? -((int*)&pos)[side] - size : ((int*)&pos)[side];
		    break;
		case VEC3_Y:
			plane.normal = (Vec3){0,(dir.y < 0) ? FIXED_ONE : -FIXED_ONE,0};
		    plane.distance = (dir.y < 0) ? -((int*)&pos)[side] - size : ((int*)&pos)[side];
		    break;
		case VEC3_Z:
			plane.normal = (Vec3){0,0,(dir.z < 0) ? FIXED_ONE : -FIXED_ONE};
		    plane.distance = (dir.z < 0) ? -((int*)&pos)[side] - size : ((int*)&pos)[side];
		    break;
	}
	return plane;
}

int treeRayTraceDistance(Voxel* voxel,Vec3 position,Vec3 dir,int side){
	if(!voxel)
		return INT_MAX;
    int block_size = depthToSize(voxel->depth);
	Vec3 pos = voxelWorldPos(voxel);
    Vec2 axis = g_axis_table[side << 1];
    Plane plane = getPlane(voxel,dir,side);
    return rayPlaneIntersection(position,dir,plane);
}

static void addSubVoxel(Vec3 vpos,Vec3 pos,int depth,int remove_depth,VoxelType voxel_type){
	if(--depth == -1)
		return;
	bool offset_x = pos.x >> depth & 1;
	bool offset_y = pos.y >> depth & 1;
	bool offset_z = pos.z >> depth & 1;
	for(int i = 0;i < 8;i++){
		Vec3 lpos = {(i & 1 << 2) >> 2,(i & 1 << 1) >> 1,(i & 1 << 0) >> 0};
		if(offset_x == lpos.x && offset_y == lpos.y && offset_z == lpos.z){
			Vec3 pos2 = {vpos.x + offset_x << 1,vpos.y + offset_y << 1,vpos.z + offset_z << 1};
			addSubVoxel(pos2,pos,depth,remove_depth,voxel_type);
			continue;
		}
		voxelSet(&g_voxel,vec3Add(vpos,lpos),remove_depth - depth,voxel_type);
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

static char keyTranslate(char key){
    char key_char_table[] = {
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
        [KEY_OEM_PERIOD] = '.',[KEY_OEM_MINUS] = '/',[KEY_RETURN] = '\n',[KEY_BACK] = KEYTRANSLATE_BACK,
    };
    char key_char_shift_table[] = {
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

void configSave(void){
#ifdef _MSC_VER
    win32SaveConfig();
#elif defined(__linux__)
    linuxSaveConfig();
#endif
}

static Entity* hand;

void keyPress(int key){
    if(g_voxel_interact){
        if(g_voxel_interact->type == VOXEL_CONSOLE){
            consoleInput(keyTranslate(key));
            return;
        }
        if(g_voxel_interact->type == VOXEL_STRING){
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
        }
    }
	switch(key){
		case KEY_G:{
			if(!g_equipped_staff)
				break;
			Entity* staff = entityCreate(g_surface.position,ENTITY_STAFF);
			staff->velocity = vec3Shr(getLookDirection(g_surface.angle),4);
			staff->model = g_equipped.model;
			staff->staff = g_equipped;
			voxelMenuStaffEditorDefault();
			g_equipped = (Staff){0};
			g_equipped_staff = false;
		} break;
		case KEY_E:{
			if(!g_options.editor)
				break;
			entityCreate(g_surface.position,ENTITY_SLIME);
		} break;
		case KEY_M:{
			if(!g_options.editor)
				break;
			g_movement_fly ^= true;
		} break;
		case KEY_V:{
			if(!g_options.editor)
				break;

			g_voxel_placement ^= true;
            
            if(g_voxel_placement)
                hand->health = 0;
            else
                hand = entityCreate(g_surface.position,ENTITY_WEAPON);
		} break;
		case KEY_O:{
			static Vec3 pre_settings_position;
			if(in_settings){
				voxelSet(&g_voxel,(Vec3){0,0,(1 << 4) - 1},4,VOXEL_AIR);
				
				g_surface.position = pre_settings_position;

                configSave();
			}
			else{
				pre_settings_position = g_surface.position;
                
				voxelSet(&g_voxel,(Vec3){0,0,(1 << 5) - 2},5,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_voxel,(Vec3){0,0,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_voxel,(Vec3){0,1,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_voxel,(Vec3){0,2,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_voxel,(Vec3){0,3,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_voxel,(Vec3){0,4,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_voxel,(Vec3){1,4,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_voxel,(Vec3){2,4,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_voxel,(Vec3){3,4,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_voxel,(Vec3){4,4,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_voxel,(Vec3){4,3,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_voxel,(Vec3){4,2,(1 << 7) - 4},7,VOXEL_MENU);
				voxelSet(&g_voxel,(Vec3){4,1,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_voxel,(Vec3){4,0,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_voxel,(Vec3){3,0,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_voxel,(Vec3){2,0,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);
				voxelSet(&g_voxel,(Vec3){1,0,(1 << 7) - 4},7,VOXEL_UNDESTRUCTIBLE);

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
			g_voxel_select -= 1;
			if(g_voxel_select == -1)
				g_voxel_select = VOXEL_ECOUNT - 1;
		} break;
		case KEY_RIGHT:{
			g_voxel_select += 1;
			if(g_voxel_select == VOXEL_ECOUNT)
				g_voxel_select = 0;
		} break;
		case KEY_ADD:{
			g_edit_depth += 1;
		} break;
		case KEY_SUBTRACT:{
			g_edit_depth -= 1;
		} break;
	}
}

static Vec2 voxelPosition2D(Voxel* voxel,Vec3 position,Vec3 dir,int side){
    return uvMirror(voxelGuiPositionGet(voxel,position,dir,side),side << 1 | dir.a[side] < 0);
}

bool blockOutlinePositionGet(Vec3* position){
	int side;
	Vec3 dir = getLookDirection(g_surface.angle);
	Voxel* voxel = treeRayTraceAndInit(g_surface.position,dir,&side);
	if(!voxel)
		return false;
	Vec3 block_pos_i = (Vec3){voxel->position_x,voxel->position_y,voxel->position_z};
	block_pos_i.a[side] += (dir.a[side] < 0) * 2 - 1;
			
	if(voxel->depth > g_edit_depth){
		int shift = voxel->depth - g_edit_depth;
		*position = vec3Shr(block_pos_i,shift);
	}
	else{
		int shift = g_edit_depth - voxel->depth;
		*position = vec3Shl(block_pos_i,shift);
	}
	if(voxel->depth < g_edit_depth){
		int depht_difference = g_edit_depth - voxel->depth;
		Vec2 uv = voxelPosition2D(voxel,g_surface.position,dir,side);
		Vec2 axis = g_axis_table[side << 1];
		uv.x *= 1 << depht_difference;
		uv.y *= 1 << depht_difference;
		position->a[side] += ((dir.a[side] > 0) << depht_difference) - (dir.a[side] > 0);
		position->a[axis.x] += uv.x >> FIXED_PRECISION;
		position->a[axis.y] += uv.y >> FIXED_PRECISION;
	}
	return true;
}

VoxelSerialized* g_voxel_template;

static void voxelTemplatePlace(VoxelSerialized* voxel_array,int voxel_index,Vec3 position,int depth){
	if(voxel_index == -1)
		return;
	VoxelSerialized* voxel = (VoxelSerialized*)((char*)voxel_array + voxel_index);
	if(voxel->type == VOXEL_PARENT){
		VoxelSerializedParent* voxel_parent = (VoxelSerializedParent*)voxel;
		for(int i = 0;i < 8;i++){
			Vec3 child_position = {
				position.x << 1 | (i >> 0 & 1),
				position.y << 1 | (i >> 1 & 1),
				position.z << 1 | (i >> 2 & 1)
			};
			voxelTemplatePlace(voxel_array,voxel_parent->child_s[i],child_position,depth + 1);
		}
		return;
	}
	voxelSet(&g_voxel,position,depth,voxel->type);
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
    if(!voxel)
		return;
    if(keyDown(KEY_LMENU)){
        if(voxel->type == VOXEL_CONSOLE || voxel->type == VOXEL_STRING)
            g_voxel_interact = voxel;
        return;
    }
    if(g_voxel_interact){
        g_voxel_interact = 0;
        return;
    }
	if(!g_voxel_placement){
		if(voxelGuiOnClick(voxel,g_voxel_pointed.side)){
			g_gui_clicked = true;
			return;
		}
		if(!g_attack_animation && !g_equipped_staff){
			Vec3 direction = getLookDirection(g_surface.angle);
			int side;
			Voxel* voxel = treeRayTraceAndInit(g_surface.position,direction,&side);
			int distance;
			if(voxel)
				distance = treeRayTraceDistance(voxel,g_surface.position,direction,side);
			else
				distance = INT_MAX;

			entityVoxelInsert();
			Entity* entity = entityRayCollisionRecursive(&g_voxel,g_surface.position,direction);
			voxelEntityRemove();
			if(entity && vec3Distance(g_surface.position,entity->position) < FIXED_ONE * 6 && vec3Distance(g_surface.position,entity->position) < distance){
				entityHit(entity);
			}
			else if(distance < FIXED_ONE * 6){
				Vec3 spawn_position = rayHitPosition(voxel,g_surface.position,direction,side);
				Vec3 color = rayLuminance(g_surface.position,direction);
				for(int i = 0x10;i--;){
					Entity* particle = entityCreate(vec3Mix(spawn_position,g_surface.position,FIXED_ONE / 16),ENTITY_PARTICLE);
					particle->health = tRnd() % 0x80 + 0x80;
					particle->size = FIXED_ONE / 16;
					particle->velocity = vec3Shr(vec3Rnd(),4);
					particle->color = vec3Shl(color,4);
				}	
				audioPlay(spawn_position,AUDIO_PUNCH_HIT);
			}
			audioPlay(g_surface.position,AUDIO_PUNCH);
			g_attack_animation = FIXED_ONE;
		}
		if(voxel){
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
							pickup->velocity.x += tRnd() % (FIXED_ONE / 8) - FIXED_ONE / 16;
							pickup->velocity.y += tRnd() % (FIXED_ONE / 8) - FIXED_ONE / 16;
							pickup->velocity.z += tRnd() % (FIXED_ONE / 8) + FIXED_ONE / 8;
							pickup->color_emit = (Vec3){tRnd() % FIXED_ONE,tRnd() % FIXED_ONE,tRnd() % FIXED_ONE};
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
	}
	VoxelStatic* voxel_s = g_voxel_static + voxel->type;
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
	
	Vec3 octree_position;
	if(!blockOutlinePositionGet(&octree_position))
		return;
	if(keyDown(KEY_LCONTROL)){
		static struct{
			Vec3 position;
			int depth;
			bool setted;
		} voxel_fill_begin;
		if(!voxel_fill_begin.setted || voxel_fill_begin.depth != g_edit_depth){
			voxel_fill_begin.position = octree_position;
			voxel_fill_begin.depth = g_edit_depth;
			voxel_fill_begin.setted = true;
		}
		else{
			Vec3 begin = voxel_fill_begin.position;
			VoxelType type = g_voxel_select;
			for(int x = tMin(begin.x,octree_position.x);x <= tMax(begin.x,octree_position.x);x++){
				for(int y = tMin(begin.y,octree_position.y);y <= tMax(begin.y,octree_position.y);y++){
					for(int z = tMin(begin.z,octree_position.z);z <= tMax(begin.z,octree_position.z);z++)
						voxelSet(&g_voxel,(Vec3){x,y,z},g_edit_depth,type);
				}
			}
			voxel_fill_begin.setted = false;
		}
		return;
	}
	if(g_voxel_template){
		voxelTemplatePlace(g_voxel_template,0,octree_position,g_edit_depth);
		return;
	}

	voxelSet(&g_voxel,octree_position,g_edit_depth,g_voxel_select);
}

void rButtonDown(void){
	if(!g_options.editor){
		if(g_equipped_staff)
			staffSkip();
		return;
	}
	int side;
	Vec3 direction = getLookDirection(g_surface.angle);
	Voxel* voxel = treeRayTraceAndInit(g_surface.position,direction,&side);
	if(!voxel)
		return;
	Vec3 block_pos_i = {voxel->position_x,voxel->position_y,voxel->position_z};

	Voxel voxel_cpy = *voxel;

	voxelSet(&g_voxel,(Vec3){voxel->position_x,voxel->position_y,voxel->position_z},voxel->depth,VOXEL_AIR);

	if(voxel->depth >= g_edit_depth)
		return;
			
	int depht_difference = g_edit_depth - voxel_cpy.depth;
	Vec3 octree_position = vec3Shl(block_pos_i,depht_difference);
	Vec2 axis = g_axis_table[side << 1];
	Vec2 uv = voxelGuiPositionGet(&voxel_cpy,g_surface.position,direction,side);
    uv = uvMirror(uv,side << 1 | direction.a[side] < 0);
	uv.x *= 1 << depht_difference;
	uv.y *= 1 << depht_difference;
	octree_position.a[side] += ((direction.a[side] < 0) << depht_difference) - (direction.a[side] < 0);
	octree_position.a[axis.x] += uv.x >> FIXED_PRECISION;
	octree_position.a[axis.y] += uv.y >> FIXED_PRECISION;
	Vec3 pos_i;
	int depht_difference_size = 1 << depht_difference;
	pos_i.x = (int)octree_position.x & depht_difference_size - 1;
	pos_i.y = (int)octree_position.y & depht_difference_size - 1;
	pos_i.z = (int)octree_position.z & depht_difference_size - 1;

	addSubVoxel(vec3Shl(block_pos_i,1),pos_i,depht_difference,g_edit_depth,voxel_cpy.type);
}

void mButtonDown(void){
	if(keyDown(KEY_LCONTROL)){
		Vec3 octree_position;
		if(!blockOutlinePositionGet(&octree_position))
			return;
		Voxel* voxel = voxelGet(octree_position,g_edit_depth);
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
	int side;
	Vec3 direction = getLookDirection(g_surface.angle);
	Voxel* voxel = treeRayTraceAndInit(g_surface.position,direction,&side);
	if(!voxel)
		return;
	g_voxel_select = voxel->type;
}

void mouseMove(int delta_x,int delta_y){
	g_surface.angle.x += delta_x << 3;
	g_surface.angle.y += delta_y << 3;

    g_surface.angle.y = tClamp(g_surface.angle.y,FIXED_ONE / 4,FIXED_ONE - FIXED_ONE / 4);
}

static int mandelbrot(int px,int py){
	int x = 0;
	int y = 0;

	for(int i = 0;i < 0x100;i++){
		int xtemp = fixedMulR(x,x) - fixedMulR(y,y) + px;
		y = fixedMulR(x * 2,y) + py;
		x = xtemp;
		if(x + y > FIXED_ONE * 0x10)
			return i;
	}
	return 0x100;
}

VoxelPointed g_voxel_pointed;

void boxQuadWireframeDraw(Vec3 position,Vec3 size,int color){
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
			if(points[table[i][j]].z && points[table[i][(j + 1) % 4]].z)
				drawLine(&g_surface,points[table[i][j]].x,points[table[i][j]].y,points[table[i][(j + 1) % 4]].x,points[table[i][(j + 1) % 4]].y,pixelColorToColor(color));
		}
	}
}

static void generateBlockOutline(Vec3 sub_block_pos,int sub_block_size){
	Vec3 look_direction = getLookDirection(g_surface.angle);
	TraverseInit init = initTraverse(g_surface.position);
	int side;
	Voxel* voxel = treeRayTrace(init.voxel,init.pos,look_direction,&side);

	g_voxel_pointed.voxel = voxel;
	g_voxel_pointed.side = side;

	if(!voxel)
		return;

	Vec2 uv = voxelGuiPositionGet(voxel,g_surface.position,look_direction,side);

	g_voxel_pointed.uv = uv;

    uv = uvMirror(uv,side << 1 | look_direction.a[side] < 0);

	if(g_voxel_static[g_voxel_pointed.voxel->type].no_blockplace || !g_voxel_placement)
		return;

	int edit_size = depthToSize(g_edit_depth);

	Vec3 block_pos_i = {voxel->position_x,voxel->position_y,voxel->position_z};

	block_pos_i.a[side] += (look_direction.a[side] < 0) * 2 - 1;
	Vec3 octree_position;
	if(voxel->depth > g_edit_depth){
		int difference = voxel->depth - g_edit_depth;
		octree_position = (Vec3){block_pos_i.x >> difference,block_pos_i.y >> difference,block_pos_i.z >> difference};
	}
	else{
		int difference = g_edit_depth - voxel->depth;
		octree_position = (Vec3){block_pos_i.x << difference,block_pos_i.y << difference,block_pos_i.z << difference};
	}
	Vec3 point[8];
	Vec2 screen_point[8];

	if(voxel->depth < g_edit_depth){
		Vec2 axis = g_axis_table[side << 1];
		uv.x *= 1 << g_edit_depth - voxel->depth;
		uv.y *= 1 << g_edit_depth - voxel->depth;
		octree_position.a[side] += ((look_direction.a[side] > 0) << (g_edit_depth - voxel->depth)) - (look_direction.a[side] > 0);
		octree_position.a[axis.x] += uv.x >> 16;
		octree_position.a[axis.y] += uv.y >> 16;
	}
	
	octree_position = (Vec3){octree_position.x * edit_size,octree_position.y * edit_size,octree_position.z * edit_size};
	fixedMul(&sub_block_size,edit_size);
	sub_block_pos = (Vec3){sub_block_pos.x * edit_size,sub_block_pos.y * edit_size,sub_block_pos.z * edit_size};

	boxQuadWireframeDraw(vec3Add(octree_position,sub_block_pos),vec3Single(sub_block_size),0xFFFFFF);
}

#define GEN_SIZE (1 << 4)

FileContent fileRead(char* path){
#ifdef __linux__
    return linuxFileRead(path);
#else
    return (FileContent){0};
#endif
}

void worldDestroy(void){
    allocatorFreeListFreeAll(&g_allocator_world);
    g_voxel_link_list = 0;
    g_voxel_tick_list = 0;
    g_voxel = (Voxel){0};
}

void worldDefaultGenerate(void){
    voxelSet(&g_voxel,(Vec3){64,64,64},7,VOXEL_CONSOLE);
    voxelSet(&g_voxel,(Vec3){0,0,0},1,VOXEL_BLOCK);
	voxelSet(&g_voxel,(Vec3){0,1,0},1,VOXEL_BLOCK);
	voxelSet(&g_voxel,(Vec3){1,1,0},1,VOXEL_BLOCK);
	voxelSet(&g_voxel,(Vec3){1,0,0},1,VOXEL_BLOCK);
}

static String worldNameToPath(String name){
    String path = STRING_LITERAL("world/");
    String path_name = stringConcat(path,name);
    String path_name_ext = stringConcat(path_name,(String)STRING_LITERAL(".octvxl\0"));
    return path_name_ext;
}

bool worldExist(String name){
    return fileRead(worldNameToPath(name).data).content;
}

bool worldLoad(String name){
    FileContent file = fileRead(worldNameToPath(name).data);

    struct{
        int32 size;
        char* content;
    } world = {.size = *(int32*)file.content,.content = (char*)(file.content + sizeof world.size)};
    
    if(!world.content)
        return false;
    Voxel** voxel_array = virtualAllocate(sizeof(Voxel*) * world.size);
	int index_i = 0;
	g_voxel = *octreeDeserializeRecursive((void*)world.content,0,0,0,(Vec3){0,0,0},voxel_array,&index_i);
	index_i = 0;
	octreeDeserializeLink((void*)world.content,0,0,(Vec3){0,0,0},voxel_array,&index_i);
	virtualFree(voxel_array,sizeof(Voxel*) * world.size);
    return true;
}

void worldSave(String name){
#ifdef __linux__
    linuxOctreeSerialize(&g_voxel,worldNameToPath(name).data);
#endif
}

char g_voxel_lighting_tree[0x100000];

void mainInit(void){
    if(!worldLoad((String)STRING_LITERAL("world_1")))
        worldDefaultGenerate();
    
    threadInit();
    openclInit();

	if(g_options.editor){
		g_movement_fly = true;
		g_voxel_placement = true;
	}
    
    if(!g_voxel_placement)
        hand = entityCreate(g_surface.position,ENTITY_WEAPON);

	for(int i = 0;i < 5 * 6;i++){
		int offset_x = i / 5 * 0x2800;
		int offset_y = i % 5 * 0x2800;
		g_inventory_gui[i] = (VoxelGuiElement){
			.type = VOXEL_GUI_INVENTORY_SLOT,
			.position = (Vec2){offset_x + 0x800,0x800 + offset_y},
			.inventory_slot = g_inventory + i,
		};
	}
	g_surface.backend = g_options.render_backend;
	entityInit();
	surfaceInit(&g_surface);
	texturesGenerate();
}

#define SKYBOX_POLYGON_SIZE 16

static void setViewPlanes(void){
    Vec3 corner_angle[] = {
		screenRayDirection(g_surface.rotation_matrix,-FIXED_ONE,-FIXED_ONE,g_options.fov.x,g_options.fov.y),
		screenRayDirection(g_surface.rotation_matrix,FIXED_ONE,-FIXED_ONE,g_options.fov.x,g_options.fov.y),
		screenRayDirection(g_surface.rotation_matrix,FIXED_ONE,FIXED_ONE,g_options.fov.x,g_options.fov.y),
		screenRayDirection(g_surface.rotation_matrix,-FIXED_ONE,FIXED_ONE,g_options.fov.x,g_options.fov.y),
	};
	g_view_plane[0].normal = vec3Cross(corner_angle[0],corner_angle[1]);
	g_view_plane[1].normal = vec3Cross(corner_angle[1],corner_angle[2]);
	g_view_plane[2].normal = vec3Cross(corner_angle[2],corner_angle[3]);
	g_view_plane[3].normal = vec3Cross(corner_angle[3],corner_angle[0]);
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

		int x = 0xA00 * 2;
		int y = offset * 0x2000 + 0x1000;

        int frame_color = 0x202020;
		int color = 0x8080800;
		if(g_spell_static[spell_slot->spell_type].adjective){
			color = 0xA02020;
            frame_color = 0x200000;
        }

        int thickness = 0x100;
        
        int size_x = FIXED_ONE / 8 - thickness * 6;
        int size_y = FIXED_ONE / 8 - thickness * 6;

        gui2dFrameDraw(x,y,size_x,size_y,color,thickness,bottom_left);
        gui2dRectangleDraw(x + thickness,y + thickness,size_x - thickness * 2,size_y - thickness * 2,frame_color,bottom_left);
        
		switch(spell_slot->spell_type){
			case SPELL_BOLT:{
                gui2dEllipsesDraw(x + 0x300,y + 0x300,0x0A00,0x0A00,0xFF0000,bottom_left);
			} break;
			case SPELL_BOMB:{
                return;
				int size = 0x300;
                int fuse = 0x800;

                gui2dEllipsesDraw(x,y,size * 4,size * 4,0x202020,bottom_left);
                gui2dEllipsesDraw(x - size / 2,y + size / 2,size,size,0x606060,bottom_left);

                gui2dRectangleDraw(x - size * 2 - size / 4,y - size / 8 - size / 2,size,size * 4,0x404040,bottom_left);
                gui2dRectangleDraw(x - size * 2 - size / 4 - fuse / 2,y,fuse,size,0x404040,bottom_left);
                gui2dRectangleDraw(x - size * 2 - size / 4 - fuse / 2 - size / 2,y,size,size,0x404040,bottom_left);
#if 0
				drawEllipses(&g_surface,x,y,fixedMulR(size * 4,g_options.fov.x),fixedMulR(size * 4,g_options.fov.y),vec3Single(1 << 14));
				drawEllipses(&g_surface,x - size / 2,y + size / 2,fixedMulR(size,g_options.fov.x),fixedMulR(size,g_options.fov.y),vec3Single(1 << 18));
				drawRectangle(&g_surface,x - size * 2 - size / 4,y - size / 8 - size / 2,fixedMulR(size,g_options.fov.x),fixedMulR(size,g_options.fov.y) * 4,vec3Single(1 << 16));
				
				drawRectangle(&g_surface,x - size * 2 - size / 4 - fuse / 2,y,fixedMulR(fuse,g_options.fov.x),fixedMulR(size,g_options.fov.y),pixelColorToColor(0x83B2EB));
				drawRectangle(&g_surface,x - size * 2 - size / 4 - fuse / 2 - size / 2,y,fixedMulR(size,g_options.fov.x),fixedMulR(size,g_options.fov.y),pixelColorToColor(0x1050FF));
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
}

void frameRender(void){
    if(g_options.gl_wireframe)
		surfaceClear(&g_surface);

    g_surface.rotation_matrix[0] = tCos(g_surface.angle.x);
    g_surface.rotation_matrix[1] = tSin(g_surface.angle.x);
    g_surface.rotation_matrix[2] = tCos(g_surface.angle.y);
    g_surface.rotation_matrix[3] = tSin(g_surface.angle.y);
    
	setViewPlanes();

	lightingOctree();
    
#if !defined(__wasm__) && !defined(__linux__)
	if(g_surface.backend == RENDER_BACKEND_GL)
		antiAliasingEnableGL(false);
#endif
	for(int i = 0;i < (SKYBOX_POLYGON_SIZE * SKYBOX_POLYGON_SIZE);i++){
		int size = FIXED_ONE / SKYBOX_POLYGON_SIZE;

		int x = i / SKYBOX_POLYGON_SIZE * FIXED_ONE / SKYBOX_POLYGON_SIZE;
		int y = i % SKYBOX_POLYGON_SIZE * FIXED_ONE / SKYBOX_POLYGON_SIZE;

		for(int k = 0;k < countof(g_axis_table);k++){
			Vec3 coordinates[] = {
				g_surface.position,
				g_surface.position,
				g_surface.position,
				g_surface.position,
			};

		    coordinates[1].a[g_axis_table[k].x] += size * 2;
		    coordinates[3].a[g_axis_table[k].x] += size * 2;
		    coordinates[3].a[g_axis_table[k].y] += size * 2;
		    coordinates[2].a[g_axis_table[k].y] += size * 2;

			Vec3 color[4];

			for(int j = 0;j < countof(coordinates);j++){
			    coordinates[j].a[g_axis_table[k].x] += i / SKYBOX_POLYGON_SIZE * size * 2 - FIXED_ONE;
				coordinates[j].a[g_axis_table[k].y] += i % SKYBOX_POLYGON_SIZE * size * 2 - FIXED_ONE;
				coordinates[j].a[k >> 1] += k % 2 ? -FIXED_ONE : FIXED_ONE;
			}

			if(
				!pointInScreenSpace(coordinates[0]) && 
				!pointInScreenSpace(coordinates[1]) && 
				!pointInScreenSpace(coordinates[2]) && 
				!pointInScreenSpace(coordinates[3])
			)
				continue;
            
			for(int j = 0;j < countof(coordinates);j += 1){
				color[j] = vec3Shl(skyboxSample(vec3Direction(g_surface.position,coordinates[j])),4);
				color[j] = vec3MulS(color[j],g_exposure);
			}
            
			Vec2 texture_coordinates[] = {
				{x,y},
				{x + size,y},
				{x + size,y + size},
				{x,y + size},
			};
            LightmapTree* lightmap = memoryArenaAllocateZero(&g_arena_frame,sizeof *lightmap);
            if(g_surface.backend == RENDER_BACKEND_SOFTWARE){
                for(int i = countof(lightmap->child);i--;){
                    lightmap->child[i] = memoryArenaAllocateZero(&g_arena_frame,sizeof(*lightmap->child[i]));
                    lightmap->child[i]->luminance = color[i];
                }
            }
            drawSkyboxPolygon3d(&g_surface,g_textures + g_skybox_textures[k],texture_coordinates,coordinates,color,lightmap);
		}
	}
#if !defined(__wasm__) && !defined(__linux__)
	if(g_surface.backend == RENDER_BACKEND_GL)
		antiAliasingEnableGL(true);
#endif
	entityDynamicLighting();

	entityVoxelInsert();
	
	if(voxelPositionGet(g_surface.position)->type == VOXEL_WATER){
		static int c[512 * 512 * 2];
		for(int i = 0;i < 512 * 512;i += 1){
			int x = i / 512;
			int y = i % 512;

			int n_x = x * FIXED_ONE / 256 - FIXED_ONE;
			int n_y = y * FIXED_ONE / 256 - FIXED_ONE;

			int offset_x = tCos((g_tick << 8) + n_x) >> 6;
			int offset_y = tCos((g_tick << 8) + n_x) >> 6;
			Vec3 direction = screenRayDirection(g_surface.rotation_matrix,n_x + offset_x,n_y + offset_y,g_options.fov.x,g_options.fov.y);

			Vec3 color = vec3Shl(rayLuminance(g_surface.position,vec3Normalize(direction)),4);

			c[i] = colorToPixelColor(color);
			drawRectangle(&g_surface,n_x,-n_y,FIXED_ONE / 64,FIXED_ONE / 64,color);
		}
	}
	else{
		octreeDraw(&g_voxel);
        if(g_surface.backend == RENDER_BACKEND_SOFTWARE)
            spanDrawList(&g_surface);
	}

	voxelEntityRemove();

    if(g_options.lighting_engine){
        int brightness_acc = 0;

        TraverseInit init = initTraverse(g_surface.position);

        int exposure_samle_row_size = 32;

        for(int i = 0;i < exposure_samle_row_size * exposure_samle_row_size;i++){
            int x = i / exposure_samle_row_size;
            int y = i % exposure_samle_row_size;

            int n_x = x * FIXED_ONE / (exposure_samle_row_size / 2) - FIXED_ONE;
            int n_y = y * FIXED_ONE / (exposure_samle_row_size / 2) - FIXED_ONE;

            Vec3 luminance = rayLuminanceInit(init,g_surface.position,screenRayDirection(g_surface.rotation_matrix,n_x,n_y,g_options.fov.x,g_options.fov.y));

            brightness_acc += tMax(luminance.x,tMax(luminance.y,luminance.z)) >> 4;
        }

        if(brightness_acc){
            g_exposure = tMix(g_exposure,fixedDivR(FIXED_ONE,brightness_acc >> 4),FIXED_ONE / 0x100);
            g_exposure = tMix(g_exposure,FIXED_ONE,FIXED_ONE / 0x40);
        }
    }
    
	if(g_luminance_overlay){
		for(int i = 0;i < 64 * 64;i++){
			int x = i / 64;
			int y = i % 64;

			int n_x = x * FIXED_ONE / 32 - FIXED_ONE;
			int n_y = y * FIXED_ONE / 32 - FIXED_ONE;

			Vec3 color = vec3Shl(rayLuminance(g_surface.position,screenRayDirection(g_surface.rotation_matrix,n_x,n_y,g_options.fov.x,g_options.fov.y)),4);

			drawRectangle(&g_surface,n_x / 4 - FIXED_ONE + FIXED_ONE / 4,n_y / 4 - FIXED_ONE + FIXED_ONE / 4,FIXED_ONE / 128,FIXED_ONE / 128,color);
		}
    }
    
	if(g_options.editor){
		for(Voxel* voxel = g_voxel_link_list;voxel;voxel = voxel->next_voxel_link){
            for(int i = 0;i < voxel->n_link;i++){
                Vec3 p1 = pointToScreen(vec3AddS(voxelWorldPos(voxel),depthToSize(voxel->depth) / 2));
                Vec3 p2 = pointToScreen(vec3AddS(voxelWorldPos(voxel->links[i]),depthToSize(voxel->links[i]->depth) / 2));
                if(p1.z <= 0 || p2.z <= 0)
                    continue;
                drawLine(&g_surface,p1.x,p1.y,p2.x,p2.y,pixelColorToColor(0xFF00FF));    
            }
		}
		entityDrawHitbox();
	}

	generateBlockOutline(vec3Single(0),FIXED_ONE);

	genBlockSelect();

	if(g_equipped_staff){
        int percentage = fixedDivR(g_mana,g_equipped.mana_max);
        gui2dFrameDraw(0x2800,0x1000,0x1000,0x8000,0x808080,0x100,(Gui2dFlags){0});
        gui2dRectangleDraw(0x2900,0x1100 + fixedMulR(0x7E00,FIXED_ONE - percentage),0x0E00,fixedMulR(0x7E00,percentage),0xA060FF,(Gui2dFlags){0});
        gui2dRectangleDraw(0x2900,0x1100,0x0E00,fixedMulR(0x7E00,FIXED_ONE - percentage),0x503080,(Gui2dFlags){0});
        gui2dStringDraw(0x2900,0x8A00,(String)STRING_LITERAL("mana"),0xE00,0x281840,0x1400,(Gui2dFlags){0});
	}
    
    gui2dFrameDraw(0x1800,0x1000,0x1000,0x8000,0x808080,0x100,(Gui2dFlags){0});
    gui2dRectangleDraw(0x1900,0x1100 + fixedMulR(0x7E00,FIXED_ONE - g_health),0x0E00,fixedMulR(0x7E00,g_health),0xFF3030,(Gui2dFlags){0});
    gui2dRectangleDraw(0x1900,0x1100,0x0E00,fixedMulR(0x7E00,FIXED_ONE - g_health),0x801818,(Gui2dFlags){0});
    gui2dStringDraw(0x1900,0x8A00,(String)STRING_LITERAL("health"),0xE00,0x401010,0x1400,(Gui2dFlags){0});
    
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

	if(g_boss){
		int progress = (fixedDivR(g_boss->health,g_entity_static[ENTITY_BOSS].health));
        int bar_width = 0x10000;
        progress = fixedMulR(progress,bar_width);
        gui2dRectangleDraw(0x1000,-0x8000,0x1000,progress,0xFF3030,(Gui2dFlags){.middle_y = true});
        gui2dRectangleDraw(0x1000,-0x8000 + progress,0x1000,bar_width - progress,0x801818,(Gui2dFlags){.middle_y = true});
        gui2dFrameDraw(0x1000,-0x8000,0x1000,bar_width,0x808080,0x100,(Gui2dFlags){.middle_y = true});

        gui2dStringDraw(0x1100,0x1000,(String)STRING_LITERAL("boss"),0xE00,0x401010,0x1400,(Gui2dFlags){.middle_y = true});
    }

    //crosshair
    gui2dRectangleDraw(-0x400,-0x80,0x800,0x100,0x00FF00,(Gui2dFlags){.middle_x = true,.middle_y = true});
    gui2dRectangleDraw(-0x80,-0x400,0x100,0x800,0x00FF00,(Gui2dFlags){.middle_x = true,.middle_y = true});
    
    memoryArenaFree(&g_arena_frame);
}


