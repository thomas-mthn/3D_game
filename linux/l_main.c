#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "../langext.h"
#include "../draw.h"
#include "../memory.h"
#include "../main.h"

#include "l_main.h"
#include "l_syscall.h"

#define O_RDONLY   0
#define O_WRONLY   (1 << 0)
#define O_CREAT    (1 << 6)
#define O_TRUNC    (1 << 9)
#define O_NONBLOCK (1 << 11)

structure(LinuxDirent){
    unsigned long  d_ino;
    unsigned long  d_off;
    unsigned short d_reclen;
    char           d_name[];
};

structure(InputEvent){
    TimeVal time;
    uint16_t type;
    uint16_t code;
    int value;
};

static Display* display;
static Window window;
static int screen;
static GC gc;

Texture linuxLoadImage(char* path){
	int file = systemOpen(path,O_RDONLY,0);
	if(file < 0)
		return (Texture){0};
#pragma pack(push,1) 
	struct{
	  uint16 type;
	  unsigned size;
	  uint16  reserved_1;
	  uint16  reserved_2;
	  unsigned off_bits;
	} file_header;
#pragma pack(pop)

	systemRead(file,(char*)&file_header,sizeof file_header);
	
	struct{
		unsigned size;
		int  width;
		int  height;
		uint16 planes;
		uint16 bit_count;
		unsigned compression;
		unsigned size_image;
		int  xpels_per_meter;
		int  ypels_per_meter;
		unsigned clr_used;
		unsigned clr_important;
	} info_header;
	
	systemRead(file,(char*)&info_header,sizeof info_header);
    
	int image_size = info_header.width * info_header.height;
	
	int* image_data = tMalloc(image_size * sizeof(unsigned) * 2);
	
	uint8* temp = tMalloc(image_size * 3);
	systemRead(file,temp,image_size * 3);
    print(stringMake("size: "));
    printNumberNL(image_size);
	for(int i = 0;i < image_size;i++){
		image_data[i] = temp[i * 3 + 0] << 0;
		image_data[i] |= temp[i * 3 + 1] << 8;
		image_data[i] |= temp[i * 3 + 2] << 16;
	}
	tFree(temp);
	return (Texture){.pixel_data = image_data,.size = info_header.width};
}

int* linuxThreadCreate(void (*entry_point)(void*),void* parameters){
    int stack_size = 0x100000;
    void* stack = tMalloc(stack_size);
    void* stack_top = (void*)((uintptr_t)(stack + stack_size) & ~0xF);
    int* child_tid_futex = tMalloc(sizeof(int));
    long flags = CLONE_THREAD | CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_SYSVSEM | CLONE_CHILD_CLEARTID;
    long tid = systemClone(flags,stack_top,0,child_tid_futex,0);
    if(!tid){
        entry_point(parameters);
        tFree(stack);
        systemProcessExit(0);
    }
    return child_tid_futex;
}

void linuxThreadWait(int** thread_handles,int n_thread){
    for(int i = 0;i < n_thread;i++){
        while(*thread_handles[i])
            systemFutex(thread_handles[i],FUTEX_WAIT,0,0,0,0);
    }
}

void linuxOctreeSerialize(Voxel* root_voxel){
	int voxel_mem_count = voxelMemoryCountRecursive(root_voxel);
	int voxel_count = voxelChildCountRecursive(root_voxel);
	VoxelSerialized* voxel_diskdata = memoryScratchGet(MEMORY_SCRATCH_MAX_SIZE);
	octreeSerialize(voxel_diskdata,root_voxel);
    unsigned file = systemOpen("world/world_1.octvxl",O_CREAT | O_TRUNC | O_WRONLY,0644);
    systemWrite(file,&voxel_count,sizeof voxel_count);
    systemWrite(file,voxel_diskdata,voxel_mem_count);
    systemClose(file);
}

