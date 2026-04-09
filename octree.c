#include "octree.h"
#include "dda.h"
#include "memory.h"
#include "texture.h"
#include "main.h"
#include "octree_render.h"
#include "voxel_menu.h"
#include "geometry.h"
#include "voxel_gui.h"
#include "libc.h"
#include "opengl.h"

Voxel g_voxel;
Voxel* g_voxel_tick_list;

static void toggleVsync(VoxelGuiElement* self){
	g_vsync ^= true;
	vsyncSet(false);
}

static void voxelButtonExecute(VoxelGuiElement* self){
	if(self->button.voxel->linked){
		Voxel* door = self->button.voxel->linked;
		door->opened ^= true;
		if(!door->animation){
			door->animation = FIXED_ONE;
			voxelTickListAdd(door);
		}
		else{
			door->animation = FIXED_ONE - door->animation;
		}
	}
}

static VoxelGuiElement voxel_menu_gui[] = {
	{.type = VOXEL_GUI_BUTTON,.button.on_click = toggleSmoothLighting,.position = {0x1000,FIXED_ONE - 0x2000}},
	{.type = VOXEL_GUI_STRING,.position = {0x2800,FIXED_ONE - 0x1400},.string.string = STRING_LITERAL("toggle smooth lighting")},
	{.type = VOXEL_GUI_BUTTON,.button.on_click = changeRenderBackend,.position = {0x1000,FIXED_ONE - 0x4000}},
	{.type = VOXEL_GUI_STRING,.position = {0x2800,FIXED_ONE - 0x3400},.string.string = STRING_LITERAL("render backend")},
	{.type = VOXEL_GUI_BUTTON,.button.on_click = antiAliasingLoadMenu,.position = {0x1000,FIXED_ONE - 0x6000}},
	{.type = VOXEL_GUI_STRING,.position = {0x2800,FIXED_ONE - 0x5400},.string.string = STRING_LITERAL("anti aliasing")},
	{.type = VOXEL_GUI_BUTTON,.button.on_click = toggleVsync,.position = {0x1000,FIXED_ONE - 0x8000}},
	{.type = VOXEL_GUI_STRING,.position = {0x2800,FIXED_ONE - 0x7400},.string.string = STRING_LITERAL("toggle vsync")},
	{.type = VOXEL_GUI_BUTTON,.button.on_click = debugOptions,.position = {0x1000,FIXED_ONE - 0xA000}},
	{.type = VOXEL_GUI_STRING,.position = {0x2800,FIXED_ONE - 0x9400},.string.string = STRING_LITERAL("debug options")},
};

static VoxelGuiElement voxel_button_gui[] = {
	{.type = VOXEL_GUI_BUTTON,.button.on_click = voxelButtonExecute,.position = {FIXED_ONE / 4,FIXED_ONE / 4},.button.size = FIXED_ONE / 2},
};

static VoxelGuiElement staff_inspector_gui[] = {
	{.type = VOXEL_GUI_STRING,.position = {0x1000,FIXED_ONE - 0x1C00},.string.size = 0x1000,.string.string = STRING_LITERAL("staff editor")},
};

static VoxelGuiElement voxel_string_gui[] = {
    {
        .type = VOXEL_GUI_RECTANGLE,
        .rectangle.size = {FIXED_ONE,0x2000},
        .rectangle.flags = {RECTANGLE_RELATIVE_SIZE_X},
        .rectangle.color = 0xF02020
    },
    {
        .type = VOXEL_GUI_RECTANGLE,
        .rectangle.size = {0x2000,FIXED_ONE},
        .rectangle.flags = {RECTANGLE_RELATIVE_SIZE_Y},
        .rectangle.color = 0xF02020
    },
    {
        .type = VOXEL_GUI_RECTANGLE,
        .position = {0,FIXED_ONE - 0x800},
        .rectangle.size = {FIXED_ONE,0x2000},
        .rectangle.flags = {RECTANGLE_RELATIVE_SIZE_X},
        .rectangle.color = 0xF02020
    },
    {
        .type = VOXEL_GUI_RECTANGLE,
        .position = {FIXED_ONE - 0x800},
        .rectangle.size = {0x2000,FIXED_ONE},
        .rectangle.flags = {RECTANGLE_RELATIVE_SIZE_Y},
        .rectangle.color = 0xF02020
    },
};

