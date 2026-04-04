#include "w_draw_opengl.h"
#include "w_kernel.h"
#include "w_user.h"
#include "w_gdi.h"
#include "w_main.h"

#include "../main.h"

#define GL_VIEWPORT 0x0BA2

#define GL_COLOR_BUFFER_BIT 16384
#define GL_DEPTH_BUFFER_BIT 256

#define GL_NEAREST 0x2600
#define GL_LINEAR  0x2601

#define GL_REPEAT 0x2901

#define GL_BGRA 0x80E1
#define GL_RGBA16F 0x881A

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
} GlDrawType;

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
} GlDataType;

typedef enum{
	GL_TEXTURE_MAG_FILTER = 0x2800,
	GL_TEXTURE_MIN_FILTER,
	GL_TEXTURE_WRAP_S,
	GL_TEXTURE_WRAP_T,
} GlTextureParameterType;

typedef enum{
	GL_VENDOR = 0x1F00,
	GL_RENDERER,
	GL_VERSION,
	GL_EXTENSIONS,
} GlGetStringName;

typedef enum{
	GL_NEVER = 0x0200,
	GL_LESS,
	GL_EQUAL,
	GL_LEQUAL,
	GL_GREATER,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_ALWAYS,
} GlAlphaTesting;

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
} GlFormat;

typedef enum{
	GL_POINT = 0x1B00,
	GL_LINE,
	GL_FILL
} GlPolygonModeType;

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
} GlDrawBufferMode;

static Proc (stdcall *WglGetProcAddress)(const char* function_name);
static int (stdcall *WglMakeCurrent)(void* context,void* gl_context);
static void* (stdcall *WglCreateContext)(void* context);
static unsigned (stdcall *WglSwapIntervalEXT)(unsigned status);
static int (stdcall *WglDeleteContext)(void* gl_context);
static int (stdcall *WglChoosePixelFormatARB)(
	void* hdc,
	const int* attribute_list,
	const float* attribute_list_f,
	unsigned max_formats,
	int* formats,
	unsigned* n_formats
);
static void* (stdcall *WglCreateContextAttribsARB)(void* hdc,void* share_context,const int* attribute_list);

static void (stdcall *GlClear)(unsigned mask);
static void (stdcall *GlClearColor)(float red,float green,float blue,float alpha);
static void (stdcall *GlEnable)(int cap);
static void (stdcall *GlDisable)(int cap);
static void (stdcall *GlAlphaFunc)(GlAlphaTesting func,float ref);
static void (stdcall *GlViewport)(int x,int y,int width,int height);
static int  (stdcall *GlGetError)(void);

static void (stdcall *GlReadPixels)(int x,int y,int width,int height,GlFormat format,GlDataType type,void* pixels);

static void (stdcall *GlPolygonMode)(GlDrawBufferMode face,GlPolygonModeType mode);
static void (stdcall *GlBegin)(GlDrawType type);
static void (stdcall *GlEnd)(void);
static void (stdcall *GlVertex2f)(float x,float y);
static void (stdcall *GlVertex4f)(float x,float y,float z,float w);
static void (stdcall *GlColor3b)(int red,int green,int blue);
static void (stdcall *GlTexCoord2f)(float u,float v);
static void (stdcall *GlDrawArrays)(GlDrawType type,int first,int count);
static void (stdcall *GlDrawElements)(GlDrawType type,int count,GlDataType data_type,void* indices);
static int  (stdcall *GlGetUniformLocation)(unsigned program,const char* name);
static void (stdcall *GlUniform1i)(int loc,int v1);
static void (stdcall *GlUniform3f)(int loc,float v1,float v2,float v3);

static void (stdcall *GlGenTextures)(unsigned n,unsigned* textures);
static void (stdcall *GlDeleteTextures)(int n,unsigned* textures);
static void (stdcall *GlBindTexture)(GlTextureType target,unsigned texture);
static void (stdcall *GlTexImage2D)(
	GlTextureType target,
	int level,
	int static_format,
	int width,
	int height,
	int border,
	int format,
	GlDataType type,
	void* pixels
);
static void (stdcall *GlTexParameteri)(GlTextureType target,GlTextureParameterType pname,int param);
static void (stdcall *GlTexParameterf)(GlTextureType target,GlTextureParameterType pname,float param);
static void (stdcall *GlGetFloatv)(GlTextureParameterType pname,float* param);
static void (stdcall *GlGetIntegerv)(int pname,int* param);
static void (stdcall *GlActiveTexture)(int texture);
static void (stdcall *GlCreateBuffers)(unsigned n,unsigned *buffers);
static void (stdcall *GlGenBuffers)(unsigned n,unsigned* buffers);
static void (stdcall *GlDeleteBuffers)(int n,unsigned* buffers);
static void (stdcall *GlBindBuffer)(unsigned target,unsigned buffer);
static void (stdcall *GlEnableVertexAttribArray)(unsigned index);
static void (stdcall *GlVertexAttribPointer)(unsigned index,int size,unsigned type,unsigned char normalized,unsigned stride,const void *pointer);
static void (stdcall *GlShaderSource)(unsigned shader,int count,const char **string,int *length);
static void (stdcall *GlCompileShader)(unsigned shader);
static void (stdcall *GlAttachShader)(unsigned program,unsigned shader);
static void (stdcall *GlDeleteShader)(unsigned shader); 
static void (stdcall *GlLinkProgram)(unsigned program);
static void (stdcall *GlUseProgram)(unsigned program);
static void (stdcall *GlDeleteProgram)(unsigned program);

static const char* (stdcall *GlGetString)(GlGetStringName name);
static const char* (stdcall *GlGetStringi)(GlGetStringName name,unsigned index);

static void (stdcall *GlGenVertexArrays)(int n,unsigned* arrays);
static void (stdcall *GlDeleteVertexArrays)(int n,unsigned* arrays);
static void (stdcall *GlBindVertexArray)(unsigned array);

static void (stdcall *GlBufferData)(unsigned target,unsigned size,const void *data,unsigned usage);

static unsigned (stdcall *GlCreateProgram)();
static unsigned (stdcall *GlCreateShader)(unsigned shader);

static void (stdcall *GlGenFramebuffers)(int n,unsigned* ids);
static void (stdcall *GlBindFramebuffer)(int target,unsigned framebuffer);
static void (stdcall *GlFramebufferTexture2D)(int target,int attachment,int textarget,unsigned texture,int level);
static void (stdcall *GlGenRenderbuffers)(int n,unsigned* renderbuffers);
static void (stdcall *GlBlitFramebuffer)(int srcX0,int srcY0,int srcX1,int srcY1,int dstX0,int dstY0,int dstX1,int dstY1,unsigned mask,unsigned filter);

static int convertColor(int color){
	return tClamp((color >> 13),INT8_MIN,INT8_MAX);
}

static char *vertex_circle_source = ""
"#version 330 core\n"
"layout (location = 0) in vec3 verticles;"
"layout (location = 1) in vec2 coordinates;"
"layout (location = 2) in vec3 lighting;"

"out vec2 coordinates_io;"
"out vec3 lighting_io;"

"void main(){"
	"lighting_io = lighting;"
	"coordinates_io = coordinates;"
	"gl_Position = vec4(verticles.xy,0.0,verticles.z);"
"}";

static char* fragment_circle_source = ""
"#version 330 core\n"
"out vec4 FragColor;"

"in vec2 coordinates_io;"
"in vec3 lighting_io;"

"void main(){"
	"if(dot(coordinates_io,coordinates_io) > 1.0)"
		"discard;"
	"FragColor = vec4(lighting_io,1.0);"
"}";

static char *vertex_source = ""
"#version 330 core\n"
"layout (location = 0) in vec3 verticles;"

"void main(){"
	"gl_Position = vec4(verticles.xy,0.0,verticles.z);"
"}";

static char *fragment_source = ""
"#version 330 core\n"
"out vec4 FragColor;"

"uniform sampler2D ourTexture;"
"uniform vec3 color;"

"void main(){"
	"FragColor = vec4(color,1.0);"
"}";

static char *vertex_lighting_source = ""
"#version 330 core\n"
"layout (location = 0) in vec3 verticles;"
"layout (location = 1) in vec3 lighting;"

"out vec3 lighting_io;"

"void main(){"
	"lighting_io = lighting;"
	"gl_Position = vec4(verticles.xy,0.0,verticles.z);"
"}";

static char *fragment_lighting_source = ""
"#version 330 core\n"
"out vec4 FragColor;"

"in vec3 lighting_io;"

"void main(){"
	"FragColor = vec4(lighting_io,1.0f);"
"}";

static char *vertex_texture_lighting_source = ""
"#version 330 core\n"
"layout (location = 0) in vec3 verticles;"
"layout (location = 1) in vec2 textcoords;"
"layout (location = 2) in vec3 lighting;"

"out vec3 lighting_io;"
"out vec2 textcoords_io;"

"void main(){"
	"textcoords_io = textcoords;"
	"lighting_io = lighting;"
	"gl_Position = vec4(verticles.xy,0.0,verticles.z);"
"}";

static char *fragment_texture_lighting_source = ""
"#version 330 core\n"
"out vec4 FragColor;"
"in vec2 textcoords_io;"
"in vec3 lighting_io;"

