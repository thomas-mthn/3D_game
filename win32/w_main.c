#include "w_main.h"
#include "w_kernel.h"
#include "w_user.h"
#include "w_audio.h"
#include "w_opencl.h"

#include "../main.h"
#include "../memory.h"
#include "../draw.h"
#include "../equib_model.h"

#define WINDOW_CLASS_NAME "a_class"

static Msg msg;
static WndClassA windowclass;
static bool initialized;
static bool cursor_out_window;
static unsigned frequency;

void* g_window;

static bool cursor_visible = true;

void win32Print(String string){
    if(!GetConsoleWindow())
    	AllocConsole();
    WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE),string.data,string.size,0,0);
}

void showCursor(bool show){
	if(show != cursor_visible)
		ShowCursor(show);

	cursor_visible = show;
}

static Vec2 getCursorPosition(void){
    Point cursor;
    GetCursorPos(&cursor);
    ScreenToClient(g_window,&cursor);
    return (Vec2){cursor.y,cursor.x};
}

static unsigned getTick(void){
    unsigned count[2];
    QueryPerformanceCounter(count);
    count[0] /= (frequency / N_TICK_SECOND);
    return count[0];
}

static void cursorClipAndHide(void){
	Rect rect;
	GetClientRect(g_window,&rect);
	Point top_left = {rect.left,rect.top};
    Point bottom_right = {rect.right,rect.bottom};

    ClientToScreen(g_window, &top_left);
    ClientToScreen(g_window, &bottom_right);

    Rect screen_rect = {
        .left = top_left.x,
        .top = top_left.y,
        .right = bottom_right.x,
        .bottom = bottom_right.y
    };
	ClipCursor(&screen_rect);
	showCursor(false);
}

void win32OctreeSerialize(Voxel* root_voxel){
	int voxel_mem_count = voxelMemoryCountRecursive(root_voxel);
	int voxel_count = voxelChildCountRecursive(root_voxel);
	VoxelSerialized* voxel_diskdata = memoryScratchGet(MEMORY_SCRATCH_MAX_SIZE);
	octreeSerialize(voxel_diskdata,root_voxel);
	void* file = CreateFileA("world/world_1.octvxl",GENERIC_WRITE,0,0,CREATE_ALWAYS,0,0);
	WriteFile(file,&voxel_count,sizeof voxel_count,0,0);
	WriteFile(file,voxel_diskdata,voxel_mem_count,0,0);
	CloseHandle(file);
}

bool win32OctreeDeserialize(void){
	void* file = CreateFileA("world/world_1.octvxl",GENERIC_READ,0,0,OPEN_EXISTING,0,0);
	if(file == INVALID_HANDLE_VALUE)
		return false;
	unsigned file_size = GetFileSize(file,0);
	VoxelSerializedParent* voxel_diskdata = memoryScratchGet(MEMORY_SCRATCH_MAX_SIZE);
	int voxel_count;
	ReadFile(file,&voxel_count,sizeof(voxel_count),0,0);
	ReadFile(file,voxel_diskdata,file_size - sizeof(voxel_count),0,0);
	CloseHandle(file);
	//TODO: remove malloc
	Voxel** voxel_array = tMallocZero(sizeof(Voxel*) * voxel_count);
	int index_i = 0;
	g_voxel = *octreeDeserializeRecursive(voxel_diskdata,0,0,0,(Vec3){0,0,0},voxel_array,&index_i);
	index_i = 0;
	octreeDeserializeLink(voxel_diskdata,0,0,(Vec3){0,0,0},voxel_array,&index_i);
	tFree(voxel_array);
	return true;
}

Voxel* win32LoadModel(char* path){
	void* file = CreateFileA(path,GENERIC_READ,0,0,OPEN_EXISTING,0,0);
	if(file == INVALID_HANDLE_VALUE)
		return 0;
	unsigned file_size = GetFileSize(file,0);
	VoxelSerializedParent* voxel_diskdata = memoryScratchGet(MEMORY_SCRATCH_MAX_SIZE);
	ReadFile(file,voxel_diskdata,file_size,0,0);
	CloseHandle(file);
	Voxel* result = octreeDeserializeRecursive(voxel_diskdata,0,0,0,(Vec3){0,0,0},0,0);
	return result;
}

