#include "memory.h"

#ifdef __linux__
#include <stdlib.h>
#include "linux/l_syscall.h"
#include "tmath.h"
#elif !defined(__wasm__)
#include "win32/w_kernel.h"
#include "win32/w_user.h"
#include "win32/w_main.h"
#endif

static char memory_scratch[0x1000000];
static char* memory_scratch_ptr = memory_scratch;

static void errorBox(void){
#if !defined(__wasm__) && !defined(__linux__)
    showCursor(true);
    char* error_string = 0;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        0,
        GetLastError(),
        1024,
        error_string,
        0,
        0
    );
    MessageBoxA(0,error_string,"error",0);
#endif
}

void* memoryScratchGet(int size){
    if(size == MEMORY_SCRATCH_MAX_SIZE || memory_scratch_ptr + size > memory_scratch + countof(memory_scratch)){
        memory_scratch_ptr = memory_scratch;
        return memory_scratch_ptr;
    }
    char* ptr = memory_scratch_ptr;
    memory_scratch_ptr += size;
    return ptr;
}

void* memoryScratchGetZero(int size){
    if(size == MEMORY_SCRATCH_MAX_SIZE)
        size = countof(memory_scratch);
    char* memory = memoryScratchGet(size);
    for(int i = 0;i < size;i++)
        memory[i] = 0;
    return memory;
}

#ifdef __linux__
static struct{
    void* address;
    size_t size;
} memory_table[0x1000];
#endif

void* memoryArenaGet(void){
    int size = 0x40000000;
#ifdef __linux__
    void* address = systemMemoryMap(0,size,PROT_READ | PROT_WRITE,MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
    int hash = tHash((size_t)address) % countof(memory_table);
    while(memory_table[hash].address){
        hash += 1;
        hash %= countof(memory_table);
    }
    memory_table[hash].address = address;
    memory_table[hash].size = size;
    return address;
#elif !defined(__wasm__)
    unsigned error;
    void* mem;
    do{
        mem = VirtualAlloc(0,size,MEM_COMMIT | MEM_RESERVE,PAGE_READWRITE);
        size >>= 1;
    } while(!mem && size >= 0x1000);
    if(!mem)
        errorBox();
    return mem;
#else
    return 0;
#endif
}

void memoryArenaFree(void* address){
#ifdef __linux__
    int hash = tHash((size_t)address % countof(memory_table));
    while(memory_table[hash].address != address){
        hash += 1;
        hash %= countof(memory_table);
    }
    systemMemoryUnmap(memory_table[hash].address,memory_table[hash].size);
    memory_table[hash].address = 0;
#elif !defined(__wasm__)
    if(!VirtualFree(address,0,MEM_RELEASE))
        errorBox();
#endif
}

structure(MemoryBlock){
	MemoryBlock* previous;
    MemoryBlock* next;
    MemoryBlock* next_free;
    union{
        size_t size;
        enum{
            MEMBLOCK_FREE = (1 << 0),
        } flags;
    };
};

static char memory[0x10000000];
static char* memory_ptr = memory;
static MemoryBlock* block_list;
static MemoryBlock* free_list;

static size_t blockSize(MemoryBlock* block){
    return block->size & ~(sizeof(void*) - 1);
}

void* tMalloc(size_t size){
    if(!size)
        return nullptr;
    //alignment
    size = (size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
	MemoryBlock* block_previous = 0;
	for(MemoryBlock* block = free_list;block;block = block->next_free){
		if(blockSize(block) >= size){
            if(block_previous)
                block_previous->next_free = block->next_free;
            else
                free_list = block->next_free;
            block->flags &= ~MEMBLOCK_FREE;

			if(blockSize(block) < size + sizeof(MemoryBlock) * 2)
				return block + 1;
			
			MemoryBlock* block_new = (void*)((char*)(block + 1) + size);
			block_new->size = blockSize(block) - size - sizeof(MemoryBlock);
			block_new->next = block->next;
			block_new->previous = block;
			
			if(block->next)
				block->next->previous = block_new;

			block->next = block_new;
			block->size = size;
		
			block_new->flags |= MEMBLOCK_FREE;
            block_new->next_free = free_list;
            free_list = block_new;
			return block + 1;
		}
		block_previous = block;
	}
	MemoryBlock* block = (void*)memory_ptr;
	block->size = size;
	block->next = 0;
    block->next_free = 0;
    block->flags &= ~MEMBLOCK_FREE;
	memory_ptr += sizeof(MemoryBlock) + size;
    static MemoryBlock* tail;
	if(block_list){
		block->previous = tail;
		tail->next = block;
	}
	else{
		block->previous = 0;
		block_list = block;
	};
    tail = block;
	return block + 1;
}

void tFree(void* address){
    if(!address)
        return;
    MemoryBlock* header = (MemoryBlock*)address - 1;
    if(header->next && (header->next->flags & MEMBLOCK_FREE)){
		if(free_list == header->next){
			free_list = header->next->next_free;
		}
		else{
			for(MemoryBlock* block = free_list;;block = block->next_free){
				if(block->next_free == header->next){
					block->next_free = header->next->next_free;
					break;
				}
			}
		}
		if(header->next->next)
			header->next->next->previous = header;
        header->size += blockSize(header->next) + sizeof(MemoryBlock);
		header->next_free = header->next->next_free;
        header->next = header->next->next;
    }
    if(header->previous && (header->previous->flags & MEMBLOCK_FREE)){
		if(header->next)
			header->next->previous = header->previous;
		header->previous->next = header->next;
		header->previous->size += blockSize(header) + sizeof(MemoryBlock);
		return;
    }
	header->flags |= MEMBLOCK_FREE;
    header->next_free = free_list;
    free_list = header;
}   

void* tMallocZero(size_t size){
    char* memory = tMalloc(size);
    for(int i = size;i--;)
        memory[i] = 0;
    return (void*)memory;
}