"uniform sampler2D ourTexture;"
"vec3 CubicHermite (vec3 A, vec3 B, vec3 C, vec3 D, float t){"
	"float t2 = t*t;"
    "float t3 = t*t*t;"
    "vec3 a = -A/2.0 + (3.0*B)/2.0 - (3.0*C)/2.0 + D/2.0;"
    "vec3 b = A - (5.0*B)/2.0 + 2.0*C - D / 2.0;"
    "vec3 c = -A/2.0 + C/2.0;"
   	"vec3 d = B;"
    "return a*t3 + b*t2 + c*t + d;"
"}"
"vec3 BicubicHermiteTextureSample(vec2 P){"
	"float c_textureSize = float(textureSize(ourTexture,0).x);"
	"float c_onePixel = (1.0 / c_textureSize);"
	"float c_twoPixels = (2.0 / c_textureSize);"
    "vec2 pixel = P * c_textureSize + 0.5;"
    
    "vec2 frac = fract(pixel);"
    "pixel = floor(pixel) / c_textureSize - vec2(c_onePixel/2.0);"
    
    "vec3 C00 = texture(ourTexture, pixel + vec2(-c_onePixel ,-c_onePixel)).rgb;"
    "vec3 C10 = texture(ourTexture, pixel + vec2( 0.0        ,-c_onePixel)).rgb;"
    "vec3 C20 = texture(ourTexture, pixel + vec2( c_onePixel ,-c_onePixel)).rgb;"
    "vec3 C30 = texture(ourTexture, pixel + vec2( c_twoPixels,-c_onePixel)).rgb;"
    
    "vec3 C01 = texture(ourTexture, pixel + vec2(-c_onePixel , 0.0)).rgb;"
    "vec3 C11 = texture(ourTexture, pixel + vec2( 0.0        , 0.0)).rgb;"
    "vec3 C21 = texture(ourTexture, pixel + vec2( c_onePixel , 0.0)).rgb;"
    "vec3 C31 = texture(ourTexture, pixel + vec2( c_twoPixels, 0.0)).rgb;"    
    
    "vec3 C02 = texture(ourTexture, pixel + vec2(-c_onePixel , c_onePixel)).rgb;"
    "vec3 C12 = texture(ourTexture, pixel + vec2( 0.0        , c_onePixel)).rgb;"
    "vec3 C22 = texture(ourTexture, pixel + vec2( c_onePixel , c_onePixel)).rgb;"
    "vec3 C32 = texture(ourTexture, pixel + vec2( c_twoPixels, c_onePixel)).rgb;"    
    
    "vec3 C03 = texture(ourTexture, pixel + vec2(-c_onePixel , c_twoPixels)).rgb;"
    "vec3 C13 = texture(ourTexture, pixel + vec2( 0.0        , c_twoPixels)).rgb;"
    "vec3 C23 = texture(ourTexture, pixel + vec2( c_onePixel , c_twoPixels)).rgb;"
    "vec3 C33 = texture(ourTexture, pixel + vec2( c_twoPixels, c_twoPixels)).rgb;"    
    
    "vec3 CP0X = CubicHermite(C00, C10, C20, C30, frac.x);"
    "vec3 CP1X = CubicHermite(C01, C11, C21, C31, frac.x);"
    "vec3 CP2X = CubicHermite(C02, C12, C22, C32, frac.x);"
    "vec3 CP3X = CubicHermite(C03, C13, C23, C33, frac.x);"
    
    "return CubicHermite(CP0X, CP1X, CP2X, CP3X, frac.y);"
"}"
"void main(){"
	"FragColor = vec4(lighting_io,1.0);"
	"vec3 texture_color = BicubicHermiteTextureSample(textcoords_io);"
	"if(texture(ourTexture,textcoords_io).a > 0.5)"
		"discard;"
	"FragColor.rgb *= texture_color;"
"}";  

static char *fragment_texture_lighting_skybox_source = ""
"#version 330 core\n"
"out vec4 FragColor;"
"in vec2 textcoords_io;"
"in vec3 lighting_io;"

"uniform sampler2D ourTexture;"
"void main(){"
	"FragColor = vec4(lighting_io,1.0);"
	"vec3 texture_color = texture(ourTexture,textcoords_io).rgb;"
	"FragColor.rgb *= texture_color;"
"}";  

int g_smaa_max;

static unsigned hdr_fbo;
static bool hdr = false;
static int anti_aliasing;
bool g_vsync = true;

static bool modern_gl;

structure(ShaderProgram){
	unsigned vertex_shader;
	unsigned fragment_shader;
	unsigned id;
	unsigned* vao;
};

static ShaderProgram shader_program;
static ShaderProgram shader_lighting_program;
static ShaderProgram shader_texture_lighting_program;
static ShaderProgram shader_circle_program;
static ShaderProgram shader_skybox_program;

static unsigned vao;
static unsigned vao_lighting;
static unsigned vao_lighting_texture;
static unsigned vao_circle;

static unsigned ebo_quad;

structure(VertexLightingTexture){
	float pos[3];
	float texture_pos[2];
	float lighting[3];
};

structure(VertexLighting){
	float pos[3];
	float lighting[3];
};

structure(Vertex){
	float pos[3];
};

structure(VertexCircle){
	float pos[3];
	float coordinates[2];
	float lighting[3];
};

static bool cstringInString(char* haystack,char* needle){
    for(;*haystack;++haystack){
        char* h = haystack;
        char* n = needle;

        while(*h && *n && *h == *n){
            h += 1;
            n += 1;
        }
        if(!*n)
            return true;
    }
    return false;
}

static ShaderProgram* current_shaderprogram;
static unsigned current_texture;

static int quad_indices[0x1000];
static char vertex_buffer[countof(quad_indices)];
static int vertex_buffer_ptr;
static GlDrawType buffer_drawtype;

static void batchDraw(void){
	if(!vertex_buffer_ptr)
		return;
	if(!current_shaderprogram->vao)
		return;
	GlBindVertexArray(*current_shaderprogram->vao);
	if(*current_shaderprogram->vao == vao_lighting_texture)
		GlBindTexture(GL_TEXTURE_2D,current_texture);
	else
		GlBindTexture(GL_TEXTURE_2D,0);
	GlUseProgram(current_shaderprogram->id);
	GlBufferData(GL_ARRAY_BUFFER,vertex_buffer_ptr,vertex_buffer,GL_DYNAMIC_DRAW);
	GlBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo_quad);
	if(buffer_drawtype == GL_TRIANGLES)
		GlDrawElements(buffer_drawtype,vertex_buffer_ptr,GL_UNSIGNED_INT,0);
	else
		GlDrawArrays(buffer_drawtype,0,vertex_buffer_ptr);
	vertex_buffer_ptr = 0;
}

void deleteTextureGL(unsigned texture){
	if(!texture)
		return;
	GlDeleteTextures(1,&texture);
}

void drawColoredPolygonGL(DrawSurface surface,Vec2* coordinats,Vec3* color,int n_point){
	if(modern_gl){
		if(current_shaderprogram != &shader_lighting_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLighting) * 4 || buffer_drawtype != GL_TRIANGLES){
			batchDraw();
			current_shaderprogram = &shader_lighting_program;
			buffer_drawtype = GL_TRIANGLES;
		}
		float color_div = FIXED_ONE << 4;
		if(hdr)
			color_div *= 0.33f;
		VertexLighting* vertex = (VertexLighting*)(vertex_buffer + vertex_buffer_ptr);
		vertex[0] = (VertexLighting){
			.pos = {-(float)coordinats[0].y / FIXED_ONE,-(float)coordinats[0].x / FIXED_ONE,1.0f},
			.lighting = {(float)color[0].z / color_div,(float)color[0].y / color_div,(float)color[0].x / color_div}
		};
		vertex[1] = (VertexLighting){
			.pos = {-(float)coordinats[1].y / FIXED_ONE,-(float)coordinats[1].x / FIXED_ONE,1.0f},
			.lighting = {(float)color[1].z / color_div,(float)color[1].y / color_div,(float)color[1].x / color_div}
		};
		vertex[2] = (VertexLighting){
			.pos = {-(float)coordinats[2].y / FIXED_ONE,-(float)coordinats[2].x / FIXED_ONE,1.0f},
			.lighting = {(float)color[2].z / color_div,(float)color[2].y / color_div,(float)color[2].x / color_div}
		};
		vertex[3] = (VertexLighting){
			.pos = {-(float)coordinats[3].y / FIXED_ONE,-(float)coordinats[3].x / FIXED_ONE,1.0f},
			.lighting = {(float)color[3].z / color_div,(float)color[3].y / color_div,(float)color[3].x / color_div}
		};
		vertex_buffer_ptr += sizeof(VertexLighting) * 4;
	}
	else{
		GlBegin(GL_QUADS);
		GlColor3b(convertColor(color[0].x),convertColor(color[0].y),convertColor(color[0].z));
		GlVertex2f((float)coordinats[0].y / FIXED_ONE,-(float)coordinats[0].x / FIXED_ONE);

		GlColor3b(convertColor(color[1].x),convertColor(color[1].y),convertColor(color[1].z));
		GlVertex2f((float)coordinats[1].y / FIXED_ONE,-(float)coordinats[1].x / FIXED_ONE);

		GlColor3b(convertColor(color[2].x),convertColor(color[2].y),convertColor(color[2].z));
		GlVertex2f((float)coordinats[2].y / FIXED_ONE,-(float)coordinats[2].x / FIXED_ONE);

		GlColor3b(convertColor(color[3].x),convertColor(color[3].y),convertColor(color[3].z));
		GlVertex2f((float)coordinats[3].y / FIXED_ONE,-(float)coordinats[3].x / FIXED_ONE);
		GlEnd();
	}
}

