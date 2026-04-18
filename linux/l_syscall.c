#include "l_syscall.h"

long systemOpen(char* filename,int flags,int mode){
    long ret;
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "a"(2),"D"(filename),"S"(flags),"d"(mode)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long systemClose(unsigned file_descriptor){
    long ret;
    
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "a"(3),"D"(file_descriptor)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long systemWrite(unsigned file_descriptor,void* buffer,size_t buffer_size){
    long ret;
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "a"(1),"D"(file_descriptor),"S"(buffer),"d"(buffer_size)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long systemFileStat(unsigned file_descriptor,KernelStat* stat){
    long ret;
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "a"(5),"D"(file_descriptor),"S"(stat)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long systemRead(unsigned file_descriptor,void* buffer,size_t buffer_size){
    long ret;
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "a"(0),"D"(file_descriptor),"S"(buffer),"d"(buffer_size)
        : "rcx", "r11", "memory"
    );
    return ret;
}

void* systemMemoryMap(void* address,size_t length,int protection,int flags,int fd,long offset){
    void* ret;

    register long r10 __asm__("r10") = (long)flags;
    register long r8  __asm__("r8")  = (long)fd;
    register long r9  __asm__("r9")  = (long)offset;
    
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(0x09),
          "D"(address),
          "S"(length),
          "d"(protection),
          "r"(r10),
          "r"(r8),
          "r"(r9)
        : "rcx", "r11", "memory"
    );

    return ret;
}

long systemMemoryUnmap(void* address,size_t length){
    long ret;

    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(0x0b),
          "D"(address),
          "S"(length)
        : "rcx","r11","memory"
    );
    return ret;
}

long systemSignalAction(SignalType signal,SignalAction* action,SignalAction* old_action){
    long ret;
    register long r10 __asm__("r10") = (long)sizeof(sigset__t);
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "a"(0x0D),"D"(signal),"S"(action),"d"(old_action),"r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long systemGetdents(unsigned file_descriptor,char* buffer,int buffer_length){
    long ret;
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "a"(0x4e),"D"(file_descriptor),"S"(buffer),"d"(buffer_length)
        : "rcx", "r11", "memory"
    );
    return ret;
}

long systemTimeGet(TimeSpec* timeval){
    long ret;
    asm volatile(
        "syscall"
        : "=a" (ret)
        : "a"(0xe4),"D"(1),"S"(timeval)
        : "rcx", "r11", "memory"
    );
    return ret;
}

void systemProcessExit(int exit_code){
    asm volatile(
        "syscall"
        :
        : "a"(231),"D"(exit_code)
        : "rcx", "r11", "memory"
    );
}

long systemClone(long flags,void* stack_ptr,int* parent_id,int* child_tid,long tls){
    long ret;

    register long r10 __asm__("r10") = (long)child_tid;
    register long r8  __asm__("r8")  = (long)tls;

    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(0x38),
          "D"(flags),
          "S"(stack_ptr),
          "d"(parent_id),
          "r"(r10),
          "r"(r8)
        : "rcx", "r11", "memory"
    );

    return ret;
}

long systemFutex(int* address, int operation, unsigned value,TimeVal* utime, int* address_2, unsigned flags){
    long ret;

    register long r10 __asm__("r10") = (long)utime;
    register long r8  __asm__("r8")  = (long)address_2;
    register long r9  __asm__("r9")  = (long)flags;

    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(0xCA),
          "D"(address),
          "S"(operation),
          "d"(value),
          "r"(r10),
          "r"(r8),
          "r"(r9)
        : "rcx", "r11", "memory"
    );

    return ret;
}
