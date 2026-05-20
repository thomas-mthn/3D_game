#version 330 core
out vec4 FragColor;
in vec2 textcoords_io;
in vec3 world_pos_io;
in vec3 lightmap_pos_io;
flat in vec3 luminance_io;
flat in ivec3 normal_io;
flat in int lightmap_index_io;

uniform sampler2D ourTexture;
uniform isampler2D lightmap;
uniform vec3 camera_position;

int floatExponent(float x){
    int bits = floatBitsToInt(x);
    return max(((bits >> 23) & 0xFF) - 127,14);
}
    
ivec2 to2D(int index){
    return ivec2(index % 4096,index / 4096);
}

uint bitRotateRight(uint x,int r){
    r &= 31;
    return (x >> r) | (x << (32 - r));
}

uint tHash(uint x){
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
	return x;
}

uint hash4(uint x,uint y,uint z,uint w,uint n_x,uint n_y,uint n_z){
    uint h = 0x811C9DC5u;

    uint v[7];
    v[0] = x;
    v[1] = y;
    v[2] = z;
    v[3] = w;
    v[4] = n_x + 0x80u >> 8u;
    v[5] = n_y + 0x80u >> 8u;
    v[6] = n_z + 0x80u >> 8u;

    for(int i = 0;i < 7;i++){
        h ^= v[i];
        h = tHash(h);
    }

    return h;
}

uint luxelHashGet(ivec3 position,int depth,ivec3 normal){
	return hash4(uint(position.x),uint(position.y),uint(position.z),uint(depth),uint(normal.x),uint(normal.y),uint(normal.z));
}

uint luxelGet(uint hash){
	return hash % 0x40000u * 4u;
}

vec4 luxelGetColor(ivec3 world_pos,float d,ivec3 normal){
    int depth = floatExponent(d);
    
    ivec3 world_pos_s = world_pos >> depth;
    
    int hash_entry;
    uint hash = luxelHashGet(world_pos_s,depth,normal);

    hash_entry = int(luxelGet(hash));
#if 1
    for(int i = 0;i < 4;i++){
        hash_entry = int(luxelGet(hash + uint(i)));
        if(uint(texelFetch(lightmap,to2D(hash_entry + 3),0).r) == hash)
            break;
        if(i == 3)
            return vec4(0.0,0.0,0.0,0.0);
    }
#endif
    float red   = intBitsToFloat(texelFetch(lightmap,to2D(hash_entry + 2),0).r);
    float green = intBitsToFloat(texelFetch(lightmap,to2D(hash_entry + 1),0).r);
    float blue  = intBitsToFloat(texelFetch(lightmap,to2D(hash_entry + 0),0).r);

    return vec4(red,green,blue,1.0);
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
    ivec3 normal = normal_io;

    ivec3 world_pos = ivec3(world_pos_io);

    float d = (distance(camera_position,world_pos_io) + 30000.0f) / 64.0;
    float dt = dot(normalize(world_pos_io - camera_position),vec3(normal) / 0x10000);
    float s_angle = abs(dt) / 2.0 + 0.5;
    s_angle = 1.0 / s_angle;
    d *= s_angle;
    int depth = floatExponent(d);
    vec3 color[4];
    color[0] = vec3(0.0);
    color[1] = vec3(0.0);
    color[2] = vec3(0.0);
    color[3] = vec3(0.0);

    FragColor.a = 1.0;
#if 1
    for(int j = 0;j < 3;j++){
        int offset = 1 << depth;

        ivec3 offsets[4];
        offsets[0] = ivec3(0.0);
        offsets[1] = ivec3(offset,0,0);
        offsets[2] = ivec3(offset,offset,0);
        offsets[3] = ivec3(0,offset,0);
        int i = 0;
        for(;i < 4;i++){
            vec4 result = luxelGetColor(ivec3(lightmap_pos_io) + offsets[i],d,normal_io);
            if(result.a < 0.5)
                break;
            color[i] = result.rgb;
        }
        if(i == 4)
            break;
        if(j == 2){
            depth -= 2;
            d /= 4.0;
            for(int k = 0;k < 3;k++){
                vec4 result = luxelGetColor(ivec3(lightmap_pos_io),d,normal_io);
                if(result.a > 0.5){
                    FragColor.rgb = result.rgb;
                    vec3 texture_color = BicubicHermiteTextureSample(ourTexture,textcoords_io);           
                    FragColor.rgb *= texture_color;
                    return;
                }
                d *= 2.0;
                depth += 1;
            }
            FragColor = vec4(0.0,0.0,0.0,1.0);
            return;
        }
        d *= 2.0;
        depth += 1;
    }
#endif
    vec2 uv = fract(lightmap_pos_io.rg / (1 << depth));

    vec3 lx1 = mix(color[0],color[1],uv.x);
    vec3 lx2 = mix(color[3],color[2],uv.x);
  
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
    FragColor.rgb *= luminance_io;
}   