void drawPolygonGL(DrawSurface surface,Vec2* coordinats,int n_point,Vec3 color){
	if(modern_gl){
		Vec3 gl_color[] = {
			color,
			color,
			color,
			color,
		};
		drawColoredPolygonGL(surface,coordinats,gl_color,n_point);
	}
	else{
		GlBegin(GL_QUADS);
		GlColor3b(convertColor(color.x),convertColor(color.y),convertColor(color.z));
		GlVertex2f((float)coordinats[0].y / FIXED_ONE,-(float)coordinats[0].x / FIXED_ONE);
		GlVertex2f((float)coordinats[1].y / FIXED_ONE,-(float)coordinats[1].x / FIXED_ONE);
		GlVertex2f((float)coordinats[2].y / FIXED_ONE,-(float)coordinats[2].x / FIXED_ONE);
		GlVertex2f((float)coordinats[3].y / FIXED_ONE,-(float)coordinats[3].x / FIXED_ONE);
		GlEnd();
	}
}

void drawColoredPolygon3dGL(DrawSurface* surface,Vec3* coordinats,Vec3* color){
	if(modern_gl){
		if(current_shaderprogram != &shader_lighting_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLighting) * 4 || buffer_drawtype != GL_TRIANGLES){
			batchDraw();
			current_shaderprogram = &shader_lighting_program;
			buffer_drawtype = GL_TRIANGLES;
		}
		float color_div = FIXED_ONE << 4;
		if(hdr)
			color_div *= 0.33f;
		VertexLighting* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
		vertex[0] = (VertexLighting){
			.pos = {-(float)coordinats[0].y / FIXED_ONE,-(float)coordinats[0].x / FIXED_ONE,(float)coordinats[0].z / FIXED_ONE},
			.lighting = {(float)color[0].z / color_div,(float)color[0].y / color_div,(float)color[0].x / color_div},
		};
		vertex[1] = (VertexLighting){
			.pos = {-(float)coordinats[1].y / FIXED_ONE,-(float)coordinats[1].x / FIXED_ONE,(float)coordinats[1].z / FIXED_ONE},
			.lighting = {(float)color[1].z / color_div,(float)color[1].y / color_div,(float)color[1].x / color_div},
		};
		vertex[2] = (VertexLighting){
			.pos = {-(float)coordinats[2].y / FIXED_ONE,-(float)coordinats[2].x / FIXED_ONE,(float)coordinats[2].z / FIXED_ONE},
			.lighting = {(float)color[2].z / color_div,(float)color[2].y / color_div,(float)color[2].x / color_div},
		};
		vertex[3] = (VertexLighting){
			.pos = {-(float)coordinats[3].y / FIXED_ONE,-(float)coordinats[3].x / FIXED_ONE,(float)coordinats[3].z / FIXED_ONE},
			.lighting = {(float)color[3].z / color_div,(float)color[3].y / color_div,(float)color[3].x / color_div},
		};
		vertex_buffer_ptr += sizeof(VertexLighting) * 4;
	}
	else{
		GlBegin(GL_QUADS);
		GlColor3b(convertColor(color[0].x),convertColor(color[0].y),convertColor(color[0].z));
		GlVertex2f((float)coordinats[0].y / FIXED_ONE,-(float)coordinats[0].x / FIXED_ONE);

		GlColor3b(convertColor(color[1].x),convertColor(color[1].y),convertColor(color[1].z));
		GlVertex2f((float)coordinats[1].y / FIXED_ONE,-(float)coordinats[1].x / FIXED_ONE);

		GlColor3b(convertColor(color[2].x),convertColor(color[2].y),convertColor(color[2].z));
		GlVertex2f((float)coordinats[2].y / FIXED_ONE,-(float)coordinats[2].x / FIXED_ONE);

		GlColor3b(convertColor(color[3].x),convertColor(color[3].y),convertColor(color[3].z));
		GlVertex2f((float)coordinats[3].y / FIXED_ONE,-(float)coordinats[3].x / FIXED_ONE);
		GlEnd();
	}
}

void drawPolygon3dGL(DrawSurface* surface,Vec3* coordinats,Vec3 color){
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
	if(modern_gl){
		Vec3 gl_color[] = {
			color,
			color,
			color,
			color,
		};
		drawColoredPolygon3dGL(surface,d_point,gl_color);
	}
	else{
		GlBegin(GL_QUADS);
		GlColor3b(convertColor(color.x),convertColor(color.y),convertColor(color.z));
		GlVertex2f((float)coordinats[0].y / FIXED_ONE,-(float)coordinats[0].x / FIXED_ONE);
		GlVertex2f((float)coordinats[1].y / FIXED_ONE,-(float)coordinats[1].x / FIXED_ONE);
		GlVertex2f((float)coordinats[2].y / FIXED_ONE,-(float)coordinats[2].x / FIXED_ONE);
		GlVertex2f((float)coordinats[3].y / FIXED_ONE,-(float)coordinats[3].x / FIXED_ONE);
		GlEnd();
	}
}

static bool g_anisotropic;
static float g_anisotropic_max;

static void textureUpdate(Texture* texture){
	int size = texture->size;
	int offset = 0;
	for(int i = 0;size;i++){
		GlTexImage2D(GL_TEXTURE_2D,i,GL_BGRA,size,size,0,GL_BGRA,GL_UNSIGNED_BYTE,texture->pixel_data + offset);
		offset += size * size;
		size /= 2;
	}
}

void textureUpdateGL(Texture* texture){
	if(!texture->gl_id)
		return;
	GlBindTexture(GL_TEXTURE_2D,texture->gl_id);
	textureUpdate(texture);
}

static void textureUpload(Texture* texture){
	GlGenTextures(1,&texture->gl_id);
	GlBindTexture(GL_TEXTURE_2D,texture->gl_id);
	if(modern_gl){
		GlTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_LINEAR);
		GlTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		GlTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
		GlTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	}
	else{
		GlTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
		GlTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	}
		
	if(g_anisotropic)
		GlTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,g_anisotropic_max);
	
	textureUpdate(texture);
}

void drawTexturePolygonGL(DrawSurface surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3 color,int n_point){
	if(!texture->gl_id)
		textureUpload(texture);
	else
		GlBindTexture(GL_TEXTURE_2D,texture->gl_id);

	if(modern_gl){
		if(current_shaderprogram != &shader_texture_lighting_program || current_texture != texture->gl_id || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLightingTexture) * 4 || buffer_drawtype != GL_TRIANGLES){
			batchDraw();
			current_texture = texture->gl_id;
			current_shaderprogram = &shader_texture_lighting_program;
			buffer_drawtype = GL_TRIANGLES;
		}
		float color_div = FIXED_ONE << 4;
		if(hdr)
			color_div *= 0.33f;
		VertexLightingTexture* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
		vertex[0] = (VertexLightingTexture){
			.pos = {-(float)coordinats[0].y / FIXED_ONE,-(float)coordinats[0].x / FIXED_ONE,1.0f},
			.lighting = {(float)color.x / color_div,(float)color.y / color_div,(float)color.z / color_div},
			.texture_pos = {(float)texture_coordinats[0].x / FIXED_ONE,(float)texture_coordinats[0].y / FIXED_ONE}
		};
		vertex[1] = (VertexLightingTexture){
			.pos = {-(float)coordinats[1].y / FIXED_ONE,-(float)coordinats[1].x / FIXED_ONE,1.0f},
			.lighting = {(float)color.x / color_div,(float)color.y / color_div,(float)color.z / color_div},
			.texture_pos = {(float)texture_coordinats[1].x / FIXED_ONE,(float)texture_coordinats[1].y / FIXED_ONE}
		};
		vertex[2] = (VertexLightingTexture){
			.pos = {-(float)coordinats[2].y / FIXED_ONE,-(float)coordinats[2].x / FIXED_ONE,1.0f},
			.lighting = {(float)color.x / color_div,(float)color.y / color_div,(float)color.z / color_div},
			.texture_pos = {(float)texture_coordinats[2].x / FIXED_ONE,(float)texture_coordinats[2].y / FIXED_ONE}
		};
		vertex[3] = (VertexLightingTexture){
			.pos = {-(float)coordinats[3].y / FIXED_ONE,-(float)coordinats[3].x / FIXED_ONE,1.0f},
			.lighting = {(float)color.x / color_div,(float)color.y / color_div,(float)color.z / color_div},
			.texture_pos = {(float)texture_coordinats[3].x / FIXED_ONE,(float)texture_coordinats[3].y / FIXED_ONE}
		};
		vertex_buffer_ptr += sizeof(VertexLightingTexture) * 4;
	}
	else{
		GlEnable(GL_TEXTURE_2D);
		GlBegin(GL_QUADS);
		GlColor3b(convertColor(color.x),convertColor(color.y),convertColor(color.z));
		GlTexCoord2f((float)texture_coordinats[0].x / FIXED_ONE,(float)texture_coordinats[0].y / FIXED_ONE);
		GlVertex2f((float)coordinats[0].y / FIXED_ONE,-(float)coordinats[0].x / FIXED_ONE);

		GlTexCoord2f((float)texture_coordinats[1].x / FIXED_ONE,(float)texture_coordinats[1].y / FIXED_ONE);
		GlVertex2f((float)coordinats[1].y / FIXED_ONE,-(float)coordinats[1].x / FIXED_ONE);

		GlTexCoord2f((float)texture_coordinats[2].x / FIXED_ONE,(float)texture_coordinats[2].y / FIXED_ONE);
		GlVertex2f((float)coordinats[2].y / FIXED_ONE,-(float)coordinats[2].x / FIXED_ONE);

		GlTexCoord2f((float)texture_coordinats[3].x / FIXED_ONE,(float)texture_coordinats[3].y / FIXED_ONE);
		GlVertex2f((float)coordinats[3].y / FIXED_ONE,-(float)coordinats[3].x / FIXED_ONE);
		GlEnd();

		GlDisable(GL_TEXTURE_2D);
	}
}

