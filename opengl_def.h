#ifndef OPENGL_DEF_H
#define OPENGL_DEF_H

#include "langext.h"

#define GL_VIEWPORT 0x0BA2

#define GL_COLOR_BUFFER_BIT 16384
#define GL_DEPTH_BUFFER_BIT 256

#define GL_NEAREST 0x2600
#define GL_LINEAR  0x2601

#define GL_REPEAT 0x2901

#define GL_BGRA 0x80E1
#define GL_RGBA16F 0x881A
#define GL_R32F 0x822E
#define GL_R32I 0x8235

#define GL_NEAREST_MIPMAP_NEAREST         0x2700
#define GL_LINEAR_MIPMAP_NEAREST          0x2701
#define GL_NEAREST_MIPMAP_LINEAR          0x2702
#define GL_LINEAR_MIPMAP_LINEAR           0x2703

#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE

#define GL_ARRAY_BUFFER         0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31

#define GL_ALPHA_TEST  0x0BC0
#define GL_SAMPLES     0x80A9
#define GL_MULTISAMPLE 0x809D

#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0

#define WGL_DRAW_TO_WINDOW_ARB       0x2001
#define WGL_SUPPORT_OPENGL_ARB       0x2010
#define WGL_DOUBLE_BUFFER_ARB        0x2011
#define WGL_PIXEL_TYPE_ARB           0x2013
#define WGL_TYPE_RGBA_ARB            0x202B
#define WGL_TYPE_RGBA_FLOAT_ARB      0x21A0
#define WGL_COLOR_BITS_ARB           0x2014
#define WGL_DEPTH_BITS_ARB           0x2022
#define WGL_STENCIL_BITS_ARB         0x2023
#define WGL_SAMPLE_BUFFERS_ARB       0x2041
#define WGL_SAMPLES_ARB              0x2042

#define GL_READ_FRAMEBUFFER  0x8CA8
#define GL_DRAW_FRAMEBUFFER  0x8CA9

#define GL_TEXTURE0 0x84C0 

#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092

#define GL_NUM_EXTENSIONS 0x821D

#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515

#define GL_TEXTURE_CUBE_MAP_SEAMLESS 0x884F

#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE_WRAP_R 0x8072
#if _WIN64
typedef long long (stdcall *Proc)();
#else
typedef int (stdcall *Proc)();
#endif

typedef enum{
	GL_POINTS,        
	GL_LINES,      
	GL_LINE_LOOP,     
	GL_LINE_STRIP,    
	GL_TRIANGLES,     
	GL_TRIANGLE_STRIP,
	GL_TRIANGLE_FAN,  
	GL_QUADS,         
	GL_QUAD_STRIP,
	GL_POLYGON,    
} DrawType;

typedef enum{
	GL_TEXTURE_1D = 0x0DE0,
	GL_TEXTURE_2D,
} GlTextureType;

typedef enum{
	GL_BYTE = 0x1400,
	GL_UNSIGNED_BYTE,
	GL_SHORT,
	GL_UNSIGNED_SHORT,
	GL_INT,
	GL_UNSIGNED_INT,
	GL_FLOAT,
	GL_2_BYTES,
	GL_3_BYTES,
	GL_4_BYTES,
	GL_DOUBLE,
	GL_HALF_FLOAT,
} DataType;

typedef enum{
	GL_TEXTURE_MAG_FILTER = 0x2800,
	GL_TEXTURE_MIN_FILTER,
	GL_TEXTURE_WRAP_S,
	GL_TEXTURE_WRAP_T,
} TextureParameterType;

typedef enum{
	GL_VENDOR = 0x1F00,
	GL_RENDERER,
	GL_VERSION,
	GL_EXTENSIONS,
} GetStringName;

typedef enum{
	GL_NEVER = 0x0200,
	GL_LESS,
	GL_EQUAL,
	GL_LEQUAL,
	GL_GREATER,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_ALWAYS,
} AlphaTesting;

typedef enum{
	GL_COLOR_INDEX = 0x1900,
	GL_STENCIL_INDEX,
	GL_DEPTH_COMPONENT,
	GL_RED,
	GL_GREEN,
	GL_BLUE,
	GL_ALPHA,
	GL_RGB,
	GL_RGBA,
	GL_LUMINANCE,
	GL_LUMINANCE_ALPHA,
} Format;

#define GL_RED_INTEGER 0x8D94

typedef enum{
	GL_POINT = 0x1B00,
	GL_LINE,
	GL_FILL
} PolygonModeType;

typedef enum{
	GL_FRONT_LEFT = 0x0400,
	GL_FRONT_RIGHT,
	GL_BACK_LEFT,
	GL_BACK_RIGHT,
	GL_FRONT,
	GL_BACK,
	GL_LEFT,
	GL_RIGHT,
	GL_FRONT_AND_BACK,
	GL_AUX0,
	GL_AUX1,
	GL_AUX2,
	GL_AUX3
} DrawBufferMode;

typedef enum{
    GL_SHADER_TYPE           = 0x8B4F,
    GL_DELETE_STATUS         = 0x8B80,
    GL_COMPILE_STATUS        = 0x8B81,
    GL_INFO_LOG_LENGTH       = 0x8B84,
    GL_SHADER_SOURCE_LENGTH  = 0x8B88,
} ShaderInfoType;

