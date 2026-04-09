#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "linux.h"
#include "draw.h"
#include "langext.h"
#include "main.h"
#include "memory.h"

#define O_RDONLY   0
#define O_CREAT    0x40
#define O_NONBLOCK 0x800

structure(LinuxDirent){
    unsigned long  d_ino;
    unsigned long  d_off;
    unsigned short d_reclen;
    char           d_name[];
};

structure(TimeVal){
    long tv_sec;
    long tv_usec;
};

structure(InputEvent){
    TimeVal time;
    uint16_t type;
    uint16_t code;
    int value;
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
    // Each timespec is two fields:
    long               st_atim_tv_sec;
    long               st_atim_tv_nsec;
    long               st_mtim_tv_sec;
    long               st_mtim_tv_nsec;
    long               st_ctim_tv_sec;
    long               st_ctim_tv_nsec;

    long               st_blksize;

    long long          st_blocks;
};

static Display* display;
static Window window;
static int screen;
static GC gc;

int* linuxLoadImage(char* path){
	int file = systemOpen(path,O_RDONLY,0);
	if(file < 0)
		return nullptr;
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
	
	char* temp = tMalloc(image_size * 3);
	systemRead(file,temp,image_size * 3);
	for(int i = 0;i < image_size;i++){
		image_data[i]  = temp[i * 3 + 0] << 16;
		image_data[i] |= temp[i * 3 + 1] << 8;
		image_data[i] |= temp[i * 3 + 2] << 0;
	}
	tFree(temp);
	return image_data;
}

void linuxOctreeSerialize(Voxel* root_voxel){
	int voxel_mem_count = voxelMemoryCountRecursive(root_voxel);
	int voxel_count = voxelChildCountRecursive(root_voxel);
	VoxelSerialized* voxel_diskdata = memoryScratchGet(MEMORY_SCRATCH_MAX_SIZE);
	octreeSerialize(voxel_diskdata,root_voxel);
    unsigned file = systemOpen("world/world_1.octvxl",O_CREAT,0644);
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
    printNumberNL(file_size);
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

int main(void){
    g_options.fast_startup = true;
    g_options.editor = true;
    g_surface.height = 600;
    g_surface.width = 800;
    int fd = systemOpen("/dev/input/by-id",0,0);
    char buf[0x1000];
    
    unsigned mouse_file;
    
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
        800,600,           
        1,                 
        BlackPixel(display, screen),
        WhitePixel(display, screen)
    );
    
    XStoreName(display, window, "3D Game");

    XSelectInput(display,window,ExposureMask | KeyPressMask | PointerMotionMask | ButtonPressMask | StructureNotifyMask);

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
				case ButtonPress:{
                    switch(event.xbutton.button){
                        case Button2:{
                            mButtonDown();
                        } break;
                        case Button3:{
                            rButtonDown();
                        } break;
                        case Button1:{
                            Pixmap blank;
                            XColor dummy;
                            char data[1] = {0};
    
                            blank = XCreateBitmapFromData(display,window,data,1,1);
                            Cursor cursor = XCreatePixmapCursor(display,blank,blank,&dummy,&dummy,0,0);
					
                            XGrabPointer(display,window,true,0,GrabModeAsync,GrabModeAsync,window,cursor,CurrentTime);

                            XFreePixmap(display, blank);
                    
                            cursor_in_window = true;
                            
                            if(cursor_in_window){
                                lButtonDown();
                                break;
                            }
                        } break;
                    }

				} break;
				case KeyPress:{
					if(event.xkey.keycode == KEY_ESCAPE){
						XUngrabPointer(display, CurrentTime);
						XUndefineCursor(display, window);
                        cursor_in_window = false;
					}
                    keyPress(event.xkey.keycode);
				} break;
			}
		}
		
		char keys[32];
		XQueryKeymap(display,keys);
		for(int i = 0;i < 0x100;i++)
			g_key[i] = !!(keys[i / 8] & 1 << i % 8) << 7;
		
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
