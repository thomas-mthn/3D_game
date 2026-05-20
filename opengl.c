#include "opengl.h"
#include "opengl_def.h"
#include "main.h"
#include "memory.h"
#include "console.h"
#include "lighting.h"

#include "platform/library.h"

#ifdef __linux__
#include <X11/Xlib.h>
#include "linux/l_main.h"
#endif

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


static int convertColor(int color){
	return tClamp((color >> 13),INT8_MIN,INT8_MAX);
}

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
static ShaderProgram shader_lightmap_texture;
static ShaderProgram shader_lightmap;

static unsigned vao;
static unsigned vao_lighting;
static unsigned vao_lighting_texture;
static unsigned vao_circle;
static unsigned vao_lightmap_texture;
static unsigned vao_lightmap;

static unsigned ebo_quad;

structure(VertexLightmapTexture){
    float pos[3];
    float texture_pos[2];
    int lightmap_index;
    float world_pos[3];
    float lightmap_pos[3];
    float color[3];
    Vec3i normal;
};

structure(VertexLightmap){
    float pos[3];
    int lightmap_index;
    float world_pos[3];
    float lightmap_pos[3];
    float u[3];
    float v[3];
    Vec3i normal;
};

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
	if(*current_shaderprogram->vao == vao_lighting_texture || *current_shaderprogram->vao == vao_lightmap_texture)
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

void drawColoredPolygonGL(DrawSurface* surface,Vec2* coordinats,Vec3* color,int n_point){
    if(current_shaderprogram != &shader_lighting_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLighting) * 4 || buffer_drawtype != GL_TRIANGLES){
        batchDraw();
        current_shaderprogram = &shader_lighting_program;
        buffer_drawtype = GL_TRIANGLES;
    }
    float color_div = FIXED_ONE * 16;
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
	point_2[0] = pointToScreenRenderer(coordinats[0],surface->rotation_matrix,surface->position,g_surface.fov);
	point_2[1] = pointToScreenRenderer(coordinats[1],surface->rotation_matrix,surface->position,g_surface.fov);
	point_2[2] = pointToScreenRenderer(coordinats[2],surface->rotation_matrix,surface->position,g_surface.fov);
	point_2[3] = pointToScreenRenderer(coordinats[3],surface->rotation_matrix,surface->position,g_surface.fov);

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
    float color_div = FIXED_ONE * 16;
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

void deleteTextureGL(unsigned texture){
	if(!texture)
		return;
	glDeleteTextures(1,&texture);
}

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

static unsigned lightmap_texture;

