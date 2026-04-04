#ifndef VOXEL_MENU_H
#define VOXEL_MENU_H

#include "texture.h"

structure(VoxelGuiElement);

struct Staff;

void toggleSmoothLighting(VoxelGuiElement* self);
void changeRenderBackend(VoxelGuiElement* self);
void antiAliasingLoadMenu(VoxelGuiElement* self);
void debugOptions(VoxelGuiElement* self);
void spinningStaffSpin(void);
void staffEditorCreateMenu(struct Staff* staff);

extern Texture g_spinning_staff;

#endif