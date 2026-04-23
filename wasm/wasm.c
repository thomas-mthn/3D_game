#include "wasm.h"

#include "../draw.h"
#include "../memory.h"
#include "../string.h"
#include "../libc.h"

char g_wasm_world_data[0x1000000];

static int offset = 4;
static int n_polygon;

structure(VertexLighting){
	float pos[3];
	float lighting[3];
};

static void coloredPolygon3d(DrawSurface* surface,Vec3* coordinats,Vec3* color,LightmapTree* lightmap){
    Vec3 point_2[4];
	point_2[0] = pointToScreenRenderer(coordinats[0],g_tri,g_position,g_options.fov);
	point_2[1] = pointToScreenRenderer(coordinats[1],g_tri,g_position,g_options.fov);
	point_2[2] = pointToScreenRenderer(coordinats[2],g_tri,g_position,g_options.fov);
	point_2[3] = pointToScreenRenderer(coordinats[3],g_tri,g_position,g_options.fov);

	Vec3 d_point[] = {
		{point_2[0].x,point_2[0].y,point_2[0].z},
		{point_2[1].x,point_2[1].y,point_2[1].z},
		{point_2[3].x,point_2[3].y,point_2[3].z},
		{point_2[2].x,point_2[2].y,point_2[2].z}
	};

    float color_div = FIXED_ONE << 4;

    VertexLighting* vertex = (void*)(g_wasm_world_data + offset);
    vertex[0] = (VertexLighting){
        .pos = {-(float)d_point[0].y / FIXED_ONE,-(float)d_point[0].x / FIXED_ONE,(float)d_point[0].z / FIXED_ONE},
        .lighting = {(float)color[0].z / color_div,(float)color[0].y / color_div,(float)color[0].x / color_div},
    };
    vertex[1] = (VertexLighting){
        .pos = {-(float)d_point[1].y / FIXED_ONE,-(float)d_point[1].x / FIXED_ONE,(float)d_point[1].z / FIXED_ONE},
        .lighting = {(float)color[1].z / color_div,(float)color[1].y / color_div,(float)color[1].x / color_div},
    };
    vertex[2] = (VertexLighting){
        .pos = {-(float)d_point[2].y / FIXED_ONE,-(float)d_point[2].x / FIXED_ONE,(float)d_point[2].z / FIXED_ONE},
        .lighting = {(float)color[2].z / color_div,(float)color[2].y / color_div,(float)color[2].x / color_div},
    };
    vertex[3] = (VertexLighting){
        .pos = {-(float)d_point[3].y / FIXED_ONE,-(float)d_point[3].x / FIXED_ONE,(float)d_point[3].z / FIXED_ONE},
        .lighting = {(float)color[3].z / color_div,(float)color[3].y / color_div,(float)color[3].x / color_div},
    };
    offset += sizeof(VertexLighting) * 4;

    n_polygon += 1;
}

void wasmPolygon(DrawSurface* surface,Vec2* coordinats,int n_point,Vec3 color){

}

void wasmPolygon3d(DrawSurface* surface,Vec3* coordinats,Vec3 color){
    Vec3 gl_color[] = {
        color,
        color,
        color,
        color,
    };
    coloredPolygon3d(surface,coordinats,gl_color,0);
}

Texture wasmLoadImage(String path){
    String scan = {.data = g_wasm_world_data,.size = countof(g_wasm_world_data)};
    String texture = stringInString(scan,path);
    texture = stringForwardSlice(texture,path.size);
    
    if(!texture.data)
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

    tMemcpy(&file_header,texture.data,sizeof file_header);
    texture = stringForwardSlice(texture,sizeof file_header);
    
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
	
    tMemcpy(&info_header,texture.data,sizeof info_header);
    texture = stringForwardSlice(texture,sizeof info_header);

	int image_size = info_header.width * info_header.height;
	
	int* image_data = tMalloc(image_size * sizeof(unsigned) * 2);
	
	uint8* temp = tMalloc(image_size * 3);
    tMemcpy(temp,texture.data,image_size * 3);
	for(int i = 0;i < image_size;i++){
		image_data[i] = temp[i * 3 + 0] << 0;
		image_data[i] |= temp[i * 3 + 1] << 8;
		image_data[i] |= temp[i * 3 + 2] << 16;
	}
	tFree(temp);
	return (Texture){.pixel_data = image_data,.size = info_header.width};
}

void wasmLeftButtonDown(void){
	lButtonDown();
}

void wasmRightButtonDown(void){
	rButtonDown();
}

void wasmMiddleButtonDown(void){
	mButtonDown();
}

void wasmKeyDown(int key){
	keyPress(key);
}

static int world_size;

FileContent wasmOctreeDeserialize(char* name){
    return (FileContent){.content = g_wasm_world_data + sizeof(int),.size = world_size};
}

void wasmInit(int voxel_serial_size,int width,int height){
    g_surface.backend = RENDER_BACKEND_WEBGL;
	g_surface.width = width;
	g_surface.height = height;
    world_size = voxel_serial_size;
    
	mainInit();
}

void wasmMouseMove(int delta_x,int delta_y){
	mouseMove(delta_x,delta_y);
}

int* wasmFrame(int tick){
    n_polygon = 0;
    offset = 4;
	surfaceBlit(&g_surface);
	tick *= N_TICK_SECOND;
	tick /= 1000;
	static int prev_tick;
	if(!prev_tick)
		prev_tick = tick;
	int n_tick = tick - prev_tick;
	prev_tick = tick;

	for(int i = tAbs(n_tick);i--;)
		tickRun();
	frameRender();
	for(int i = 0;i < g_surface.width * g_surface.height;i++){
		g_surface.data[i] = 
			(g_surface.data[i] & 0x00FF00) | 
			(g_surface.data[i] & 0x0000FF) << 16 | 
			(g_surface.data[i] & 0xFF0000) >> 16; 
        g_surface.data[i] |= 0xFF000000;
	}
    *(int*)g_wasm_world_data = 1;//n_polygon;
    return g_surface.data;
}