static Lresult stdcall windowMessageHandler(void* window,unsigned msg,Wparam wparam,Lparam lparam){
    switch(msg){
		case WM_KEYDOWN:{
			switch(wparam){
				case KEY_ESCAPE:{
					if(!cursor_out_window){
						cursor_out_window = true;
						showCursor(true);
						ClipCursor(0);
					}
				} break;
				case KEY_F7:{
					if(!g_options.editor)
						break;
					Vec3 block_pos;
					if(!blockOutlinePositionGet(&block_pos))
						break;
					Voxel* voxel = voxelGet(block_pos,g_edit_depth);
					int sub_voxel_count = voxelMemoryCountRecursive(voxel);
					VoxelSerialized* voxel_serial = memoryScratchGet(MEMORY_SCRATCH_MAX_SIZE);
					octreeSerialize(voxel_serial,voxel);
					void* file = CreateFileA("model/created.octvxl",GENERIC_WRITE,0,0,CREATE_ALWAYS,0,0);
					WriteFile(file,voxel_serial,sub_voxel_count,0,0);
					CloseHandle(file);
				} break;
				case KEY_F8:{
					if(!g_options.editor)
						break;
					win32OctreeDeserialize();
				} break;
				case KEY_F9:{
					if(!g_options.editor)
						break;
					win32OctreeSerialize(&g_voxel);
				} break;
				case KEY_F11:{
					ShowWindow(g_window,IsZoomed(g_window) ? SW_NORMAL : SW_MAXIMIZE);
				} break;
			}
			keyPress(wparam);
		} break;
		case WM_MBUTTONDOWN:{
			mButtonDown();
		} break;
		case WM_RBUTTONDOWN:{
			rButtonDown();
		} break;
		case WM_LBUTTONDOWN:{
			if(cursor_out_window){
				cursor_out_window = false;
				cursorClipAndHide();
				break;
			}
			lButtonDown();
		} break;
		case WM_LBUTTONUP:{
			lButtonUp();
		} break;
		case WM_INPUT:{
			if(cursor_out_window)
				break;
			char data[sizeof(RawInput)];
			unsigned size = sizeof(RawInput);
			GetRawInputData((void*)lparam,RID_INPUT,data,&size,sizeof(RawInputHeader));
			RawInput* raw_input = (RawInput*)data;
			if(raw_input->header.type == RIM_TYPEMOUSE){
				RawMouse* mouse = &raw_input->data.mouse;
				mouseMove(mouse->last_x,mouse->last_y);
			}
		} break;
		case WM_SIZE:{
			if(!initialized)
				break;
			Rect clientRect; 
			GetClientRect(window, &clientRect);
			int width  = clientRect.right - clientRect.left;
			int height = clientRect.bottom - clientRect.top;
			//surfaceChangeSize(&g_surface,width,height);
		} break;
		case WM_ACTIVATE:{
			Rect rect;
			GetClientRect(window,&rect);
			MapWindowPoints(window,0,(Point*)&rect,2);
			ClipCursor(&rect);
		} break;
		case WM_QUIT: case WM_CLOSE:{
			ExitProcess(0);
		} break;
    }
    return DefWindowProcA(window,msg,wparam,lparam);
}

#define ICON_SIZE 16