void drawTexturePolygon3dGL(DrawSurface surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3 color,int n_point){
	if(!texture->gl_id)
		textureUpload(texture);
	else
		GlBindTexture(GL_TEXTURE_2D,texture->gl_id);
	
	if(modern_gl){
		if(current_shaderprogram != &shader_texture_lighting_program || current_texture != texture->gl_id || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLightingTexture) * 4 || buffer_drawtype != GL_TRIANGLES){
			batchDraw();
			current_texture = texture->gl_id;
			current_shaderprogram = &shader_texture_lighting_program;
			buffer_drawtype = GL_TRIANGLES;
		}
		float color_div = FIXED_ONE << 4;
		if(hdr)
			color_div *= 0.33f;
		VertexLightingTexture* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
		vertex[0] = (VertexLightingTexture){
			.pos = {-(float)coordinats[0].y / FIXED_ONE,-(float)coordinats[0].x / FIXED_ONE,(float)coordinats[0].z / FIXED_ONE},
			.lighting = {(float)color.x / color_div,(float)color.y / color_div,(float)color.z / color_div},
			.texture_pos = {(float)texture_coordinats[0].x / FIXED_ONE,(float)texture_coordinats[0].y / FIXED_ONE}
		};
		vertex[1] = (VertexLightingTexture){
			.pos = {-(float)coordinats[1].y / FIXED_ONE,-(float)coordinats[1].x / FIXED_ONE,(float)coordinats[1].z / FIXED_ONE},
			.lighting = {(float)color.x / color_div,(float)color.y / color_div,(float)color.z / color_div},
			.texture_pos = {(float)texture_coordinats[1].x / FIXED_ONE,(float)texture_coordinats[1].y / FIXED_ONE}
		};
		vertex[2] = (VertexLightingTexture){
			.pos = {-(float)coordinats[2].y / FIXED_ONE,-(float)coordinats[2].x / FIXED_ONE,(float)coordinats[2].z / FIXED_ONE},
			.lighting = {(float)color.x / color_div,(float)color.y / color_div,(float)color.z / color_div},
			.texture_pos = {(float)texture_coordinats[2].x / FIXED_ONE,(float)texture_coordinats[2].y / FIXED_ONE}
		};
		vertex[3] = (VertexLightingTexture){
			.pos = {-(float)coordinats[3].y / FIXED_ONE,-(float)coordinats[3].x / FIXED_ONE,(float)coordinats[3].z / FIXED_ONE},
			.lighting = {(float)color.x / color_div,(float)color.y / color_div,(float)color.z / color_div},
			.texture_pos = {(float)texture_coordinats[3].x / FIXED_ONE,(float)texture_coordinats[3].y / FIXED_ONE}
		};
		vertex_buffer_ptr += sizeof(VertexLightingTexture) * 4;
	}
	else{
		GlEnable(GL_TEXTURE_2D);
		GlBegin(GL_QUADS);
		GlColor3b(convertColor(color.x),convertColor(color.y),convertColor(color.z));
		GlTexCoord2f((float)texture_coordinats[0].x / FIXED_ONE,(float)texture_coordinats[0].y / FIXED_ONE);
		GlVertex2f((float)coordinats[0].y / FIXED_ONE,-(float)coordinats[0].x / FIXED_ONE);

		GlTexCoord2f((float)texture_coordinats[1].x / FIXED_ONE,(float)texture_coordinats[1].y / FIXED_ONE);
		GlVertex2f((float)coordinats[1].y / FIXED_ONE,-(float)coordinats[1].x / FIXED_ONE);

		GlTexCoord2f((float)texture_coordinats[2].x / FIXED_ONE,(float)texture_coordinats[2].y / FIXED_ONE);
		GlVertex2f((float)coordinats[2].y / FIXED_ONE,-(float)coordinats[2].x / FIXED_ONE);

		GlTexCoord2f((float)texture_coordinats[3].x / FIXED_ONE,(float)texture_coordinats[3].y / FIXED_ONE);
		GlVertex2f((float)coordinats[3].y / FIXED_ONE,-(float)coordinats[3].x / FIXED_ONE);
		GlEnd();

		GlDisable(GL_TEXTURE_2D);
	}
}

void drawColoredTexturePolygonGL(DrawSurface surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3* color,int n_point){
	if(!texture->gl_id)
		textureUpload(texture);
	if(modern_gl){
		if(current_shaderprogram != &shader_texture_lighting_program || current_texture != texture->gl_id || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLightingTexture) * 4 || buffer_drawtype != GL_TRIANGLES){
			batchDraw();
			current_texture = texture->gl_id;
			current_shaderprogram = &shader_texture_lighting_program;
			buffer_drawtype = GL_TRIANGLES;
		}
		float color_div = FIXED_ONE << 4;
		if(hdr)
			color_div *= 0.33f;
		VertexLightingTexture* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
		vertex[0] = (VertexLightingTexture){
			.pos = {(float)coordinats[0].y / FIXED_ONE,-(float)coordinats[0].x / FIXED_ONE,1.0f},
			.lighting = {(float)color[0].x / color_div,(float)color[0].y / color_div,(float)color[0].z / color_div},
			.texture_pos = {(float)texture_coordinats[0].x / FIXED_ONE,(float)texture_coordinats[0].y / FIXED_ONE}
		};
		vertex[1] = (VertexLightingTexture){
			.pos = {(float)coordinats[1].y / FIXED_ONE,-(float)coordinats[1].x / FIXED_ONE,1.0f},
			.lighting = {(float)color[1].x / color_div,(float)color[1].y / color_div,(float)color[1].z / color_div},
			.texture_pos = {(float)texture_coordinats[1].x / FIXED_ONE,(float)texture_coordinats[1].y / FIXED_ONE}
		};
		vertex[2] = (VertexLightingTexture){
			.pos = {(float)coordinats[2].y / FIXED_ONE,-(float)coordinats[2].x / FIXED_ONE,1.0f},
			.lighting = {(float)color[2].x / color_div,(float)color[2].y / color_div,(float)color[2].z / color_div},
			.texture_pos = {(float)texture_coordinats[2].x / FIXED_ONE,(float)texture_coordinats[2].y / FIXED_ONE}
		};
		vertex[3] = (VertexLightingTexture){
			.pos = {(float)coordinats[3].y / FIXED_ONE,-(float)coordinats[3].x / FIXED_ONE,1.0f},
			.lighting = {(float)color[3].x / color_div,(float)color[3].y / color_div,(float)color[3].z / color_div},
			.texture_pos = {(float)texture_coordinats[3].x / FIXED_ONE,(float)texture_coordinats[3].y / FIXED_ONE}
		};
		vertex_buffer_ptr += sizeof(VertexLightingTexture) * 4;
	}
	else{
		GlEnable(GL_TEXTURE_2D);
		GlBindTexture(GL_TEXTURE_2D,texture->gl_id);
		GlBegin(GL_QUADS);
		GlColor3b(convertColor(color[0].x),convertColor(color[0].y),convertColor(color[0].z));
		GlTexCoord2f((float)texture_coordinats[0].x / FIXED_ONE,(float)texture_coordinats[0].y / FIXED_ONE);
		GlVertex2f((float)coordinats[0].y / FIXED_ONE,-(float)coordinats[0].x / FIXED_ONE);

		GlColor3b(convertColor(color[1].x),convertColor(color[1].y),convertColor(color[1].z));
		GlTexCoord2f((float)texture_coordinats[1].x / FIXED_ONE,(float)texture_coordinats[1].y / FIXED_ONE);
		GlVertex2f((float)coordinats[1].y / FIXED_ONE,-(float)coordinats[1].x / FIXED_ONE);

		GlColor3b(convertColor(color[2].x),convertColor(color[2].y),convertColor(color[2].z));
		GlTexCoord2f((float)texture_coordinats[2].x / FIXED_ONE,(float)texture_coordinats[2].y / FIXED_ONE);
		GlVertex2f((float)coordinats[2].y / FIXED_ONE,-(float)coordinats[2].x / FIXED_ONE);

		GlColor3b(convertColor(color[3].x),convertColor(color[3].y),convertColor(color[3].z));
		GlTexCoord2f((float)texture_coordinats[3].x / FIXED_ONE,(float)texture_coordinats[3].y / FIXED_ONE);
		GlVertex2f((float)coordinats[3].y / FIXED_ONE,-(float)coordinats[3].x / FIXED_ONE);
		GlEnd();
		GlDisable(GL_TEXTURE_2D);
	}
}

