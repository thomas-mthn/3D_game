#include "voxel_menu.h"
#include "voxel_gui.h"
#include "main.h"
#include "octree_render.h"
#include "memory.h"
#include "staff.h"
#include "draw.h"
#include "opengl.h"

Texture g_spinning_staff = {.size = 0x100};

void staffEditorCreateMenu(struct Staff* staff){
	VoxelGuiElement menu_static[] = {
		{.type = VOXEL_GUI_STRING,.position = {0x1000,FIXED_ONE - 0x1C00},.string.size = 0x1000,.string.string = STRING_LITERAL("staff editor")},
		{.type = VOXEL_GUI_STRING,.position = {0x1000,FIXED_ONE - 0x3C00},.string.size = 0x800,.string.string = STRING_LITERAL("reload   =")},
		{.type = VOXEL_GUI_NUMBER,.position = {0x6800,FIXED_ONE - 0x3C00},.string.size = 0x800,.number = &g_equipped.reload},
		{.type = VOXEL_GUI_STRING,.position = {0x1000,FIXED_ONE - 0x4C00},.string.size = 0x800,.string.string = STRING_LITERAL("delay    =")},
		{.type = VOXEL_GUI_NUMBER,.position = {0x6800,FIXED_ONE - 0x4C00},.string.size = 0x800,.number = &g_equipped.delay},
		{.type = VOXEL_GUI_STRING,.position = {0x1000,FIXED_ONE - 0x5C00},.string.size = 0x800,.string.string = STRING_LITERAL("capacity =")},
		{.type = VOXEL_GUI_NUMBER,.position = {0x6800,FIXED_ONE - 0x5C00},.string.size = 0x800,.number = &g_equipped.capacity},
		{.type = VOXEL_GUI_STRING,.position = {0x1000,FIXED_ONE - 0x6C00},.string.size = 0x800,.string.string = STRING_LITERAL("mana gen =")},
		{.type = VOXEL_GUI_NUMBER,.position = {0x6800,FIXED_ONE - 0x6C00},.string.size = 0x800,.number = &g_equipped.mana_generation},
		{.type = VOXEL_GUI_STRING,.position = {0x1000,FIXED_ONE - 0x7C00},.string.size = 0x800,.string.string = STRING_LITERAL("mana max =")},
		{.type = VOXEL_GUI_NUMBER,.position = {0x6800,FIXED_ONE - 0x7C00},.string.size = 0x800,.number = &g_equipped.mana_max},
		{.type = VOXEL_GUI_IMAGE,.position = {0x1000,FIXED_ONE - 0xCC00},.image = &g_spinning_staff},
	};
	VoxelGuiElement* menu = tMalloc(sizeof(*menu) * (countof(menu_static) + staff->capacity));
	for(int i = 0;i < countof(menu_static);i++)
		menu[i] = menu_static[i];
	for(int i = 0;i < staff->capacity;i++){
		int offset = i * 0x2800;
		menu[countof(menu_static) + i] = (VoxelGuiElement){
			.type = VOXEL_GUI_INVENTORY_SLOT,
			.position = {offset + 0x800,0x800},
			.inventory_slot = staff->spell_array + i,
		};
	}
	g_voxel_static[VOXEL_STAFF_INSPECTOR].side[VEC3_X * 2].n_gui = countof(menu_static) + staff->capacity;
	g_voxel_static[VOXEL_STAFF_INSPECTOR].side[VEC3_X * 2].gui = menu;
}

