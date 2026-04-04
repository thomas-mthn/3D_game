#ifndef THREAD_H
#define THREAD_H

#ifdef __linux__
#include <pthread.h>

typedef pthread_t thread_t;

#define THREAD_ENTRY void*
#define THREAD_RETURN void*
#else
#include "win32/w_kernel.h"

typedef void* thread_t;

#define THREAD_ENTRY unsigned stdcall
#define THREAD_RETURN unsigned
#endif

static void threadWait(thread_t* threads,int n_thread){
#ifdef __linux__
	for(int i = 0;i < n_thread;i++)
        pthread_join(threads[i],NULL);
#else
	WaitForMultipleObjects(n_thread,threads,true,INFINITE);
	for(int i = 0;i < n_thread;i++)
		CloseHandle(threads[i]);
#endif
}

static thread_t threadCreate(THREAD_RETURN (*entry)(void*),void* arguments){
#ifdef __linux__
	thread_t thread;
	pthread_create(&thread,NULL,entry,arguments);
	return thread;
#else
	return CreateThread(0,0,entry,arguments,0,0);
#endif
}

#endif