void lightmapUploadGL(void){
    Vec3 camera_position = g_surface.position;
        
    if(IS_FLOAT(real))
        camera_position = vec3MulS(camera_position,0x10000);
    
    glUseProgram(shader_lightmap.id);
    glUniform3f(glGetUniformLocation(shader_lightmap.id,"camera_position"),camera_position.x,camera_position.y,camera_position.z);
    glUseProgram(shader_lightmap_texture.id);
    glUniform3f(glGetUniformLocation(shader_lightmap_texture.id,"camera_position"),camera_position.x,camera_position.y,camera_position.z);
    
    glActiveTexture(GL_TEXTURE0 + 1);
    if(!lightmap_texture){
        glGenTextures(1,&lightmap_texture);
        
        glBindTexture(GL_TEXTURE_2D,lightmap_texture);
    
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);

        glTexImage2D(GL_TEXTURE_2D,0,GL_R32I,0x1000,0x1000,0,GL_RED_INTEGER,GL_INT,0);

        goto cleanup;
    }

    int n_part = 0x4;
    
    int offset = N_LUXEL_CACHE / n_part * (g_time.frame_tick % n_part);

    static struct{
        float  luminance[3];
        uint32 hash;
    } light[N_LUXEL_CACHE];
    
    for(int i = 0;i < countof(light) / n_part;i += 1){
        if((g_luxel_cache[offset + i].n_sample & ~LUXEL_DIRECTSAMPLED) > 0x10){
            light[i].luminance[0] = (float)g_luxel_cache[offset + i].luminance_direct.x / FIXED_ONE / 16;
            light[i].luminance[1] = (float)g_luxel_cache[offset + i].luminance_direct.y / FIXED_ONE / 16;
            light[i].luminance[2] = (float)g_luxel_cache[offset + i].luminance_direct.z / FIXED_ONE / 16;
            light[i].luminance[0] += (float)g_luxel_cache[offset + i].luminance.x / FIXED_ONE / 16;
            light[i].luminance[1] += (float)g_luxel_cache[offset + i].luminance.y / FIXED_ONE / 16;
            light[i].luminance[2] += (float)g_luxel_cache[offset + i].luminance.z / FIXED_ONE / 16;
            light[i].hash = g_luxel_cache[offset + i].hash;
        }
        else if(g_luxel_cache[offset + i].refresh){
            light[i].luminance[0] = (float)g_luxel_cache[offset + i].pre_refresh.x / FIXED_ONE / 16;
            light[i].luminance[1] = (float)g_luxel_cache[offset + i].pre_refresh.y / FIXED_ONE / 16;
            light[i].luminance[2] = (float)g_luxel_cache[offset + i].pre_refresh.z / FIXED_ONE / 16;
            light[i].hash = g_luxel_cache[offset + i].hash;
        }
        else{
            light[i].hash = 0;
        }
    }

    int upload_size = N_LUXEL_CACHE / 0x1000 * 4 / n_part;
    
    glTexSubImage2D(GL_TEXTURE_2D,0,0,upload_size * (g_time.frame_tick % n_part),0x1000,upload_size,GL_RED_INTEGER,GL_INT,light);
 cleanup:
    glActiveTexture(GL_TEXTURE0);
}