static void coloredTexturePolygon3d(DrawSurface surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color,ShaderProgram* shader_program){
	if(!texture->gl_id)
		textureUpload(texture);
	if(modern_gl){
		if(current_shaderprogram != shader_program || current_texture != texture->gl_id || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLightingTexture) * 4 || buffer_drawtype != GL_TRIANGLES){
			batchDraw();
			current_texture = texture->gl_id;
			current_shaderprogram = shader_program;
			buffer_drawtype = GL_TRIANGLES;
		}
		float color_div = FIXED_ONE << 4;
		if(hdr)
			color_div *= 0.25f;
		VertexLightingTexture* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
		vertex[0] = (VertexLightingTexture){
			.pos = {-(float)coordinats[0].y / FIXED_ONE,-(float)coordinats[0].x / FIXED_ONE,(float)coordinats[0].z / FIXED_ONE},
			.lighting = {(float)color[0].z / color_div,(float)color[0].y / color_div,(float)color[0].x / color_div},
			.texture_pos = {(float)texture_coordinats[0].x / FIXED_ONE,(float)texture_coordinats[0].y / FIXED_ONE},
		};
		vertex[1] = (VertexLightingTexture){
			.pos = {-(float)coordinats[1].y / FIXED_ONE,-(float)coordinats[1].x / FIXED_ONE,(float)coordinats[1].z / FIXED_ONE},
			.lighting = {(float)color[1].z / color_div,(float)color[1].y / color_div,(float)color[1].x / color_div},
			.texture_pos = {(float)texture_coordinats[1].x / FIXED_ONE,(float)texture_coordinats[1].y / FIXED_ONE},
		};
		vertex[2] = (VertexLightingTexture){
			.pos = {-(float)coordinats[2].y / FIXED_ONE,-(float)coordinats[2].x / FIXED_ONE,(float)coordinats[2].z / FIXED_ONE},
			.lighting = {(float)color[2].z / color_div,(float)color[2].y / color_div,(float)color[2].x / color_div},
			.texture_pos = {(float)texture_coordinats[2].x / FIXED_ONE,(float)texture_coordinats[2].y / FIXED_ONE},
		};
		vertex[3] = (VertexLightingTexture){
			.pos = {-(float)coordinats[3].y / FIXED_ONE,-(float)coordinats[3].x / FIXED_ONE,(float)coordinats[3].z / FIXED_ONE},
			.lighting = {(float)color[3].z / color_div,(float)color[3].y / color_div,(float)color[3].x / color_div},
			.texture_pos = {(float)texture_coordinats[3].x / FIXED_ONE,(float)texture_coordinats[3].y / FIXED_ONE},
		};
		vertex_buffer_ptr += sizeof(VertexLightingTexture) * 4;
	}
	else{
		GlEnable(GL_TEXTURE_2D);
		GlBindTexture(GL_TEXTURE_2D,texture->gl_id);
		GlBegin(GL_QUADS);
		GlColor3b(convertColor(color[0].x),convertColor(color[0].y),convertColor(color[0].z));
		GlTexCoord2f((float)texture_coordinats[0].x / FIXED_ONE,(float)texture_coordinats[0].y / FIXED_ONE);
		GlVertex4f((float)coordinats[0].y / FIXED_ONE,-(float)coordinats[0].x / FIXED_ONE,0.0f,(float)coordinats[0].z / FIXED_ONE);

		GlColor3b(convertColor(color[1].x),convertColor(color[1].y),convertColor(color[1].z));
		GlTexCoord2f((float)texture_coordinats[1].x / FIXED_ONE,(float)texture_coordinats[1].y / FIXED_ONE);
		GlVertex4f((float)coordinats[1].y / FIXED_ONE,-(float)coordinats[1].x / FIXED_ONE,0.0f,(float)coordinats[1].z / FIXED_ONE);

		GlColor3b(convertColor(color[2].x),convertColor(color[2].y),convertColor(color[2].z));
		GlTexCoord2f((float)texture_coordinats[2].x / FIXED_ONE,(float)texture_coordinats[2].y / FIXED_ONE);
		GlVertex4f((float)coordinats[2].y / FIXED_ONE,-(float)coordinats[2].x / FIXED_ONE,0.0f,(float)coordinats[2].z / FIXED_ONE);

		GlColor3b(convertColor(color[3].x),convertColor(color[3].y),convertColor(color[3].z));
		GlTexCoord2f((float)texture_coordinats[3].x / FIXED_ONE,(float)texture_coordinats[3].y / FIXED_ONE);
		GlVertex4f((float)coordinats[3].y / FIXED_ONE,-(float)coordinats[3].x / FIXED_ONE,0.0f,(float)coordinats[3].z / FIXED_ONE);
		GlEnd();
		GlDisable(GL_TEXTURE_2D);
	}
}

void drawColoredTexturePolygon3dGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color){
	coloredTexturePolygon3d(*surface,texture,texture_coordinats,coordinats,color,&shader_texture_lighting_program);
}

void drawColoredTextureSkyboxPolygon3dGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color){
	coloredTexturePolygon3d(*surface,texture,texture_coordinats,coordinats,color,&shader_skybox_program);
}

void drawLineGL(DrawSurface surface,int x1,int y1,int x2,int y2,Vec3 color){
	if(modern_gl){
		if(current_shaderprogram != &shader_lighting_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLighting) * 2 || buffer_drawtype != GL_LINES){
			batchDraw();
			current_shaderprogram = &shader_lighting_program;
			buffer_drawtype = GL_LINES;
		}
		float color_div = FIXED_ONE << 4;
		VertexLighting* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
		vertex[0] = (VertexLighting){
			.pos = {-(float)y1 / FIXED_ONE,-(float)x1 / FIXED_ONE,1.0f},
			.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div}
		};
		vertex[1] = (VertexLighting){
			.pos = {-(float)y2 / FIXED_ONE,-(float)x2 / FIXED_ONE,1.0f},
			.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div}
		};
		vertex_buffer_ptr += sizeof(VertexLighting) * 2;
	}
	else{
		GlBegin(GL_LINES);
		GlColor3b(convertColor(color.x),convertColor(color.y),convertColor(color.z));
		GlVertex2f((float)y1 / FIXED_ONE,-(float)x1 / FIXED_ONE);
		GlVertex2f((float)y2 / FIXED_ONE,-(float)x2 / FIXED_ONE);
		GlEnd();
	}
}

void drawSegmentGL(DrawSurface surface,int x1,int y1,int x2,int y2,int thickness,Vec3 color){
	if(modern_gl){
		if(current_shaderprogram != &shader_lighting_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLighting) * 4 || buffer_drawtype != GL_TRIANGLES){
			batchDraw();
			current_shaderprogram = &shader_lighting_program;
			buffer_drawtype = GL_TRIANGLES;
		}
		float color_div = FIXED_ONE << 4;
		VertexLighting* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
		Vec2 direction = vec2Direction((Vec2){x1,y1},(Vec2){x2,y2});
		//direction = (Vec2){-direction.y,direction.x};
		Vec2 quad[] = {
			vec2AddR((Vec2){x1,y1},vec2ShrR(vec2MulRS(vec2Rotate(direction,FIXED_ONE / 8 * 3),thickness << 8),8)),
			vec2AddR((Vec2){x1,y1},vec2ShrR(vec2MulRS(vec2Rotate(direction,FIXED_ONE / 8 * 5),thickness << 8),8)),
			vec2AddR((Vec2){x2,y2},vec2ShrR(vec2MulRS(vec2Rotate(direction,FIXED_ONE / 8 * 1),thickness << 8),8)),
			vec2AddR((Vec2){x2,y2},vec2ShrR(vec2MulRS(vec2Rotate(direction,FIXED_ONE / 8 * 7),thickness << 8),8)),
		};
		vertex[0] = (VertexLighting){
			.pos = {(float)quad[0].y / FIXED_ONE,-(float)quad[0].x / FIXED_ONE,1.0f},
			.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
		};
		vertex[1] = (VertexLighting){
			.pos = {(float)quad[1].y / FIXED_ONE,-(float)quad[1].x / FIXED_ONE,1.0f},
			.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div}
		};
		vertex[2] = (VertexLighting){
			.pos = {(float)quad[3].y / FIXED_ONE,-(float)quad[3].x / FIXED_ONE,1.0f},
			.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div}
		};
		vertex[3] = (VertexLighting){
			.pos = {(float)quad[2].y / FIXED_ONE,-(float)quad[2].x / FIXED_ONE,1.0f},
			.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div}
		};
		vertex_buffer_ptr += sizeof(VertexLighting) * 4;
	}
	else{
		GlBegin(GL_LINES);
		GlColor3b(convertColor(color.x),convertColor(color.y),convertColor(color.z));
		GlVertex2f((float)y1 / FIXED_ONE,-(float)x1 / FIXED_ONE);
		GlVertex2f((float)y2 / FIXED_ONE,-(float)x2 / FIXED_ONE);
		GlEnd();
	}
}

