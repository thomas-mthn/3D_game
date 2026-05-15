#version 330 core
out vec4 FragColor;
in vec2 textcoords_io;
in vec3 world_pos_io;
flat in vec3 u_io;
flat in vec3 v_io;
flat in int lightmap_index_io;

uniform sampler2D ourTexture;
uniform isampler2D lightmap;
uniform vec3 camera_position;

int floatExponent(float x){
    int bits = floatBitsToInt(x);
    return ((bits >> 23) & 0xFF) - 127;
}
    
ivec2 to2D(int index){
    return ivec2(index % 4096,index / 4096);
}

uint hash4(uint x,uint y,uint z,uint w){
    uint h = 0x811C9DC5u;
    
    h ^= x;
	h *= 0x27d4eb2du;
    h ^= y;
	h *= 0x165667b1u;
    h ^= z;
	h *= 0x1b873593u;
    h ^= w;
    h *= 0x85ebca6bu;
    
    h ^= h >> 16;
    return h;
}

uint luxelHashGet(ivec3 position,int depth){
	return hash4(uint(position.x),uint(position.y),uint(position.z),uint(depth));
}

uint luxelGet(uint hash){
	return hash % 0x40000u * 4u;
}

vec3 luxelGetColor(ivec3 world_pos,float d){
    int depth = floatExponent(d);
    depth = max(depth,14);
    
    ivec3 world_pos_s = world_pos >> depth;
    
    int hash_entry;
    uint hash = luxelHashGet(world_pos_s,depth);

    int parent = 0;
    hash_entry = int(luxelGet(hash));
#if 1
    for(int i = 0;i < 4;i++){
        hash_entry = int(luxelGet(hash + uint(i)));
        if(uint(texelFetch(lightmap,to2D(hash_entry + 3),0).r) == hash)
            break;
        if(i == 3){
            if(parent == 2){
                return vec3(0.0,0.0,0.0);
            }
            else{
                parent += 1;
                i = -1;
                hash = luxelHashGet(world_pos >> depth + parent,depth + parent);
            }
        }
    }
#endif
    float red   = intBitsToFloat(texelFetch(lightmap,to2D(hash_entry + 2),0).r);
    float green = intBitsToFloat(texelFetch(lightmap,to2D(hash_entry + 1),0).r);
    float blue  = intBitsToFloat(texelFetch(lightmap,to2D(hash_entry + 0),0).r);
    return vec3(red,green,blue);
}

vec3 CubicHermite(vec3 A,vec3 B,vec3 C,vec3 D,float t){
	float t2 = t * t;
    float t3 = t * t * t;
    vec3 a = -A / 2.0 + (3.0 * B) / 2.0 - (3.0 * C) / 2.0 + D / 2.0;
    vec3 b = A - (5.0 * B) / 2.0 + 2.0 * C - D / 2.0;
    vec3 c = -A / 2.0 + C / 2.0;
   	vec3 d = B;
    return a * t3 + b * t2 + c * t + d;
}

vec3 BicubicHermiteTextureSample(sampler2D texture_2d,vec2 P){
	float c_textureSize = float(textureSize(texture_2d,0).x);
	float c_onePixel = (1.0 / c_textureSize);
	float c_twoPixels = (2.0 / c_textureSize);
    vec2 pixel = P * c_textureSize + 0.5;
    
    vec2 frac = fract(pixel);
    pixel = floor(pixel) / c_textureSize - vec2(c_onePixel/2.0);
    
    vec3 C00 = texture(texture_2d,pixel + vec2(-c_onePixel ,-c_onePixel)).rgb;
    vec3 C10 = texture(texture_2d,pixel + vec2( 0.0        ,-c_onePixel)).rgb;
    vec3 C20 = texture(texture_2d,pixel + vec2( c_onePixel ,-c_onePixel)).rgb;
    vec3 C30 = texture(texture_2d,pixel + vec2( c_twoPixels,-c_onePixel)).rgb;
    
    vec3 C01 = texture(texture_2d,pixel + vec2(-c_onePixel , 0.0)).rgb;
    vec3 C11 = texture(texture_2d,pixel + vec2( 0.0        , 0.0)).rgb;
    vec3 C21 = texture(texture_2d,pixel + vec2( c_onePixel , 0.0)).rgb;
    vec3 C31 = texture(texture_2d,pixel + vec2( c_twoPixels, 0.0)).rgb;
    
    vec3 C02 = texture(texture_2d,pixel + vec2(-c_onePixel , c_onePixel)).rgb;
    vec3 C12 = texture(texture_2d,pixel + vec2( 0.0        , c_onePixel)).rgb;
    vec3 C22 = texture(texture_2d,pixel + vec2( c_onePixel , c_onePixel)).rgb;
    vec3 C32 = texture(texture_2d,pixel + vec2( c_twoPixels, c_onePixel)).rgb;
    
    vec3 C03 = texture(texture_2d,pixel + vec2(-c_onePixel , c_twoPixels)).rgb;
    vec3 C13 = texture(texture_2d,pixel + vec2( 0.0        , c_twoPixels)).rgb;
    vec3 C23 = texture(texture_2d,pixel + vec2( c_onePixel , c_twoPixels)).rgb;
    vec3 C33 = texture(texture_2d,pixel + vec2( c_twoPixels, c_twoPixels)).rgb;  
    
    vec3 CP0X = CubicHermite(C00,C10,C20,C30,frac.x);
    vec3 CP1X = CubicHermite(C01,C11,C21,C31,frac.x);
    vec3 CP2X = CubicHermite(C02,C12,C22,C32,frac.x);
    vec3 CP3X = CubicHermite(C03,C13,C23,C33,frac.x);
    
    return CubicHermite(CP0X,CP1X,CP2X,CP3X,frac.y);
}

void main(){
    ivec3 world_pos = ivec3(world_pos_io);

    float d = (distance(camera_position,world_pos_io) + 30000.0f) / 32.0;
    int depth = floatExponent(d);
    depth = max(depth,14);
    vec3 world_pos_n = fract(vec3(world_pos) / (1 << depth));
    vec2 uv = vec2(dot(world_pos_n,u_io),dot(world_pos_n,v_io));

    ivec3 offset_u = ivec3(u_io * (1 << depth));
    ivec3 offset_v = ivec3(v_io * (1 << depth));

    vec3 ll = luxelGetColor(world_pos,d);
    vec3 lh = luxelGetColor(world_pos + offset_u,d);
    vec3 hh = luxelGetColor(world_pos + offset_u + offset_v,d);
    vec3 hl = luxelGetColor(world_pos + offset_v,d);

    vec3 lx1 = mix(ll,lh,uv.x);
    vec3 lx2 = mix(hl,hh,uv.x);
    
    FragColor.a = 1.0;
  
    FragColor.rgb = mix(lx1,lx2,uv.y);
#if 0
    FragColor.rgb = fract(world_pos_io / 0x10000);
    FragColor.rgb = fract(camera_position / 0x10000);
    FragColor.rgb = world_pos_n;

    ivec3 world_pos_s = world_pos >> depth;
    FragColor.r = fract(float(luxelHashGet(world_pos_s,depth)) / 256);
#endif
    vec3 texture_color = BicubicHermiteTextureSample(ourTexture,textcoords_io);
    FragColor.rgb *= texture_color;
}
