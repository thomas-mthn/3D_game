#ifndef THREAD_H
#define THREAD_H

#define MAX_THREAD 0x10

void threadInit(void);
void threadWork(void (*entry)(void*),void* arguments,int arg_size);

extern int g_n_thread_available;
extern int g_n_threads;

#endif
