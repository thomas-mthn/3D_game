#include "langext.h"
#include "main.h"
#include "library.h"
#include "opengl.h"
#include "memory.h"
#include "console.h"

#ifdef __linux__
#include <X11/Xlib.h>
#include "linux/l_main.h"
#endif

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

#ifdef __linux__

#define GLX_X_RENDERABLE		0x8012
#define GLX_DRAWABLE_TYPE		0x8010
#define GLX_WINDOW_BIT			0x00000001
#define GLX_RENDER_TYPE			0x8011
#define GLX_RGBA_BIT			0x00000001
#define GLX_X_VISUAL_TYPE		0x22
#define GLX_TRUE_COLOR			0x8002
#define GLX_RED_SIZE		8
#define GLX_GREEN_SIZE		9
#define GLX_BLUE_SIZE		10
#define GLX_ALPHA_SIZE		11
#define GLX_DEPTH_SIZE		12
#define GLX_SAMPLES_ARB                   100001
#define GLX_DOUBLEBUFFER	5
#define GLX_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB     0x2092
#define GLX_CONTEXT_PROFILE_MASK_ARB      0x9126
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001

structure(XVisualInfo){
    Visual *visual;
    VisualID visualid;
    int screen;
    int depth;
    int class;
    unsigned long red_mask;
    unsigned long green_mask;
    unsigned long blue_mask;
    int colormap_size;
    int bits_per_rgb;
};

static XVisualInfo* (*glxGetVisualFromFBConfig)(void* display,void* config);
static void (*glxDestroyContext)(void* display,void* context);
static void (*glxSwapBuffers)(void* display,size_t window);
static void* (*glxChooseFBConfig)(void* display,int screen,int* attribute_list,int* elements);
static void* (*glxCreateContextAttribsARB)(
    void* display,
    void* config,
    void* share_context,
    int direct,
    int* attribute_list
);
static int (*glxMakeCurrent)(void* display,size_t drawable,void* ctx);
static void** (*glxGetFBConfigs)(void* display,int screen,int* n_elements);
static int (*glxGetFBConfigAttrib)(void* display,void* config,int attribute,int* value);
#endif

static funcptr_t (stdcall *openglGetProcAddress)(const char* function_name);
static int (stdcall *wglMakeCurrent)(void* context,void* gl_context);
static void* (stdcall *wglCreateContext)(void* context);
static unsigned (stdcall *wglSwapIntervalEXT)(unsigned status);
static int (stdcall *wglDeleteContext)(void* gl_context);
static int (stdcall *wglChoosePixelFormatARB)(
	void* hdc,
	const int* attribute_list,
	const float* attribute_list_f,
	unsigned max_formats,
	int* formats,
	unsigned* n_formats
);
static void* (stdcall *wglCreateContextAttribsARB)(void* hdc,void* share_context,const int* attribute_list);

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
static int  (stdcall *glGetUniformLocation)(unsigned program,const char* name);
static void (stdcall *glUniform1i)(int loc,int v1);
static void (stdcall *glUniform3f)(int loc,float v1,float v2,float v3);

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
static void (stdcall *glVertexAttribPointer)(unsigned index,int size,unsigned type,unsigned char normalized,unsigned stride,const void *pointer);
static void (stdcall *glShaderSource)(unsigned shader,int count,const char **string,int *length);
static void (stdcall *glCompileShader)(unsigned shader);
static void (stdcall *glAttachShader)(unsigned program,unsigned shader);
static void (stdcall *glDeleteShader)(unsigned shader); 
static void (stdcall *glLinkProgram)(unsigned program);
static void (stdcall *glUseProgram)(unsigned program);
static void (stdcall *glDeleteProgram)(unsigned program);

static char* (stdcall *glGetString)(GetStringName name);
static char* (stdcall *glGetStringi)(GetStringName name,unsigned index);

static void (stdcall *glGenVertexArrays)(int n,unsigned* arrays);
static void (stdcall *glDeleteVertexArrays)(int n,unsigned* arrays);
static void (stdcall *glBindVertexArray)(unsigned array);

static void (stdcall *glBufferData)(unsigned target,unsigned size,const void *data,unsigned usage);

static unsigned (stdcall *glCreateProgram)();
static unsigned (stdcall *glCreateShader)(unsigned shader);

static void (stdcall *glGenFramebuffers)(int n,unsigned* ids);
static void (stdcall *glBindFramebuffer)(int target,unsigned framebuffer);
static void (stdcall *glFramebufferTexture2D)(int target,int attachment,int textarget,unsigned texture,int level);
static void (stdcall *glGenRenderbuffers)(int n,unsigned* renderbuffers);
static void (stdcall *glBlitFramebuffer)(int srcX0,int srcY0,int srcX1,int srcY1,int dstX0,int dstY0,int dstX1,int dstY1,unsigned mask,unsigned filter);

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
static DrawType buffer_drawtype;

static void batchDraw(void){
	if(!vertex_buffer_ptr)
		return;
	if(!current_shaderprogram->vao)
		return;
	glBindVertexArray(*current_shaderprogram->vao);
	if(*current_shaderprogram->vao == vao_lighting_texture)
		glBindTexture(GL_TEXTURE_2D,current_texture);
	else
		glBindTexture(GL_TEXTURE_2D,0);
	glUseProgram(current_shaderprogram->id);
	glBufferData(GL_ARRAY_BUFFER,vertex_buffer_ptr,vertex_buffer,GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo_quad);
	if(buffer_drawtype == GL_TRIANGLES)
		glDrawElements(buffer_drawtype,vertex_buffer_ptr,GL_UNSIGNED_INT,0);
	else
		glDrawArrays(buffer_drawtype,0,vertex_buffer_ptr);
	vertex_buffer_ptr = 0;
}

void deleteTextureGL(unsigned texture){
	if(!texture)
		return;
	glDeleteTextures(1,&texture);
}

