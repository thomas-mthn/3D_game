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
    VOXEL_GUI_COLORPICKER,
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
            real size;
            void (*on_click)(VoxelGuiElement* self);
            void* self_data;
            Voxel* voxel;
        } button;
        struct{
            String string;
            real size;
        } string;
        struct{
            int* number;
            real size;
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
        } rectangle;
        struct{
            Vec3* color;
        } colorpicker;
    };
};

void drawGuiChar(Voxel* voxel,int side,Vec2 uv,char string_char,real scale,real thickness,int color);
void drawGuiString(Voxel* voxel,int side,Vec2 uv,String string,real scale,real thickness,int color);
void drawGuiCircle(Voxel* voxel,Vec2i axis,Vec3 block_pos,Vec2 uv,real size,int color,int side);
void drawGuiRectangle(Voxel* voxel,Vec2i axis,Vec3 block_pos,Vec2 uv,Vec2 size,int color,int side);
void drawGuiFrame(Voxel* voxel,Vec2i axis,Vec3 block_pos,Vec2 uv,Vec2 size,int color,real thickness,int side);

void voxelGuiDraw(Voxel* voxel,Vec3 block_pos,int side,VoxelGuiElement* gui,int n_gui);
bool voxelGuiOnClick(Voxel* voxel,int side,VoxelGuiElement* gui,int n_gui);
void voxelGuiOnRelease(Voxel* voxel,int side);

Vec2 uvMirror(Vec2 uv,int side);
Vec2 voxelGuiPositionGet(Voxel* voxel,Vec3 position,Vec3 dir,int side);

extern SpellType g_spell_hold;

#endif
