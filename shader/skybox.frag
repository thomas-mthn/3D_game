#version 330 core
out vec4 FragColor;
in vec2 textcoords_io;
in vec3 lighting_io;

uniform sampler2D ourTexture;
void main(){
	FragColor = vec4(lighting_io,1.0);
	vec3 texture_color = texture(ourTexture,textcoords_io).rgb;
	FragColor.rgb *= texture_color;
}