void spinningStaffSpin(void){
	static int counter;
	if(counter++ % 0x10 == 0 && g_equipped.model){
		if(!g_spinning_staff.pixel_data)
			g_spinning_staff.pixel_data = tMallocZero(g_spinning_staff.size * g_spinning_staff.size * sizeof(int) * 2);
		static DrawSurface surface_model;
		static Vec2 angle = {0x1000};
		angle.y += FIXED_ONE / 24;
		for(int i = 0;i < g_spinning_staff.size * g_spinning_staff.size;i++)
			g_spinning_staff.pixel_data[i] = 0xFF000000;
		surface_model = (DrawSurface){
			.width = g_spinning_staff.size,
			.height = g_spinning_staff.size,
			.backend = RENDER_BACKEND_SOFTWARE,
		};
		surfaceInit(&surface_model);
		tFree(surface_model.data);
		surface_model.data = g_spinning_staff.pixel_data;
		int camera_distance = FIXED_ONE * 2;
		Vec3 direction = vec3Normalize(getLookDirection(angle));
		Vec3 camera_position = vec3AddRS(vec3MulRS((Vec3){-direction.x,-direction.y,-direction.z},camera_distance * 2),FIXED_ONE);
		Vec3 luminance[] = {
			vec3Single(FIXED_ONE / 2),
			vec3Single(FIXED_ONE / 3),
			vec3Single(FIXED_ONE / 2),
			vec3Single(FIXED_ONE / 3),
			vec3Single(FIXED_ONE / 4),
			vec3Single(FIXED_ONE),
		};
		voxelModelRasterize(&surface_model,angle,luminance,g_equipped.model,camera_position,camera_distance);
		generateMipmaps(&g_spinning_staff);
#if !defined(__wasm__) && !defined(__linux__)
		textureUpdateGL(&g_spinning_staff);
#endif
		//g_spinning_staff
		surface_model.data = 0;
		surfaceDestroy(&surface_model);
	}
}

static void changeRenderBackendSoftware(VoxelGuiElement* self);
static void changeRenderBackendGL(VoxelGuiElement* self);

static void renderLines(VoxelGuiElement* self);
static void goBack(VoxelGuiElement* self);
static void toggleMovement(VoxelGuiElement* self);
static void antiAliasingSet(VoxelGuiElement* self);
static void toggleVsync(VoxelGuiElement* self);

static void toggleEditorMode(VoxelGuiElement* self){
	entityDestroyAll();
	if(g_options.editor){
		g_movement_fly = true;
		g_voxel_placement = true;
	}
	else{
		g_movement_fly = false;
		g_voxel_placement = false;
	}
}

static VoxelGuiElement voxel_menu_gui[] = {
	{.type = VOXEL_GUI_BUTTON,.button.on_click = goBack,.position = {0x1000,FIXED_ONE - 0x2000}},
	{.type = VOXEL_GUI_STRING,.position = {0x2000,FIXED_ONE - 0x1C00},.string.string = STRING_LITERAL("go back")},
	{.type = VOXEL_GUI_BUTTON,.button.on_click = renderLines,.position = {0x1000,FIXED_ONE - 0x4000}},
	{.type = VOXEL_GUI_STRING,.position = {0x2000,FIXED_ONE - 0x3C00},.string.string = STRING_LITERAL("render lines")},
	{.type = VOXEL_GUI_BUTTON,.button.on_click = toggleMovement,.position = {0x1000,FIXED_ONE - 0x6000}},
	{.type = VOXEL_GUI_STRING,.position = {0x2000,FIXED_ONE - 0x5C00},.string.string = STRING_LITERAL("movement mode")},
	{.type = VOXEL_GUI_CHECKBOX,.checkbox.state = &g_options.editor,.position = {0x1000,FIXED_ONE - 0x8000}},
	{.type = VOXEL_GUI_STRING,.position = {0x2000,FIXED_ONE - 0x7C00},.string.string = STRING_LITERAL("editor mode")},
	{.type = VOXEL_GUI_CHECKBOX,.checkbox.state = &g_luminance_overlay,.position = {0x1000,FIXED_ONE - 0xA000}},
	{.type = VOXEL_GUI_STRING,.position = {0x2000,FIXED_ONE - 0x9C00},.string.string = STRING_LITERAL("luminance overlay")},
    {.type = VOXEL_GUI_CHECKBOX,.checkbox.state = &g_options.fast_startup,.position = {0x1000,FIXED_ONE - 0xC000}},
	{.type = VOXEL_GUI_STRING,.position = {0x2000,FIXED_ONE - 0xBC00},.string.string = STRING_LITERAL("fast startup")},
};

