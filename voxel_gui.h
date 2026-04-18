#ifndef VOXEL_GUI_H
#define VOXEL_GUI_H

#include "langext.h"
#include "vec2.h"
#include "vec3.h"
#include "staff.h"
#include "string.h"

structure(Voxel);
structure(Texture);
structure(InventorySlot);
structure(DrawSurface);

typedef enum {
	VOXEL_GUI_BUTTON,
	VOXEL_GUI_CHECKBOX,
	VOXEL_GUI_STRING,
	VOXEL_GUI_NUMBER,
	VOXEL_GUI_IMAGE,
	VOXEL_GUI_INVENTORY_SLOT,
    VOXEL_GUI_RECTANGLE,
} VoxelGuiElementType;

structure(VoxelGuiElement){
	VoxelGuiElementType type;
    Vec2 position;
    union{
        struct{
            bool* state;
            void (*on_click)(VoxelGuiElement* self);
        } checkbox;
        struct{
            int size;
            void (*on_click)(VoxelGuiElement* self);
            void* self_data;
            Voxel* voxel;
        } button;
        struct{
            String string;
            int size;
        } string;
        struct{
            int* number;
            int size;
        } number;
        struct{
            Texture* image;
        } image;
        struct{
            InventorySlot* slot;
        } inventory_slot;
        struct{
            Vec2 size;
            int color;
            enum{
                RECTANGLE_ABSOLUTE_SIZE_X = (1 << 0),
                RECTANGLE_ABSOLUTE_SIZE_Y = (1 << 1),
            } flags;
        } rectangle;
    };
};

void drawGuiChar(Voxel* voxel,int side,Vec2 uv,char string_char,int scale,int thickness);
void drawGuiString(Voxel* voxel,int side,Vec2 uv,String string,int scale,int thickness);
void drawGuiCircle(Voxel* voxel,Vec2 axis,Vec3 block_pos,Vec2 uv,int size,int color);
void drawGuiRectangle(Voxel* voxel,Vec2 axis,Vec3 block_pos,Vec2 uv,Vec2 size,int color,int side);
void drawGuiFrame(Voxel* voxel,Vec2 axis,Vec3 block_pos,Vec2 uv,Vec2 size,int color,int thickness,int side);

void voxelGuiDraw(Voxel* voxel,Vec3 block_pos,int side);
bool voxelGuiOnClick(Voxel* voxel,int side);
void voxelGuiOnRelease(Voxel* voxel,int side);

extern SpellType g_spell_hold;

#endif