void drawColoredPolygonGL(DrawSurface* surface,Vec2* coordinats,Vec3* color,int n_point){
    if(current_shaderprogram != &shader_lighting_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLighting) * 4 || buffer_drawtype != GL_TRIANGLES){
        batchDraw();
        current_shaderprogram = &shader_lighting_program;
        buffer_drawtype = GL_TRIANGLES;
    }
    float color_div = FIXED_ONE << 4;
    if(hdr)
        color_div *= 0.33f;
    VertexLighting* vertex = (VertexLighting*)(vertex_buffer + vertex_buffer_ptr);
    for(int i = 4;i--;){
        vertex[i] = (VertexLighting){
            .pos = {-(float)coordinats[i].y / FIXED_ONE,-(float)coordinats[i].x / FIXED_ONE,1.0f},
            .lighting = {(float)color[i].z / color_div,(float)color[i].y / color_div,(float)color[i].x / color_div}
        };
    }
    vertex_buffer_ptr += sizeof(VertexLighting) * 4;
}

void drawPolygonGL(DrawSurface* surface,Vec2* coordinats,int n_point,Vec3 color){
    Vec3 gl_color[] = {
        color,
        color,
        color,
        color,
    };
    drawColoredPolygonGL(surface,coordinats,gl_color,n_point);
}

void drawColoredPolygon3dGL(DrawSurface* surface,Vec3* coordinats,Vec3* color,LightmapTree* lightmap){
    Vec3 point_2[4];
	point_2[0] = pointToScreenRenderer(coordinats[0],surface->rotation_matrix,surface->position,g_options.fov);
	point_2[1] = pointToScreenRenderer(coordinats[1],surface->rotation_matrix,surface->position,g_options.fov);
	point_2[2] = pointToScreenRenderer(coordinats[2],surface->rotation_matrix,surface->position,g_options.fov);
	point_2[3] = pointToScreenRenderer(coordinats[3],surface->rotation_matrix,surface->position,g_options.fov);

	Vec3 d_point[] = {
		{point_2[0].x,point_2[0].y,point_2[0].z},
		{point_2[1].x,point_2[1].y,point_2[1].z},
		{point_2[3].x,point_2[3].y,point_2[3].z},
		{point_2[2].x,point_2[2].y,point_2[2].z}
	};
    if(current_shaderprogram != &shader_lighting_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLighting) * 4 || buffer_drawtype != GL_TRIANGLES){
        batchDraw();
        current_shaderprogram = &shader_lighting_program;
        buffer_drawtype = GL_TRIANGLES;
    }
    float color_div = FIXED_ONE << 4;
    if(hdr)
        color_div *= 0.33f;
    VertexLighting* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
    for(int i = 4;i--;){
        vertex[i] = (VertexLighting){
            .pos = {-(float)d_point[i].y / FIXED_ONE,-(float)d_point[i].x / FIXED_ONE,(float)d_point[i].z / FIXED_ONE},
            .lighting = {(float)color[i].z / color_div,(float)color[i].y / color_div,(float)color[i].x / color_div},
        };
    }
    vertex_buffer_ptr += sizeof(VertexLighting) * 4;
}

void drawPolygon3dGL(DrawSurface* surface,Vec3* coordinats,Vec3 color){
    Vec3 gl_color[] = {
        color,
        color,
        color,
        color,
    };
    drawColoredPolygon3dGL(surface,coordinats,gl_color,0);
}

static bool anisotropic;
static float anisotropic_max;

static void textureUpdate(Texture* texture){
	int size = texture->size;
	int offset = 0;
	for(int i = 0;size;i++){
		glTexImage2D(GL_TEXTURE_2D,i,GL_BGRA,size,size,0,GL_BGRA,GL_UNSIGNED_BYTE,texture->pixel_data + offset);
		offset += size * size;
		size /= 2;
	}
}

void textureUpdateGL(Texture* texture){
	if(!texture->gl_id)
		return;
	glBindTexture(GL_TEXTURE_2D,texture->gl_id);
	textureUpdate(texture);
}

static void textureUpload(Texture* texture){
	glGenTextures(1,&texture->gl_id);
	glBindTexture(GL_TEXTURE_2D,texture->gl_id);

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
		
	if(anisotropic)
		glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,anisotropic_max);
	
	textureUpdate(texture);
}

void drawTexturePolygonGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3 color,int n_point){
	if(!texture->gl_id)
		textureUpload(texture);
	else
		glBindTexture(GL_TEXTURE_2D,texture->gl_id);

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
    for(int i = 4;i--;){
        vertex[i] = (VertexLightingTexture){
            .pos = {-(float)coordinats[i].y / FIXED_ONE,-(float)coordinats[i].x / FIXED_ONE,1.0f},
            .lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
            .texture_pos = {(float)texture_coordinats[i].x / FIXED_ONE,(float)texture_coordinats[i].y / FIXED_ONE}
        };
    }
    vertex_buffer_ptr += sizeof(VertexLightingTexture) * 4;
}

