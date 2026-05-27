#version 330 core
layout (location = 0) in vec3 verticles;
layout (location = 1) in int lightmap_index;
layout (location = 2) in vec3 world_pos;
layout (location = 3) in vec3 color;
layout (location = 4) in ivec3 normal;
layout (location = 5) in vec3 lightmap_pos;
layout (location = 6) in vec3 vnormal;

out vec3 world_pos_io;
out vec3 lightmap_pos_io;
flat out vec3 color_io;
flat out int lightmap_index_io;
flat out ivec3 normal_io;
flat out vec3 vnormal_io;

void main(){
    lightmap_index_io = lightmap_index;
    world_pos_io = world_pos;
    lightmap_pos_io = lightmap_pos;
    color_io = color;
    normal_io = normal;
    vnormal_io = vnormal;
	gl_Position = vec4(verticles.xy,0.0,verticles.z);
}