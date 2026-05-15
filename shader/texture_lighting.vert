#version 330 core
layout (location = 0) in vec3 verticles;
layout (location = 1) in vec2 textcoords;
layout (location = 2) in vec3 lighting;

out vec3 lighting_io;
out vec2 textcoords_io;

void main(){
	textcoords_io = textcoords;
	lighting_io = lighting;
	gl_Position = vec4(verticles.xy,0.0,verticles.z);
}