void drawSegment3dGL(DrawSurface surface,Vec3* coordinats,int thickness,Vec3 color){
	if(modern_gl){
		if(current_shaderprogram != &shader_lighting_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLighting) * 4 || buffer_drawtype != GL_TRIANGLES){
			batchDraw();
			current_shaderprogram = &shader_lighting_program;
			buffer_drawtype = GL_TRIANGLES;
		}
		float color_div = FIXED_ONE << 4;
		VertexLighting* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
		vertex[0] = (VertexLighting){
			.pos = {-(float)coordinats[0].y / FIXED_ONE,-(float)coordinats[0].x / FIXED_ONE,(float)coordinats[0].z / FIXED_ONE},
			.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
		};
		vertex[1] = (VertexLighting){
			.pos = {-(float)coordinats[1].y / FIXED_ONE,-(float)coordinats[1].x / FIXED_ONE,(float)coordinats[1].z / FIXED_ONE},
			.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
		};
		vertex[2] = (VertexLighting){
			.pos = {-(float)coordinats[2].y / FIXED_ONE,-(float)coordinats[2].x / FIXED_ONE,(float)coordinats[2].z / FIXED_ONE},
			.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
		};
		vertex[3] = (VertexLighting){
			.pos = {-(float)coordinats[3].y / FIXED_ONE,-(float)coordinats[3].x / FIXED_ONE,(float)coordinats[3].z / FIXED_ONE},
			.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
		};
		vertex_buffer_ptr += sizeof(VertexLighting) * 4;
	}
	else{
		/*
		GlBegin(GL_LINES);
		GlColor3b(convertColor(color.x),convertColor(color.y),convertColor(color.z));
		GlVertex2f((float)y1 / FIXED_ONE,-(float)x1 / FIXED_ONE);
		GlVertex2f((float)y2 / FIXED_ONE,-(float)x2 / FIXED_ONE);
		GlEnd();
		*/
	}
}

void drawRectangleGL(DrawSurface surface,int x,int y,int size_x,int size_y,Vec3 color){
	if(modern_gl){
		if(current_shaderprogram != &shader_lighting_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLighting) * 4 || buffer_drawtype != GL_TRIANGLES){
			batchDraw();
			current_shaderprogram = &shader_lighting_program;
			buffer_drawtype = GL_TRIANGLES;
		}
		float color_div = FIXED_ONE << 4;
		VertexLighting* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
		vertex[0] = (VertexLighting){
			.pos = {(float)(y) / FIXED_ONE,-(float)(x) / FIXED_ONE,1.0f},
			.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div}
		};
		vertex[1] = (VertexLighting){
			.pos = {(float)(y + size_y) / FIXED_ONE,-(float)(x) / FIXED_ONE,1.0f},
			.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div}
		};
		vertex[2] = (VertexLighting){
			.pos = {(float)(y + size_y) / FIXED_ONE,-(float)(x + size_x) / FIXED_ONE,1.0f},
			.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div}
		};
		vertex[3] = (VertexLighting){
			.pos = {(float)(y) / FIXED_ONE,-(float)(x + size_x) / FIXED_ONE,1.0f},
			.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div}
		};
		vertex_buffer_ptr += sizeof(VertexLighting) * 4;
	}
	else{
		GlBegin(GL_QUADS);
		GlColor3b(convertColor(color.x),convertColor(color.y),convertColor(color.z));
		GlVertex2f((float)y / FIXED_ONE,-(float)x / FIXED_ONE);
		GlVertex2f((float)(y + size_y) / FIXED_ONE,-(float)x / FIXED_ONE);
		GlVertex2f((float)(y + size_y) / FIXED_ONE,-(float)(x + size_x) / FIXED_ONE);
		GlVertex2f((float)y / FIXED_ONE,-(float)(x + size_x) / FIXED_ONE);
		GlEnd();
	}
}

void drawEllipsesGL(DrawSurface surface,int x,int y,int size_x,int size_y,Vec3 color){
	if(!modern_gl)
		return;
	if(current_shaderprogram != &shader_circle_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexCircle) * 4 || buffer_drawtype != GL_TRIANGLES){
		batchDraw();
		current_shaderprogram = &shader_circle_program;
		buffer_drawtype = GL_TRIANGLES;
	}
	float color_div = FIXED_ONE << 4;
	VertexCircle* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
	vertex[0] = (VertexCircle){
		.pos = {(float)(y - size_y) / FIXED_ONE,-(float)(x - size_x) / FIXED_ONE,1.0f},
		.coordinates = {-1.0f,-1.0f},
		.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
	};
	vertex[1] = (VertexCircle){
		.pos = {(float)(y + size_y) / FIXED_ONE,-(float)(x - size_x) / FIXED_ONE,1.0f},
		.coordinates = {1.0f,-1.0f},
		.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
	};
	vertex[2] = (VertexCircle){
		.pos = {(float)(y + size_y) / FIXED_ONE,-(float)(x + size_x) / FIXED_ONE,1.0f},
		.coordinates = {1.0f,1.0f},
		.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
	};
	vertex[3] = (VertexCircle){
		.pos = {(float)(y - size_y) / FIXED_ONE,-(float)(x + size_x) / FIXED_ONE,1.0f},
		.coordinates = {-1.0f,1.0f},
		.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
	};
	vertex_buffer_ptr += sizeof(VertexCircle) * 4;
}

void drawCircle3dGL(DrawSurface surface,Vec3* coordinates,Vec3 color){
	if(!modern_gl)
		return;
	if(current_shaderprogram != &shader_circle_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexCircle) * 4 || buffer_drawtype != GL_TRIANGLES){
		batchDraw();
		current_shaderprogram = &shader_circle_program;
		buffer_drawtype = GL_TRIANGLES;
	}
	float color_div = FIXED_ONE << 4;
	VertexCircle* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
	vertex[0] = (VertexCircle){
		.pos = {-(float)(coordinates[0].y) / FIXED_ONE,-(float)(coordinates[0].x) / FIXED_ONE,(float)coordinates[0].z / FIXED_ONE},
		.coordinates = {-1.0f,-1.0f},
		.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
	};
	vertex[1] = (VertexCircle){
		.pos = {-(float)(coordinates[1].y) / FIXED_ONE,-(float)(coordinates[1].x) / FIXED_ONE,(float)coordinates[1].z / FIXED_ONE},
		.coordinates = {1.0f,-1.0f},
		.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
	};
	vertex[2] = (VertexCircle){
		.pos = {-(float)(coordinates[2].y) / FIXED_ONE,-(float)(coordinates[2].x) / FIXED_ONE,(float)coordinates[2].z / FIXED_ONE},
		.coordinates = {1.0f,1.0f},
		.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
	};
	vertex[3] = (VertexCircle){
		.pos = {-(float)(coordinates[3].y) / FIXED_ONE,-(float)(coordinates[3].x) / FIXED_ONE,(float)coordinates[3].z / FIXED_ONE},
		.coordinates = {-1.0f,1.0f},
		.lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
	};
	vertex_buffer_ptr += sizeof(VertexCircle) * 4;
}

static void shaderProgramDelete(ShaderProgram shader_program){
	GlDeleteShader(shader_program.fragment_shader);
	GlDeleteShader(shader_program.vertex_shader);
	GlDeleteProgram(shader_program.id);
}

static ShaderProgram shaderProgramCreate(char* vertex_source,char* fragment_source){
	ShaderProgram program = {
		.id = GlCreateProgram(),
		.vertex_shader = GlCreateShader(GL_VERTEX_SHADER),
		.fragment_shader = GlCreateShader(GL_FRAGMENT_SHADER),
	};

	GlShaderSource(program.vertex_shader,1,(const char**)&vertex_source,0);
	GlShaderSource(program.fragment_shader,1,(const char**)&fragment_source,0);

	GlCompileShader(program.vertex_shader);
	GlCompileShader(program.fragment_shader);
	GlAttachShader(program.id,program.vertex_shader);
	GlAttachShader(program.id,program.fragment_shader);
	GlLinkProgram(program.id);
	GlUseProgram(program.id);

	return program;
}

static unsigned g_vbo;

