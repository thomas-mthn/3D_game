#ifndef WIN32_H
#define WIN32_H

#include "../langext.h"
#include "../string.h"
#include "../texture.h"
#include "../main.h"

structure(Voxel);

void showCursor(bool show);
void createWindow(void);
FileContent win32OctreeDeserialize(char* path);
void win32OctreeSerialize(Voxel* root_voxel);
Voxel* win32LoadModel(char* path);
FileContent win32FileRead(char* file_name);
Texture win32LoadImage(char* path);
void win32SaveConfig(void);
void win32Print(String string);

extern int g_n_threads;
extern void* g_window;

#endif