VoxelGuiElement g_inventory_gui[31] = {
	[30] = {.type = VOXEL_GUI_STRING,.position = {0x1000,FIXED_ONE - 0x1C00},.string.size = 0x1000,.string.string = STRING_LITERAL("inventory")},
};

void voxelMenuStaffEditorDefault(void){
	tFree(g_voxel_static[VOXEL_STAFF_INSPECTOR].side[VEC3_X * 2].gui);
	g_voxel_static[VOXEL_STAFF_INSPECTOR].side[VEC3_X * 2].gui = staff_inspector_gui;
	g_voxel_static[VOXEL_STAFF_INSPECTOR].side[VEC3_X * 2].n_gui = countof(staff_inspector_gui);
}

void voxelMenuMainSet(void){
	g_voxel_static[VOXEL_MENU].gui   = voxel_menu_gui;
	g_voxel_static[VOXEL_MENU].n_gui = countof(voxel_menu_gui); 
}

#define GUI_ADD(GUI) .gui = GUI,.n_gui = countof(GUI)

VoxelStatic g_voxel_static[VOXEL_ECOUNT] = {
	[VOXEL_BLOCK] = {
		.color = {0xF000,0xF000,0xF000},
	},
	[VOXEL_BLOCK_RED] = {
		.color = {0xF000,0x0F00,0x0F00},
	},
	[VOXEL_BLOCK_GREEN] = {
		.color = {0x0F00,0xF000,0x0F00},
	},
	[VOXEL_BLOCK_BLUE] = {
		.color = {0x0F00,0x0F00,0xF000},
	},
	[VOXEL_BLOCK_ORANGE] = {
		.color = {0x3F00,0x6F00,0xAF00},
	},
	[VOXEL_LIGHT] = {
		.color = {0x0F0000,0x0F0000,0x0F0000},
		.flags = VOXEL_EMITER,
	},
	[VOXEL_HDR_TEST] = {
		.color = {0x3F0000,0x3F0000,0x3F0000},
		.flags = VOXEL_EMITER,
	},
	[VOXEL_LIGHT_RED] = {
		.color = {0x3F0000,0xF000,0xF000},
		.flags = VOXEL_EMITER,
	},
	[VOXEL_LIGHT_GREEN] = {
		.color = {0xF000,0x3F0000,0xF000},
		.flags = VOXEL_EMITER,
	},
	[VOXEL_LIGHT_BLUE] = {
		.color = {0xF000,0xF000,0x3F0000},
		.flags = VOXEL_EMITER,
	},
	[VOXEL_LIGHT_YELLOW] = {
		.color = {0x1F000,0x380000,0x3F0000},
		.flags = VOXEL_EMITER,
	},
	[VOXEL_WALL] = {
		.color = {0xF000,0xF000,0xF000},
		.texture = g_textures + TEXTURE_WALL,
		.texture_size = FIXED_ONE * 4,
	},
	[VOXEL_GRASS] = {
		.texture = g_textures + TEXTURE_GRASS,
		.texture_size = FIXED_ONE,
	},
	[VOXEL_STONE] = {
		.texture = g_textures + TEXTURE_STONE,
		.texture_size = FIXED_ONE,
	},
	[VOXEL_PLANKS] = {
		.texture = g_textures + TEXTURE_PLANKS,
		.texture_size = FIXED_ONE * 4,
	},
	[VOXEL_MIRROR] = {
		.color = {0xF000,0xF000,0xF000},
	},
	[VOXEL_METALLIC] = {
		.color = {0xF000,0xF000,0xF000},
	},
	[VOXEL_MENU] = {
		.color = {0x3000,0x3000,0x3000},
        GUI_ADD(voxel_menu_gui),
		.flags = VOXEL_NO_BLOCKPLACE,
	},
	[VOXEL_STONE_BRICK] = {
		.texture = g_textures + TEXTURE_STONE_BRICK,
		.texture_size = FIXED_ONE,
	},
	[VOXEL_STONE2] = {
		.texture = g_textures + TEXTURE_STONE2,
		.texture_size = FIXED_ONE,
	},
	[VOXEL_UNDESTRUCTIBLE] = {
		.texture = g_textures + TEXTURE_UNDESTRUCTIBLE,
		.texture_size = FIXED_ONE,
		.flags = VOXEL_NO_BLOCKPLACE,
	},
	[VOXEL_CHEST] = {
		.texture = g_textures + TEXTURE_CHEST,
		.flags = VOXEL_TEXTUREFILL,
		.side[VEC3_Z * 2 + 1] = {
			.custom = true,
			.color = {FIXED_ONE,FIXED_ONE,FIXED_ONE},
		},
		.side[VEC3_Z * 2] = {
			.custom = true,
			.color = {FIXED_ONE,FIXED_ONE,FIXED_ONE},
		},
	},
	[VOXEL_MOVABLE] = {
		.texture = g_textures + TEXTURE_UNDESTRUCTIBLE,
		.flags = VOXEL_TEXTUREFILL,
	},
	[VOXEL_BOSS] = {
		.texture = g_textures + TEXTURE_UNDESTRUCTIBLE,
		.flags = VOXEL_TEXTUREFILL,
	},
	[VOXEL_BUTTON] = {
		.color = {0xF000,0xF000,0xF000},
        GUI_ADD(voxel_button_gui)
	},
	[VOXEL_STAFF_INSPECTOR] = {
		.color = {0x0F00,0x0F00,0x2000},
		.flags = VOXEL_EMITER,
		.side[VEC3_X * 2] = {
			.custom = true,
			.color = {0x0F00,0x0F00,0x2000},
            GUI_ADD(staff_inspector_gui)
		},
	},
	[VOXEL_INVENTORY] = {
		.color = {0x2000,0x2000,0x0F00},
		.flags = VOXEL_EMITER,
        GUI_ADD(g_inventory_gui),
	},
	[VOXEL_LADDER] = {
		.texture = g_textures + TEXTURE_PLANKS,
		.texture_size = FIXED_ONE * 4,
	},
    [VOXEL_WATER] = {
        .flags = VOXEL_TRANSLUCENT
    },
    [VOXEL_STRING] = {
        .color = {0x1F00,0x1F00,0x1F00},
        GUI_ADD(voxel_string_gui),
    },
};