void drawTexturePolygon3dGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3 color,int n_point){
    float color_div = FIXED_ONE << 4;
    if(hdr)
        color_div *= 0.33f;
    if(!texture->gl_id)
		textureUpload(texture);
	else
		glBindTexture(GL_TEXTURE_2D,texture->gl_id);
    if(n_point != 4){
        batchDraw();
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
        current_texture = texture->gl_id;
        current_shaderprogram = &shader_texture_lighting_program;
        
        glBindVertexArray(*current_shaderprogram->vao);
        glBindTexture(GL_TEXTURE_2D,current_texture);
        glUseProgram(current_shaderprogram->id);
        
        buffer_drawtype = GL_TRIANGLES;
        VertexLightingTexture vertex[3];
        Vec3 point_2[3];
        point_2[0] = pointToScreenRenderer(coordinats[0],surface->rotation_matrix,surface->position,g_options.fov);
        point_2[1] = pointToScreenRenderer(coordinats[1],surface->rotation_matrix,surface->position,g_options.fov);
        point_2[2] = pointToScreenRenderer(coordinats[2],surface->rotation_matrix,surface->position,g_options.fov);
        for(int i = 3;i--;){
            vertex[i] = (VertexLightingTexture){
                .pos = {-(float)point_2[i].y / FIXED_ONE,-(float)point_2[i].x / FIXED_ONE,(float)point_2[i].z / FIXED_ONE},
                .lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
                .texture_pos = {(float)texture_coordinats[i].x / FIXED_ONE,(float)texture_coordinats[i].y / FIXED_ONE}
            };
        }
        vertex_buffer_ptr += sizeof(VertexLightingTexture) * 3;
        
        glBufferData(GL_ARRAY_BUFFER,vertex_buffer_ptr,vertex,GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES,0,3);
        vertex_buffer_ptr = 0;
        return;
    }

	Vec3 point_2[4];
	point_2[0] = pointToScreenRenderer(coordinats[0],surface->rotation_matrix,surface->position,g_options.fov);
	point_2[1] = pointToScreenRenderer(coordinats[1],surface->rotation_matrix,surface->position,g_options.fov);
	point_2[2] = pointToScreenRenderer(coordinats[2],surface->rotation_matrix,surface->position,g_options.fov);
	point_2[3] = pointToScreenRenderer(coordinats[3],surface->rotation_matrix,surface->position,g_options.fov);

	Vec3 d_point[] = {
		{point_2[0].x,point_2[0].y,point_2[0].z},
		{point_2[1].x,point_2[1].y,point_2[1].z},
		{point_2[3].x,point_2[3].y,point_2[3].z},
		{point_2[2].x,point_2[2].y,point_2[2].z}
	};
    
    if(current_shaderprogram != &shader_texture_lighting_program || current_texture != texture->gl_id || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLightingTexture) * 4 || buffer_drawtype != GL_TRIANGLES){
        batchDraw();
        current_texture = texture->gl_id;
        current_shaderprogram = &shader_texture_lighting_program;
        buffer_drawtype = GL_TRIANGLES;
    }
    VertexLightingTexture* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
    for(int i = 4;i--;){
        vertex[i] = (VertexLightingTexture){
            .pos = {-(float)d_point[i].y / FIXED_ONE,-(float)d_point[i].x / FIXED_ONE,(float)d_point[i].z / FIXED_ONE},
            .lighting = {(float)color.x / color_div,(float)color.y / color_div,(float)color.z / color_div},
            .texture_pos = {(float)texture_coordinats[i].x / FIXED_ONE,(float)texture_coordinats[i].y / FIXED_ONE}
        };
    }
    vertex_buffer_ptr += sizeof(VertexLightingTexture) * 4;
}

void drawColoredTexturePolygonGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec2* coordinats,Vec3* color,int n_point){
	if(!texture->gl_id)
		textureUpload(texture);

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
    for(int i = 4;i--;){
        vertex[i] = (VertexLightingTexture){
            .pos = {(float)coordinats[i].y / FIXED_ONE,-(float)coordinats[i].x / FIXED_ONE,1.0f},
            .lighting = {(float)color[i].x / color_div,(float)color[i].y / color_div,(float)color[i].z / color_div},
            .texture_pos = {(float)texture_coordinats[i].x / FIXED_ONE,(float)texture_coordinats[i].y / FIXED_ONE}
        };
    }
    vertex_buffer_ptr += sizeof(VertexLightingTexture) * 4;
}

static void coloredTexturePolygon3d(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color,ShaderProgram* shader_program,int n_vertex){
    float color_div = FIXED_ONE << 4;
    if(hdr)
        color_div *= 0.25f;
    if(!texture->gl_id)
		textureUpload(texture);
    if(n_vertex != 4){
        batchDraw();
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
        current_texture = texture->gl_id;
        current_shaderprogram = &shader_texture_lighting_program;
        
        glBindVertexArray(*current_shaderprogram->vao);
        glBindTexture(GL_TEXTURE_2D,current_texture);
        glUseProgram(current_shaderprogram->id);
        
        buffer_drawtype = GL_TRIANGLES;
        VertexLightingTexture vertex[3];
        Vec3 point_2[3];
        point_2[0] = pointToScreenRenderer(coordinats[0],surface->rotation_matrix,surface->position,g_options.fov);
        point_2[1] = pointToScreenRenderer(coordinats[1],surface->rotation_matrix,surface->position,g_options.fov);
        point_2[2] = pointToScreenRenderer(coordinats[2],surface->rotation_matrix,surface->position,g_options.fov);
        for(int i = 3;i--;){
            vertex[i] = (VertexLightingTexture){
                .pos = {-(float)point_2[i].y / FIXED_ONE,-(float)point_2[i].x / FIXED_ONE,(float)point_2[i].z / FIXED_ONE},
                .lighting = {(float)color[i].x / color_div,(float)color[i].y / color_div,(float)color[i].z / color_div},
                .texture_pos = {(float)texture_coordinats[i].x / FIXED_ONE,(float)texture_coordinats[i].y / FIXED_ONE}
            };
        }
        vertex_buffer_ptr += sizeof(VertexLightingTexture) * 3;
        
        glBufferData(GL_ARRAY_BUFFER,vertex_buffer_ptr,vertex,GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES,0,3);
        vertex_buffer_ptr = 0;
        return;
    }
    Vec3 point_2[4];
	point_2[0] = pointToScreenRenderer(coordinats[0],surface->rotation_matrix,surface->position,g_options.fov);
	point_2[1] = pointToScreenRenderer(coordinats[1],surface->rotation_matrix,surface->position,g_options.fov);
	point_2[2] = pointToScreenRenderer(coordinats[2],surface->rotation_matrix,surface->position,g_options.fov);
	point_2[3] = pointToScreenRenderer(coordinats[3],surface->rotation_matrix,surface->position,g_options.fov);

	Vec3 d_point[] = {
		{point_2[0].x,point_2[0].y,point_2[0].z},
		{point_2[1].x,point_2[1].y,point_2[1].z},
		{point_2[3].x,point_2[3].y,point_2[3].z},
		{point_2[2].x,point_2[2].y,point_2[2].z}
	};
    
    if(current_shaderprogram != shader_program || current_texture != texture->gl_id || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLightingTexture) * 4 || buffer_drawtype != GL_TRIANGLES){
        batchDraw();
        current_texture = texture->gl_id;
        current_shaderprogram = shader_program;
        buffer_drawtype = GL_TRIANGLES;
    }
    VertexLightingTexture* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
    for(int i = 4;i--;){
        vertex[i] = (VertexLightingTexture){
            .pos = {-(float)d_point[i].y / FIXED_ONE,-(float)d_point[i].x / FIXED_ONE,(float)d_point[i].z / FIXED_ONE},
            .lighting = {(float)color[i].z / color_div,(float)color[i].y / color_div,(float)color[i].x / color_div},
            .texture_pos = {(float)texture_coordinats[i].x / FIXED_ONE,(float)texture_coordinats[i].y / FIXED_ONE},
        };
    }
    vertex_buffer_ptr += sizeof(VertexLightingTexture) * 4;
}

