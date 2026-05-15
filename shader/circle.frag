#version 330 core
out vec4 FragColor;

in vec2 coordinates_io;
in vec3 lighting_io;

void main(){
	if(dot(coordinates_io,coordinates_io) > 1.0)
		discard;
	FragColor = vec4(lighting_io,1.0);
}