static void octreeIndicesSet(VoxelSerialized* voxel_serial_array,int* voxel_serial_index,Voxel* voxel){
	VoxelSerialized* voxel_serial = (VoxelSerialized*)((char*)voxel_serial_array + *voxel_serial_index);
	switch(voxel->type){
		case VOXEL_PARENT:{
			VoxelSerializedParent* voxel_serial_parent = (VoxelSerializedParent*)voxel_serial;
			for(int i = 0;i < 8;i++){
				if(voxel->child_s[i])
					voxel->child_s[i]->index = ((*voxel_serial_index)++) + 1;
				octreeIndicesSet(voxel_serial_array,voxel_serial_index,voxel->child_s[i]);
			}
		} break;
	}
}

static void octreeSerializeRecursive(VoxelSerialized* voxel_serial_array,int* voxel_serial_index,Voxel* voxel){
	if(!voxel)
		return;

	VoxelSerialized* voxel_serial = (void*)((char*)voxel_serial_array + *voxel_serial_index);
	voxel_serial->type = voxel->type;
	
	switch(voxel->type){
		case VOXEL_PARENT:{
			*voxel_serial_index += sizeof(VoxelSerializedParent);
			VoxelSerializedParent* voxel_serial_parent = (void*)voxel_serial;
			for(int i = 0;i < 8;i++){
				if(voxel->child_s[i])
					voxel_serial_parent->child_s[i] = *voxel_serial_index;
				else
					voxel_serial_parent->child_s[i] = -1;
				octreeSerializeRecursive(voxel_serial_array,voxel_serial_index,voxel->child_s[i]);
			}
		} break;
		case VOXEL_BUTTON:{
			VoxelSerializedButton* voxel_serial = (void*)((char*)voxel_serial_array + *voxel_serial_index);
			voxel_serial->linked = voxel->linked ? voxel->linked->index : 0;
			*voxel_serial_index += sizeof(VoxelSerializedButton);
		} break;
        case VOXEL_STRING:{
            VoxelSerializedString* voxel_serial = (void*)((char*)voxel_serial_array + *voxel_serial_index);
            voxel_serial->string_length = voxel->string.size;
            tMemcpy(voxel_serial->string_data,voxel->string.data,voxel->string.size);
            *voxel_serial_index += sizeof(VoxelSerializedString) + voxel->string.size;
        } break;
		default:{
			*voxel_serial_index += sizeof(VoxelSerialized);
		} break;
	}
}