static void* iconGenerate(void){
    DrawSurface surface = {
		.height = ICON_SIZE,
		.width = ICON_SIZE,
		.backend = RENDER_BACKEND_SOFTWARE,
	};

	surfaceInit(&surface);

	struct{
		Vec3 coordinates[4];
		int color;
	} polygon[] = {
		{
			.coordinates = {
				{FIXED_ONE,FIXED_ONE,FIXED_ONE * 4},
				{FIXED_ONE,FIXED_ONE,FIXED_ONE},
				{FIXED_ONE,-FIXED_ONE,FIXED_ONE * 4},
				{FIXED_ONE,-FIXED_ONE,FIXED_ONE},
			},
			.color = 0x80F080
		},
		{
			.coordinates = {
				{FIXED_ONE,-FIXED_ONE,FIXED_ONE * 4},
				{FIXED_ONE,-FIXED_ONE,FIXED_ONE},
				{-FIXED_ONE,-FIXED_ONE,FIXED_ONE},
				{-FIXED_ONE,-FIXED_ONE,FIXED_ONE * 4},
			},
			.color = 0x60D060
		},
		{
			.coordinates = {
				{-FIXED_ONE,-FIXED_ONE,FIXED_ONE},
				{-FIXED_ONE,-FIXED_ONE,FIXED_ONE * 4},
				{-FIXED_ONE,FIXED_ONE,FIXED_ONE * 4},
				{-FIXED_ONE,FIXED_ONE,FIXED_ONE},
			},
			.color = 0x20A020
		},
		{
			.coordinates = {
				{-FIXED_ONE,FIXED_ONE,FIXED_ONE * 4},
				{-FIXED_ONE,FIXED_ONE,FIXED_ONE},
				{FIXED_ONE,FIXED_ONE,FIXED_ONE},
				{FIXED_ONE,FIXED_ONE,FIXED_ONE * 4},
			},
			.color = 0x40B040
		},
		{
			.coordinates = {
				{FIXED_ONE,FIXED_ONE,FIXED_ONE * 4},
				{-FIXED_ONE,FIXED_ONE,FIXED_ONE * 4},
				{-FIXED_ONE,-FIXED_ONE,FIXED_ONE  * 4},
				{FIXED_ONE,-FIXED_ONE,FIXED_ONE  * 4},
			},
			.color = 0x30C030
		},
	};

	for(int i = 0;i < countof(polygon);i++)
		drawPolygon3d(surface,polygon[i].coordinates,pixelColorToColor(polygon[i].color));

    BitmapInfoHeader bitmap_header = {
        .size = sizeof(BitmapInfoHeader),
        .width = ICON_SIZE,
        .height = ICON_SIZE,
        .planes = 1,
        .bit_count = 32,
        .size_image = ICON_SIZE * ICON_SIZE * sizeof(int)
    };

    BitmapInfo bitmap_info = {.bmi_header = bitmap_header};

	void* hdc = GetDC(0);

	void* bitmap_color = CreateDIBitmap(hdc,&bitmap_info.bmi_header,CBM_INIT,surface.data,&bitmap_info,0);

    uint8* mask_data = memoryScratchGet(ICON_SIZE * ICON_SIZE / CHAR_BIT); //tMallocZero(ICON_SIZE * ICON_SIZE / CHAR_BIT);
    void* bitmap_mask = CreateBitmap(ICON_SIZE,ICON_SIZE,1,1,mask_data);

	IconInfo icon_info = {
        .is_icon = true,
        .bitmap_color = bitmap_color,
        .bitmap_mask = bitmap_mask
    };

	void* is_icon = CreateIconIndirect(&icon_info);
	
	surfaceDestroy(&surface);

	return is_icon;
}

void createWindow(void){
	g_window = CreateWindowExA(0,WINDOW_CLASS_NAME,"3D C",WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME | WS_MAXIMIZEBOX,0,0,g_surface.window_width,g_surface.window_height,0,0,windowclass.instance,0);

	Rect client;
    GetClientRect(g_window,&client);
    SetWindowPos(g_window,0,0,0,g_surface.window_width + (g_surface.window_width - client.right),g_surface.window_height + (g_surface.window_height - client.bottom),0);

	ShowWindow(g_window,SW_MAXIMIZE);

    g_surface.window_context = GetDC(g_window);

	RawInputDevice input_device = {
		.usage_page = HID_USAGE_PAGE_GENERIC,
		.usage = HID_USAGE_GENERIC_MOUSE,
		.target = g_window,
	};
	RegisterRawInputDevices(&input_device,1,sizeof input_device);

	cursorClipAndHide();
}

