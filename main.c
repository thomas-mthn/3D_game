#include "draw.h"
#include "fixed.h"
#include "vec2.h"
#include "vec3.h"
#include "memory.h"
#include "octree.h"
#include "octree_render.h"
#include "main.h"
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

#ifndef __wasm__
#include "win32/w_main.h"
#include "win32/w_draw_opengl.h"
#include "win32/w_kernel.h"
#include <intrin.h>
#endif

//compiler functions
//-----------------------------------------------
#pragma function(memset)
void *memset(void *s,int c,size_t n) {
    unsigned char *p = s;
    while(n--){
        *p++ = (unsigned char)c;
    }
    return s;
}

#pragma function(memcpy)
void* memcpy(void* dest,const void* src,size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

#pragma function(memmove)
void* memmove(void* dest,const void* src,size_t count){
	unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;

    if (d == s || count == 0)
        return dest;

    if(d < s){
        // Safe to copy forwards
        for(size_t i = 0; i < count; i++)
            d[i] = s[i];
    } 
	else{
        // Regions overlap, copy backwards
        for(size_t i = count; i != 0; i--)
            d[i - 1] = s[i - 1];
    }
    return dest;
}

//msvc needs this
#if defined(_MSC_VER) && !defined(__POCC__)
int _fltused = 0;
#endif
//-----------------------------------------------

static int numberStringLength(int number){
    int length = 0;
    if(!number)
        return 1;
    if(number < 0)
        length += 1;
    while(number){
        number /= 10;
        length += 1;
    }
    return length;
}

void intToString(char* buffer,int number){
	int length = numberStringLength(number);
	int length_copy;
	bool negative = false;
	int number_copy;

    if(number < 0){
        negative = true;
        number = -number;
    }
	number_copy = number;
	length_copy = length;

    while(length--){
        buffer[length] = number % 10 + '0';
        number /= 10;
    }
    if(negative)
        buffer[0] = '-';
    buffer[length_copy] = 0;
}

InventorySlot g_inventory[6 * 5];

bool g_editor = true;
bool g_movement_fly;
bool g_voxel_placement;
int g_edit_depth = 10;

VoxelType g_voxel_select = VOXEL_BOSS;

#define PLAYER_SPAWN_POSITION {(FIXED_ONE << 8) - FIXED_ONE * 20 - FIXED_ONE * 2,(FIXED_ONE << 8) - FIXED_ONE * 20 - FIXED_ONE * 2,(FIXED_ONE << 8) + FIXED_ONE * 4}
#define PLAYER_SPAWN_ANGLE {FIXED_ONE / 2,FIXED_ONE / 2}

Vec3 g_position = PLAYER_SPAWN_POSITION;
Vec3 g_velocity;
Vec2 g_angle = PLAYER_SPAWN_ANGLE;
int g_exposure = FIXED_ONE;
int g_tri[4];

int g_health = FIXED_ONE;
#ifdef __wasm__
bool g_smooth_lighting = true;
#else
bool g_smooth_lighting = true;
#endif
bool g_luminance_overlay;

uint8 g_key[0x100];
Vec2 g_cursor;

bool g_pickup_collected;

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

Vec2 g_fov = {FIXED_ONE / 2,FIXED_ONE / 3};

static void pointInScreenSpaceSide(Vec3 point,int* sides){
	Vec3 transformed = vec3ShrR(vec3SubR(point,g_position),0);
	
	if(vec3Dot(transformed,getLookDirection(g_angle)) < 0)
		sides[0] += 1;
	
	for(int i = 0;i < 4;i++){
		if(vec3Dot(transformed,g_view_plane[i].normal) > 0)
			sides[i + 1] += 1;
	}
}

bool pointInScreenSpace(Vec3 point){
	Vec3 transformed = vec3ShrR(vec3SubR(point,g_position),0);
	
	if(vec3Dot(transformed,getLookDirection(g_angle)) < 0)
		return false;
	
	for(int i = 0;i < 4;i++){
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
	Vec3 pos = vec3SubR(point,g_position);
	int temp;
	temp  = fixedMulR(pos.y,g_tri[0]) - fixedMulR(pos.x,g_tri[1]);
	pos.x = fixedMulR(pos.y,g_tri[1]) + fixedMulR(pos.x,g_tri[0]);
	pos.y = temp;
	temp  = fixedMulR(pos.z,g_tri[2]) - fixedMulR(pos.x,g_tri[3]);
	pos.x = fixedMulR(pos.z,g_tri[3]) + fixedMulR(pos.x,g_tri[2]);
	pos.z = temp;

	if(pos.x <= 0)
		return vec3Single(0);

	screen_point.x = fixedDivR(fixedMulR(pos.z,g_fov.x),pos.x);
	screen_point.y = fixedDivR(fixedMulR(pos.y,g_fov.y),pos.x);
	if(screen_point.x > FIXED_ONE * 16 || screen_point.x < -FIXED_ONE * 16 || screen_point.y > FIXED_ONE * 16 || screen_point.y < -FIXED_ONE * 16)
		return vec3Single(0);
	screen_point.z = pos.x;
	return screen_point;
}

Vec3 pointToScreenRenderer(Vec3 point,int* tri,Vec3 renderer_position,Vec2 fov){
	Vec3 screen_point;
	Vec3 pos = vec3SubR(point,renderer_position);
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

void tickRun(void){
	if(g_health <= 0){
		entityDestroyAll();
		g_position = (Vec3)PLAYER_SPAWN_POSITION;
		g_angle = (Vec2)PLAYER_SPAWN_ANGLE;
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
	}
	if(g_equipped_staff){
		g_mana += g_equipped.mana_generation;
		g_mana = tMin(g_mana,g_equipped.mana_max);
	}
	int n_entity = 0;
	for(Entity* entity = g_entity;entity;entity = entity->next)
		n_entity += 1;
	int change = fixedDivR(FIXED_ONE,(n_entity + 1) * FIXED_ONE) / 64;
	
	if(tRnd() % FIXED_ONE < change && !g_editor)
		entitySpawn();
		
	Voxel* previous = 0;
	for(Voxel* voxel = g_voxel_tick_list;voxel;voxel = voxel->next_voxel_tick){
		switch(voxel->type){
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
	 
	entityVoxelInsert();
	entityTick();
	voxelEntityRemove();
	entityDestroy();

	if(g_movement_fly)
		movementFly();
	else
		movementNormal();

	spinningStaffSpin();
	
	g_tick += 1;
}

int leadingZeroCount(int value){
#if !defined(__wasm__) && !defined(__clang__)
	return 31 - _lzcnt_u32(value);
#else
	int index = 0;
    while(value){
        index += 1;
		*(unsigned*)&value >>= 1;
    }
	return index - 1;
#endif
}

int bitScanReverse(int value){
	unsigned index;
#ifndef __wasm__
	_BitScanReverse(&index,value);
#else
	index = 0;
    while(value){
        index += 1;
		*(unsigned*)&value >>= 1;
    }
#endif
	return index;
}

unsigned g_tick;

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

Vec2 voxelLocalPositionGet(Voxel* voxel,Vec3 position,Vec3 dir,int side){
    int block_size = depthToSize(voxel->depth);
	Vec3 pos = voxelWorldPos(voxel);
    Vec2 axis = g_axis_table[side << 1];
    Plane plane = getPlane(voxel,dir,side);
    int dst = rayPlaneIntersection(position,dir,plane);
    Vec3 hit_pos = vec3AddR(position,vec3MulRS(dir,dst));
	Vec2 uv = vec2DivRS((Vec2){((int*)&hit_pos)[axis.x] - ((int*)&pos)[axis.x],((int*)&hit_pos)[axis.y] - ((int*)&pos)[axis.y]},block_size);
	return uv;
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
		voxelSet(&g_voxel,vec3AddR(vpos,lpos),remove_depth - depth,voxel_type);
	}
}

bool g_test_bool;
static bool g_in_settings;
 
void keyPress(int key){
	switch(key){
		case 'G':{
			if(!g_equipped_staff)
				break;
			Entity* staff = entityCreate(g_position,ENTITY_STAFF);
			staff->velocity = vec3ShrR(getLookDirection(g_angle),4);
			staff->model = g_equipped.model;
			staff->staff = g_equipped;
			voxelMenuStaffEditorDefault();
			g_equipped = (Staff){0};
			g_equipped_staff = false;
		} break;
		case 'E':{
			if(!g_editor)
				break;
			entityCreate(g_position,ENTITY_SLIME);
		} break;
		case 'M':{
			if(!g_editor)
				break;
			g_movement_fly ^= true;
		} break;
		case 'V':{
			if(!g_editor)
				break;
			g_voxel_placement ^= true;
		} break;
		case 'O':{
			static Vec3 pre_settings_position;
			if(g_in_settings){
				voxelSet(&g_voxel,(Vec3){0,0,(1 << 4) - 1},4,VOXEL_AIR);
				
				g_position = pre_settings_position;
			}
			else{
				pre_settings_position = g_position;

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

				g_position = (Vec3){0xA0000,0xA0000,(1 << 25) - 0x80000};
			}
			g_in_settings ^= true;
		} break;
		case KEY_F3:{
			g_test_bool ^= true;
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

bool blockOutlinePositionGet(Vec3* position){
	int side;
	Vec3 dir = getLookDirection(g_angle);
	Voxel* voxel = treeRayTraceAndInit(g_position,dir,&side);
	if(!voxel)
		return false;
	Vec3 block_pos_i = (Vec3){voxel->position_x,voxel->position_y,voxel->position_z};
	((int*)&block_pos_i)[side] += (((int*)&dir)[side] < 0) * 2 - 1;
			
	if(voxel->depth > g_edit_depth){
		int shift = voxel->depth - g_edit_depth;
		*position = vec3ShrR(block_pos_i,shift);
	}
	else{
		int shift = g_edit_depth - voxel->depth;
		*position = vec3ShlR(block_pos_i,shift);
	}
	if(voxel->depth < g_edit_depth){
		int depht_difference = g_edit_depth - voxel->depth;
		Vec2 uv = voxelLocalPositionGet(voxel,g_position,dir,side);
		Vec2 axis = g_axis_table[side << 1];
		uv.x *= 1 << depht_difference;
		uv.y *= 1 << depht_difference;
		((int*)position)[side] += ((((int*)&dir)[side] > 0) << depht_difference) - (((int*)&dir)[side] > 0);
		((int*)position)[axis.x] += uv.x >> FIXED_PRECISION;
		((int*)position)[axis.y] += uv.y >> FIXED_PRECISION;
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
		Entity* entity = entityCreate(g_position,ENTITY_PICKUP);
		entity->velocity = vec3ShrR(getLookDirection(g_angle),2);
		entity->pickup_type = g_spell_hold;
		g_spell_hold = 0;
	}
}

static Voxel* g_button_link;
Entity* g_boss;

void lButtonDown(void){
	Voxel* voxel = g_voxel_pointed.voxel;
	if(!g_voxel_placement){
		if(voxelGuiOnClick(voxel,g_voxel_pointed.side)){
			g_gui_clicked = true;
			return;
		}
		if(!g_attack_animation && !g_equipped_staff){
			Vec3 direction = getLookDirection(g_angle);
			int side;
			Voxel* voxel = treeRayTraceAndInit(g_position,direction,&side);
			int distance;
			if(voxel)
				distance = treeRayTraceDistance(voxel,g_position,direction,side);
			else
				distance = INT_MAX;

			entityVoxelInsert();
			Entity* entity = entityRayCollisionRecursive(&g_voxel,g_position,direction);
			voxelEntityRemove();
			if(entity && vec3Distance(g_position,entity->position) < FIXED_ONE * 6 && vec3Distance(g_position,entity->position) < distance){
				entityHit(entity);
			}
			else if(distance < FIXED_ONE * 6){
				Vec3 spawn_position = rayHitPosition(voxel,g_position,direction,side);
				Vec3 color = rayLuminance(g_position,direction);
				repeat(0x10){
					Entity* particle = entityCreate(vec3Mix(spawn_position,g_position,FIXED_ONE / 16),ENTITY_PARTICLE);
					particle->health = tRnd() % 0x80 + 0x80;
					particle->size = FIXED_ONE / 16;
					particle->velocity = vec3ShrR(vec3Rnd(),4);
					particle->color = vec3ShlR(color,4);
				}	
				audioPlay(spawn_position,AUDIO_PUNCH_HIT);
			}
			audioPlay(g_position,AUDIO_PUNCH);
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
						repeat(0x10){
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
	if(!voxel)
		return;
	VoxelStatic* voxel_s = g_voxel_static + voxel->type;
	if(g_voxel_placement){
		if(g_button_link){
			if(voxel->type == VOXEL_MOVABLE)
				g_button_link->linked = voxel;

			g_button_link = 0;
			return;
		}
		else if(voxel->type == VOXEL_BUTTON){
			g_button_link = voxel;
			return;
		}
	}
	
	if(!g_voxel_placement)
		return;

	if(voxel_s->flags & VOXEL_FLAG_NO_BLOCK_PLACE)
		return;
	
	Vec3 octree_position;
	if(!blockOutlinePositionGet(&octree_position))
		return;
	if(keyDown(KEY_LCONTROL)){
		static struct{
			Vec3 position;
			int depth;
			bool setted;
		} g_voxel_fill_begin;
		if(!g_voxel_fill_begin.setted || g_voxel_fill_begin.depth != g_edit_depth){
			g_voxel_fill_begin.position = octree_position;
			g_voxel_fill_begin.depth = g_edit_depth;
			g_voxel_fill_begin.setted = true;
		}
		else{
			Vec3 begin = g_voxel_fill_begin.position;
			VoxelType type = g_voxel_select;
			for(int x = tMin(begin.x,octree_position.x);x <= tMax(begin.x,octree_position.x);x++){
				for(int y = tMin(begin.y,octree_position.y);y <= tMax(begin.y,octree_position.y);y++){
					for(int z = tMin(begin.z,octree_position.z);z <= tMax(begin.z,octree_position.z);z++)
						voxelSet(&g_voxel,(Vec3){x,y,z},g_edit_depth,type);
				}
			}
			g_voxel_fill_begin.setted = false;
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
	if(!g_editor){
		if(g_equipped_staff)
			staffSkip();
		return;
	}
	int side;
	Vec3 direction = getLookDirection(g_angle);
	Voxel* voxel = treeRayTraceAndInit(g_position,direction,&side);
	if(!voxel)
		return;
	Vec3 block_pos_i = (Vec3){voxel->position_x,voxel->position_y,voxel->position_z};

	Voxel voxel_cpy = *voxel;

	voxelSet(&g_voxel,(Vec3){voxel->position_x,voxel->position_y,voxel->position_z},voxel->depth,VOXEL_AIR);

	if(voxel->depth >= g_edit_depth)
		return;
			
	int depht_difference = g_edit_depth - voxel_cpy.depth;
	Vec3 octree_position = vec3ShlR(block_pos_i,depht_difference);
	Vec2 axis = g_axis_table[side << 1];
	Vec2 uv = voxelLocalPositionGet(&voxel_cpy,g_position,direction,side);
	uv.x *= 1 << depht_difference;
	uv.y *= 1 << depht_difference;
	((int*)&octree_position)[side] += ((((int*)&direction)[side] < 0) << depht_difference) - (((int*)&direction)[side] < 0);
	((int*)&octree_position)[axis.x] += uv.x >> FIXED_PRECISION;
	((int*)&octree_position)[axis.y] += uv.y >> FIXED_PRECISION;
	Vec3 pos_i;
	int depht_difference_size = 1 << depht_difference;
	pos_i.x = (int)octree_position.x & depht_difference_size - 1;
	pos_i.y = (int)octree_position.y & depht_difference_size - 1;
	pos_i.z = (int)octree_position.z & depht_difference_size - 1;

	addSubVoxel(vec3ShlR(block_pos_i,1),pos_i,depht_difference,g_edit_depth,voxel_cpy.type);
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
	Vec3 direction = getLookDirection(g_angle);
	Voxel* voxel = treeRayTraceAndInit(g_position,direction,&side);
	if(!voxel)
		return;
	g_voxel_select = voxel->type;
}

void mouseMove(int delta_x,int delta_y){
	g_angle.x += delta_x << 3;
	g_angle.y += delta_y << 3;
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
	Vec3 point[] = {
		vec3AddR(position,(Vec3){0,0,0}),
		vec3AddR(position,(Vec3){0,0,size.z}),
		vec3AddR(position,(Vec3){0,size.y,0}),
		vec3AddR(position,(Vec3){0,size.y,size.z}),
		vec3AddR(position,(Vec3){size.x,0,0}),
		vec3AddR(position,(Vec3){size.x,0,size.z}),
		vec3AddR(position,(Vec3){size.x,size.y,0}),
		vec3AddR(position,(Vec3){size.x,size.y,size.z}),
	};
	Vec3 point_transformed[8];
	for(int i = 0;i < 8;i++)
		point_transformed[i] = pointToScreen(point[i]);
	int table[][4] = {
		{0,1,3,2},
		{4,5,7,6},
		{0,1,5,4},
		{2,3,7,6},
		{0,2,6,4},
		{1,3,7,5},
	};
	for(int i = 0;i < 6;i++){
		for(int j = 0;j < 4;j++){
			if(point_transformed[table[i][j]].z && point_transformed[table[i][(j + 1) % 4]].z)
				drawLine(g_surface,point_transformed[table[i][j]].x,point_transformed[table[i][j]].y,point_transformed[table[i][(j + 1) % 4]].x,point_transformed[table[i][(j + 1) % 4]].y,pixelColorToColor(color));
		}
	}
}

static void generateBlockOutline(Vec3 sub_block_pos,int sub_block_size){
	Vec3 look_direction = getLookDirection(g_angle);
	TraverseInit init = initTraverse(g_position);
	int side;
	Voxel* voxel = treeRayTrace(init.voxel,init.pos,look_direction,&side);

	g_voxel_pointed.voxel = voxel;
	g_voxel_pointed.side = side;

	if(!voxel)
		return;

	Vec2 uv = voxelLocalPositionGet(voxel,g_position,look_direction,side);

	g_voxel_pointed.uv = uv;

	if((g_voxel_static[g_voxel_pointed.voxel->type].flags & VOXEL_FLAG_NO_BLOCK_PLACE) || !g_voxel_placement)
		return;

	int edit_size = depthToSize(g_edit_depth);

	Vec3 block_pos_i = (Vec3){voxel->position_x,voxel->position_y,voxel->position_z};

	((int*)&block_pos_i)[side] += (((int*)&look_direction)[side] < 0.0f) * 2 - 1;
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
		((int*)&octree_position)[side] += ((((int*)&look_direction)[side] > 0) << (g_edit_depth - voxel->depth)) - (((int*)&look_direction)[side] > 0);
		((int*)&octree_position)[axis.x] += uv.x >> 16;
		((int*)&octree_position)[axis.y] += uv.y >> 16;
	}
	
	octree_position = (Vec3){octree_position.x * edit_size,octree_position.y * edit_size,octree_position.z * edit_size};
	fixedMul(&sub_block_size,edit_size);
	sub_block_pos = (Vec3){sub_block_pos.x * edit_size,sub_block_pos.y * edit_size,sub_block_pos.z * edit_size};

	boxQuadWireframeDraw(vec3AddR(octree_position,sub_block_pos),vec3Single(sub_block_size),0xFFFFFF);
}

#define GEN_SIZE (1 << 4)

void worldDefaultGenerate(void){
	voxelSet(&g_voxel,(Vec3){0,0,0},1,VOXEL_GRASS);
	voxelSet(&g_voxel,(Vec3){0,1,0},1,VOXEL_GRASS);
	voxelSet(&g_voxel,(Vec3){1,1,0},1,VOXEL_GRASS);
	voxelSet(&g_voxel,(Vec3){1,0,0},1,VOXEL_GRASS);
}

char g_voxel_lighting_tree[0x100000];

#include "win32/w_console.h"

void mainInit(void){
	if(g_editor){
		g_movement_fly = true;
		g_voxel_placement = true;
	}
	for(int i = 0;i < 6 * 5;i++){
		int offset_x = i / 5 * 0x2800;
		int offset_y = i % 5 * 0x2800;
		g_inventory_gui[i] = (VoxelGuiElement){
			.type = VOXEL_GUI_INVENTORY_SLOT,
			.position = (Vec2){offset_x + 0x800,0x800 + offset_y},
			.inventory_slot = g_inventory + i,
		};
	}
#ifndef __wasm__
	g_hand_model = win32LoadModel("model/hand.octvxl");
#endif
	entityInit();
	surfaceInit(&g_surface);
	texturesGenerate();
}

#define SKYBOX_POLYGON_SIZE 16

Plane g_view_plane[4];

static void setViewPlanes(void){
    Vec3 corner_angle[] = {
		screenRayDirection(g_tri,-FIXED_ONE,-FIXED_ONE,g_fov.x,g_fov.y),
		screenRayDirection(g_tri,FIXED_ONE,-FIXED_ONE,g_fov.x,g_fov.y),
		screenRayDirection(g_tri,FIXED_ONE,FIXED_ONE,g_fov.x,g_fov.y),
		screenRayDirection(g_tri,-FIXED_ONE,FIXED_ONE,g_fov.x,g_fov.y),
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

static unsigned stdcall rayTracePixels(void* args_void){
	RayTracePixelsArg* args = args_void;
	int tri[] = {tCos(args->trace_angle.x),tSin(args->trace_angle.x),tCos(args->trace_angle.y),tSin(args->trace_angle.y)};
	for(int i = args->offset;i < args->offset + args->amount;i++){
		int x = i / args->trace_size;
		int y = i % args->trace_size;

		int n_x = x * FIXED_ONE / (args->trace_size / 2) - FIXED_ONE;
		int n_y = y * FIXED_ONE / (args->trace_size / 2) - FIXED_ONE;

		n_x += (tRnd() % FIXED_ONE) / (args->trace_size / 2);
		n_y += (tRnd() % FIXED_ONE) / (args->trace_size / 2);

		Vec3 direction = screenRayDirection(tri,n_x,n_y,g_fov.x,g_fov.y);

		Vec3 color = vec3ShlR(rayLuminanceTrace(args->trace_position,vec3Normalize(direction)),4);

		args->buffer[i] = vec3Mix(args->buffer[i],color,FIXED_ONE / (args->trace_tick + 1));
	}
}

static void nextSpellDraw(void){
	drawString(g_surface,FIXED_ONE - FIXED_ONE / 8 - 0x1A00,FIXED_ONE - FIXED_ONE / 8 + 0x1000,"next spell",-1,0x800,pixelColorToColor(0xFFFFFF),0x800);

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

		int x = FIXED_ONE - FIXED_ONE / 8 - 0xA00;
		int y = FIXED_ONE - FIXED_ONE / 8 - offset * 0x1800;

		int color = 0x808080;
		if(g_spell_static[spell_slot->spell_type].adjective)
			color = 0xA02020;

		drawFrame(g_surface,x,y,fixedMulR(FIXED_ONE / 4,g_fov.x),fixedMulR(FIXED_ONE / 4,g_fov.y),pixelColorToColor(color),0x100);

		x += 0x1000;
		y += 0x0B00;

		switch(spell_slot->spell_type){
			case SPELL_BOLT:{
				drawEllipses(g_surface,x,y,fixedMulR(FIXED_ONE / 10,g_fov.x),fixedMulR(FIXED_ONE / 10,g_fov.y),pixelColorToColor(0xFF0000));
			} break;
			case SPELL_BOMB:{
				int size = 0x600;
				drawEllipses(g_surface,x,y,fixedMulR(size * 4,g_fov.x),fixedMulR(size * 4,g_fov.y),vec3Single(1 << 14));
				drawEllipses(g_surface,x - size / 2,y + size / 2,fixedMulR(size,g_fov.x),fixedMulR(size,g_fov.y),vec3Single(1 << 18));
				drawRectangle(g_surface,x - size * 2 - size / 4,y - size / 8 - size / 2,fixedMulR(size,g_fov.x),fixedMulR(size,g_fov.y) * 4,vec3Single(1 << 16));
				int fuse = 0x800;
				drawRectangle(g_surface,x - size * 2 - size / 4 - fuse / 2,y,fixedMulR(fuse,g_fov.x),fixedMulR(size,g_fov.y),pixelColorToColor(0x83B2EB));
				drawRectangle(g_surface,x - size * 2 - size / 4 - fuse / 2 - size / 2,y,fixedMulR(size,g_fov.x),fixedMulR(size,g_fov.y),pixelColorToColor(0x1050FF));
			} break;
			case SPELL_ORB:{
				drawEllipses(g_surface,x,y,fixedMulR(FIXED_ONE / 10,g_fov.x),fixedMulR(FIXED_ONE / 10,g_fov.y),pixelColorToColor(0x00FF00));
			} break;
			case SPELL_ADJ_SPEED:{
				Vec2 string_uv = vec2AddR((Vec2){x,y},(Vec2){-0x600,0x800});
				drawString(g_surface,string_uv.x,string_uv.y,">>",-1,0x800,pixelColorToColor(0xFFFFFF),0x800);
			} break;
			case SPELL_ADJ_DAMAGE:{
				Vec2 string_uv = vec2AddR((Vec2){x,y},(Vec2){-0x600,0x800});
				drawString(g_surface,string_uv.x,string_uv.y,"#+",-1,0x800,pixelColorToColor(0xFFFFFF),0x800);
			} break;
			case SPELL_ADJ_DOUBLER:{
				Vec2 string_uv = vec2AddR((Vec2){x,y},(Vec2){-0x600,0x800});
				drawString(g_surface,string_uv.x,string_uv.y,"x2",-1,0x800,pixelColorToColor(0xFFFFFF),0x800);
			} break;
			default:{
				drawEllipses(g_surface,x,y,fixedMulR(FIXED_ONE / 10,g_fov.x),fixedMulR(FIXED_ONE / 10,g_fov.y),pixelColorToColor(0xFF00FF));
			} break;
		}

		if(g_spell_static[spell_slot->spell_type].adjective)
			continue;

		break;
	}
}
#include "texture_markov.h"
void frameRender(void){
	g_tri[0] = tCos(g_angle.x);
	g_tri[1] = tSin(g_angle.x);
	g_tri[2] = tCos(g_angle.y);
	g_tri[3] = tSin(g_angle.y);

	setViewPlanes();

#ifndef _DEBUG_FAST
	lightingOctree();
#endif	
#ifndef __wasm__
	if(g_surface.backend == RENDER_BACKEND_GL)
		antiAliasingEnableGL(false);
#endif
	for(int i = 0;i < (SKYBOX_POLYGON_SIZE * SKYBOX_POLYGON_SIZE);i++){
		int size = FIXED_ONE / SKYBOX_POLYGON_SIZE;

		int x = i / SKYBOX_POLYGON_SIZE * FIXED_ONE / SKYBOX_POLYGON_SIZE;
		int y = i % SKYBOX_POLYGON_SIZE * FIXED_ONE / SKYBOX_POLYGON_SIZE;

		for(int k = 0;k < countof(g_axis_table);k++){
			Vec3 coordinates[] = {
				g_position,
				g_position,
				g_position,
				g_position,
			};

			((int*)(coordinates + 1))[g_axis_table[k].x] += size * 2;
			((int*)(coordinates + 2))[g_axis_table[k].x] += size * 2;
			((int*)(coordinates + 2))[g_axis_table[k].y] += size * 2;
			((int*)(coordinates + 3))[g_axis_table[k].y] += size * 2;

			Vec3 color[4];

			for(int j = 0;j < countof(coordinates);j++){
				((int*)(coordinates + j))[g_axis_table[k].x] += i / SKYBOX_POLYGON_SIZE * size * 2 - FIXED_ONE;
				((int*)(coordinates + j))[g_axis_table[k].y] += i % SKYBOX_POLYGON_SIZE * size * 2 - FIXED_ONE;
				((int*)(coordinates + j))[k >> 1] += k % 2 ? -FIXED_ONE : FIXED_ONE;
			}

			if(
				!pointInScreenSpace(coordinates[0]) && 
				!pointInScreenSpace(coordinates[1]) && 
				!pointInScreenSpace(coordinates[2]) && 
				!pointInScreenSpace(coordinates[3])
			)
				continue;

			for(int j = 0;j < countof(coordinates);j++){
				color[j] = vec3ShlR(skyboxSample(vec3Direction(g_position,coordinates[j])),4);
				vec3MulS(color + j,g_exposure);
				coordinates[j] = pointToScreenRenderer(coordinates[j],g_tri,g_position,g_fov);
			}

			Vec2 texture_coordinates[] = {
				{x,y},
				{x + size,y},
				{x + size,y + size},
				{x,y + size},
			};
			if(g_surface.backend == RENDER_BACKEND_SOFTWARE){
				spanQuadLightingTextureAdd(&g_surface,g_textures + g_skybox_textures[k],texture_coordinates,coordinates,color);
			}
			else{
				drawSkyboxPolygon3d(g_surface,g_textures + g_skybox_textures[k],texture_coordinates,coordinates,color);
			}
		}
	}
#ifndef __wasm__
	if(g_surface.backend == RENDER_BACKEND_GL)
		antiAliasingEnableGL(true);
#endif
#ifndef _DEBUG_FAST
	entityDynamicLighting();
#endif
	if(g_render_lines)
		surfaceClear(&g_surface);

	entityVoxelInsert();
	
	if(voxelPositionGet(g_position)->type == VOXEL_WATER){
		static int c[512 * 512 * 2];
		for(int i = 0;i < 512 * 512;i++){
			int x = i / 512;
			int y = i % 512;

			int n_x = x * FIXED_ONE / 256 - FIXED_ONE;
			int n_y = y * FIXED_ONE / 256 - FIXED_ONE;

			int offset_x = tCos((g_tick << 8) + n_x) >> 6;
			int offset_y = tCos((g_tick << 8) + n_x) >> 6;
			Vec3 direction = screenRayDirection(g_tri,n_x + offset_x,n_y + offset_y,g_fov.x,g_fov.y);

			Vec3 color = vec3ShlR(rayLuminance(g_position,vec3Normalize(direction)),4);

			c[i] = colorToPixelColor(color);
			//drawRectangle(g_surface,n_x,n_y,FIXED_ONE / 64,FIXED_ONE / 64,color);
		}
		generateMipmaps(&(Texture){.pixel_data = c,.size = 512});
		markovTextureRemix((Texture){.pixel_data = c,.size = 512},2);
		for(int i = 0;i < 512 * 512;i++){
			int x = i / 512;
			int y = i % 512;
			int n_x = x * FIXED_ONE / 256 - FIXED_ONE;
			int n_y = y * FIXED_ONE / 256 - FIXED_ONE;
			drawRectangle(g_surface,n_x,n_y,FIXED_ONE / 256,FIXED_ONE / 256,pixelColorToColor(c[i]));
		}
	}
	else{
		octreeDraw(&g_voxel);
		spanDrawList();
		/*
#define SIZE 512
		static int c[SIZE * SIZE * 2];
		openglDownloadFramebuffer((Texture){.pixel_data = c,.size = SIZE});
		Vec2 crd[] = {{-FIXED_ONE,-FIXED_ONE},{-FIXED_ONE,FIXED_ONE},{FIXED_ONE,FIXED_ONE},{FIXED_ONE,-FIXED_ONE}};
		for(int i = 0;i < SIZE * SIZE;i++)
			c[i] &= ~0xFF000000;
		generateMipmaps(&(Texture){.pixel_data = c,.size = SIZE});
		markovTextureRemix((Texture){.pixel_data = c,.size = SIZE},2);
		drawTexturePolygon(g_surface,&(Texture){.pixel_data = c,.size = SIZE},g_texture_coordinates_fill,crd,vec3Single(FIXED_ONE << 4),4);
		*/
	}

	//raytracing
	/*
#define TRACE_SIZE 512
	static Vec3 buffer[TRACE_SIZE * TRACE_SIZE];
	static int n_trace_tick = 0;
	static Vec2 trace_angle;
	static Vec3 trace_position;

	void* threads[16];
	RayTracePixelsArg thread_arguments[countof(threads)];
	int n_thread = tClamp(g_n_threads - 1,1,countof(threads));

	for(int i = 0;i < n_thread;i++){
		thread_arguments[i] = (RayTracePixelsArg){
			.buffer = buffer,
			.amount = TRACE_SIZE * TRACE_SIZE / n_thread,
			.offset = TRACE_SIZE * TRACE_SIZE / n_thread * i,
			.trace_size = TRACE_SIZE,
			.trace_tick = n_trace_tick,
			.trace_angle = trace_angle,
			.trace_position = trace_position,
		};
		threads[i] = CreateThread(0,0,rayTracePixels,thread_arguments + i,0,0);
	}

	WaitForMultipleObjects(n_thread,threads,true,INFINITE);

	for(int i = 0;i < n_thread;i++)
		CloseHandle(threads[i]);
	static Texture render_texture;
	textureDestroy(render_texture);
	render_texture = textureCreate(TRACE_SIZE);

	for(int i = 0;i < TRACE_SIZE * TRACE_SIZE;i++)
		render_texture.pixel_data[i] = colorToPixelColor(buffer[i]);

	Vec2 screen_coordinates[] = {
		{FIXED_ONE,FIXED_ONE},
		{FIXED_ONE,-FIXED_ONE},
		{-FIXED_ONE,-FIXED_ONE},
		{-FIXED_ONE,FIXED_ONE},
	};

	drawTexturePolygon(g_surface,&render_texture,g_texture_coordinates_fill,screen_coordinates,vec3Single(FIXED_ONE << 4),4);
	
	n_trace_tick += 1;
	if(keyDown('W') || keyDown('D') || keyDown('A') || keyDown('S')){
		n_trace_tick = 0;
		trace_angle = g_angle;
		trace_position = g_position;
	}
	*/
	voxelEntityRemove();

	int brightness_acc = 0;

	TraverseInit init = initTraverse(g_position);

	int exposure_samle_row_size = 32;

	for(int i = 0;i < exposure_samle_row_size * exposure_samle_row_size;i++){
		int x = i / exposure_samle_row_size;
		int y = i % exposure_samle_row_size;

		int n_x = x * FIXED_ONE / (exposure_samle_row_size / 2) - FIXED_ONE;
		int n_y = y * FIXED_ONE / (exposure_samle_row_size / 2) - FIXED_ONE;

		Vec3 luminance = rayLuminanceInit(init,g_position,screenRayDirection(g_tri,n_x,n_y,g_fov.x,g_fov.y));

		brightness_acc += tMax(luminance.x,tMax(luminance.y,luminance.z)) >> 4;
	}

	if(brightness_acc){
		g_exposure = tMix(g_exposure,fixedDivR(FIXED_ONE,brightness_acc >> 4),FIXED_ONE / 0x100);
		g_exposure = tMix(g_exposure,FIXED_ONE,FIXED_ONE / 0x40);
	}

	/*
	for(int i = 0;i < 256 * 256;i++){
		int x = i / 256;
		int y = i % 256;

		int n_x = x * FIXED_ONE / 128 - FIXED_ONE;
		int n_y = y * FIXED_ONE / 128 - FIXED_ONE;

		int voxel_intersect = treeRayTraceIntersectCountAndInit(g_position,screenRayDirection(g_tri,n_x,n_y,g_fov.x,g_fov.y));

		Vec3 color = {0,0,voxel_intersect << 14};

		drawRectangle(g_surface,n_x / 2 - FIXED_ONE + FIXED_ONE / 2,n_y / 2 - FIXED_ONE + FIXED_ONE / 2,FIXED_ONE / 64,FIXED_ONE / 64,color);
	}
	*/

	if(g_luminance_overlay){
		for(int i = 0;i < 64 * 64;i++){
			int x = i / 64;
			int y = i % 64;

			int n_x = x * FIXED_ONE / 32 - FIXED_ONE;
			int n_y = y * FIXED_ONE / 32 - FIXED_ONE;

			Vec3 color = vec3ShlR(rayLuminance(g_position,screenRayDirection(g_tri,n_x,n_y,g_fov.x,g_fov.y)),4);

			drawRectangle(g_surface,n_x / 4 - FIXED_ONE + FIXED_ONE / 4,n_y / 4 - FIXED_ONE + FIXED_ONE / 4,FIXED_ONE / 128,FIXED_ONE / 128,color);
		}
	}

	if(g_editor){
		for(Voxel* voxel = g_voxel_link_list;voxel;voxel = voxel->next_voxel_link){
			if(!voxel->linked)
				continue;
			Vec3 p1 = pointToScreen(vec3AddRS(voxelWorldPos(voxel),depthToSize(voxel->depth) / 2));
			Vec3 p2 = pointToScreen(vec3AddRS(voxelWorldPos(voxel->linked),depthToSize(voxel->linked->depth) / 2));
			if(p1.z <= 0 || p2.z <= 0)
				continue;
			drawLine(g_surface,p1.x,p1.y,p2.x,p2.y,pixelColorToColor(0xFF00FF));
		}
		entityDrawHitbox();
	}

	generateBlockOutline(vec3Single(0),FIXED_ONE);

	genBlockSelect();

	if(g_equipped_staff){
		int progress = (fixedDivR(g_mana,g_equipped.mana_max)) / 4;
		
		drawRectangle(g_surface,-FIXED_ONE + FIXED_ONE / 8,-(progress) - FIXED_ONE / 2,FIXED_ONE / 32,progress,pixelColorToColor(0xA060FF));
		drawFrame(g_surface,-FIXED_ONE + FIXED_ONE / 8, - FIXED_ONE / 2 - FIXED_ONE / 4,FIXED_ONE / 32,FIXED_ONE / 4,pixelColorToColor(0x808080),0x100);

		drawString(g_surface,-FIXED_ONE + FIXED_ONE / 8,-0x8000,"mana",-1,0x800,pixelColorToColor(0xFFFFFF),0xC0);
	}

	int progress = (fixedDivR(g_health,FIXED_ONE)) / 4;
		
	drawRectangle(g_surface,-FIXED_ONE + FIXED_ONE / 8 - 0xA00,-(progress) - FIXED_ONE / 2,FIXED_ONE / 32,progress,pixelColorToColor(0x3030FF));
	drawFrame(g_surface,-FIXED_ONE + FIXED_ONE / 8 - 0xA00, - FIXED_ONE / 2 - FIXED_ONE / 4,FIXED_ONE / 32,FIXED_ONE / 4,pixelColorToColor(0x808080),0x100);

	drawString(g_surface,-FIXED_ONE + FIXED_ONE / 8 - 0xA00,-0x8000,"health",-1,0x800,pixelColorToColor(0x000000),0xC0);

	if(g_button_link){
		Vec3 p1 = (Vec3){0};
		Vec3 p2 = pointToScreen(vec3AddRS(voxelWorldPos(g_button_link),depthToSize(g_button_link->depth) / 2));
		if(p2.z > 0)
			drawLine(g_surface,p1.x,p1.y,p2.x,p2.y,pixelColorToColor(0xFF00FF));
	}

	if(g_spell_hold){
		switch(g_spell_hold){
			case SPELL_BOLT:{
				drawCircle(g_surface,0,0,0x800,pixelColorToColor(0xFF0000));
			} break;
			case SPELL_BOMB:{
				drawCircle(g_surface,0,0,0x800,pixelColorToColor(0x0000FF));
			} break;
			case SPELL_ORB:{
				drawCircle(g_surface,0,0,0x800,pixelColorToColor(0x00FF00));
			} break;
		}
	}

	//draw next spell in line
	if(g_equipped.capacity)
		nextSpellDraw();

	if(g_boss){
		int progress = (fixedDivR(g_boss->health,g_entity_static[ENTITY_BOSS].health)) / 2;
		drawRectangle(g_surface,-FIXED_ONE + FIXED_ONE / 8 - 0xA00,-(progress) + FIXED_ONE / 4,FIXED_ONE / 16,progress,pixelColorToColor(0x3030FF));
		drawFrame(g_surface,-FIXED_ONE + FIXED_ONE / 8 - 0xA00,-FIXED_ONE / 4,FIXED_ONE / 16,FIXED_ONE / 2,pixelColorToColor(0x808080),0x100);

		drawString(g_surface,-FIXED_ONE + FIXED_ONE / 8 - 0xA00,FIXED_ONE / 8,"boss",-1,0x1000,pixelColorToColor(0xFFFFFF),0xC0);
	}

	drawLine(g_surface,-0x200,0,0x200,0,pixelColorToColor(0x00FF00));
	drawLine(g_surface,0,-0x200,0,0x200,pixelColorToColor(0x00FF00));
}


