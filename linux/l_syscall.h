#ifndef L_SYSCALL_H
#define L_SYSCALL_H

#include "../langext.h"

#define PROT_READ  0x01
#define PROT_WRITE 0x02

#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20

#define CLONE_VM             0x00000100
#define CLONE_FS             0x00000200
#define CLONE_FILES          0x00000400
#define CLONE_SIGHAND        0x00000800
#define CLONE_PTRACE         0x00002000
#define CLONE_VFORK          0x00004000
#define CLONE_PARENT         0x00008000
#define CLONE_THREAD         0x00010000
#define CLONE_NEWNS          0x00020000
#define CLONE_SYSVSEM        0x00040000
#define CLONE_SETTLS         0x00080000
#define CLONE_PARENT_SETTID  0x00100000
#define CLONE_CHILD_CLEARTID 0x00200000
#define CLONE_DETACHED       0x00400000
#define CLONE_UNTRACED       0x00800000
#define CLONE_CHILD_SETTID   0x01000000
#define CLONE_NEWUTS         0x04000000
#define CLONE_NEWIPC         0x08000000
#define CLONE_NEWUSER        0x10000000
#define CLONE_NEWPID         0x20000000
#define CLONE_NEWNET         0x40000000
#define CLONE_IO             0x80000000

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

structure(TimeVal){
    long tv_sec;
    long tv_usec;
};

structure(KernelStat){
    unsigned int       st_mode;
    unsigned long      st_ino;
    unsigned long      st_dev;

    unsigned long      st_rdev;
    unsigned long      st_nlink; 

    unsigned int       st_uid;  
    unsigned int       st_gid;

    long long          st_size;

    long               st_atim_tv_sec;
    long               st_atim_tv_nsec;
    long               st_mtim_tv_sec;
    long               st_mtim_tv_nsec;
    long               st_ctim_tv_sec;
    long               st_ctim_tv_nsec;

    long               st_blksize;

    long long          st_blocks;
    uint8              __padding[32];
};

long systemOpen(char* filename,int flags,int mode);
long systemClose(unsigned file_descriptor);
long systemWrite(unsigned file_descriptor,void* buffer,size_t buffer_size);
long systemFileStat(unsigned file_descriptor,KernelStat* stat);
long systemRead(unsigned file_descriptor,void* buffer,size_t buffer_size);
void* systemMemoryMap(void* address,size_t length,int protection,int flags,int fd,long offset);
long systemMemoryUnmap(void* address,size_t length);
long systemGetdents(unsigned file_descriptor,char* buffer,int buffer_length);
long systemTimeGet(TimeVal* timeval);
long systemProcessExit(int exit_code);
long systemClone(long flags,void* stack_ptr,int* parent_id,int* child_tid,long tls);
long systemFutex(int* address,int operation,unsigned value,TimeVal* utime,int* address_2,unsigned flags);

#endif