static unsigned stdcall audioThread(void* arg){
	for(;;){
		audioDeviceBufferUpdate();
		Sleep(1);
	}
	return 0;
}

Texture win32LoadImage(char* path){
	void* image_file = LoadImageA(0,path,IMAGE_BITMAP,0,0,LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    Bitmap bmp;
    GetObjectA(image_file,sizeof(Bitmap),&bmp);
	int size = bmp.bmWidth;
	BitmapInfo bi = {
		.bmi_header.size = sizeof(BitmapInfoHeader),
		.bmi_header.width = size,
		.bmi_header.height = size,
		.bmi_header.planes = 1,
		.bmi_header.bit_count = 32,
	};
    int* image_data = tMalloc(size * size * sizeof(unsigned) * 2);
    GetDIBits(GetDC(0),image_file,0,size,image_data,&bi,0);
	DeleteObject(image_file);
	return (Texture){.pixel_data = image_data,.size = size};
}

void win32SaveConfig(void){
	void* config_file = CreateFileA("config.bin",GENERIC_WRITE,0,0,CREATE_ALWAYS,0,0);
	WriteFile(config_file,&g_options,sizeof g_options,0,0);
	CloseHandle(config_file);
}

int main(void){
	windowclass = (WndClassA){
		.window_icon = iconGenerate(),
		.wnd_proc    = windowMessageHandler,
		.class_name  = WINDOW_CLASS_NAME
	};

	RegisterClassA(&windowclass);

	SystemInfo system_info;
	GetSystemInfo(&system_info);
	g_n_threads = system_info.number_of_processors;
	createWindow();

	initialized = true;

	void* config_file = CreateFileA("config.bin",GENERIC_READ,0,0,OPEN_EXISTING,0,0);
	if(config_file == INVALID_HANDLE_VALUE){
		win32SaveConfig();
	}
	else{
		ReadFile(config_file,&g_options,sizeof g_options,0,0);
		CloseHandle(config_file);
	}
        
	if(!win32OctreeDeserialize())
		worldDefaultGenerate();

	initOpenCL();
	mainInit();

	g_hand_model = win32LoadModel("model/hand.octvxl");

	audioInit();

    unsigned frequency_buffer[2];
    QueryPerformanceFrequency(frequency_buffer);
	frequency = frequency_buffer[0];
	int prev_tick = getTick();

	CreateThread(0,0,audioThread,0,0,0);

	for(;;){
		while(PeekMessageA(&msg,g_window,0,0,0)){
            GetMessageA(&msg,g_window,0,0);
            DispatchMessageA(&msg);
        }
		
		if(cursor_visible){
			Sleep(1);
			continue;
		}

		if(GetForegroundWindow() == g_window){
            g_cursor = getCursorPosition();
            GetKeyboardState(g_key);
        }

		unsigned tick = getTick();
		
        unsigned n_tick = tick - prev_tick;

		if((int)n_tick < 0)
			n_tick = 0;

		unsigned time_pre[2];
		unsigned time_post[2];
		QueryPerformanceCounter(time_pre);

        for(int i = n_tick;i--;)
            tickRun();

        prev_tick = tick;

		frameRender();

		QueryPerformanceCounter(time_post);

		static unsigned counter;
		static unsigned fps;
		counter += 1;
		if(counter % 0x20 == 0)
			fps = frequency / (time_post[0] - time_pre[0]);

		drawString(g_surface,-FIXED_ONE + 0x800,FIXED_ONE - 0x4000,(String)STRING_LITERAL("fps"),0x800,pixelColorToColor(0xFFFFFF),0xC0);
		drawNumber(g_surface,-FIXED_ONE + 0x800,FIXED_ONE - 0x2000,fps,0x800);

        surfaceBlit(g_surface);
	}
}
