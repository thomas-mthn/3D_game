#version 330 core
out vec4 FragColor;
in vec2 textcoords_io;
in vec3 lighting_io;

uniform sampler2D ourTexture;
vec3 CubicHermite (vec3 A, vec3 B, vec3 C, vec3 D, float t){
	float t2 = t*t;
    float t3 = t*t*t;
    vec3 a = -A/2.0 + (3.0*B)/2.0 - (3.0*C)/2.0 + D/2.0;
    vec3 b = A - (5.0*B)/2.0 + 2.0*C - D / 2.0;
    vec3 c = -A/2.0 + C/2.0;
   	vec3 d = B;
    return a*t3 + b*t2 + c*t + d;
}

vec3 BicubicHermiteTextureSample(vec2 P){
	float c_textureSize = float(textureSize(ourTexture,0).x);
	float c_onePixel = (1.0 / c_textureSize);
	float c_twoPixels = (2.0 / c_textureSize);
    vec2 pixel = P * c_textureSize + 0.5;
    
    vec2 frac = fract(pixel);
    pixel = floor(pixel) / c_textureSize - vec2(c_onePixel/2.0);
    
    vec3 C00 = texture(ourTexture, pixel + vec2(-c_onePixel ,-c_onePixel)).rgb;
    vec3 C10 = texture(ourTexture, pixel + vec2( 0.0        ,-c_onePixel)).rgb;
    vec3 C20 = texture(ourTexture, pixel + vec2( c_onePixel ,-c_onePixel)).rgb;
    vec3 C30 = texture(ourTexture, pixel + vec2( c_twoPixels,-c_onePixel)).rgb;
    
    vec3 C01 = texture(ourTexture, pixel + vec2(-c_onePixel , 0.0)).rgb;
    vec3 C11 = texture(ourTexture, pixel + vec2( 0.0        , 0.0)).rgb;
    vec3 C21 = texture(ourTexture, pixel + vec2( c_onePixel , 0.0)).rgb;
    vec3 C31 = texture(ourTexture, pixel + vec2( c_twoPixels, 0.0)).rgb;
    
    vec3 C02 = texture(ourTexture, pixel + vec2(-c_onePixel , c_onePixel)).rgb;
    vec3 C12 = texture(ourTexture, pixel + vec2( 0.0        , c_onePixel)).rgb;
    vec3 C22 = texture(ourTexture, pixel + vec2( c_onePixel , c_onePixel)).rgb;
    vec3 C32 = texture(ourTexture, pixel + vec2( c_twoPixels, c_onePixel)).rgb;
    
    vec3 C03 = texture(ourTexture, pixel + vec2(-c_onePixel , c_twoPixels)).rgb;
    vec3 C13 = texture(ourTexture, pixel + vec2( 0.0        , c_twoPixels)).rgb;
    vec3 C23 = texture(ourTexture, pixel + vec2( c_onePixel , c_twoPixels)).rgb;
    vec3 C33 = texture(ourTexture, pixel + vec2( c_twoPixels, c_twoPixels)).rgb;  
    
    vec3 CP0X = CubicHermite(C00, C10, C20, C30, frac.x);
    vec3 CP1X = CubicHermite(C01, C11, C21, C31, frac.x);
    vec3 CP2X = CubicHermite(C02, C12, C22, C32, frac.x);
    vec3 CP3X = CubicHermite(C03, C13, C23, C33, frac.x);
    
    return CubicHermite(CP0X, CP1X, CP2X, CP3X, frac.y);
}

void main(){
	FragColor = vec4(lighting_io,1.0);
	vec3 texture_color = BicubicHermiteTextureSample(textcoords_io);
	if(texture(ourTexture,textcoords_io).a > 0.5)
		discard;
	FragColor.rgb *= texture_color;
}