void drawColoredTexturePolygon3dGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color,LightmapTree* lightmap,int n_vertex){
	coloredTexturePolygon3d(surface,texture,texture_coordinats,coordinats,color,&shader_texture_lighting_program,n_vertex);
}

void drawColoredTextureSkyboxPolygon3dGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color,LightmapTree* lightmap){
	coloredTexturePolygon3d(surface,texture,texture_coordinats,coordinats,color,&shader_skybox_program,4);
}

void drawLineGL(DrawSurface* surface,int x1,int y1,int x2,int y2,Vec3 color){
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

void drawSegmentGL(DrawSurface* surface,int x1,int y1,int x2,int y2,int thickness,Vec3 color){
    if(current_shaderprogram != &shader_lighting_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLighting) * 4 || buffer_drawtype != GL_TRIANGLES){
        batchDraw();
        current_shaderprogram = &shader_lighting_program;
        buffer_drawtype = GL_TRIANGLES;
    }
    float color_div = FIXED_ONE << 4;
    VertexLighting* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
    Vec2 direction = vec2Direction((Vec2){x1 << 8,y1 << 8},(Vec2){x2 << 8,y2 << 8});
        
    Vec2 quad[] = {
        vec2Add((Vec2){x1,y1},vec2MulS(vec2Rotate(direction,FIXED_ONE / 8 * 3),thickness)),
        vec2Add((Vec2){x1,y1},vec2MulS(vec2Rotate(direction,FIXED_ONE / 8 * 5),thickness)),
        vec2Add((Vec2){x2,y2},vec2MulS(vec2Rotate(direction,FIXED_ONE / 8 * 7),thickness)),
        vec2Add((Vec2){x2,y2},vec2MulS(vec2Rotate(direction,FIXED_ONE / 8 * 1),thickness)),
    };
    for(int i = 4;i--;){
        vertex[i] = (VertexLighting){
            .pos = {-(float)quad[i].y / FIXED_ONE,-(float)quad[i].x / FIXED_ONE,1.0f},
            .lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
        };
    }
    vertex_buffer_ptr += sizeof(VertexLighting) * 4;
}

void drawSegment3dGL(DrawSurface* surface,Vec3* coordinats,int thickness,Vec3 color){
    if(current_shaderprogram != &shader_lighting_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLighting) * 4 || buffer_drawtype != GL_TRIANGLES){
        batchDraw();
        current_shaderprogram = &shader_lighting_program;
        buffer_drawtype = GL_TRIANGLES;
    }
    float color_div = FIXED_ONE << 4;
    VertexLighting* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
    for(int i = 4;i--;){
        vertex[i] = (VertexLighting){
            .pos = {-(float)coordinats[i].y / FIXED_ONE,-(float)coordinats[i].x / FIXED_ONE,(float)coordinats[i].z / FIXED_ONE},
            .lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
        };
    }
    vertex_buffer_ptr += sizeof(VertexLighting) * 4;
}

void drawRectangleGL(DrawSurface* surface,int x,int y,int size_x,int size_y,Vec3 color){
    if(current_shaderprogram != &shader_lighting_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLighting) * 4 || buffer_drawtype != GL_TRIANGLES){
        batchDraw();
        current_shaderprogram = &shader_lighting_program;
        buffer_drawtype = GL_TRIANGLES;
    }
    float color_div = FIXED_ONE << 4;
    VertexLighting* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
    Vec2 position[] = {{y,x},{y + size_y,x},{y + size_y,x + size_x},{y,x + size_x}};
    for(int i = 4;i--;){
        vertex[i] = (VertexLighting){
            .pos = {-(float)(position[i].x) / FIXED_ONE,-(float)(position[i].y) / FIXED_ONE,1.0f},
            .lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div}
        };
    }
    vertex_buffer_ptr += sizeof(VertexLighting) * 4;
}

void drawEllipsesGL(DrawSurface* surface,int x,int y,int size_x,int size_y,Vec3 color){
    if(!modern_gl)
		return;
	if(current_shaderprogram != &shader_circle_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexCircle) * 4 || buffer_drawtype != GL_TRIANGLES){
		batchDraw();
		current_shaderprogram = &shader_circle_program;
		buffer_drawtype = GL_TRIANGLES;
	}
	float color_div = FIXED_ONE << 4;
	VertexCircle* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
    float gl_coordinates[][2] = {{-1.0f,-1.0f},{1.0f,-1.0f},{1.0f,1.0f},{-1.0f,1.0f}};
    Vec2 position[] = {{y - size_y,x - size_x},{y + size_y,x - size_x},{y + size_y,x + size_x},{y - size_y,x + size_x}};
    for(int i = 4;i--;){
        vertex[i] = (VertexCircle){
            .pos = {-(float)(position[i].x) / FIXED_ONE,-(float)(position[i].y) / FIXED_ONE,1.0f},
            .coordinates = {gl_coordinates[i][0],gl_coordinates[i][1]},
            .lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
        };
    }
	vertex_buffer_ptr += sizeof(VertexCircle) * 4;
}