static void textureUpload(Texture* texture){
	glGenTextures(1,&texture->gl_id);
	glBindTexture(GL_TEXTURE_2D,texture->gl_id);

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
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
    float color_div = FIXED_ONE * 16;
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
    float color_div = FIXED_ONE * 16;
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
        point_2[0] = pointToScreenRenderer(coordinats[0],surface->rotation_matrix,surface->position,g_surface.fov);
        point_2[1] = pointToScreenRenderer(coordinats[1],surface->rotation_matrix,surface->position,g_surface.fov);
        point_2[2] = pointToScreenRenderer(coordinats[2],surface->rotation_matrix,surface->position,g_surface.fov);
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
	point_2[0] = pointToScreenRenderer(coordinats[0],surface->rotation_matrix,surface->position,g_surface.fov);
	point_2[1] = pointToScreenRenderer(coordinats[1],surface->rotation_matrix,surface->position,g_surface.fov);
	point_2[2] = pointToScreenRenderer(coordinats[2],surface->rotation_matrix,surface->position,g_surface.fov);
	point_2[3] = pointToScreenRenderer(coordinats[3],surface->rotation_matrix,surface->position,g_surface.fov);

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
    float color_div = FIXED_ONE * 16;
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
    float color_div = FIXED_ONE * 16;
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
        point_2[0] = pointToScreenRenderer(coordinats[0],surface->rotation_matrix,surface->position,g_surface.fov);
        point_2[1] = pointToScreenRenderer(coordinats[1],surface->rotation_matrix,surface->position,g_surface.fov);
        point_2[2] = pointToScreenRenderer(coordinats[2],surface->rotation_matrix,surface->position,g_surface.fov);
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
	point_2[0] = pointToScreenRenderer(coordinats[0],surface->rotation_matrix,surface->position,g_surface.fov);
	point_2[1] = pointToScreenRenderer(coordinats[1],surface->rotation_matrix,surface->position,g_surface.fov);
	point_2[2] = pointToScreenRenderer(coordinats[2],surface->rotation_matrix,surface->position,g_surface.fov);
	point_2[3] = pointToScreenRenderer(coordinats[3],surface->rotation_matrix,surface->position,g_surface.fov);

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

void drawLightmapPolygon3dGL(DrawSurface* surface,Vec3* coordinats,int lightmap_index,Vec3 normal,int side){
    float color_div = FIXED_ONE * 16;
    if(hdr)
        color_div *= 0.25f;
    Vec3 point_2[4];
	point_2[0] = pointToScreenRenderer(coordinats[0],surface->rotation_matrix,surface->position,g_surface.fov);
	point_2[1] = pointToScreenRenderer(coordinats[1],surface->rotation_matrix,surface->position,g_surface.fov);
	point_2[2] = pointToScreenRenderer(coordinats[2],surface->rotation_matrix,surface->position,g_surface.fov);
	point_2[3] = pointToScreenRenderer(coordinats[3],surface->rotation_matrix,surface->position,g_surface.fov);

	Vec3 d_point[] = {
		{point_2[0].x,point_2[0].y,point_2[0].z},
		{point_2[1].x,point_2[1].y,point_2[1].z},
		{point_2[3].x,point_2[3].y,point_2[3].z},
		{point_2[2].x,point_2[2].y,point_2[2].z}
	};
    
    if(current_shaderprogram != &shader_lightmap || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLightmap) * 4 || buffer_drawtype != GL_TRIANGLES){
        batchDraw();
        current_shaderprogram = &shader_lightmap;
        buffer_drawtype = GL_TRIANGLES;
    }
    VertexLightmap* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
 
    int g_index[] = {0,1,3,2};

    Vec3 u = vec3NormalToU(normal);
    Vec3 v = vec3Cross(normal,u);
    Vec3i normal_i = {
        IS_FLOAT(real) ? normal.x * 0x10000 : normal.x,
        IS_FLOAT(real) ? normal.y * 0x10000 : normal.y,
        IS_FLOAT(real) ? normal.z * 0x10000 : normal.z,
    };

    for(int i = 4;i--;)
        coordinats[i] = vec3Add(coordinats[i],vec3MulS(normal,REAL_EPSILON));
    
    for(int i = 4;i--;){
        if(IS_FLOAT(real))
            coordinats[g_index[i]] = vec3MulS(coordinats[g_index[i]],FIXED_ONE * 0x10000);
        Vec3 w_pos = {
            .x = tAbs(vec3Dot(coordinats[g_index[i]],u)),
            .y = tAbs(vec3Dot(coordinats[g_index[i]],v)),
            .z = tAbs(vec3Dot(coordinats[g_index[i]],normal)),
        };
        vertex[i] = (VertexLightmap){
            .pos = {-(float)d_point[i].y / FIXED_ONE,-(float)d_point[i].x / FIXED_ONE,(float)d_point[i].z / FIXED_ONE},
            .lightmap_index = lightmap_index,
            .lightmap_pos = {(float)w_pos.x,(float)w_pos.y,(float)w_pos.z},
            .world_pos = {(float)coordinats[g_index[i]].x,(float)coordinats[g_index[i]].y,(float)coordinats[g_index[i]].z},
            .u = {(float)u.x / FIXED_ONE,(float)u.y / FIXED_ONE,(float)u.z / FIXED_ONE},
            .v = {(float)v.x / FIXED_ONE,(float)v.y / FIXED_ONE,(float)v.z / FIXED_ONE},
            .normal = normal_i,
        };
    }
    vertex_buffer_ptr += sizeof(VertexLightmap) * 4;
}

