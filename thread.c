#include "thread.h"
#include "langext.h"
#include "main.h"
#include "tmath.h"

#ifdef __linux__
#include <pthread.h>
#include <bits/pthreadtypes.h>

#include "linux/l_syscall.h"
#include "memory.h"
#include "string.h"

#define THREAD_ENTRY void*
#define THREAD_RETURN void*

typedef pthread_t thread_t;

#elif defined(_MSC_VER)

#include "win32/w_kernel.h"
#include <intrin.h>

#define THREAD_ENTRY unsigned stdcall
#define THREAD_RETURN unsigned

typedef void* thread_t;

#else

#define THREAD_ENTRY void
#define THREAD_RETURN void

typedef void* thread_t;

#endif

#ifdef _MSC_VER
static void* cond;
static void* cond_main;
#elif defined(__linux__)
static pthread_mutex_t mutex;
static pthread_mutex_t mutex_main;
static pthread_cond_t cond;
static pthread_cond_t cond_main;
#endif

static thread_t threads[MAX_THREAD];
static void (*work)(void*);
static void* work_arguments[0x10];
static int n_working_thread;
static bool work_todo[0x10];

int g_n_thread_available = 1;
int g_n_threads = 1;

static THREAD_ENTRY threadLoop(void* arguments){
    int id = *(int*)arguments;
    for(;;){
#ifdef __linux__
        pthread_mutex_lock(&mutex);
        while(!work_todo[id])
            pthread_cond_wait(&cond,&mutex);
        pthread_mutex_unlock(&mutex);
        work_todo[id] = false;
        
        work(work_arguments[id]);
        
        pthread_mutex_lock(&mutex);
        if(!--n_working_thread)
            pthread_cond_signal(&cond_main);
        pthread_mutex_unlock(&mutex);
        
#elif defined(_MSC_VER)
        while(!work_todo[id])
            WaitForSingleObject(cond,INFINITE);
                work_todo[id] = false;

        work(work_arguments[id]);
        
        if(!_InterlockedDecrement(&n_working_thread))
            SetEvent(cond_main);
#endif

    }
}

void threadInit(void){
#ifdef __linux__
    int cpuinfo_file = systemOpen("/proc/cpuinfo",0,0);
    if(cpuinfo_file == -1)
        goto no_cpuinfo;
    
    char* buffer = virtualAllocate(0x1000000);
    int file_size = 0;
    for(;;){
        int s = systemRead(cpuinfo_file,buffer + file_size,0x1000000);
        if(!s)
            break;
        file_size += s;
    }
    String cpuinfo = {.data = buffer,.size = file_size};
    for(int core_counter = 0;;core_counter += 1){
        cpuinfo = stringInString(cpuinfo,(String)STRING_LITERAL("processor"));
        if(!cpuinfo.data){
            g_n_thread_available = core_counter;
            systemClose(cpuinfo_file);
            break;
        }
        cpuinfo.data += 1;
        cpuinfo.size -= 1;
    }
 no_cpuinfo:
    virtualFree(buffer,0x1000000);
    
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&mutex_main, NULL);
    pthread_cond_init(&cond, NULL);
    pthread_cond_init(&cond_main, NULL);
#elif defined(_MSC_VER)
    SystemInfo system_info;
	GetSystemInfo(&system_info);
	g_n_thread_available = system_info.number_of_processors;

    cond = CreateEventA(0,true,false,0);
    cond_main = CreateEventA(0,true,false,0);
#endif
    if(!g_options.multi_thread)
        return;
    g_n_threads = tMin(g_n_thread_available,MAX_THREAD);
    static int thread_id[0x10];
    for(int i = g_n_threads;i--;){
        thread_id[i] = i;
#ifdef __linux__
        pthread_create(threads + i,0,threadLoop,thread_id + i);
#elif defined(_MSC_VER)
        threads[i] = CreateThread(0,0,threadLoop,thread_id + i,0,0);
#endif
    }
}

void threadWork(void (*entry)(void*),void* arguments,int arg_size){
    work = entry;
    for(int i = g_n_threads;i--;){
        work_arguments[i] = (void*)((char*)arguments + arg_size * i);
        work_todo[i] = true;
    }
    n_working_thread = g_n_threads;

    if(g_n_threads == 1){
        work(arguments);
        return;
    }
#ifdef __linux__
    pthread_cond_broadcast(&cond);
    
    while(n_working_thread)
        pthread_cond_wait(&cond_main,&mutex);
    
#elif defined(_MSC_VER)
    ResetEvent(cond_main);

    SetEvent(cond);

    while(n_working_thread)
        WaitForSingleObject(cond_main,INFINITE);

    ResetEvent(cond);
#endif
}
