#include "octree.h"
#include "console.h"
#include "dda.h"
#include "main.h"
#include "octree_render.h"
#include "voxel_menu.h"
#include "geometry.h"
#include "voxel_gui.h"
#include "libc.h"
#include "opengl.h"

AllocatorFreeList g_allocator_world; 

Voxel g_voxel;
Voxel* g_voxel_tick_list;

static void toggleVsync(VoxelGuiElement* self){
	g_vsync ^= true;
	vsyncSet(false);
}

void voxelLinkSignal(Voxel* voxel){
    for(int i = voxel->n_link;i--;){
        Voxel* door = voxel->links[i];
        for(int j = door->n_link;j--;){
            if(door->links[j]->animation)
                goto skip;
        }
		door->opened ^= true;
		if(!door->animation){
			door->animation = FIXED_ONE;
			voxelTickListAdd(door);
		}
		else{
			door->animation = FIXED_ONE - door->animation;
		}
    skip:;
    }
}

static void voxelButtonExecute(VoxelGuiElement* self){
    voxelLinkSignal(self->button.voxel);
}

static VoxelGuiElement voxel_menu_gui[] = {
	{
        .type = VOXEL_GUI_BUTTON,
        .button.on_click = toggleSmoothLighting,
        .position = {0x1000,FIXED_ONE - 0x2000}
    },
	{
        .type = VOXEL_GUI_STRING,
        .position = {0x2800,FIXED_ONE - 0x1400},
        .string.string = STRING_LITERAL("toggle smooth lighting")
    },
	{
        .type = VOXEL_GUI_BUTTON,
        .button.on_click = changeRenderBackend,
        .position = {0x1000,FIXED_ONE - 0x4000}
    },
	{
        .type = VOXEL_GUI_STRING,
        .position = {0x2800,FIXED_ONE - 0x3400},
        .string.string = STRING_LITERAL("render backend")
    },
	{
        .type = VOXEL_GUI_BUTTON,
        .button.on_click = antiAliasingLoadMenu,
        .position = {0x1000,FIXED_ONE - 0x6000}
    },
	{
        .type = VOXEL_GUI_STRING,
        .position = {0x2800,FIXED_ONE - 0x5400},
        .string.string = STRING_LITERAL("anti aliasing")
    },
	{
        .type = VOXEL_GUI_BUTTON,
        .button.on_click = toggleVsync,
        .position = {0x1000,FIXED_ONE - 0x8000}
    },
	{
        .type = VOXEL_GUI_STRING,
        .position = {0x2800,FIXED_ONE - 0x7400},
        .string.string = STRING_LITERAL("toggle vsync")
    },
	{
        .type = VOXEL_GUI_BUTTON,
        .button.on_click = debugOptions,
        .position = {0x1000,FIXED_ONE - 0xA000}
    },
	{
        .type = VOXEL_GUI_STRING,.
        position = {0x2800,FIXED_ONE - 0x9400},
        .string.string = STRING_LITERAL("debug options")
    },
};

static VoxelGuiElement voxel_button_gui[] = {
	{.type = VOXEL_GUI_BUTTON,.button.on_click = voxelButtonExecute,.position = {FIXED_ONE / 4,FIXED_ONE / 4},.button.size = FIXED_ONE / 2},
};

static VoxelGuiElement staff_inspector_gui[] = {
	{
        .type = VOXEL_GUI_STRING,
        .position = {0x1000,FIXED_ONE - 0x1C00},
        .string.size = 0x1000,
        .string.string = STRING_LITERAL("staff editor")
    },
};

#define FRAME_SIZE 0x400