static void (stdcall *glClear)(unsigned mask);
static void (stdcall *glClearColor)(float red,float green,float blue,float alpha);
static void (stdcall *glEnable)(int cap);
static void (stdcall *glDisable)(int cap);
static void (stdcall *glAlphaFunc)(AlphaTesting func,float ref);
static void (stdcall *glViewport)(int x,int y,int width,int height);
static int  (stdcall *glGetError)(void);

static void (stdcall *glReadPixels)(int x,int y,int width,int height,Format format,DataType type,void* pixels);

static void (stdcall *glPolygonMode)(DrawBufferMode face,PolygonModeType mode);
static void (stdcall *glBegin)(DrawType type);
static void (stdcall *glEnd)(void);
static void (stdcall *glVertex2f)(float x,float y);
static void (stdcall *glVertex4f)(float x,float y,float z,float w);
static void (stdcall *glColor3b)(int red,int green,int blue);
static void (stdcall *glTexCoord2f)(float u,float v);
static void (stdcall *glDrawArrays)(DrawType type,int first,int count);
static void (stdcall *glDrawElements)(DrawType type,int count,DataType data_type,void* indices);
static int  (stdcall *glGetUniformLocation)(unsigned program,char* name);
static void (stdcall *glUniform1i)(int loc,int v1);
static void (stdcall *glUniform1f)(int loc,float v1);
static void (stdcall *glUniform2f)(int loc,float v1,float v2);
static void (stdcall *glUniform3f)(int loc,float v1,float v2,float v3);
static void (stdcall *glUniform4f)(int loc,float v1,float v2,float v3,float v4);

static void (stdcall *glGenTextures)(unsigned n,unsigned* textures);
static void (stdcall *glDeleteTextures)(int n,unsigned* textures);
static void (stdcall *glBindTexture)(GlTextureType target,unsigned texture);
static void (stdcall *glTexImage2D)(
	GlTextureType target,
	int level,
	int static_format,
	int width,
	int height,
	int border,
	int format,
	DataType type,
	void* pixels
);
static void (stdcall *glTexSubImage2D)(
    GlTextureType target,
    int level,
    int x_offset,
    int y_offset,
    int width,
    int height,
    int format,
    DataType type,
    void* pixels
);
static void (stdcall *glTexImage1D)(
	GlTextureType target,
	int level,
	int static_format,
	int width,
	int border,
	int format,
	DataType type,
	void* pixels
);
static void (stdcall *glTexParameteri)(GlTextureType target,TextureParameterType pname,int param);
static void (stdcall *glTexParameterf)(GlTextureType target,TextureParameterType pname,float param);
static void (stdcall *glGetFloatv)(TextureParameterType pname,float* param);
static void (stdcall *glGetIntegerv)(int pname,int* param);
static void (stdcall *glActiveTexture)(int texture);
static void (stdcall *glCreateBuffers)(unsigned n,unsigned *buffers);
static void (stdcall *glGenBuffers)(unsigned n,unsigned* buffers);
static void (stdcall *glDeleteBuffers)(int n,unsigned* buffers);
static void (stdcall *glBindBuffer)(unsigned target,unsigned buffer);
static void (stdcall *glEnableVertexAttribArray)(unsigned index);
static void (stdcall *glVertexAttribPointer)(unsigned index,int size,unsigned type,unsigned char normalized,unsigned stride,void *pointer);
static void (stdcall *glVertexAttribIPointer)(unsigned index,int size,int type,int stride,void* pointer);
static void (stdcall *glShaderSource)(unsigned shader,int count,char **string,int *length);
static void (stdcall *glCompileShader)(unsigned shader);
static void (stdcall *glAttachShader)(unsigned program,unsigned shader);
static void (stdcall *glDeleteShader)(unsigned shader); 
static void (stdcall *glLinkProgram)(unsigned program);
static void (stdcall *glUseProgram)(unsigned program);
static void (stdcall *glDeleteProgram)(unsigned program);
static void (stdcall *glGetShaderiv)(unsigned shader,ShaderInfoType name,int* params);
static void (stdcall *glGetShaderInfoLog)(unsigned shader,int max_length,int* length,char* info_log);

static char* (stdcall *glGetString)(GetStringName name);
static char* (stdcall *glGetStringi)(GetStringName name,unsigned index);

static void (stdcall *glGenVertexArrays)(int n,unsigned* arrays);
static void (stdcall *glDeleteVertexArrays)(int n,unsigned* arrays);
static void (stdcall *glBindVertexArray)(unsigned array);

static void (stdcall *glBufferData)(unsigned target,unsigned size,void *data,unsigned usage);

static unsigned (stdcall *glCreateProgram)();
static unsigned (stdcall *glCreateShader)(unsigned shader);

static void (stdcall *glGenFramebuffers)(int n,unsigned* ids);
static void (stdcall *glBindFramebuffer)(int target,unsigned framebuffer);
static void (stdcall *glFramebufferTexture2D)(int target,int attachment,int textarget,unsigned texture,int level);
static void (stdcall *glGenRenderbuffers)(int n,unsigned* renderbuffers);
static void (stdcall *glBlitFramebuffer)(int srcX0,int srcY0,int srcX1,int srcY1,int dstX0,int dstY0,int dstX1,int dstY1,unsigned mask,unsigned filter);

#endif