void drawLightmapTexturePolygon3dGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,int lightmap_index,int side,Vec3 color){
    float color_div = FIXED_ONE * 16;
    if(hdr)
        color_div *= 0.25f;
    if(!texture->gl_id)
		textureUpload(texture);
    Vec3 point_2[4];
	point_2[0] = pointToScreenRenderer(coordinats[0],surface->rotation_matrix,surface->position,g_surface.fov);
	point_2[1] = pointToScreenRenderer(coordinats[1],surface->rotation_matrix,surface->position,g_surface.fov);
	point_2[2] = pointToScreenRenderer(coordinats[2],surface->rotation_matrix,surface->position,g_surface.fov);
	point_2[3] = pointToScreenRenderer(coordinats[3],surface->rotation_matrix,surface->position,g_surface.fov);

	Vec3 d_point[] = {
		{point_2[0].x,point_2[0].y,point_2[0].z},
		{point_2[1].x,point_2[1].y,point_2[1].z},
		{point_2[3].x,point_2[3].y,point_2[3].z},
		{point_2[2].x,point_2[2].y,point_2[2].z}
	};
    
    if(current_shaderprogram != &shader_lightmap_texture || current_texture != texture->gl_id || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLightmapTexture) * 4 || buffer_drawtype != GL_TRIANGLES){
        batchDraw();
        current_texture = texture->gl_id;
        current_shaderprogram = &shader_lightmap_texture;
        buffer_drawtype = GL_TRIANGLES;
    }
    VertexLightmapTexture* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
 
    int g_index[] = {0,1,3,2};
    
    for(int i = 4;i--;)
        coordinats[i].a[side >> 1] += side & 1 ? REAL_EPSILON : -REAL_EPSILON;
      
    Vec3 normal = g_normal_table[side];
    Vec3 u = vec3NormalToU(normal);
    Vec3 v = vec3Cross(normal,u);
    Vec3i normal_i= {
        IS_FLOAT(real) ? g_normal_table[side].x * 0x10000 : g_normal_table[side].x,
        IS_FLOAT(real) ? g_normal_table[side].y * 0x10000 : g_normal_table[side].y,
        IS_FLOAT(real) ? g_normal_table[side].z * 0x10000 : g_normal_table[side].z,
    };
    
    for(int i = 4;i--;){
        if(IS_FLOAT(real))
            coordinats[g_index[i]] = vec3MulS(coordinats[g_index[i]],FIXED_ONE * 0x10000);
        Vec3 w_pos = {
            .x = tAbs(vec3Dot(coordinats[g_index[i]],u)),
            .y = tAbs(vec3Dot(coordinats[g_index[i]],v)),
            .z = tAbs(vec3Dot(coordinats[g_index[i]],normal)),
        };
        vertex[i] = (VertexLightmapTexture){
            .pos = {-(float)d_point[i].y / FIXED_ONE,-(float)d_point[i].x / FIXED_ONE,(float)d_point[i].z / FIXED_ONE},
            .texture_pos = {(float)texture_coordinats[i].x / FIXED_ONE,(float)texture_coordinats[i].y / FIXED_ONE},
            .lightmap_index = lightmap_index,
            .lightmap_pos = {(float)w_pos.x,(float)w_pos.y,(float)w_pos.z},
            .world_pos = {(float)coordinats[g_index[i]].x,(float)coordinats[g_index[i]].y,(float)coordinats[g_index[i]].z},
            .color = {(float)color.z / FIXED_ONE,(float)color.y / FIXED_ONE,(float)color.x / FIXED_ONE},
            .normal = normal_i,
        };
    }
    vertex_buffer_ptr += sizeof(VertexLightmapTexture) * 4;
}

void drawColoredTextureSkyboxPolygon3dGL(DrawSurface* surface,Texture* texture,Vec2* texture_coordinats,Vec3* coordinats,Vec3* color,LightmapTree* lightmap){
	coloredTexturePolygon3d(surface,texture,texture_coordinats,coordinats,color,&shader_skybox_program,4);
}

void drawLineGL(DrawSurface* surface,real x1,real y1,real x2,real y2,Vec3 color){
    if(current_shaderprogram != &shader_lighting_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLighting) * 2 || buffer_drawtype != GL_LINES){
        batchDraw();
        current_shaderprogram = &shader_lighting_program;
        buffer_drawtype = GL_LINES;
    }
    float color_div = FIXED_ONE * 16;
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