void drawCircle3dGL(DrawSurface* surface,Vec3* coordinates,Vec3 color){
	if(!modern_gl)
		return;
	if(current_shaderprogram != &shader_circle_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexCircle) * 4 || buffer_drawtype != GL_TRIANGLES){
		batchDraw();
		current_shaderprogram = &shader_circle_program;
		buffer_drawtype = GL_TRIANGLES;
	}
	float color_div = FIXED_ONE << 4;
	VertexCircle* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
    float gl_coordinates[][2] = {{-1.0f,-1.0f},{1.0f,-1.0f},{1.0f,1.0f},{-1.0f,1.0f}};
    for(int i = 4;i--;){
        vertex[i] = (VertexCircle){
            .pos = {-(float)(coordinates[i].y) / FIXED_ONE,-(float)(coordinates[i].x) / FIXED_ONE,(float)coordinates[i].z / FIXED_ONE},
            .coordinates = {gl_coordinates[i][0],gl_coordinates[i][1]},
            .lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
        };
    }
	vertex_buffer_ptr += sizeof(VertexCircle) * 4;
}

static void shaderProgramDelete(ShaderProgram shader_program){
	glDeleteShader(shader_program.fragment_shader);
	glDeleteShader(shader_program.vertex_shader);
	glDeleteProgram(shader_program.id);
}

static ShaderProgram shaderProgramCreate(char* vertex_source,char* fragment_source){
	ShaderProgram program = {
		.id = glCreateProgram(),
		.vertex_shader = glCreateShader(GL_VERTEX_SHADER),
		.fragment_shader = glCreateShader(GL_FRAGMENT_SHADER),
	};

	glShaderSource(program.vertex_shader,1,(const char**)&vertex_source,0);
	glShaderSource(program.fragment_shader,1,(const char**)&fragment_source,0);

	glCompileShader(program.vertex_shader);
	glCompileShader(program.fragment_shader);
	glAttachShader(program.id,program.vertex_shader);
	glAttachShader(program.id,program.fragment_shader);
	glLinkProgram(program.id);
	glUseProgram(program.id);

	return program;
}

static unsigned g_vbo;

static void modernGlInit(int pxf){
#ifdef __linux__
    XDestroyWindow(g_surface.display,g_surface.window);
    int context_attributes[] = {
        GLX_SAMPLES_ARB,g_options.multi_sample,
        GLX_X_RENDERABLE,true,
        GLX_DRAWABLE_TYPE,GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER,true,
        0
    };
    int gl_context_attribute[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB,3,
        GLX_CONTEXT_MINOR_VERSION_ARB,3,
        0
    };
    int fbcount;
    void** fb_configs = glxChooseFBConfig(g_surface.display,g_surface.screen,context_attributes,&fbcount);
    void* fb_config = fb_configs[0];
    
    XVisualInfo* vi = glxGetVisualFromFBConfig(g_surface.display,fb_config);
    XSetWindowAttributes swa = {0};

    swa.colormap = XCreateColormap(
        g_surface.display,
        RootWindow(g_surface.display,g_surface.screen),
        vi->visual,
        AllocNone
    );
    swa.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;
    g_surface.window = XCreateWindow(
        g_surface.display,
        RootWindow(g_surface.display,g_surface.screen),
        10,10,           
        640 * 2,480 * 2,
        0,
        vi->depth,
        InputOutput,
        vi->visual,
        CWColormap | CWEventMask,
        &swa
    );
    linuxWindowInit();
    g_surface.gl_context = glxCreateContextAttribsARB(g_surface.display,fb_config,0,true,gl_context_attribute);
    glxMakeCurrent(g_surface.display,g_surface.window,g_surface.gl_context);
#elif defined(_MSC_VER)
	PixelFormatDescriptor format;
	int context_attributes[] = {
		WGL_CONTEXT_MAJOR_VERSION_ARB,3,
		WGL_CONTEXT_MINOR_VERSION_ARB,3,
		0
	};
	DescribePixelFormat(g_surface.window_context,pxf,sizeof format,&format);
	ChoosePixelFormat(g_surface.window_context,&format);
	SetPixelFormat(g_surface.window_context,pxf,&format);
	void* context_arb = wglCreateContextAttribsARB(g_surface.window_context,0,context_attributes);
	wglMakeCurrent(g_surface.window_context,context_arb);
#endif

    openglPolygonFill(!g_options.gl_wireframe);
    
	glCreateBuffers(1,&g_vbo);
	glBindBuffer(GL_ARRAY_BUFFER,g_vbo);

	int vertex_size;

	glGenVertexArrays(1,&vao_lighting_texture);
	glBindVertexArray(vao_lighting_texture);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);

	glVertexAttribPointer(0,3,GL_FLOAT,0,sizeof(VertexLightingTexture),(void*)offsetof(VertexLightingTexture,pos));
	glVertexAttribPointer(1,2,GL_FLOAT,0,sizeof(VertexLightingTexture),(void*)offsetof(VertexLightingTexture,texture_pos));
	glVertexAttribPointer(2,3,GL_FLOAT,0,sizeof(VertexLightingTexture),(void*)offsetof(VertexLightingTexture,lighting));

	shader_texture_lighting_program     = shaderProgramCreate(vertex_texture_lighting_source,fragment_texture_lighting_source);
	shader_texture_lighting_program.vao = &vao_lighting_texture;
	
	shader_skybox_program     = shaderProgramCreate(vertex_texture_lighting_source,fragment_texture_lighting_skybox_source);
	shader_skybox_program.vao = &vao_lighting_texture;
	
	vertex_size = sizeof(float) * 3;

	glGenVertexArrays(1,&vao);
	glBindVertexArray(vao);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0,3,GL_FLOAT,0,vertex_size,(void*)0);
	shader_program = shaderProgramCreate(vertex_source,fragment_source);
	shader_program.vao = &vao;

	glGenVertexArrays(1,&vao_lighting);
	glBindVertexArray(vao_lighting);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(0,3,GL_FLOAT,0,sizeof(VertexLighting),(void*)offsetof(VertexLighting,pos));
	glVertexAttribPointer(1,3,GL_FLOAT,0,sizeof(VertexLighting),(void*)offsetof(VertexLighting,lighting));

	shader_lighting_program = shaderProgramCreate(vertex_lighting_source,fragment_lighting_source);
	shader_lighting_program.vao = &vao_lighting;

	glGenVertexArrays(1,&vao_circle);
	glBindVertexArray(vao_circle);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0,3,GL_FLOAT,0,sizeof(VertexCircle),(void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1,2,GL_FLOAT,0,sizeof(VertexCircle),(void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2,3,GL_FLOAT,0,sizeof(VertexCircle),(void*)(5 * sizeof(float)));

	shader_circle_program     = shaderProgramCreate(vertex_circle_source,fragment_circle_source);
	shader_circle_program.vao = &vao_circle;

	glClearColor(0.5f,0.5f,0.5f,1.0f);

	int quad_indicess[] = {
		0,1,2,
		0,3,2   
	};
	   
	for(int i = 0;i < countof(quad_indices);i++)
		quad_indices[i] = quad_indicess[i % countof(quad_indicess)] + i / countof(quad_indicess) * 4;

	glGenBuffers(1,&ebo_quad);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo_quad);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof quad_indices,quad_indices,GL_DYNAMIC_DRAW);
}