bool linuxOctreeDeserialize(void){
    int file = systemOpen("world/world_1.octvxl",0,0);
    
	if(file < 0)
		return false;
    KernelStat stat;
    systemFileStat(file,&stat);
	unsigned file_size = stat.st_size;
	VoxelSerializedParent* voxel_diskdata = memoryScratchGet(MEMORY_SCRATCH_MAX_SIZE);
   	int voxel_count;
    
    systemRead(file,&voxel_count,sizeof voxel_count);
    systemRead(file,voxel_diskdata,file_size - sizeof(voxel_count));
    systemClose(file);
	//TODO: remove malloc
	Voxel** voxel_array = tMallocZero(sizeof(Voxel*) * voxel_count);
	int index_i = 0;
	g_voxel = *octreeDeserializeRecursive(voxel_diskdata,0,0,0,(Vec3){0,0,0},voxel_array,&index_i);
	index_i = 0;
	octreeDeserializeLink(voxel_diskdata,0,0,(Vec3){0,0,0},voxel_array,&index_i);
	tFree(voxel_array);
	return true;
}

void linuxBlit(int* data,int width,int height){
	XImage* image = XCreateImage(
        display,
        DefaultVisual(display, screen),
        24,
        ZPixmap,
        0,
        (char*)data,
        width,
        height,
        32,
        0
    );
	XPutImage(
		display,
		window,
		gc,
		image,
		0, 0,
		0, 0,
		width,
		height
	);
	image->data = 0;
	XDestroyImage(image);
	XFlush(display);
    for(int i = 0;i < width * height;i++)
        data[i] = 0;
}

static unsigned getTick(void){
    TimeVal ts;
    systemTimeGet(&ts);

    uint64 ns = (uint64)ts.tv_sec * 1000000000ULL + ts.tv_usec;

    return ns / (1000000000ULL / N_TICK_SECOND);
}

void linuxPrint(String string){
    systemWrite(1,string.data,string.size);
}

#define EV_REL 0x02

#define REL_X 0x00
#define REL_Y 0x01

#define E_AGAIN -11

static bool cursor_in_window = false;

void linuxSaveConfig(void){
    int config_file = systemOpen("config.bin",O_WRONLY | O_CREAT,0);
    systemWrite(config_file,&g_options,sizeof g_options);
    systemClose(config_file);
}

int main(void){
    int cpuinfo_file = systemOpen("/proc/cpuinfo",0,0);
    if(cpuinfo_file == -1)
        goto no_cpuinfo;
    
    char* buffer = memoryScratchGet(MEMORY_SCRATCH_MAX_SIZE);
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
            g_n_threads = core_counter;
            systemClose(cpuinfo_file);
            break;
        }
        cpuinfo.data += 1;
        cpuinfo.size -= 1;
    }
    
