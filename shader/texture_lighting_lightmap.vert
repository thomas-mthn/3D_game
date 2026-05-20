#version 330 core
layout (location = 0) in vec3 verticles;
layout (location = 1) in vec2 textcoords;
layout (location = 2) in int lightmap_index;
layout (location = 3) in vec3 world_pos;
layout (location = 4) in vec3 luminance;
layout (location = 5) in ivec3 normal;
layout (location = 6) in vec3 lightmap_pos;

out vec2 textcoords_io;
out vec3 world_pos_io;
out vec3 lightmap_pos_io;
flat out vec3 luminance_io;
flat out int lightmap_index_io;
flat out ivec3 normal_io;

void main(){
	textcoords_io = textcoords;
    lightmap_index_io = lightmap_index;
    world_pos_io = world_pos;
    lightmap_pos_io = lightmap_pos;
    luminance_io = luminance;
    normal_io = normal;
	gl_Position = vec4(verticles.xy,0.0,verticles.z);
}