void vsyncSet(bool value){
	if(wglSwapIntervalEXT)
		wglSwapIntervalEXT(value);
}

static void anisotropicSet(void){
    if(modern_gl){
        int n;
        glGetIntegerv(GL_NUM_EXTENSIONS,&n);
        for(int i = 0;i < n;i++){
            char* ext = glGetStringi(GL_EXTENSIONS,i);
            anisotropic = cstringInString((char*)ext,"GL_EXT_texture_filter_anisotropic");
            if(anisotropic)
                break;
        }
    }
    else{
        char* ext = glGetString(GL_EXTENSIONS);
        anisotropic = cstringInString((char*)ext,"GL_EXT_texture_filter_anisotropic");
    }
    if(anisotropic)
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,&anisotropic_max);
}

bool createSurfaceGL(DrawSurface* surface){
	static void* gl_lib;
	if(!gl_lib){
#ifdef __linux__
        gl_lib = libraryLoad("libGL.so.1");
#else
		gl_lib = libraryLoad("opengl32");
#endif
		if(!gl_lib){
			print((String)STRING_LITERAL("opengl library not found\n"));
			return false;
		}
		struct{
			char* name;
			funcptr_t* fn_ptr;
		} functions[] = {
#ifdef _MSC_VER
			{.name = "wglGetProcAddress",.fn_ptr = (funcptr_t*)&openglGetProcAddress},
			{.name = "wglCreateContext",.fn_ptr = (funcptr_t*)&wglCreateContext},
			{.name = "wglDeleteContext",.fn_ptr = (funcptr_t*)&wglDeleteContext},
			{.name = "wglMakeCurrent",.fn_ptr = (funcptr_t*)&wglMakeCurrent},
#elif defined(__linux__)
            {.name = "glXGetProcAddress",.fn_ptr = (funcptr_t*)&openglGetProcAddress},
            {.name = "glXMakeCurrent",.fn_ptr = (funcptr_t*)&glxMakeCurrent},
            {.name = "glXChooseFBConfig",.fn_ptr = (funcptr_t*)&glxChooseFBConfig},
            {.name = "glXSwapBuffers",.fn_ptr = (funcptr_t*)&glxSwapBuffers},
            {.name = "glXDestroyContext",.fn_ptr = (funcptr_t*)&glxDestroyContext},
            {.name = "glXGetVisualFromFBConfig",.fn_ptr = (funcptr_t*)&glxGetVisualFromFBConfig},
            {.name = "glXGetFBConfigs",.fn_ptr = (funcptr_t*)&glxGetFBConfigs},
            {.name = "glXGetFBConfigAttrib",.fn_ptr = (funcptr_t*)&glxGetFBConfigAttrib},
#endif
			{.name = "glClear",.fn_ptr = (funcptr_t*)&glClear},
			{.name = "glClearColor",.fn_ptr = (funcptr_t*)&glClearColor},
			{.name = "glEnable",.fn_ptr = (funcptr_t*)&glEnable},
			{.name = "glDisable",.fn_ptr = (funcptr_t*)&glDisable},
			{.name = "glAlphaFunc",.fn_ptr = (funcptr_t*)&glAlphaFunc},
			{.name = "glGetError",.fn_ptr = (funcptr_t*)&glGetError},
			{.name = "glPolygonMode",.fn_ptr = (funcptr_t*)&glPolygonMode},
			{.name = "glBegin",.fn_ptr = (funcptr_t*)&glBegin},
			{.name = "glEnd",.fn_ptr = (funcptr_t*)&glEnd},

			{.name = "glVertex2f",.fn_ptr = (funcptr_t*)&glVertex2f},
			{.name = "glVertex4f",.fn_ptr = (funcptr_t*)&glVertex4f},
			{.name = "glColor3b",.fn_ptr = (funcptr_t*)&glVertex4f},
			{.name = "glTexCoord2f",.fn_ptr = (funcptr_t*)&glTexCoord2f},

			{.name = "glGenTextures",.fn_ptr = (funcptr_t*)&glGenTextures},
			{.name = "glDeleteTextures",.fn_ptr = (funcptr_t*)&glDeleteTextures},
			{.name = "glBindTexture",.fn_ptr = (funcptr_t*)&glBindTexture},
			{.name = "glTexImage2D",.fn_ptr = (funcptr_t*)&glTexImage2D},
			{.name = "glTexParameteri",.fn_ptr = (funcptr_t*)&glTexParameteri},
			{.name = "glTexParameterf",.fn_ptr = (funcptr_t*)&glTexParameterf},
			{.name = "glGetFloatv",.fn_ptr = (funcptr_t*)&glGetFloatv},
			{.name = "glGetIntegerv",.fn_ptr = (funcptr_t*)&glGetIntegerv},
			{.name = "glGetString",.fn_ptr = (funcptr_t*)&glGetString},
			{.name = "glViewport",.fn_ptr = (funcptr_t*)&glViewport},

			{.name = "glDrawArrays",.fn_ptr = (funcptr_t*)&glDrawArrays},
			{.name = "glDrawElements",.fn_ptr = (funcptr_t*)&glDrawElements},
			{.name = "glReadPixels",.fn_ptr = (funcptr_t*)&glReadPixels},
		};
		for(int i = countof(functions);i--;){
            *functions[i].fn_ptr = libraryFunctionLoad(gl_lib,functions[i].name);
			if(!*functions[i].fn_ptr){
				debugPrint("function not found in opengl library: ");
				printNL(stringMake(functions[i].name));
				libraryUnload(gl_lib);
				gl_lib = 0;
				return false;
			}
		}
	}
#ifdef _MSC_VER
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
	surface->gl_context = wglCreateContext(surface->window_context);
	wglMakeCurrent(surface->window_context,surface->gl_context);

	const char* gl_version = glGetString(GL_VERSION);
	if(gl_version[0] == '3' || gl_version[0] == '4')
		modern_gl = true;
#endif
	static bool modern_gl_loaded;
	if(!modern_gl_loaded){
		//debugPrint((void*)glGetString(GL_EXTENSIONS));
		struct{
			char* name;
			funcptr_t* fn_ptr;
		} functions[] =  {
#ifdef _MSC_VER
            {.name = "wglChoosePixelFormatARB",.fn_ptr = (funcptr_t*)&wglChoosePixelFormatARB},
			{.name = "wglCreateContextAttribsARB",.fn_ptr = (funcptr_t*)&wglCreateContextAttribsARB},
#elif __linux__
			{.name = "glXCreateContextAttribsARB",.fn_ptr = (funcptr_t*)&glxCreateContextAttribsARB},
#endif
            {.name = "glCreateBuffers",.fn_ptr = (funcptr_t*)&glCreateBuffers},
			{.name = "glGenBuffers",.fn_ptr = (funcptr_t*)&glGenBuffers},
			{.name = "glDeleteBuffers",.fn_ptr = (funcptr_t*)&glDeleteBuffers},
			{.name = "glBindBuffer",.fn_ptr = (funcptr_t*)&glBindBuffer},

			{.name = "glEnableVertexAttribArray",.fn_ptr = (funcptr_t*)&glEnableVertexAttribArray},
			{.name = "glVertexAttribPointer",.fn_ptr = (funcptr_t*)&glVertexAttribPointer},
			{.name = "glShaderSource",.fn_ptr = (funcptr_t*)&glShaderSource},
			{.name = "glCompileShader",.fn_ptr = (funcptr_t*)&glCompileShader},
			{.name = "glAttachShader",.fn_ptr = (funcptr_t*)&glAttachShader},
			{.name = "glDeleteShader",.fn_ptr = (funcptr_t*)&glDeleteShader},
			{.name = "glLinkProgram",.fn_ptr = (funcptr_t*)&glLinkProgram},
			{.name = "glUseProgram",.fn_ptr = (funcptr_t*)&glUseProgram},
			{.name = "glDeleteProgram",.fn_ptr = (funcptr_t*)&glDeleteProgram},

			{.name = "glCreateProgram",.fn_ptr = (funcptr_t*)&glCreateProgram},
			{.name = "glCreateShader",.fn_ptr = (funcptr_t*)&glCreateShader},
			{.name = "glBufferData",.fn_ptr = (funcptr_t*)&glBufferData},
			{.name = "wglSwapIntervalEXT",.fn_ptr = (funcptr_t*)&wglSwapIntervalEXT},

			{.name = "glUniform1i",.fn_ptr = (funcptr_t*)&glUniform1i},
			{.name = "glUniform3f",.fn_ptr = (funcptr_t*)&glUniform3f},
			{.name = "glGetUniformLocation",.fn_ptr = (funcptr_t*)&glGetUniformLocation},
			{.name = "glActiveTexture",.fn_ptr = (funcptr_t*)&glActiveTexture},
			{.name = "glGenVertexArrays",.fn_ptr = (funcptr_t*)&glGenVertexArrays},
			{.name = "glDeleteVertexArrays",.fn_ptr = (funcptr_t*)&glDeleteVertexArrays},
			{.name = "glBindVertexArray",.fn_ptr = (funcptr_t*)&glBindVertexArray},
			{.name = "glGenFramebuffers",.fn_ptr = (funcptr_t*)&glGenFramebuffers},

			{.name = "glBindFramebuffer",.fn_ptr = (funcptr_t*)&glBindFramebuffer},
			{.name = "glFramebufferTexture2D",.fn_ptr = (funcptr_t*)&glFramebufferTexture2D},
			{.name = "glGenRenderbuffers",.fn_ptr = (funcptr_t*)&glGenRenderbuffers},
			{.name = "glBlitFramebuffer",.fn_ptr = (funcptr_t*)&glBlitFramebuffer},
			{.name = "glGetStringi",.fn_ptr = (funcptr_t*)&glGetStringi},
		};
		for(int i = countof(functions);i--;){
			*functions[i].fn_ptr = openglGetProcAddress(functions[i].name);
			if(!*functions[i].fn_ptr){
				debugPrint("extension function not found in opengl library: ");
				printNL(stringMake(functions[i].name));		        
				return false;
			}
		}
		modern_gl_loaded = true;
	}
#ifdef __linux__
    modern_gl = true;
    int context_attributes[] = {
        GLX_SAMPLES_ARB,0,
        GLX_X_RENDERABLE,true,
        GLX_DRAWABLE_TYPE,GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER, true,
        GLX_RED_SIZE,8,
        GLX_GREEN_SIZE,8,
        GLX_BLUE_SIZE,8,
        0
    };
    //query max supported SMAA
    for(int i = 1;i < 0x100;i <<= 1){
        context_attributes[1] = i;
        int n_fb;
        glxChooseFBConfig(g_surface.display,g_surface.screen,context_attributes,&n_fb);
        if(!n_fb)
            break;
        g_smaa_max = i;
    }
    modernGlInit(0);
    anisotropicSet();
    return true;
#endif
	if(modern_gl){
		//query max supported SMAA
		int pxf;
		unsigned numf;
        int px[] = {
            WGL_SAMPLES_ARB,0,
            WGL_DRAW_TO_WINDOW_ARB,true,
            WGL_SUPPORT_OPENGL_ARB,true,
            WGL_DOUBLE_BUFFER_ARB,true,
            WGL_PIXEL_TYPE_ARB,WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB,24,
            WGL_SAMPLE_BUFFERS_ARB,1,
            0
        };
		for(int i = 1;i < 0x100;i <<= 1){
            px[1] = i;
			wglChoosePixelFormatARB(surface->window_context,px,0,1,&pxf,&numf);
			if(!numf)
				break;
			g_smaa_max = i;
		}
        anisotropicSet();
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
			wglChoosePixelFormatARB(surface->window_context,px,0,1,&pxf,&numf);
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
			wglChoosePixelFormatARB(surface->window_context,px,0,1,&pxf,&numf);
		}
		//wglMakeCurrent(surface->window_context,wglCreateContext(surface->window_context));

		wglDeleteContext(surface->window_context);
        /*
		DestroyWindow(g_window);

		createWindow();
        */
		modernGlInit(pxf);
	}
	else{
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER,0.1f);
	}
	vsyncSet(g_vsync);
    return true;
}

