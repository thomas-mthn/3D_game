#include "memory.h"

#ifndef __wasm__
#include "win32/w_kernel.h"
#include "win32/w_user.h"
#include "win32/w_main.h"
#else
uint8 g_wasm_memory[0x1000000];
uint8* g_wasm_memory_ptr = (uint8*)g_wasm_memory;
#endif

static char g_memory_scratch[0x1000000];
static char* g_memory_scratch_ptr = g_memory_scratch;

static void errorBox(void){
    showCursor(true);
    char* error_string = 0;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        0,
        GetLastError(),
        1024,
        &error_string,
        0,
        0
    );
    MessageBoxA(0,error_string,"error",0);
}



void* memoryScratchGet(int size){
    if(size == MEMORY_SCRATCH_MAX_SIZE || g_memory_scratch_ptr + size > g_memory_scratch + countof(g_memory_scratch)){
        g_memory_scratch_ptr = g_memory_scratch;
        return g_memory_scratch_ptr;
    }
    char* ptr = g_memory_scratch_ptr;
    g_memory_scratch_ptr += size;
    return ptr;
}

void* memoryScratchGetZero(int size){
    if(size == MEMORY_SCRATCH_MAX_SIZE)
        size = countof(g_memory_scratch);
    char* memory = memoryScratchGet(size);
    for(int i = 0;i < size;i++)
        memory[i] = 0;
    return memory;
}

void* memoryArenaGet(void){
    int size = 0x40000000;
    unsigned error;
    void* mem;
    do{
        mem = VirtualAlloc(0,size,MEM_COMMIT | MEM_RESERVE,PAGE_READWRITE);
        size >>= 1;
    } while(!mem && size >= 0x1000);
    if(!mem)
        errorBox();
    return mem;
}

void memoryArenaFree(void* address){
    if(!VirtualFree(address,0,MEM_RELEASE))
        errorBox();
}

void* tMalloc(size_t size){
    if(!size)
        return 0;
#ifndef __wasm__
    void* mem = HeapAlloc(GetProcessHeap(),0,size);
    return mem;
#else
    uint8* ret_ptr = g_wasm_memory_ptr;
    g_wasm_memory_ptr += size;
    return ret_ptr;
#endif
}

void* tMallocZero(size_t size){
    if(!size)
        return 0;
#ifndef __wasm__
    void* mem = HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size);
    return mem;
#else
    uint8* ret_ptr = g_wasm_memory_ptr;
    g_wasm_memory_ptr += size;
    return ret_ptr;
#endif
}

void tFree(void* address){
    if(!address)
        return;
#ifndef __wasm__
    HeapFree(GetProcessHeap(),0,address);
#endif
}