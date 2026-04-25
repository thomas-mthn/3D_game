#include <X11/X.h>
#include <X11/Xutil.h>
#include <dlfcn.h>

#include "../langext.h"
#include "../draw.h"
#include "../memory.h"
#include "../main.h"
#include "../gui2d.h"
#include "../console.h"

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

void linuxOctreeSerialize(Voxel* root_voxel,char* file_name){
	int voxel_mem_count = voxelMemoryCountRecursive(root_voxel);
	int voxel_count = voxelChildCountRecursive(root_voxel);
	VoxelSerialized* voxel_diskdata = virtualAllocate(voxel_mem_count + sizeof voxel_count);
	octreeSerialize(voxel_diskdata,root_voxel);
    unsigned file = systemOpen(file_name,O_CREAT | O_TRUNC | O_WRONLY,0644);
    systemWrite(file,&voxel_count,sizeof voxel_count);
    systemWrite(file,voxel_diskdata,voxel_mem_count);
    systemClose(file);
    virtualFree(voxel_diskdata,voxel_mem_count + sizeof voxel_count);
}

FileContent linuxFileRead(char* file_name){
    int file = systemOpen(file_name,0,0);

	if(file < 0)
		return (FileContent){0};
    
    KernelStat stat;
    systemFileStat(file,&stat);
	unsigned file_size = stat.st_size;
	char* content = virtualAllocate(file_size);
    
    systemRead(file,content,file_size);
    systemClose(file);
    
	return (FileContent){.content = content,.size = file_size};
}

void linuxBlit(int* data,int width,int height){
	XImage* image = XCreateImage(
        g_surface.display,
        DefaultVisual(g_surface.display,g_surface.screen),
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
		g_surface.display,
		g_surface.window,
		gc,
		image,
		0, 0,
		0, 0,
		width,
		height
	);
	image->data = 0;
	XDestroyImage(image);
	XFlush(g_surface.display);
    for(int i = 0;i < width * height;i++)
        data[i] = 0;
}