static void contextGlExit(void){
	glDeleteBuffers(1,&g_vbo);
	glDeleteVertexArrays(1,&vao);
	glDeleteVertexArrays(1,&vao_lighting);
	glDeleteVertexArrays(1,&vao_lighting_texture);
	shaderProgramDelete(shader_program);
	shaderProgramDelete(shader_lighting_program);
	shaderProgramDelete(shader_texture_lighting_program);
	shaderProgramDelete(shader_lighting_program);
	//textureResetGL();
#ifdef _MSC_VER
	wglMakeCurrent(0,0);
	wglDeleteContext(g_surface.window_context);
#elif __linux__
    glxMakeCurrent(g_surface.display,0,0);
    glxDestroyContext(g_surface.display,g_surface.gl_context);
#endif
}

void openglPolygonFill(bool fill){
	glPolygonMode(GL_FRONT_AND_BACK,fill ? GL_FILL : GL_LINE);
}

void openglDownloadFramebuffer(Texture texture){
	int dims[4] = {0};
	glGetIntegerv(GL_VIEWPORT,dims);
	int fb_width = dims[2];
	int fb_height = dims[3];
    int fb_memsize = fb_width * fb_height * sizeof(int);
	int* framebuffer = virtualAllocate(fb_memsize);
	glReadPixels(0,0,fb_width,fb_height,GL_RGBA,GL_UNSIGNED_BYTE,framebuffer);
	for(int i = 0;i < texture.size * texture.size;i++){
		int x = i / texture.size;
		int y = i % texture.size;

		int fx = x * fb_height / texture.size;
		int fy = y * fb_width  / texture.size;

		texture.pixel_data[i] = framebuffer[fx * fb_width + fy];
	}
    virtualFree(framebuffer,fb_memsize);
}