void drawSegmentGL(DrawSurface* surface,real x1,real y1,real x2,real y2,real thickness,Vec3 color){
    if(current_shaderprogram != &shader_lighting_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLighting) * 4 || buffer_drawtype != GL_TRIANGLES){
        batchDraw();
        current_shaderprogram = &shader_lighting_program;
        buffer_drawtype = GL_TRIANGLES;
    }
    float color_div = FIXED_ONE * 16;
    VertexLighting* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
    Vec2 direction = vec2Direction((Vec2){realShr(x1,8),realShr(y1,8)},(Vec2){realShr(x2,8),realShr(y2,8)});
        
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
    float color_div = FIXED_ONE ;
    VertexLighting* vertex = (void*)(vertex_buffer + vertex_buffer_ptr);
    for(int i = 4;i--;){
        vertex[i] = (VertexLighting){
            .pos = {-(float)coordinats[i].y / FIXED_ONE,-(float)coordinats[i].x / FIXED_ONE,(float)coordinats[i].z / FIXED_ONE},
            .lighting = {(float)color.z / color_div,(float)color.y / color_div,(float)color.x / color_div},
        };
    }
    vertex_buffer_ptr += sizeof(VertexLighting) * 4;
}

void drawRectangleGL(DrawSurface* surface,real x,real y,real size_x,real size_y,Vec3 color){
    if(current_shaderprogram != &shader_lighting_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexLighting) * 4 || buffer_drawtype != GL_TRIANGLES){
        batchDraw();
        current_shaderprogram = &shader_lighting_program;
        buffer_drawtype = GL_TRIANGLES;
    }
    float color_div = FIXED_ONE * 16;
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

