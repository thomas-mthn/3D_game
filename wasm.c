#include "main.h"
#include "draw.h"
#include "octree.h"
#include "memory.h"

#ifdef __wasm__

char g_wasm_world_data[0x100000];

void wasmLeftButtonDown(void){
	lButtonDown();
}

void wasmRightButtonDown(void){
	rButtonDown();
}

void wasmMiddleButtonDown(void){
	mButtonDown();
}

void wasmKeyDown(int key){
	keyPress(key);
}

void wasmInit(int voxel_serial_size,int width,int height){
	g_surface.width = width;
	g_surface.height = height;
	if(!voxel_serial_size){
		worldDefaultGenerate();
	}
	else{
		Voxel** voxel_array = tMallocZero(sizeof(Voxel*) * *(int*)g_wasm_world_data);
		int index_i = 0;
		g_voxel = *octreeDeserializeRecursive((VoxelSerialized*)(g_wasm_world_data + sizeof(int)),0,0,0,(Vec3){0,0,0},voxel_array,&index_i);
		index_i = 0;
		octreeDeserializeLink((VoxelSerialized*)(g_wasm_world_data + sizeof(int)),0,0,(Vec3){0,0,0},voxel_array,&index_i);
		tFree(voxel_array);
	}
	mainInit();
}

void wasmMouseMove(int delta_x,int delta_y){
	mouseMove(delta_x,delta_y);
}

int* wasmFrame(int tick){
	surfaceBlit(g_surface);
	tick *= N_TICK_SECOND;
	tick /= 1000;
	static int prev_tick;
	if(!prev_tick)
		prev_tick = tick;
	int n_tick = tick - prev_tick;
	prev_tick = tick;

	repeat(tAbs(n_tick))
		tickRun();
	frameRender();
	for(int i = 0;i < g_surface.width * g_surface.height;i++){
		/*
		g_surface.data[i] = 
			(g_surface.data[i] & 0x00FF00) | 
			(g_surface.data[i] & 0x0000FF) << 16 | 
			(g_surface.data[i] & 0xFF0000) >> 16; 
		*/
        g_surface.data[i] |= 0xFF000000;
	}
    return g_surface.data;
}
#endif