static void modernGlInit(int pxf){
	PixelFormatDescriptor format;
	int context_attributes[] = {
		WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
		WGL_CONTEXT_MINOR_VERSION_ARB, 3,
		0 // End
	};
	DescribePixelFormat(g_surface.window_context,pxf,sizeof format,&format);
	ChoosePixelFormat(g_surface.window_context,&format);
	SetPixelFormat(g_surface.window_context,pxf,&format);
	void* context_arb = WglCreateContextAttribsARB(g_surface.window_context,0,context_attributes);
	WglMakeCurrent(g_surface.window_context,context_arb);

	GlCreateBuffers(1,&g_vbo);
	GlBindBuffer(GL_ARRAY_BUFFER,g_vbo);

	int vertex_size;

	GlGenVertexArrays(1,&vao_lighting_texture);
	GlBindVertexArray(vao_lighting_texture);

	GlEnableVertexAttribArray(0);
	GlEnableVertexAttribArray(1);
	GlEnableVertexAttribArray(2);
	GlEnableVertexAttribArray(3);

	GlVertexAttribPointer(0,3,GL_FLOAT,0,sizeof(VertexLightingTexture),(void*)offsetof(VertexLightingTexture,pos));
	GlVertexAttribPointer(1,2,GL_FLOAT,0,sizeof(VertexLightingTexture),(void*)offsetof(VertexLightingTexture,texture_pos));
	GlVertexAttribPointer(2,3,GL_FLOAT,0,sizeof(VertexLightingTexture),(void*)offsetof(VertexLightingTexture,lighting));

	shader_texture_lighting_program     = shaderProgramCreate(vertex_texture_lighting_source,fragment_texture_lighting_source);
	shader_texture_lighting_program.vao = &vao_lighting_texture;
	
	shader_skybox_program     = shaderProgramCreate(vertex_texture_lighting_source,fragment_texture_lighting_skybox_source);
	shader_skybox_program.vao = &vao_lighting_texture;
	
	vertex_size = sizeof(float) * 3;

	GlGenVertexArrays(1,&vao);
	GlBindVertexArray(vao);

	GlEnableVertexAttribArray(0);
	GlVertexAttribPointer(0,3,GL_FLOAT,0,vertex_size,(void*)0);
	shader_program = shaderProgramCreate(vertex_source,fragment_source);
	shader_program.vao = &vao;

	GlGenVertexArrays(1,&vao_lighting);
	GlBindVertexArray(vao_lighting);

	GlEnableVertexAttribArray(0);
	GlEnableVertexAttribArray(1);
	GlEnableVertexAttribArray(2);

	GlVertexAttribPointer(0,3,GL_FLOAT,0,sizeof(VertexLighting),(void*)offsetof(VertexLighting,pos));
	GlVertexAttribPointer(1,3,GL_FLOAT,0,sizeof(VertexLighting),(void*)offsetof(VertexLighting,lighting));

	shader_lighting_program = shaderProgramCreate(vertex_lighting_source,fragment_lighting_source);
	shader_lighting_program.vao = &vao_lighting;

	GlGenVertexArrays(1,&vao_circle);
	GlBindVertexArray(vao_circle);

	GlEnableVertexAttribArray(0);
	GlVertexAttribPointer(0,3,GL_FLOAT,0,sizeof(VertexCircle),(void*)0);
	GlEnableVertexAttribArray(1);
	GlVertexAttribPointer(1,2,GL_FLOAT,0,sizeof(VertexCircle),(void*)(3 * sizeof(float)));
	GlEnableVertexAttribArray(2);
	GlVertexAttribPointer(2,3,GL_FLOAT,0,sizeof(VertexCircle),(void*)(5 * sizeof(float)));

	shader_circle_program     = shaderProgramCreate(vertex_circle_source,fragment_circle_source);
	shader_circle_program.vao = &vao_circle;

	GlClearColor(0.5f,0.5f,0.5f,1.0f);

	int quad_indicess[] = {
		0,1,2,
		0,3,2   
	};
	   
	for(int i = 0;i < countof(quad_indices);i++)
		quad_indices[i] = quad_indicess[i % countof(quad_indicess)] + i / countof(quad_indicess) * 4;

	GlGenBuffers(1,&ebo_quad);
	GlBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo_quad);
	GlBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof quad_indices,quad_indices,GL_DYNAMIC_DRAW);
}

void vsyncSet(bool value){
	if(WglSwapIntervalEXT)
		WglSwapIntervalEXT(value);
}

bool createSurfaceGL(DrawSurface* surface){
	static void* gl_lib;
	if(!gl_lib){
		gl_lib = LoadLibraryA("opengl32");
		if(!gl_lib){
			print((String)STRING_LITERAL("opengl32 not found\n"));
			return false;
		}
		struct{
			char* name;
			void (**fn_ptr)(void);
		} functions[] =  {
			{.name = "wglGetProcAddress",.fn_ptr = (void(**)(void))&WglGetProcAddress},
			{.name = "wglCreateContext",.fn_ptr = (void(**)(void))&WglCreateContext},
			{.name = "wglDeleteContext",.fn_ptr = (void(**)(void))&WglDeleteContext},
			{.name = "wglMakeCurrent",.fn_ptr = (void(**)(void))&WglMakeCurrent},

			{.name = "glClear",.fn_ptr = (void(**)(void))&GlClear},
			{.name = "glClearColor",.fn_ptr = (void(**)(void))&GlClearColor},
			{.name = "glEnable",.fn_ptr = (void(**)(void))&GlEnable},
			{.name = "glDisable",.fn_ptr = (void(**)(void))&GlDisable},
			{.name = "glAlphaFunc",.fn_ptr = (void(**)(void))&GlAlphaFunc},
			{.name = "glGetError",.fn_ptr = (void(**)(void))&GlGetError},
			{.name = "glPolygonMode",.fn_ptr = (void(**)(void))&GlPolygonMode},
			{.name = "glBegin",.fn_ptr = (void(**)(void))&GlBegin},
			{.name = "glEnd",.fn_ptr = (void(**)(void))&GlEnd},

			{.name = "glVertex2f",.fn_ptr = (void(**)(void))&GlVertex2f},
			{.name = "glVertex4f",.fn_ptr = (void(**)(void))&GlVertex4f},
			{.name = "glColor3b",.fn_ptr = (void(**)(void))&GlVertex4f},
			{.name = "glTexCoord2f",.fn_ptr = (void(**)(void))&GlTexCoord2f},

			{.name = "glGenTextures",.fn_ptr = (void(**)(void))&GlGenTextures},
			{.name = "glDeleteTextures",.fn_ptr = (void(**)(void))&GlDeleteTextures},
			{.name = "glBindTexture",.fn_ptr = (void(**)(void))&GlBindTexture},
			{.name = "glTexImage2D",.fn_ptr = (void(**)(void))&GlTexImage2D},
			{.name = "glTexParameteri",.fn_ptr = (void(**)(void))&GlTexParameteri},
			{.name = "glTexParameterf",.fn_ptr = (void(**)(void))&GlTexParameterf},
			{.name = "glGetFloatv",.fn_ptr = (void(**)(void))&GlGetFloatv},
			{.name = "glGetIntegerv",.fn_ptr = (void(**)(void))&GlGetIntegerv},
			{.name = "glGetString",.fn_ptr = (void(**)(void))&GlGetString},
			{.name = "glViewport",.fn_ptr = (void(**)(void))&GlViewport},

			{.name = "glDrawArrays",.fn_ptr = (void(**)(void))&GlDrawArrays},
			{.name = "glDrawElements",.fn_ptr = (void(**)(void))&GlDrawElements},
			{.name = "glReadPixels",.fn_ptr = (void(**)(void))&GlReadPixels},
		};
		for(int i = countof(functions);i--;){
		    	*functions[i].fn_ptr = (void(*)(void))GetProcAddress(gl_lib,functions[i].name);
			if(!*functions[i].fn_ptr){
				debugPrint("function not found in opengl library: ");
				printNL(stringMake(functions[i].name));
				FreeLibrary(gl_lib);
				gl_lib = 0;
				return false;
			}
		}
	}
	PixelFormatDescriptor pfd = {
		.size = sizeof(PixelFormatDescriptor),
		.version = 1,
		.flags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		.color_bits = 32,
		.depth_bits = 0,
		.layer_type = PFD_MAIN_PLANE,
	};

	PixelFormatDescriptor pfd_t;
	SetPixelFormat(surface->window_context,ChoosePixelFormat(surface->window_context,&pfd),&pfd);
	surface->gl_context = WglCreateContext(surface->window_context);
	WglMakeCurrent(surface->window_context,surface->gl_context);

	const char* gl_version = GlGetString(GL_VERSION);
	if(gl_version[0] == '3' || gl_version[0] == '4')
		modern_gl = true;

	static bool modern_gl_loaded;
	if(!modern_gl_loaded){
		debugPrint((void*)GlGetString(GL_EXTENSIONS));
		struct{
			char* name;
			void (**fn_ptr)(void);
		} functions[] =  {
			{.name = "glCreateBuffers",.fn_ptr = (void(**)(void))&GlCreateBuffers},
			{.name = "glGenBuffers",.fn_ptr = (void(**)(void))&GlGenBuffers},
			{.name = "glDeleteBuffers",.fn_ptr = (void(**)(void))&GlDeleteBuffers},
			{.name = "glBindBuffer",.fn_ptr = (void(**)(void))&GlBindBuffer},

			{.name = "glEnableVertexAttribArray",.fn_ptr = (void(**)(void))&GlEnableVertexAttribArray},
			{.name = "glVertexAttribPointer",.fn_ptr = (void(**)(void))&GlVertexAttribPointer},
			{.name = "glShaderSource",.fn_ptr = (void(**)(void))&GlShaderSource},
			{.name = "glCompileShader",.fn_ptr = (void(**)(void))&GlCompileShader},
			{.name = "glAttachShader",.fn_ptr = (void(**)(void))&GlAttachShader},
			{.name = "glDeleteShader",.fn_ptr = (void(**)(void))&GlDeleteShader},
			{.name = "glLinkProgram",.fn_ptr = (void(**)(void))&GlLinkProgram},
			{.name = "glUseProgram",.fn_ptr = (void(**)(void))&GlUseProgram},
			{.name = "glDeleteProgram",.fn_ptr = (void(**)(void))&GlDeleteProgram},

			{.name = "glCreateProgram",.fn_ptr = (void(**)(void))&GlCreateProgram},
			{.name = "glCreateShader",.fn_ptr = (void(**)(void))&GlCreateShader},
			{.name = "glBufferData",.fn_ptr = (void(**)(void))&GlBufferData},
			{.name = "wglSwapIntervalEXT",.fn_ptr = (void(**)(void))&WglSwapIntervalEXT},

			{.name = "wglChoosePixelFormatARB",.fn_ptr = (void(**)(void))&WglChoosePixelFormatARB},
			{.name = "wglCreateContextAttribsARB",.fn_ptr = (void(**)(void))&WglCreateContextAttribsARB},
			{.name = "glUniform1i",.fn_ptr = (void(**)(void))&GlUniform1i},
			{.name = "glUniform3f",.fn_ptr = (void(**)(void))&GlUniform3f},
			{.name = "glGetUniformLocation",.fn_ptr = (void(**)(void))&GlGetUniformLocation},
			{.name = "glActiveTexture",.fn_ptr = (void(**)(void))&GlActiveTexture},
			{.name = "glGenVertexArrays",.fn_ptr = (void(**)(void))&GlGenVertexArrays},
			{.name = "glDeleteVertexArrays",.fn_ptr = (void(**)(void))&GlDeleteVertexArrays},
			{.name = "glBindVertexArray",.fn_ptr = (void(**)(void))&GlBindVertexArray},
			{.name = "glGenFramebuffers",.fn_ptr = (void(**)(void))&GlGenFramebuffers},

			{.name = "glBindFramebuffer",.fn_ptr = (void(**)(void))&GlBindFramebuffer},
			{.name = "glFramebufferTexture2D",.fn_ptr = (void(**)(void))&GlFramebufferTexture2D},
			{.name = "glGenRenderbuffers",.fn_ptr = (void(**)(void))&GlGenRenderbuffers},
			{.name = "glBlitFramebuffer",.fn_ptr = (void(**)(void))&GlBlitFramebuffer},
			{.name = "glGetStringi",.fn_ptr = (void(**)(void))&GlGetStringi},
		};
		for(int i = countof(functions);i--;){
			*functions[i].fn_ptr = (void(*)(void))WglGetProcAddress(functions[i].name);
			if(!*functions[i].fn_ptr){
				debugPrint("extension function not found in opengl library: ");
				printNL(stringMake(functions[i].name));		        
				return false;
			}
		}
		modern_gl_loaded = true;
	}

	if(modern_gl){
		//query max supported SMAA
		int pxf;
		unsigned numf;
		for(int i = 1;i < 0x100;i <<= 1){
			int px[] = {
				WGL_DRAW_TO_WINDOW_ARB,true,
				WGL_SUPPORT_OPENGL_ARB,true,
				WGL_DOUBLE_BUFFER_ARB,true,
				WGL_PIXEL_TYPE_ARB,WGL_TYPE_RGBA_ARB,
				WGL_COLOR_BITS_ARB,24,
				WGL_DEPTH_BITS_ARB,0,
				WGL_STENCIL_BITS_ARB,0,
				WGL_SAMPLE_BUFFERS_ARB,1,
				WGL_SAMPLES_ARB,i,
				0
			};
			WglChoosePixelFormatARB(surface->window_context,px,0,1,&pxf,&numf);
			if(!numf)
				break;
			g_smaa_max = i;
		}
		if(modern_gl){
			int n;
			GlGetIntegerv(GL_NUM_EXTENSIONS,&n);
			for(int i = 0;i < n;i++) {
				const char* ext = GlGetStringi(GL_EXTENSIONS,i);
				g_anisotropic = cstringInString((char*)ext,"GL_EXT_texture_filter_anisotropic");
				if(g_anisotropic)
					break;
			}
		}
		else{
			const char* ext = GlGetString(GL_EXTENSIONS);
			g_anisotropic = cstringInString((char*)ext,"GL_EXT_texture_filter_anisotropic");
		}
		if(g_anisotropic)
			GlGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&g_anisotropic_max);

		if(hdr){
			int px[] = {
				WGL_DRAW_TO_WINDOW_ARB,true,
				WGL_SUPPORT_OPENGL_ARB,true,
				WGL_DOUBLE_BUFFER_ARB,true,
				WGL_PIXEL_TYPE_ARB,WGL_TYPE_RGBA_FLOAT_ARB,
				WGL_COLOR_BITS_ARB,48,
				WGL_SAMPLE_BUFFERS_ARB,g_smaa_max >= 4,
				WGL_SAMPLES_ARB,g_smaa_max >= 4 ? 4 : 0,
				0
			};
			WglChoosePixelFormatARB(surface->window_context,px,0,1,&pxf,&numf);
		}
		else{
			int px[] = {
				WGL_DRAW_TO_WINDOW_ARB,true,
				WGL_SUPPORT_OPENGL_ARB,true,
				WGL_DOUBLE_BUFFER_ARB,true,
				WGL_PIXEL_TYPE_ARB,WGL_TYPE_RGBA_ARB,
				WGL_COLOR_BITS_ARB,24,
				WGL_SAMPLE_BUFFERS_ARB,g_smaa_max >= 4,
				WGL_SAMPLES_ARB,g_smaa_max >= 4 ? 4 : 0,
				0
			};
			WglChoosePixelFormatARB(surface->window_context,px,0,1,&pxf,&numf);
		}
		//WglMakeCurrent(surface->window_context,WglCreateContext(surface->window_context));

		WglDeleteContext(surface->window_context);
		DestroyWindow(g_window);

		createWindow();
	
		modernGlInit(pxf);
	}
	else{
		GlEnable(GL_ALPHA_TEST);
		GlAlphaFunc(GL_GREATER,0.1f);
	}
	vsyncSet(g_vsync);
}

