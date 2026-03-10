#ifndef MEMORY_H
#define MEMORY_H

#include "langext.h"

#define MEMORY_SCRATCH_MAX_SIZE -1

void* memoryScratchGet(int size);
void* memoryScratchGetZero(int size);

void* tMalloc(size_t size);
void* tMallocZero(size_t size);
void tFree(void* address);

void* memoryArenaGet(void);
void memoryArenaFree(void* address);

#endif

