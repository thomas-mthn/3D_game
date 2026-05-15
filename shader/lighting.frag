#version 330 core
out vec4 FragColor;

in vec3 lighting_io;

void main(){
	FragColor = vec4(lighting_io,1.0f);
}