static void contextGlExit(void){
	GlDeleteBuffers(1,&g_vbo);
	GlDeleteVertexArrays(1,&vao);
	GlDeleteVertexArrays(1,&vao_lighting);
	GlDeleteVertexArrays(1,&vao_lighting_texture);
	shaderProgramDelete(shader_program);
	shaderProgramDelete(shader_lighting_program);
	shaderProgramDelete(shader_texture_lighting_program);
	shaderProgramDelete(shader_lighting_program);
	textureResetGL();

	WglMakeCurrent(0,0);
	WglDeleteContext(g_surface.window_context);
}

void openglPolygonFill(bool fill){	
	GlPolygonMode(GL_FRONT_AND_BACK,fill ? GL_FILL : GL_LINE);
}

#include "../memory.h"

void openglDownloadFramebuffer(Texture texture){
	int dims[4] = {0};
	GlGetIntegerv(GL_VIEWPORT,dims);
	int fb_width = dims[2];
	int fb_height = dims[3];
	int* framebuffer = memoryScratchGet(fb_width * fb_height * sizeof(int));
	GlReadPixels(0,0,fb_width,fb_height,GL_RGBA,GL_UNSIGNED_BYTE,framebuffer);
	for(int i = 0;i < texture.size * texture.size;i++){
		int x = i / texture.size;
		int y = i % texture.size;

		int fx = x * fb_height / texture.size;
		int fy = y * fb_width  / texture.size;

		texture.pixel_data[i] = framebuffer[fx * fb_width + fy];
	}
}

void antiAliasingSetGL(int amount){
	int pxf;
	unsigned numf;
	int context_attributes[] = {
		WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
		WGL_CONTEXT_MINOR_VERSION_ARB, 5,
		0 // End
	};

	contextGlExit();

	ReleaseDC(g_window,g_surface.window_context);

	DestroyWindow(g_window);

	createWindow();
	if(hdr){
		int px[] = {
			WGL_DRAW_TO_WINDOW_ARB,true,
			WGL_SUPPORT_OPENGL_ARB,true,
			WGL_DOUBLE_BUFFER_ARB,true,
			WGL_PIXEL_TYPE_ARB,WGL_TYPE_RGBA_FLOAT_ARB,
			WGL_COLOR_BITS_ARB,48,
			WGL_SAMPLE_BUFFERS_ARB,amount != 1,
			WGL_SAMPLES_ARB,amount == 1 ? 0 : amount,
			0
		};
		WglChoosePixelFormatARB(g_surface.window_context,px,0,1,&pxf,&numf);
	}
	else{
		int px[] = {
			WGL_DRAW_TO_WINDOW_ARB,true,
			WGL_SUPPORT_OPENGL_ARB,true,
			WGL_DOUBLE_BUFFER_ARB,true,
			WGL_PIXEL_TYPE_ARB,WGL_TYPE_RGBA_ARB,
			WGL_COLOR_BITS_ARB,24,
			WGL_SAMPLE_BUFFERS_ARB,amount != 1,
			WGL_SAMPLES_ARB,amount == 1 ? 0 : amount,
			0
		};
		WglChoosePixelFormatARB(g_surface.window_context,px,0,1,&pxf,&numf);
	}

	modernGlInit(pxf);

	if(WglSwapIntervalEXT)
		WglSwapIntervalEXT(true);
}

void destroySurfaceGL(DrawSurface* surface){
	contextGlExit();
	PixelFormatDescriptor pfd = {
		.size = sizeof(PixelFormatDescriptor),
		.version = 1,
		.flags = PFD_DRAW_TO_WINDOW,
		.color_bits = 32,
		.depth_bits = 0,
		.layer_type = PFD_MAIN_PLANE,
	};
	SetPixelFormat(surface->window_context,ChoosePixelFormat(surface->window_context,&pfd),&pfd);
}

void blitSurfaceGL(DrawSurface surface){
	batchDraw();
	SwapBuffers(surface.window_context);
	/*
	float sky_brightness = (float)fixedMulR(FIXED_ONE / 2,g_exposure) / FIXED_ONE;
	GlClearColor(sky_brightness,sky_brightness,sky_brightness,1.0f);
	*/
}

void surfaceClearGL(DrawSurface* surface){
	GlClear(GL_COLOR_BUFFER_BIT);
}

void changeSurfaceSizeGL(DrawSurface* surface,int width,int height){
	GlViewport(0,0,surface->window_width,surface->window_height);
}

void antiAliasingEnableGL(bool enable){
	if(enable)
		GlEnable(GL_MULTISAMPLE);
	else
		GlDisable(GL_MULTISAMPLE);
}
