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

typedef enum{
	VOXEL_GUI_BUTTON,
	VOXEL_GUI_CHECKBOX,
	VOXEL_GUI_STRING,
	VOXEL_GUI_NUMBER,
	VOXEL_GUI_IMAGE,
	VOXEL_GUI_INVENTORY_SLOT,
} VoxelGuiElementType;

structure(VoxelGuiElement){
	VoxelGuiElementType type;
	Vec2 position;
	int size;
	void (*on_click)(VoxelGuiElement* self);
	String string;
	void* self_data;
	Voxel* voxel;
	bool* checkbox_state;
	Texture* image;
	int* number;
	InventorySlot* inventory_slot;
};

void drawString3D(DrawSurface* surface,Voxel* voxel,Vec2 axis,Vec3 block_pos,Vec2 uv,String string,int scale,bool mirror,int thickness);
void drawGuiCircle(Voxel* voxel,Vec2 axis,Vec3 block_pos,Vec2 uv,int size,int color);
void drawGuiFrame(Voxel* voxel,Vec2 axis,Vec3 block_pos,Vec2 uv,Vec2 size,int color,int thickness);

void voxelGuiDraw(Voxel* voxel,Vec3 block_pos,int side);
bool voxelGuiOnClick(Voxel* voxel,int side);
void voxelGuiOnRelease(Voxel* voxel,int side);

extern SpellType g_spell_hold;

#endif
