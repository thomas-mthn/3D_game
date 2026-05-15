#version 330 core
layout (location = 0) in vec3 verticles;
layout (location = 1) in vec2 coordinates;
layout (location = 2) in vec3 lighting;

out vec2 coordinates_io;
out vec3 lighting_io;

void main(){
	lighting_io = lighting;
	coordinates_io = coordinates;
	gl_Position = vec4(verticles.xy,0.0,verticles.z);
}