void antiAliasingSetGL(int amount){
	int pxf;
	unsigned numf;
	int context_attributes[] = {
		WGL_CONTEXT_MAJOR_VERSION_ARB,3,
		WGL_CONTEXT_MINOR_VERSION_ARB,3,
		0
	};

	contextGlExit();

#ifdef __linux__
    g_options.multi_sample = amount;
#elif defined(_MSC_VER)
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
		wglChoosePixelFormatARB(g_surface.window_context,px,0,1,&pxf,&numf);
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
		wglChoosePixelFormatARB(g_surface.window_context,px,0,1,&pxf,&numf);
	}
#endif    

	modernGlInit(pxf);

	if(wglSwapIntervalEXT)
		wglSwapIntervalEXT(true);
}

void destroySurfaceGL(DrawSurface* surface){
	contextGlExit();
}

void blitSurfaceGL(DrawSurface* surface){
	batchDraw();
#ifdef __linux__
    glxSwapBuffers(g_surface.display,g_surface.window);
#elif defined(_MSC_VER)
	SwapBuffers(surface->window_context);
#endif
}

void surfaceClearGL(DrawSurface* surface){
	glClear(GL_COLOR_BUFFER_BIT);
}

void changeSurfaceSizeGL(DrawSurface* surface,int width,int height){
    glViewport(0,0,surface->window_width,surface->window_height);
}

void antiAliasingEnableGL(bool enable){
	if(enable)
		glEnable(GL_MULTISAMPLE);
	else
		glDisable(GL_MULTISAMPLE);
}