void octreeSerialize(VoxelSerialized* voxel_serial_array,Voxel* voxel){
	int voxel_serial_index = 0;
	octreeIndicesSet(voxel_serial_array,&voxel_serial_index,voxel);
	voxel_serial_index = 0;
	octreeSerializeRecursive(voxel_serial_array,&voxel_serial_index,voxel);
}

Voxel* octreeDeserializeRecursive(VoxelSerializedParent* voxel_serial_array,int index,Voxel* parent,int depth,Vec3 position,Voxel** voxel_array,int* index_i){
	VoxelSerialized* voxel_serial = (void*)((char*)voxel_serial_array + index);
	Voxel* voxel    = tMallocZero(sizeof(Voxel));
	voxel->position_x = position.x;
	voxel->position_y = position.y;
	voxel->position_z = position.z;
	voxel->type     = voxel_serial->type;
	voxel->depth    = depth;
	voxel->parent   = parent;

	if(voxel_array)
		voxel_array[(*index_i)++] = voxel;

    if(voxel->type == VOXEL_STRING){
        VoxelSerializedString* voxel_serial_string = (void*)voxel_serial;
        voxel->string.data = tMalloc(voxel_serial_string->string_length);
        tMemcpy(voxel->string.data,voxel_serial_string->string_data,voxel_serial_string->string_length);
        voxel->string.size = voxel_serial_string->string_length;
    }
    
	if(voxel->type != VOXEL_PARENT)
		return voxel;

	VoxelSerializedParent* voxel_serial_parent = (void*)voxel_serial;

	for(int i = 0;i < 8;i++){
		Vec3 child_position = {
			position.x << 1 | (i >> 0 & 1),
			position.y << 1 | (i >> 1 & 1),
			position.z << 1 | (i >> 2 & 1)
		};
		voxel->child_s[i] = octreeDeserializeRecursive(voxel_serial_array,voxel_serial_parent->child_s[i],voxel,depth + 1,child_position,voxel_array,index_i);
	}	
	return voxel;
}

void octreeDeserializeLink(VoxelSerializedParent* voxel_serial_array,int index,int depth,Vec3 position,Voxel** voxel_array,int* index_i){
	VoxelSerialized* voxel_serial = (void*)((char*)voxel_serial_array + index);

	if(voxel_serial->type == VOXEL_BUTTON){
		VoxelSerializedButton* voxel_serial_button = (void*)voxel_serial;
		Voxel* button = voxel_array[*index_i];
		button->linked = voxel_array[voxel_serial_button->linked];
		button->next_voxel_link = g_voxel_link_list;
		g_voxel_link_list = button;
	}

	*index_i += 1;

	if(voxel_serial->type != VOXEL_PARENT)
		return;

	VoxelSerializedParent* voxel_serial_parent = (void*)voxel_serial;

	for(int i = 0;i < 8;i++){
		Vec3 child_position = {
			position.x << 1 | (i >> 0 & 1),
			position.y << 1 | (i >> 1 & 1),
			position.z << 1 | (i >> 2 & 1)
		};
		if(voxel_serial_parent->child_s[i] != -1)
			octreeDeserializeLink(voxel_serial_array,voxel_serial_parent->child_s[i],depth + 1,child_position,voxel_array,index_i);
	}
}

int voxelChildCountRecursive(Voxel* voxel){
	if(!voxel)
		return 0;
	if(voxel->type != VOXEL_PARENT)
		return 1;
	int count = 0;
	for(int i = 0;i < 8;i++)
		count += voxelChildCountRecursive(voxel->child_s[i]);
	return count + 1;
}

int voxelMemoryCountRecursive(Voxel* voxel){
	if(!voxel)
		return 0;
	switch(voxel->type){
		case VOXEL_PARENT:{
			if(voxel->type != VOXEL_PARENT)
				return sizeof(VoxelSerialized);
			int count = 0;
			for(int i = 0;i < 8;i++)
				count += voxelMemoryCountRecursive(voxel->child_s[i]);
			return count + sizeof(VoxelSerializedParent);
		}
        case VOXEL_STRING:{
            return sizeof(VoxelSerializedString) + voxel->string.size;
        } 
		case VOXEL_BUTTON:{
			return sizeof(VoxelSerializedButton);
		}
		default:{
			return sizeof(VoxelSerialized);
		}
	}
}