void drawEllipsesGL(DrawSurface* surface,real x,real y,real size_x,real size_y,Vec3 color){
    if(!modern_gl)
		return;
	if(current_shaderprogram != &shader_circle_program || vertex_buffer_ptr >= countof(quad_indices) - sizeof(VertexCircle) * 4 || buffer_drawtype != GL_TRIANGLES){
		batchDraw();
		current_shaderprogram = &shader_circle_program;
		buffer_drawtype = GL_TRIANGLES;
	}
	float color_div = FIXED_ONE * 16;
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
	float color_div = FIXED_ONE * 16;
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

#include "platform/storage.h"

static ShaderProgram shaderProgramCreate(String vertex_path,String fragment_path){
	ShaderProgram program = {
		.id = glCreateProgram(),
		.vertex_shader = glCreateShader(GL_VERTEX_SHADER),
		.fragment_shader = glCreateShader(GL_FRAGMENT_SHADER),
	};

    FileContent vertex   = storageFileRead(&g_arena_frame,stringConcat(&g_arena_frame,(String)STRING_LITERAL("shader/"),vertex_path).data);
    FileContent fragment = storageFileRead(&g_arena_frame,stringConcat(&g_arena_frame,(String)STRING_LITERAL("shader/"),fragment_path).data);

	glShaderSource(program.vertex_shader,1,(void*)&vertex.content,(int*)&vertex.size);
	glShaderSource(program.fragment_shader,1,(void*)&fragment.content,(int*)&fragment.size);

	glCompileShader(program.vertex_shader);
	glCompileShader(program.fragment_shader);

    int status;
    
    glGetShaderiv(program.vertex_shader,GL_COMPILE_STATUS,&status);
    if(!status){
        String log = {.size = 512,.data = memoryArenaAllocate(&g_arena_frame,512)}; 
        glGetShaderInfoLog(program.vertex_shader,512,0,log.data);
        print((String)STRING_LITERAL("shader compilation failed:\n\n"));
        printNL(vertex_path);
        print(log);
    }

    glGetShaderiv(program.fragment_shader,GL_COMPILE_STATUS,&status);
    if(!status){
        String log = {.size = 512,.data = memoryArenaAllocate(&g_arena_frame,512)}; 
        glGetShaderInfoLog(program.fragment_shader,512,0,log.data);
        print((String)STRING_LITERAL("shader compilation failed:\n\n"));
        printNL(fragment_path);
        print(log);
    }
    
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

    glActiveTexture(GL_TEXTURE0);
    
	glCreateBuffers(1,&g_vbo);
	glBindBuffer(GL_ARRAY_BUFFER,g_vbo);

	int vertex_size;

	glGenVertexArrays(1,&vao_lighting_texture);
	glBindVertexArray(vao_lighting_texture);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(0,3,GL_FLOAT,0,sizeof(VertexLightingTexture),(void*)offsetof(VertexLightingTexture,pos));
	glVertexAttribPointer(1,2,GL_FLOAT,0,sizeof(VertexLightingTexture),(void*)offsetof(VertexLightingTexture,texture_pos));
	glVertexAttribPointer(2,3,GL_FLOAT,0,sizeof(VertexLightingTexture),(void*)offsetof(VertexLightingTexture,lighting));

	shader_texture_lighting_program     = shaderProgramCreate((String)STRING_LITERAL("texture_lighting.vert"),(String)STRING_LITERAL("texture_lighting.frag"));
	shader_texture_lighting_program.vao = &vao_lighting_texture;

	shader_skybox_program     = shaderProgramCreate((String)STRING_LITERAL("texture_lighting.vert"),(String)STRING_LITERAL("skybox.frag"));
	shader_skybox_program.vao = &vao_lighting_texture;
    
    //lightmap
    glGenVertexArrays(1,&vao_lightmap_texture);
	glBindVertexArray(vao_lightmap_texture);
    
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);
    glEnableVertexAttribArray(5);
    glEnableVertexAttribArray(6);

	glVertexAttribPointer(0,3,GL_FLOAT,0,sizeof(VertexLightmapTexture),(void*)offsetof(VertexLightmapTexture,pos));
	glVertexAttribPointer(1,2,GL_FLOAT,0,sizeof(VertexLightmapTexture),(void*)offsetof(VertexLightmapTexture,texture_pos));
    glVertexAttribIPointer(2,1,GL_INT,sizeof(VertexLightmapTexture),(void*)offsetof(VertexLightmapTexture,lightmap_index));
    glVertexAttribPointer(3,3,GL_FLOAT,0,sizeof(VertexLightmapTexture),(void*)offsetof(VertexLightmapTexture,world_pos));
    glVertexAttribPointer(4,3,GL_FLOAT,0,sizeof(VertexLightmapTexture),(void*)offsetof(VertexLightmapTexture,color));
    glVertexAttribIPointer(5,3,GL_INT,sizeof(VertexLightmapTexture),(void*)offsetof(VertexLightmapTexture,normal));
    glVertexAttribPointer(6,3,GL_FLOAT,0,sizeof(VertexLightmapTexture),(void*)offsetof(VertexLightmapTexture,lightmap_pos));

	shader_lightmap_texture     = shaderProgramCreate((String)STRING_LITERAL("texture_lighting_lightmap.vert"),(String)STRING_LITERAL("texture_lighting_lightmap.frag"));
	shader_lightmap_texture.vao = &vao_lightmap_texture;

    glUniform1i(glGetUniformLocation(shader_lightmap_texture.id,"ourTexture"),0);
    glUniform1i(glGetUniformLocation(shader_lightmap_texture.id,"lightmap"),1);

    glGenVertexArrays(1,&vao_lightmap);
	glBindVertexArray(vao_lightmap);
    
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);
    glEnableVertexAttribArray(5);
    glEnableVertexAttribArray(6);
    
	glVertexAttribPointer(0,3,GL_FLOAT,0,sizeof(VertexLightmap),(void*)offsetof(VertexLightmap,pos));
    glVertexAttribIPointer(1,1,GL_INT,sizeof(VertexLightmap),(void*)offsetof(VertexLightmap,lightmap_index));
    glVertexAttribPointer(2,3,GL_FLOAT,0,sizeof(VertexLightmap),(void*)offsetof(VertexLightmap,world_pos));
    glVertexAttribPointer(3,3,GL_FLOAT,0,sizeof(VertexLightmap),(void*)offsetof(VertexLightmap,u));
    glVertexAttribPointer(4,3,GL_FLOAT,0,sizeof(VertexLightmap),(void*)offsetof(VertexLightmap,v));
    glVertexAttribIPointer(5,3,GL_INT,sizeof(VertexLightmap),(void*)offsetof(VertexLightmap,normal));
    glVertexAttribPointer(6,3,GL_FLOAT,0,sizeof(VertexLightmap),(void*)offsetof(VertexLightmap,lightmap_pos));
    
	shader_lightmap     = shaderProgramCreate((String)STRING_LITERAL("lighting_lightmap.vert"),(String)STRING_LITERAL("lighting_lightmap.frag"));
	shader_lightmap.vao = &vao_lightmap;

    glUniform1i(glGetUniformLocation(shader_lightmap.id,"lightmap"),1);
    //lightmap end
	vertex_size = sizeof(float) * 3;

	glGenVertexArrays(1,&vao);
	glBindVertexArray(vao);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0,3,GL_FLOAT,0,vertex_size,(void*)0);
	shader_program = shaderProgramCreate((String)STRING_LITERAL("vertex.vert"),(String)STRING_LITERAL("fragment.frag"));
	shader_program.vao = &vao;

	glGenVertexArrays(1,&vao_lighting);
	glBindVertexArray(vao_lighting);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(0,3,GL_FLOAT,0,sizeof(VertexLighting),(void*)offsetof(VertexLighting,pos));
	glVertexAttribPointer(1,3,GL_FLOAT,0,sizeof(VertexLighting),(void*)offsetof(VertexLighting,lighting));

	shader_lighting_program = shaderProgramCreate((String)STRING_LITERAL("lighting.vert"),(String)STRING_LITERAL("lighting.frag"));
	shader_lighting_program.vao = &vao_lighting;

	glGenVertexArrays(1,&vao_circle);
	glBindVertexArray(vao_circle);

	glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    
	glVertexAttribPointer(0,3,GL_FLOAT,0,sizeof(VertexCircle),(void*)0);
	glVertexAttribPointer(1,2,GL_FLOAT,0,sizeof(VertexCircle),(void*)(3 * sizeof(float)));
	glVertexAttribPointer(2,3,GL_FLOAT,0,sizeof(VertexCircle),(void*)(5 * sizeof(float)));

	shader_circle_program     = shaderProgramCreate((String)STRING_LITERAL("circle.vert"),(String)STRING_LITERAL("circle.frag"));
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
            {.name = "glTexSubImage2D",.fn_ptr = (funcptr_t*)&glTexSubImage2D},
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
            {.name = "glVertexAttribIPointer",.fn_ptr = (funcptr_t*)&glVertexAttribIPointer},
			{.name = "glShaderSource",.fn_ptr = (funcptr_t*)&glShaderSource},
			{.name = "glCompileShader",.fn_ptr = (funcptr_t*)&glCompileShader},
			{.name = "glAttachShader",.fn_ptr = (funcptr_t*)&glAttachShader},
			{.name = "glDeleteShader",.fn_ptr = (funcptr_t*)&glDeleteShader},
			{.name = "glLinkProgram",.fn_ptr = (funcptr_t*)&glLinkProgram},
			{.name = "glUseProgram",.fn_ptr = (funcptr_t*)&glUseProgram},
			{.name = "glDeleteProgram",.fn_ptr = (funcptr_t*)&glDeleteProgram},
            {.name = "glGetShaderiv",.fn_ptr = (funcptr_t*)&glGetShaderiv},
			{.name = "glGetShaderInfoLog",.fn_ptr = (funcptr_t*)&glGetShaderInfoLog},

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

            {.name = "glTexImage1D",.fn_ptr = (funcptr_t*)&glTexImage1D},

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
