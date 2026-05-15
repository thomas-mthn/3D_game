#include "storage.h"
#include "../memory.h"

#ifdef __linux__
#include "../linux/l_syscall.h"
#endif
 
bool storageFileExist(char* path){
#ifdef __linux__
    int file = systemOpen(path,0,0);
    if(file < 0)
        return false;
    systemClose(file);
    return true;
#else
    return false;
#endif
}

FileContent storageFileRead(MemoryArena* arena,char* path){
#ifdef __linux__
    int file = systemOpen(path,0,0);

	if(file < 0)
		return (FileContent){0};
    
    KernelStat stat;
    systemFileStat(file,&stat);
	unsigned file_size = stat.st_size;
	char* content = memoryArenaAllocate(arena,file_size);
    
    systemRead(file,content,file_size);
    systemClose(file);
    
	return (FileContent){.content = content,.size = file_size};
#else
    return (FileContent){0};
#endif
}

bool storageFileReadStatic(void* data,size_t size,char* path){
#ifdef __linux__
    int file = systemOpen(path,0,0);

	if(file < 0)
		return false;

    systemRead(file,data,size);
    systemClose(file);
    
	return true;
#else
    return false;
#endif
}

void storageFileWrite(void* data,size_t size,char* path){
#ifdef __linux__
    int file = systemOpen(path,O_WRONLY | O_CREAT,0);

    systemWrite(file,data,size);
    systemClose(file);
#else
    
#endif
}