Voxel* voxelGet(Vec3 position,int depth){
	Voxel* voxel = &g_voxel;
	for(int i = depth - 1;i >= 0;i--){
		Vec3 sub_coord = vec3ShrR(position,i);
		sub_coord = (Vec3){sub_coord.x & 1,sub_coord.y & 1,sub_coord.z & 1};
		voxel = voxel->child[sub_coord.z][sub_coord.y][sub_coord.x];
		if(voxel->type != VOXEL_PARENT)
			return voxel;
	}
	return voxel;
}

static int g_border_block_table[][4] = {
	{1,3,5,7},
	{0,2,4,6},
	{2,3,6,7},
	{0,1,4,5},
	{4,5,6,7},
	{0,1,2,3},
};

static bool nonFullVoxel(Voxel* voxel){
	return voxel->type == VOXEL_AIR || voxel->type == VOXEL_GLASS || voxel->type == VOXEL_WATER || voxel->animation || voxel->opened;
}

static bool sideVisible(Voxel* voxel,int* adjacent){
	if(voxel->type == VOXEL_PARENT){
		for(int i = 0;i < 4;i++){
			if(sideVisible(voxel->child_s[adjacent[i]],adjacent))
				return true;
		}
		return false;
	}
	return nonFullVoxel(voxel);
}

bool squareVisible(Vec3 position,int depth,int side,VoxelType voxel_type){
	Vec2 axis = g_axis_table[side];
	Vec3 neighbour_position = position;
	((int*)&neighbour_position)[side >> 1] += (side & 1) * 2 - 1;
	Voxel* neighbour = voxelGet(neighbour_position,depth);
	if(neighbour->type == VOXEL_PARENT){
		for(int i = 0;i < 4;i++){
			if(sideVisible(neighbour->child_s[g_border_block_table[side][i]],g_border_block_table[side]))
				return true;
		}
		return false;
	}
	if(neighbour->type == voxel_type)
		return false;
	return nonFullVoxel(neighbour);
}

Vec3 rayHitPosition(Voxel* voxel,Vec3 ray_position,Vec3 ray_direction,int side){
	Plane plane = getPlane(voxel,ray_direction,side);
	int distance = rayPlaneIntersection(ray_position,ray_direction,plane) - (((int*)&ray_direction)[side] < 0 ? 0x10 : -0x10);
	return vec3AddR(ray_position,vec3MulRS(ray_direction,distance));
}

int depthToSize(int depth){
	return (FIXED_ONE << 8) * 2 >> depth;
}

Vec3 posWorldPos(Vec3 position,int depth){
	int size = depthToSize(depth);
	return (Vec3){position.x * size,position.y * size,position.z * size};
}

Vec3 voxelWorldPos(Voxel* voxel){
	int size = depthToSize(voxel->depth);
	return (Vec3){voxel->position_x * size,voxel->position_y * size,voxel->position_z * size};
}

Voxel* voxelPositionGet(Vec3 pos){
	pos.x >>= 8;
	pos.y >>= 8;
	pos.z >>= 8;
	Voxel* voxel = &g_voxel;
	for(;;){
		Voxel* child_ptr = voxel->child[(int)pos.z >> FIXED_PRECISION & 1][(int)pos.y >> FIXED_PRECISION & 1][(int)pos.x >> FIXED_PRECISION & 1];
		if(child_ptr->type != VOXEL_PARENT){
			voxel = child_ptr;
			break;
		}
		voxel = child_ptr;
		vec3Shl(&pos,1);
	}
	return voxel;
}

TraverseInit initTraverse(Vec3 pos){
	pos.x >>= 8;
	pos.y >>= 8;
	pos.z >>= 8;
	Voxel* voxel = &g_voxel;
	for(;;){
		Voxel* child_ptr = voxel->child[(int)pos.z >> FIXED_PRECISION & 1][(int)pos.y >> FIXED_PRECISION & 1][(int)pos.x >> FIXED_PRECISION & 1];
		if(child_ptr->type != VOXEL_PARENT)
			break;
		voxel = child_ptr;
		vec3Shl(&pos,1);
	}
	pos.x = fixedFract(pos.x / 2) * 2;
	pos.y = fixedFract(pos.y / 2) * 2;
	pos.z = fixedFract(pos.z / 2) * 2;
	return (TraverseInit){pos,voxel};
}

