#ifndef LINUX_H
#define LINUX_H

#include "../string.h"
#include "../texture.h"

structure(Voxel);

void linuxOctreeSerialize(Voxel* root_voxel);
bool linuxOctreeDeserialize(void);

void linuxBlit(int* data,int width,int height);
Texture linuxLoadImage(char* path);
void linuxPrint(String string);
void linuxSaveConfig(void);

int* linuxThreadCreate(void (*entry_point)(void*),void* parameters);
void linuxThreadWait(int** thread_handles,int n_thread);

#endif
