#include "syscall.h"

static long systemOpen(char* filename,int flags,int mode){
    long ret;
    
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "a"(2),"D"(filename),"S"(flags),"d"(mode)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static long systemClose(unsigned file_descriptor){
    long ret;
    
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "a"(3),"D"(file_descriptor)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static long systemWrite(unsigned file_descriptor,void* buffer,size_t buffer_size){
    long ret;
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "a"(1),"D"(file_descriptor),"S"(buffer),"d"(buffer_size)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static long systemFileStat(unsigned file_descriptor,KernelStat* stat){
    long ret;
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "a"(5),"D"(file_descriptor),"S"(stat)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static long systemRead(unsigned file_descriptor,void* buffer,size_t buffer_size){
    long ret;
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "a"(0),"D"(file_descriptor),"S"(buffer),"d"(buffer_size)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static long systemGetdents(unsigned file_descriptor,char* buffer,int buffer_length){
    long ret;
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "a"(0x4e),"D"(file_descriptor),"S"(buffer),"d"(buffer_length)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static long systemTimeGet(TimeVal* timeval){
    long ret;
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "a"(0xe4),"D"(1),"S"(timeval)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static long systemProcessExit(int exit_code){
    long ret;
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "a"(0x3c),"D"(exit_code)
        : "rcx", "r11", "memory"
    );
    return ret;
}


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