Entity* entityRayCollisionRecursive(Voxel* voxel,Vec3 position,Vec3 direction){
	int block_size = depthToSize(voxel->depth);
	Vec3 block_pos = voxelWorldPos(voxel);
	if(voxel->type == VOXEL_PARENT){
		Entity* entity_closest = 0;
		for(int i = 0;i < 8;i++){
			Entity* entity = entityRayCollisionRecursive(voxel->child_s[i],position,direction);
			if(
				entity && 
				(entity->flags & ENTITY_FLAG_HITABLE) && 
				(!entity_closest || vec3Distance(entity->position,position) < vec3Distance(entity_closest->position,position))
			)
				entity_closest = entity;
		}
		return entity_closest;
	}
	if(voxel->type != VOXEL_AIR)
		return 0;
	return entityRayCollision(voxel->entity_list,position,direction);
}

Voxel* treeRayTrace(Voxel* voxel,Vec3 position,Vec3 direction,int* side){
	Ray3 ray = initRay3(position,direction);
	iterateRay3(&ray);
	for(;;){
		unsigned combine = ray.square_pos.x | ray.square_pos.y | ray.square_pos.z;
		if(combine > 1){
			if(!voxel->depth)
				return 0;

 			ray.pos.x >>= 1;
			ray.pos.y >>= 1;
			ray.pos.z >>= 1;

			ray.pos.x += (voxel->position_x & 1) * FIXED_ONE;
			ray.pos.y += (voxel->position_y & 1) * FIXED_ONE;
			ray.pos.z += (voxel->position_z & 1) * FIXED_ONE;

			recalculateRay3(&ray);
			voxel = voxel->parent;
			iterateRay3(&ray);
			continue;
		}
	sh:;
		int index = ray.square_pos.z * 4 + ray.square_pos.y * 2 + ray.square_pos.x;
		Voxel* child = voxel->child_s[index];
		if(child->type == VOXEL_AIR){
			iterateRay3(&ray);
			continue;
		}
		if(child->type == VOXEL_PARENT){
			int x = (int[]){VEC3_Y,VEC3_X,VEC3_X}[ray.square_side];
			int y = (int[]){VEC3_Z,VEC3_Z,VEC3_Y}[ray.square_side];

			int side_delta = ((int*)&ray.side)[ray.square_side] - ((int*)&ray.delta)[ray.square_side];

			((int*)&ray.pos)[x] = fixedFract(((int*)&ray.pos)[x] + fixedMulR(side_delta,((int*)&ray.dir)[x])) << 1;
			((int*)&ray.pos)[y] = fixedFract(((int*)&ray.pos)[y] + fixedMulR(side_delta,((int*)&ray.dir)[y])) << 1;
			((int*)&ray.pos)[ray.square_side] = ((int*)&ray.dir)[ray.square_side] < 0 ? FIXED_ONE * 2 - 1 : 0;

			voxel = child;
			recalculateRay3(&ray);
			goto sh;
		}
		if(side) 
			*side = ray.square_side;
		return child;
	}
}

static int treeRayTraceIntersectCount(Voxel* voxel,Vec3 position,Vec3 direction){
	Ray3 ray = initRay3(position,direction);
	iterateRay3(&ray);
	int count = 0;
	for(;;){
		unsigned combine = ray.square_pos.x | ray.square_pos.y | ray.square_pos.z;
		if(combine > 1){
			if(!voxel->depth)
				return count;

 			ray.pos.x >>= 1;
			ray.pos.y >>= 1;
			ray.pos.z >>= 1;

			ray.pos.x += (voxel->position_x & 1) * FIXED_ONE;
			ray.pos.y += (voxel->position_y & 1) * FIXED_ONE;
			ray.pos.z += (voxel->position_z & 1) * FIXED_ONE;

			recalculateRay3(&ray);
			voxel = voxel->parent;
			iterateRay3(&ray);
			count += 1;
			continue;
		}
	sh:;
		Voxel* child = voxel->child[ray.square_pos.z][ray.square_pos.y][ray.square_pos.x];
		if(child->type == VOXEL_AIR){
			iterateRay3(&ray);
			count += 1;
			continue;
		}
		if(child->type == VOXEL_PARENT){
			int x = (int[]){VEC3_Y,VEC3_X,VEC3_X}[ray.square_side];
			int y = (int[]){VEC3_Z,VEC3_Z,VEC3_Y}[ray.square_side];

			int side_delta = ((int*)&ray.side)[ray.square_side] - ((int*)&ray.delta)[ray.square_side];

			((int*)&ray.pos)[x] = fixedFract(((int*)&ray.pos)[x] + fixedMulR(side_delta,((int*)&ray.dir)[x])) << 1;
			((int*)&ray.pos)[y] = fixedFract(((int*)&ray.pos)[y] + fixedMulR(side_delta,((int*)&ray.dir)[y])) << 1;
			((int*)&ray.pos)[ray.square_side] = ((int*)&ray.dir)[ray.square_side] < 0 ? FIXED_ONE * 2 - 1 : 0;

			voxel = child;
			recalculateRay3(&ray);
			count += 1;
			goto sh;
		}
		return count;
	}
}