static VoxelGuiElement voxel_string_gui[] = {
    {
        .type = VOXEL_GUI_RECTANGLE,
        .rectangle.size = {FIXED_ONE,FRAME_SIZE},
        .rectangle.color = 0xF02020
    },
    {
        .type = VOXEL_GUI_RECTANGLE,
        .rectangle.size = {FRAME_SIZE,FIXED_ONE},
        .rectangle.color = 0xF02020
    },
    {
        .type = VOXEL_GUI_RECTANGLE,
        .position = {0,FIXED_ONE - FRAME_SIZE},
        .rectangle.size = {FIXED_ONE,FRAME_SIZE},
        .rectangle.color = 0xF02020
    },
    {
        .type = VOXEL_GUI_RECTANGLE,
        .position = {FIXED_ONE - FRAME_SIZE},
        .rectangle.size = {FRAME_SIZE,FIXED_ONE},
        .rectangle.color = 0xF02020
    },
};

#define CONSOLE_FRAME_SIZE 0x200

static VoxelGuiElement voxel_console_gui[] = {
    {
        .type = VOXEL_GUI_RECTANGLE,
        .rectangle.size = {FIXED_ONE,CONSOLE_FRAME_SIZE},
        .rectangle.color = 0x20F020
    },
    {
        .type = VOXEL_GUI_RECTANGLE,
        .rectangle.size = {CONSOLE_FRAME_SIZE,0x1000},
        .rectangle.color = 0x20F020
    },
    {
        .type = VOXEL_GUI_RECTANGLE,
        .position = {0,0x1000},
        .rectangle.size = {FIXED_ONE,CONSOLE_FRAME_SIZE},
        .rectangle.color = 0x20F020
    },
    {
        .type = VOXEL_GUI_RECTANGLE,
        .position = {FIXED_ONE - CONSOLE_FRAME_SIZE},
        .rectangle.size = {CONSOLE_FRAME_SIZE,0x1000},
        .rectangle.color = 0x20F020
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
    [VOXEL_AIR] = {
        .translucent = true,
    },
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
	    .emiter = true,
	},
	[VOXEL_HDR_TEST] = {
		.color = {0x3F0000,0x3F0000,0x3F0000},
	    .emiter = true,
	},
	[VOXEL_LIGHT_RED] = {
		.color = {0x3F0000,0xF000,0xF000},
	    .emiter = true,
	},
	[VOXEL_LIGHT_GREEN] = {
		.color = {0xF000,0x3F0000,0xF000},
	    .emiter = true,
	},
	[VOXEL_LIGHT_BLUE] = {
		.color = {0xF000,0xF000,0x3F0000},
	    .emiter = true,
	},
	[VOXEL_LIGHT_YELLOW] = {
		.color = {0x1F000,0x380000,0x3F0000},
        .emiter = true,
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
		.texture_size = FIXED_ONE,
	},
	[VOXEL_MIRROR] = {
		.color = {0xF000,0xF000,0xF000},
        .rd_trace = true,
	},
	[VOXEL_METALLIC] = {
		.color = {0xF000,0xF000,0xF000},
	},
	[VOXEL_MENU] = {
		.color = {0x3000,0x3000,0x3000},
        GUI_ADD(voxel_menu_gui),
        .no_blockplace = true,
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
        .no_blockplace = true,
	},
	[VOXEL_CHEST] = {
		.texture = g_textures + TEXTURE_CHEST,
        .texturefill = true,
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
        .texturefill = true,
	},
    [VOXEL_DOOR] = {
		.texture = g_textures + TEXTURE_UNDESTRUCTIBLE,
        .translucent = true,
	},
	[VOXEL_BOSS] = {
		.texture = g_textures + TEXTURE_UNDESTRUCTIBLE,
        .texturefill = true,
	},
	[VOXEL_BUTTON] = {
		.color = {0xF000,0xF000,0xF000},
        GUI_ADD(voxel_button_gui)
	},
	[VOXEL_STAFF_INSPECTOR] = {
		.color = {0x0F00,0x0F00,0x2000},
		.emiter = true,
		.side[VEC3_X * 2] = {
			.custom = true,
			.color = {0x0F00,0x0F00,0x2000},
            GUI_ADD(staff_inspector_gui)
		},
	},
	[VOXEL_INVENTORY] = {
		.color = {0x2000,0x2000,0x0F00},
        .emiter = true,
        GUI_ADD(g_inventory_gui),
	},
	[VOXEL_LADDER] = {
		.texture = g_textures + TEXTURE_PLANKS,
		.texture_size = FIXED_ONE * 4,
	},
    [VOXEL_WATER] = {
        .translucent = true,
        .rd_trace = true,
    },
    [VOXEL_STRING] = {
        .color = {0x1F00,0x1F00,0x1F00},
        GUI_ADD(voxel_string_gui),
    },
    [VOXEL_CONSOLE] = {
        GUI_ADD(voxel_console_gui),
    },
    [VOXEL_GLASS] = {
        .translucent = true,
        .rd_trace = true,
    },
    [VOXEL_SLOPE_XNP] = {
        .slope = true,
        .texture = g_textures + TEXTURE_STONE_BRICK,
        .texture_size = FIXED_ONE,
        .translucent = true,
        .slope_u = {FIXED_ONE,0,0},
        .slope_v = {0,46336,46336},
        .slope_flip_x = false,
        .slope_flip_y = false,
        .slope_axis = VEC3_X,
    },
    [VOXEL_SLOPE_XNN] = {
        .slope = true,
        .texture = g_textures + TEXTURE_STONE_BRICK,
        .texture_size = FIXED_ONE,
        .translucent = true,
        .slope_u = {-FIXED_ONE,0,0},
        .slope_v = {0,-46336,46336},
        .slope_offset = {FIXED_ONE,FIXED_ONE,0},
        .slope_flip_x = true,
        .slope_flip_y = false,
        .slope_axis = VEC3_X,
    },
    [VOXEL_SLOPE_XPP] = {
        .slope = true,
        .texture = g_textures + TEXTURE_STONE_BRICK,
        .texture_size = FIXED_ONE,
        .translucent = true,
        .slope_u = {FIXED_ONE,0,0},
        .slope_v = {0,-46336,-46336},
        .slope_offset = {0,FIXED_ONE,FIXED_ONE},
        .slope_flip_x = false,
        .slope_flip_y = true,
        .slope_axis = VEC3_X,
    },
    [VOXEL_SLOPE_XPN] = {
        .slope = true,
        .texture = g_textures + TEXTURE_STONE_BRICK,
        .texture_size = FIXED_ONE,
        .translucent = true,
        .slope_u = {-FIXED_ONE,0,0},
        .slope_v = {0,46336,-46336},
        .slope_offset = {FIXED_ONE,0,FIXED_ONE},
        .slope_flip_x = true,
        .slope_flip_y = true,
        .slope_axis = VEC3_X,
    },
    [VOXEL_SLOPE_YNP] = {
        .slope = true,
        .texture = g_textures + TEXTURE_STONE_BRICK,
        .texture_size = FIXED_ONE,
        .translucent = true,
        .slope_u = {0,FIXED_ONE,0},
        .slope_v = {-46336,0,-46336},
        .slope_offset = {FIXED_ONE,0,FIXED_ONE},
        .slope_flip_x = false,
        .slope_flip_y = false,
        .slope_axis = VEC3_Y,
    },
    [VOXEL_SLOPE_YNN] = {
        .slope = true,
        .texture = g_textures + TEXTURE_STONE_BRICK,
        .texture_size = FIXED_ONE,
        .translucent = true,
        .slope_u = {0,-FIXED_ONE,0},
        .slope_v = {46336,0,-46336},
        .slope_offset = {0,FIXED_ONE,FIXED_ONE},
        .slope_flip_x = true,
        .slope_flip_y = false,
        .slope_axis = VEC3_Y,
    },
    [VOXEL_SLOPE_YPP] = {
        .slope = true,
        .texture = g_textures + TEXTURE_STONE_BRICK,
        .texture_size = FIXED_ONE,
        .translucent = true,
        .slope_u = {0,FIXED_ONE,0},
        .slope_v = {46336,0,46336},
        .slope_flip_x = false,
        .slope_flip_y = true,
        .slope_axis = VEC3_Y,
    },
    [VOXEL_SLOPE_YPN] = {
        .slope = true,
        .texture = g_textures + TEXTURE_STONE_BRICK,
        .texture_size = FIXED_ONE,
        .translucent = true,
        .slope_u = {0,-FIXED_ONE,0},
        .slope_v = {-46336,0,46336},
        .slope_offset = {FIXED_ONE,FIXED_ONE,0},
        .slope_flip_x = true,
        .slope_flip_y = true,
        .slope_axis = VEC3_Y,
    },
    [VOXEL_SLOPE_ZNP] = {
        .slope = true,
        .texture = g_textures + TEXTURE_STONE_BRICK,
        .texture_size = FIXED_ONE,
        .translucent = true,
        .slope_u = {0,0,FIXED_ONE},
        .slope_v = {46336,46336,0},
        .slope_flip_x = false,
        .slope_flip_y = false,
        .slope_axis = VEC3_Z,
    },
    [VOXEL_SLOPE_ZNN] = {
        .slope = true,
        .texture = g_textures + TEXTURE_STONE_BRICK,
        .texture_size = FIXED_ONE,
        .translucent = true,
        .slope_u = {0,0,-FIXED_ONE},
        .slope_v = {-46336,46336,0},
        .slope_offset = {FIXED_ONE,0,FIXED_ONE},
        .slope_flip_x = true,
        .slope_flip_y = false,
        .slope_axis = VEC3_Z,
    },
    [VOXEL_SLOPE_ZPP] = {
        .slope = true,
        .texture = g_textures + TEXTURE_STONE_BRICK,
        .texture_size = FIXED_ONE,
        .translucent = true,
        .slope_u = {0,0,-FIXED_ONE},
        .slope_v = {46336,46336,0},
        .slope_offset = {0,0,FIXED_ONE},
        .slope_flip_x = false,
        .slope_flip_y = true,
        .slope_axis = VEC3_Z,
    },
    [VOXEL_SLOPE_ZPN] = {
        .slope = true,
        .texture = g_textures + TEXTURE_STONE_BRICK,
        .texture_size = FIXED_ONE,
        .translucent = true,
        .slope_u = {0,0,FIXED_ONE},
        .slope_v = {-46336,46336,0},
        .slope_offset = {FIXED_ONE,0,0},
        .slope_flip_x = true,
        .slope_flip_y = true,
        .slope_axis = VEC3_Z,
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
		case VOXEL_BUTTON: case VOXEL_PRESSURE_PLATE:{
			VoxelSerializedButton* voxel_serial = (void*)((char*)voxel_serial_array + *voxel_serial_index);
            voxel_serial->n_link = voxel->n_link;
            for(int i = voxel->n_link;i--;)
                voxel_serial->links[i] = voxel->links[i]->index;
            
			*voxel_serial_index += sizeof(VoxelSerializedButton) + sizeof(int) * voxel->n_link;
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
	Voxel* voxel    = allocatorFreeListAlloc(&g_allocator_world,sizeof *voxel);
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
        voxel->string.data = allocatorFreeListAlloc(&g_allocator_world,voxel_serial_string->string_length);
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
	if(voxel_serial->type == VOXEL_BUTTON || voxel_serial->type == VOXEL_PRESSURE_PLATE){
		VoxelSerializedButton* voxel_serial_button = (void*)voxel_serial;
		Voxel* button = voxel_array[*index_i];
        if(voxel_serial_button->n_link){
            button->links = tMallocZero(sizeof(*button->links) * LINK_MAX);
            button->n_link = voxel_serial_button->n_link;
            for(int i = voxel_serial_button->n_link;i--;){
                button->links[i] = voxel_array[voxel_serial_button->links[i]];
                if(!button->links[i]->n_link)
                    button->links[i]->links = tMallocZero(sizeof(*button->links) * LINK_MAX);
                button->links[i]->links[button->links[i]->n_link++] = button;
            }
        
            button->next_voxel_link = g_voxel_link_list;
            g_voxel_link_list = button;
        }
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
		case VOXEL_PARENT:
			if(voxel->type != VOXEL_PARENT)
				return sizeof(VoxelSerialized);
			int count = 0;
			for(int i = 0;i < 8;i++)
				count += voxelMemoryCountRecursive(voxel->child_s[i]);
			return count + sizeof(VoxelSerializedParent);
        case VOXEL_STRING:
            return sizeof(VoxelSerializedString) + voxel->string.size;
		case VOXEL_BUTTON: case VOXEL_PRESSURE_PLATE:
			return sizeof(VoxelSerializedButton) + voxel->n_link * sizeof(int);
		default:
			return sizeof(VoxelSerialized);
	}
}

Voxel* voxelGet(Vec3 position,int depth){
	Voxel* voxel = &g_voxel;
	for(int i = depth - 1;i >= 0;i--){
		Vec3 sub_coord = vec3Shr(position,i);
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
	return g_voxel_static[voxel->type].translucent || voxel->animation || voxel->opened;
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
	neighbour_position.a[side >> 1] += (side & 1) * 2 - 1;
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

static int slopeDistance(Voxel* voxel,Vec3 position,Vec3 direction){
    VoxelStatic* voxel_s = g_voxel_static + voxel->type;
    Vec3 voxel_position = voxelWorldPos(voxel);
    voxel_position = vec3Add(voxel_position,vec3MulS(voxel_s->slope_offset,depthToSize(voxel->depth)));
    Vec3 relative = vec3Sub(position,voxel_position);
    Plane plane = {
        .normal = vec3Cross(voxel_s->slope_u,voxel_s->slope_v),
        .distance = 0,
    };
    return rayPlaneIntersection(relative,direction,plane);
}

static bool slopeHit(Voxel* voxel,Vec3 position,Vec3 direction){
    int distance = slopeDistance(voxel,position,direction);
    Vec3 hit_position = vec3Add(position,vec3MulS(vec3Normalize(direction),distance)); 

    return voxelPositionGet(hit_position) == voxel;
}

Vec3 rayVoxelHitPosition(Voxel* voxel,Vec3 ray_position,Vec3 ray_direction,Vec3Axis side){
    int distance;
    if(g_voxel_static[voxel->type].slope){
        distance = slopeDistance(voxel,ray_position,ray_direction);
    }
    else{
        Plane plane = getPlane(voxel,ray_direction,side);
        distance = rayPlaneIntersection(ray_position,ray_direction,plane) - (ray_direction.a[side] < 0 ? 0x10 : -0x10);
    }
	return vec3Add(ray_position,vec3MulS(ray_direction,distance));
}

RayHit rayHitPosition(Vec3 position,Vec3 direction){
    Vec3Axis side;
    Voxel* voxel = treeRayTraceAndInit(position,direction,&side,(TreeTraceFlags){0});
    if(!voxel)
        return (RayHit){0};
    return (RayHit){.voxel = voxel,.position = rayVoxelHitPosition(voxel,position,direction,side)};
}

Vec3 posWorldPos(Vec3 position,int depth){
	int size = depthToSize(depth);
	return (Vec3){position.x * size,position.y * size,position.z * size};
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
		pos = vec3Shl(pos,1);
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
		pos = vec3Shl(pos,1);
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
				entity->hitable && 
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

Voxel* treeRayTrace(Voxel* voxel,Vec3 position,Vec3 ray_position,Vec3 direction,Vec3Axis* side,TreeTraceFlags flags){
	Ray3 ray = initRay3(position,direction);
	iterateRay3(&ray);
	for(;;){
		unsigned combine = ray.square_pos.x | ray.square_pos.y | ray.square_pos.z;
		if(combine > 1){
			if(!voxel->depth)
				return 0;

            ray.pos = vec3Shr(ray.pos,1);
            
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
        switch(child->type){
            case VOXEL_PARENT:{
                int x = (int[]){VEC3_Y,VEC3_X,VEC3_X}[ray.square_side];
                int y = (int[]){VEC3_Z,VEC3_Z,VEC3_Y}[ray.square_side];
            
                int side_delta = ray.side.a[ray.square_side] - ray.delta.a[ray.square_side];

                ray.pos.a[x] = fixedFract(ray.pos.a[x] + fixedMulR(side_delta,ray.dir.a[x])) << 1;
                ray.pos.a[y] = fixedFract(ray.pos.a[y] + fixedMulR(side_delta,ray.dir.a[y])) << 1;
                ray.pos.a[ray.square_side] = ray.dir.a[ray.square_side] < 0 ? FIXED_ONE * 2 - 1 : 0;

                voxel = child;
                recalculateRay3(&ray);
            } goto sh;
            case VOXEL_AIR:{
                iterateRay3(&ray);
                continue;
            } continue;
        }
        if(!flags.everything_solid && g_voxel_static[child->type].slope && !slopeHit(child,ray_position,direction)){
            iterateRay3(&ray);
            continue;
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

 		    ray.pos = vec3Shr(ray.pos,1);

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

			int side_delta = ((int*)&ray.side)[ray.square_side] - ray.delta.a[ray.square_side];

			ray.pos.a[x] = fixedFract(ray.pos.a[x] + fixedMulR(side_delta,ray.dir.a[x])) << 1;
			ray.pos.a[y] = fixedFract(ray.pos.a[y] + fixedMulR(side_delta,ray.dir.a[y])) << 1;
			ray.pos.a[ray.square_side] = ray.dir.a[ray.square_side] < 0 ? FIXED_ONE * 2 - 1 : 0;

			voxel = child;
			recalculateRay3(&ray);
			count += 1;
			goto sh;
		}
		return count;
	}
}

Voxel* treeRayTraceAndInit(Vec3 position,Vec3 direction,Vec3Axis* side,TreeTraceFlags flags){
	TraverseInit init = initTraverse(position);
	return treeRayTrace(init.voxel,init.pos,position,direction,side,flags);
}

int treeRayTraceIntersectCountAndInit(Vec3 position,Vec3 direction){
	TraverseInit init = initTraverse(position);
	return treeRayTraceIntersectCount(init.voxel,init.pos,direction);
}

void voxelFreeRecursive(Voxel* voxel){
	if(voxel->type == VOXEL_PARENT){
		for(int i = 0;i < 8;i++)
			voxelFreeRecursive(voxel->child_s[i]);
	}
    if(g_voxel_static[voxel->type].emiter){
        int emission = tMax(tMax(g_voxel_static[voxel->type].color.x,g_voxel_static[voxel->type].color.y),g_voxel_static[voxel->type].color.z) << 8;
        emission >>= voxel->depth * 2;
        for(Voxel* v = voxel;v;v = v->parent)
            v->emission -= emission;
    }
	allocatorFreeListFree(&g_allocator_world,voxel);
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
			Voxel* node_new = allocatorFreeListAllocZero(&g_allocator_world,sizeof(Voxel));
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
            for(int j = node->n_link;j--;){
                Voxel* link = node->links[j];
                for(int k = 0;;k++){
                    if(node == link->links[k]){
                        for(int l = k;link->links[l];l++)
                            link->links[l] = link->links[l + 1];
                        break;
                    }
                }
                link->n_link -= 1;
            }
            node->n_link = 0;
			node->type = type;
            if(g_voxel_static[type].emiter){
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
	Vec3Axis side;
	Voxel* voxel = treeRayTraceAndInit(position_1,direction,&side,(TreeTraceFlags){0});
	int ray_distance = treeRayTraceDistance(voxel,position_1,direction,side);
	return ray_distance > vec3Distance(position_1,position_2);
}
