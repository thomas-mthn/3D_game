#version 330 core
out vec4 FragColor;

uniform sampler2D ourTexture;
uniform vec3 color;

void main(){
	FragColor = vec4(color,1.0);
}