Voxel* treeRayTraceAndInit(Vec3 position,Vec3 direction,int* side){
	TraverseInit init = initTraverse(position);
	return treeRayTrace(init.voxel,init.pos,direction,side);
}

int treeRayTraceIntersectCountAndInit(Vec3 position,Vec3 direction){
	TraverseInit init = initTraverse(position);
	return treeRayTraceIntersectCount(init.voxel,init.pos,direction);
}

static void voxelFreeRecursive(Voxel* voxel){
	if(voxel->type == VOXEL_PARENT){
		for(int i = 0;i < 8;i++)
			voxelFreeRecursive(voxel->child_s[i]);
	}
    
    if(g_voxel_static[voxel->type].flags & VOXEL_EMITER){
        int emission = tMax(tMax(g_voxel_static[voxel->type].color.x,g_voxel_static[voxel->type].color.y),g_voxel_static[voxel->type].color.z) << 8;
        emission >>= voxel->depth * 2;
        for(Voxel* v = voxel;v;v = v->parent)
            v->emission -= emission;
    }
    
	tFree(voxel);
}

void voxelSet(Voxel* voxel,Vec3 pos,int depth,VoxelType type){
	Voxel* node = voxel;
	for(int i = depth - 1;i >= 0;i--){
        Vec3 sub_pos = {pos.x >> i & 1,pos.y >> i & 1,pos.z >> i & 1};
		Voxel* child = node->child[sub_pos.z][sub_pos.y][sub_pos.x];
		if(child){
			node = child;
			continue;
		}
		for(int j = 0;j < 8;j++){
			Voxel* node_new = tMallocZero(sizeof(Voxel));
            node_new->position_x = (pos.x >> i + 1 << 1) + (j >> 0 & 1);
			node_new->position_y = (pos.y >> i + 1 << 1) + (j >> 1 & 1);
			node_new->position_z = (pos.z >> i + 1 << 1) + (j >> 2 & 1);
			node_new->parent = node;
			node_new->type = VOXEL_AIR;
			node_new->depth = node->depth + 1;

			node->child_s[j] = node_new;

			if(j != sub_pos.z * 4 + sub_pos.y * 2 + sub_pos.x)
				continue;
			child = node->child_s[j];
		}
		node->type = VOXEL_PARENT;
		node = child;
	}
	for(;;){
		Voxel* parent = node->parent;
		for(int i = 0;i < 8;i++){
			Voxel* child = parent->child_s[i];
			if(child == node || child->type == type)
				continue;
			if(node->type == VOXEL_PARENT){
				for(int j = 0;j < 8;j++){
					voxelFreeRecursive(node->child_s[j]);
					node->child_s[j] = 0;
				}
			}
			node->type = type;
			if(type == VOXEL_BUTTON){
				node->next_voxel_link = g_voxel_link_list;
				g_voxel_link_list = node;
			}
            if(g_voxel_static[type].flags & VOXEL_EMITER){
                int emission = tMax(tMax(g_voxel_static[type].color.x,g_voxel_static[type].color.y),g_voxel_static[type].color.z) << 8;
                emission >>= depth * 2;
                for(Voxel* v = node;v;v = v->parent)
                    v->emission += emission;
            }
			return;
		}
		if(!node->depth)
			return;
		node = parent;
	}
}

bool lineOfSight(Vec3 position_1,Vec3 position_2){
	Vec3 direction = vec3Direction(position_1,position_2);
	int side;
	Voxel* voxel = treeRayTraceAndInit(position_1,direction,&side);
	int ray_distance = treeRayTraceDistance(voxel,position_1,direction,side);
	return ray_distance > vec3Distance(position_1,position_2);
}
