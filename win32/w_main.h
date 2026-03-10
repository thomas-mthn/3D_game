#ifndef WIN32_H
#define WIN32_H

#include "../langext.h"

structure(Voxel);

void showCursor(bool show);
void createWindow(void);
bool win32OctreeDeserialize(void);
void win32OctreeSerialize(Voxel* root_voxel);
Voxel* win32LoadModel(char* path);
unsigned* win32LoadImage(char* path);

extern int g_n_threads;
extern void* g_window;

#endif