static VoxelGuiElement voxel_menu_render_backend_gui[] = {
	{.type = VOXEL_GUI_BUTTON,.button.on_click = changeRenderBackendSoftware,.position = {0x1000,FIXED_ONE - 0x2000}},
	{.type = VOXEL_GUI_STRING,.position = {0x2000,FIXED_ONE - 0x1C00},.string.string = STRING_LITERAL("software")},
	{.type = VOXEL_GUI_BUTTON,.button.on_click = changeRenderBackendGL,.position = {0x1000,FIXED_ONE - 0x4000}},
	{.type = VOXEL_GUI_STRING,.position = {0x2000,FIXED_ONE - 0x3C00},.string.string = STRING_LITERAL("opengl")},
};

static void antiAliasingSet(VoxelGuiElement* self){
	antiAliasingSetGL((int)self->button.self_data);
	voxelMenuMainSet();
}

void antiAliasingLoadMenu(VoxelGuiElement* self){
	if(!g_smaa_max)
		return;
	int n_options = (bitScanReverse(g_smaa_max) + 1) * 2 + 1;
	VoxelGuiElement* aa_menu = tMallocZero(sizeof(VoxelGuiElement) * n_options);
	int iter = 0;
	aa_menu[0].type   = VOXEL_GUI_STRING;
	aa_menu[0].string.string = (String)STRING_LITERAL("anti aliasing level");
	aa_menu[0].position = (Vec2){FIXED_ONE - 0x2000,FIXED_ONE - 0x2000};
	for(int i = g_smaa_max;i;i >>= 1){
		VoxelGuiElement* element;
		element = aa_menu + iter + 1;
		element->type = VOXEL_GUI_BUTTON;
		element->button.on_click = antiAliasingSet;
		element->button.self_data = (void*)i;
		element->position = (Vec2){FIXED_ONE - 0x2000,FIXED_ONE - 0x2000 * (iter / 2 + 1) - 0x2000};
		element = aa_menu + iter + 2;
		element->type = VOXEL_GUI_STRING;
		element->string.string = intToString(tMallocZero(0x10),i);
		element->position = (Vec2){FIXED_ONE - 0x3000,FIXED_ONE - 0x2000 * (iter / 2 + 1) - 0x2000};
		iter += 2;
	}
	g_voxel_static[VOXEL_MENU].gui   = aa_menu; 
	g_voxel_static[VOXEL_MENU].n_gui = n_options;
}

static void toggleMovement(VoxelGuiElement* self){
	g_movement_fly ^= true;
}

void debugOptions(VoxelGuiElement* self){
	g_voxel_static[VOXEL_MENU].gui   = voxel_menu_gui;
	g_voxel_static[VOXEL_MENU].n_gui = countof(voxel_menu_gui);
}

static void goBack(VoxelGuiElement* self){
	voxelMenuMainSet();
}

static void renderLines(VoxelGuiElement* self){
	g_render_lines ^= true;
#if !defined(__wasm__) && !defined(__linux__)
	if(g_surface.backend == RENDER_BACKEND_GL)
		openglPolygonFill(!g_render_lines);
#endif
}

void toggleSmoothLighting(VoxelGuiElement* self){
	g_smooth_lighting ^= true;
}

void changeRenderBackend(VoxelGuiElement* self){
	g_voxel_static[VOXEL_MENU].gui = voxel_menu_render_backend_gui;
	g_voxel_static[VOXEL_MENU].n_gui = countof(voxel_menu_render_backend_gui);
}

static void changeRenderBackendSoftware(VoxelGuiElement* self){
	surfaceChangeBackend(&g_surface,RENDER_BACKEND_SOFTWARE);
	voxelMenuMainSet();
	g_options.render_backend = g_surface.backend;
}

static void changeRenderBackendGL(VoxelGuiElement* self){
#if !defined(__wasm__) && !defined(__linux__)
	textureResetGL();
#endif
	surfaceChangeBackend(&g_surface,RENDER_BACKEND_GL);
	voxelMenuMainSet();
	g_options.render_backend = g_surface.backend;
}
