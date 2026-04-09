#ifndef MEMORY_H
#define MEMORY_H

#include "langext.h"

#define MEMORY_SCRATCH_MAX_SIZE -1

structure(MemoryArena){
    size_t capacity;
    size_t pointer;
    char* data;
};

void* virtualAllocate(size_t size);
void virtualFree(void* address,size_t size);

void* memoryScratchGet(int size);
void* memoryScratchGetZero(int size);

void* tMalloc(size_t size);
void* tMallocZero(size_t size);
void tFree(void* address);

void* memoryArenaAllocate(MemoryArena* arena,size_t size);
void memoryArenaFree(MemoryArena* arena);

#endif

