#version 330 core
layout (location = 0) in vec3 verticles;
layout (location = 1) in vec2 textcoords;
layout (location = 2) in int lightmap_index;
layout (location = 3) in vec3 world_pos;
layout (location = 4) in vec3 u;
layout (location = 5) in vec3 v;

out vec2 textcoords_io;
out vec3 world_pos_io;
flat out vec3 u_io;
flat out vec3 v_io;
flat out int lightmap_index_io;

void main(){
	textcoords_io = textcoords;
    lightmap_index_io = lightmap_index;
    world_pos_io = world_pos;
    u_io = u;
    v_io = v;
	gl_Position = vec4(verticles.xy,0.0,verticles.z);
}