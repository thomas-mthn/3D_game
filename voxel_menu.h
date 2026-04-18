#ifndef VOXEL_MENU_H
#define VOXEL_MENU_H

#include "texture.h"

structure(VoxelGuiElement);
structure(Staff);

void toggleSmoothLighting(VoxelGuiElement* self);
void changeRenderBackend(VoxelGuiElement* self);
void antiAliasingLoadMenu(VoxelGuiElement* self);
void debugOptions(VoxelGuiElement* self);
void spinningStaffSpin(void);
void staffEditorCreateMenu(Staff* staff);

void renderBackendChangeSoftware(void);
void renderBackendChangeGl(void);

extern Texture g_spinning_staff;

#endif
