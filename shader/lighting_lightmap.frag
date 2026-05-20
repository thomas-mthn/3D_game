#version 330 core
out vec4 FragColor;
in vec3 world_pos_io;
in vec3 lightmap_pos_io;
flat in vec3 u_io;
flat in vec3 v_io;
flat in int lightmap_index_io;
flat in ivec3 normal_io;

uniform isampler2D lightmap;
uniform vec3 camera_position;

int floatExponent(float x){
    int bits = floatBitsToInt(x);
    return max(((bits >> 23) & 0xFF) - 127,14);
}
    
ivec2 to2D(int index){
    return ivec2(index % 4096,index / 4096);
}

uint tHash(uint x){
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
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

void main(){
    ivec3 normal = normal_io;
    ivec3 world_pos = ivec3(world_pos_io);

    float d = (distance(camera_position,world_pos_io) + 30000.0f) / 32.0;
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

    vec2 uv = fract(lightmap_pos_io.rg / (1 << depth));

    vec3 lx1 = mix(color[0],color[1],uv.x);
    vec3 lx2 = mix(color[3],color[2],uv.x);
    
    FragColor.a = 1.0;
  
    FragColor.rgb = mix(lx1,lx2,uv.y);
#if 0
    FragColor.rgb = fract(world_pos_io / 0x10000);
    FragColor.rgb = fract(camera_position / 0x10000);
    FragColor.rgb = world_pos_n;

    ivec3 world_pos_s = world_pos >> depth;
    FragColor.r = fract(float(luxelHashGet(world_pos_s,depth)) / 256);
#endif
}