static unsigned getTick(void){
    TimeSpec ts;
    systemTimeGet(&ts);

    uint64 ns = (uint64)ts.seconds * 1000000000ULL + ts.nano_seconds;

    return ns / (15259ULL / (N_TICK_BASE / N_TICK_MODIFIER));
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

#define WINDOW_SIZE_X (640 * 2)
#define WINDOW_SIZE_Y (480 * 2)

void set_window_icon(Display *dpy, Window win) {

}

void linuxWindowInit(void){
    XStoreName(g_surface.display,g_surface.window,"3D Game");
    XSelectInput(g_surface.display,g_surface.window,ExposureMask | KeyPressMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask | StructureNotifyMask);
    
    Atom net_wm_icon = XInternAtom(g_surface.display, "_NET_WM_ICON", False);
    Atom cardinal = XInternAtom(g_surface.display, "CARDINAL", False);

    int* icon_data = iconGenerate();

    struct{
        size_t width;
        size_t height;
        size_t data[ICON_SIZE * ICON_SIZE];
    }* icon_data_linux = memoryArenaAllocate(&g_arena_frame,sizeof *icon_data_linux);

    icon_data_linux->height = ICON_SIZE;
    icon_data_linux->width = ICON_SIZE;
    
    for(int i = ICON_SIZE * ICON_SIZE;i--;)
        icon_data_linux->data[i] = icon_data[i] ? icon_data[i] | 0xFF000000 : 0;
    
    XChangeProperty(
        g_surface.display,
        g_surface.window,
        net_wm_icon,
        cardinal,
        32,
        PropModeReplace,
        (void*)icon_data_linux,
        ICON_SIZE * ICON_SIZE + 2
    );
    
    XMapWindow(g_surface.display,g_surface.window);
    XFlush(g_surface.display);
}

int main(void){
    int config_file = systemOpen("config.bin",0,0);
	if(config_file == -1){
		linuxSaveConfig();
	}
	else{
        systemRead(config_file,&g_options,sizeof g_options);
	    systemClose(config_file);
	}
    g_options.editor = true;
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

    g_surface.display = XOpenDisplay(NULL);

    if(!g_surface.display)
        return 0;

    g_surface.screen = DefaultScreen(g_surface.display);
    
    g_surface.window = XCreateSimpleWindow(
        g_surface.display,
        RootWindow(g_surface.display,g_surface.screen),
        10,10,
        WINDOW_SIZE_X,WINDOW_SIZE_Y,
        0,
        0,
        0
    );
    g_surface.window_height = WINDOW_SIZE_Y;
    g_surface.window_width = WINDOW_SIZE_X;

    linuxWindowInit();
    
	gc = XCreateGC(g_surface.display,g_surface.window, 0, NULL);
	
    XEvent event;
    
	mainInit();
	
	int prev_tick = getTick();

    int frame_counter = 0;
    int fps = 0;

    TimeSpec time_pre = {0};
    
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

        TimeSpec time_post;
        systemTimeGet(&time_post);

        if(!(frame_counter++ % 0x20))
            fps = 1000000000 / (time_post.nano_seconds - time_pre.nano_seconds + (time_post.seconds - time_pre.seconds) * 1000000000);

        g_delta = (time_post.nano_seconds - time_pre.nano_seconds) + (time_post.seconds - time_pre.seconds) * 1000000000 >> 8;
        
        time_pre = time_post;
        
        while(XPending(g_surface.display)){
			XNextEvent(g_surface.display,&event);
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
    
                            blank = XCreateBitmapFromData(g_surface.display,g_surface.window,data,1,1);
                            Cursor cursor = XCreatePixmapCursor(g_surface.display,blank,blank,&dummy,&dummy,0,0);
					
                            XGrabPointer(g_surface.display,g_surface.window,true,0,GrabModeAsync,GrabModeAsync,g_surface.window,cursor,CurrentTime);

                            XFreePixmap(g_surface.display, blank);
                    
                            cursor_in_window = true;
                        } break;
                    }

				} break;
				case KeyPress:{
                    switch(event.xkey.keycode){
                        case KEY_F4:{
                            XUngrabPointer(g_surface.display,CurrentTime);
                            XUndefineCursor(g_surface.display,g_surface.window);
                        } break;
                        case KEY_ESCAPE:{
                            XUngrabPointer(g_surface.display,CurrentTime);
                            XUndefineCursor(g_surface.display,g_surface.window);
                            cursor_in_window = false;
                        } break;
                    }
                    keyPress(event.xkey.keycode);
				} break;
			}
		}
		
		char keys[32];
		XQueryKeymap(g_surface.display,keys);
		for(int i = 0;i < 0x100;i++)
			g_key[i] = !!(keys[i / 8] & 1 << i % 8) << 7;
#if 0
        for(int i = 0;i < 0x100;i++){
            if(g_key[i]){
                debugPrint("key: ");
                printNumberNL(i);
                printNumberNL(KEY_LEFT);
            }
        }
#endif   
        unsigned mouse_keys;
        int dummy_int;
        Window dummy_window;
        XQueryPointer(g_surface.display,g_surface.window,&dummy_window,&dummy_window,&dummy_int,&dummy_int,&dummy_int,&dummy_int,&mouse_keys);

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
        
        for(int i = n_tick / (FIXED_ONE * N_TICK_MODIFIER);i--;){
            g_delta = FIXED_ONE * N_TICK_MODIFIER;
            tickRun();
        }
        
        g_delta = n_tick % FIXED_ONE * N_TICK_MODIFIER;
        tickRun();

        prev_tick = tick;
        
		frameRender();

        gui2dStringDraw(0x800,0x4A00,(String)STRING_LITERAL("fps"),0xA00,0x000000,0x1400,(Gui2dFlags){0});
        gui2dNumberDraw(0x800,0x2A00,fps,0x800,(Gui2dFlags){0});
        
        surfaceBlit(&g_surface);
    }

    XCloseDisplay(g_surface.display);
    return 0;
}

