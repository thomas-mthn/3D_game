#version 330 core
layout (location = 0) in vec3 verticles;
layout (location = 1) in vec3 lighting;

out vec3 lighting_io;

void main(){
	lighting_io = lighting;
	gl_Position = vec4(verticles.xy,0.0,verticles.z);
}