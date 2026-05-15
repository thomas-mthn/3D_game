#version 330 core
layout (location = 0) in vec3 verticles;

void main(){
	gl_Position = vec4(verticles.xy,0.0,verticles.z);
}