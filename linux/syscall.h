#ifndef L_SYSCALL_H
#define L_SYSCALL_H



long systemOpen(char* filename,int flags,int mode);
long systemClose(unsigned file_descriptor);
long systemWrite(unsigned file_descriptor,void* buffer,size_t buffer_size);
long systemFileStat(unsigned file_descriptor,KernelStat* stat);
long systemRead(unsigned file_descriptor,void* buffer,size_t buffer_size);
long systemGetdents(unsigned file_descriptor,char* buffer,int buffer_length);
long systemTimeGet(TimeVal* timeval);
long systemProcessExit(int exit_code);

#endif