no_cpuinfo:
    int config_file = systemOpen("config.bin",0,0);
	if(config_file == -1){
		linuxSaveConfig();
	}
	else{
        systemRead(config_file,&g_options,sizeof g_options);
	    systemClose(config_file);
	}
    g_options.editor = true;
    g_surface.height = 600;
    g_surface.width = 800;
    int fd = systemOpen("/dev/input/by-id",0,0);
    char buf[0x1000];
    
    unsigned mouse_file = 0;
    
    for(;;){
        int r = systemGetdents(fd,buf,sizeof buf);
        if(r <= 0)
            break;
            
        int progress = 0;
            
        while(progress < r){
            LinuxDirent* dir = (void*)(buf + progress);
            if(stringInString(stringMake(dir->d_name),stringMake("-event-mouse")).data){
                char path[0x100];
                char* path_ptr = path;
                String input_path = STRING_LITERAL("/dev/input/by-id/");
                for(int i = 0;i < input_path.size;i++)
                    *path_ptr++ = input_path.data[i];  
                for(int i = 0;dir->d_name[i];i++)
                    *path_ptr++ = dir->d_name[i];
                *path_ptr++ = 0;
                mouse_file = systemOpen(path,O_NONBLOCK,0);
                break;
            }
            progress += dir->d_reclen;
        }
    }
	
    display = XOpenDisplay(NULL);

    if(!display)
        return 0;

    screen = DefaultScreen(display);
    
    window = XCreateSimpleWindow(
        display,
        RootWindow(display, screen),
        10,10,           
        640 * 2,480 * 2,
        1,                 
        BlackPixel(display, screen),
        WhitePixel(display, screen)
    );
    
    XStoreName(display,window,"3D Game");

    XSelectInput(display,window,ExposureMask | KeyPressMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask | StructureNotifyMask);

    XMapWindow(display, window);
    
	gc = XCreateGC(display, window, 0, NULL);
	
    XEvent event;
    
    if(!linuxOctreeDeserialize())
		worldDefaultGenerate();
    
	mainInit();
	
	int prev_tick = getTick();
	
    for(;;){
        InputEvent ev;
        
        if(cursor_in_window){
            for(;;){
                ssize_t n = systemRead(mouse_file,(char*)&ev,sizeof(ev));
                if(n == E_AGAIN)
                    break;
                if(n != sizeof(ev)) 
                    continue;
                if(ev.type == EV_REL){
                    if(ev.code == REL_X)
                        mouseMove(ev.value,0);
                    if(ev.code == REL_Y)
                        mouseMove(0,ev.value);
                }
            }
        }

        TimeVal time_pre;
		TimeVal time_post;

        systemTimeGet(&time_post);
        
        while(XPending(display)){
			XNextEvent(display,&event);
			switch(event.type){
                case ConfigureNotify:{
                    surfaceChangeSize(&g_surface,event.xconfigure.width,event.xconfigure.height);
                } break;
                case ButtonRelease:{
                    switch(event.xbutton.button){
                        case Button1:{
                            lButtonUp();
                        } break;
                    }
                } break;
				case ButtonPress:{
                    switch(event.xbutton.button){
                        case Button2:{
                            mButtonDown();
                        } break;
                        case Button3:{
                            rButtonDown();
                        } break;
                        case Button1:{
                            if(cursor_in_window){
                                lButtonDown();
                                break;
                            }
                            Pixmap blank;
                            XColor dummy;
                            char data[1] = {0};
    
                            blank = XCreateBitmapFromData(display,window,data,1,1);
                            Cursor cursor = XCreatePixmapCursor(display,blank,blank,&dummy,&dummy,0,0);
					
                            XGrabPointer(display,window,true,0,GrabModeAsync,GrabModeAsync,window,cursor,CurrentTime);

                            XFreePixmap(display, blank);
                    
                            cursor_in_window = true;
                        } break;
                    }

				} break;
				case KeyPress:{
                    switch(event.xkey.keycode){
                        case KEY_F8:{
                            if(!g_options.editor)
                                break;
                            linuxOctreeDeserialize();
                        } break;
                        case KEY_F9:{
                            if(!g_options.editor)
                                break;
                            linuxOctreeSerialize(&g_voxel);
                        } break;
                        case KEY_ESCAPE:{
                            XUngrabPointer(display, CurrentTime);
                            XUndefineCursor(display, window);
                            cursor_in_window = false;
                        } break;
                    }
                    keyPress(event.xkey.keycode);
				} break;
			}
		}
		
		char keys[32];
		XQueryKeymap(display,keys);
		for(int i = 0;i < 0x100;i++)
			g_key[i] = !!(keys[i / 8] & 1 << i % 8) << 7;
        
        unsigned mouse_keys;
        int dummy_int;
        Window dummy_window;
        XQueryPointer(display,window,&dummy_window,&dummy_window,&dummy_int,&dummy_int,&dummy_int,&dummy_int,&mouse_keys);

        if(mouse_keys & 0x100)
            g_key[191] = 1 << 7;
        if(mouse_keys & 0x200)
            g_key[192] = 1 << 7;
        if(mouse_keys & 0x400)
            g_key[193] = 1 << 7;
        
		unsigned tick = getTick();
		
        unsigned n_tick = tick - prev_tick;

		if((int)n_tick < 0)
			n_tick = 0;

        for(int i = n_tick;i--;)
            tickRun();

        prev_tick = tick;
        
		frameRender();
        
        surfaceBlit(g_surface);
    }

    XCloseDisplay(display);
    return 0;
}
