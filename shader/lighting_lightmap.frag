#version 330 core
out vec4 FragColor;
in vec3 world_pos_io;
flat in vec3 u_io;
flat in vec3 v_io;
flat in int lightmap_index_io;

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

    float red   = intBitsToFloat(texelFetch(lightmap,to2D(hash_entry + 2),0).r);
    float green = intBitsToFloat(texelFetch(lightmap,to2D(hash_entry + 1),0).r);
    float blue  = intBitsToFloat(texelFetch(lightmap,to2D(hash_entry + 0),0).r);
    return vec3(red,green,blue);
}

void main(){
    ivec3 world_pos = ivec3(world_pos_io);

    float d = (distance(camera_position,world_pos_io) + 30000.0f) / 32.0;
    int depth = floatExponent(d);
    depth = max(depth,14);
    vec3 world_pos_n = fract(vec3(world_pos) / (1 << depth));
    vec2 uv = vec2(dot(world_pos_n,u_io),dot(world_pos_n,v_io));

    vec3 ll = luxelGetColor(world_pos,d);
    vec3 lh = luxelGetColor(world_pos + ivec3(u_io * (1 << depth)),d);
    vec3 hh = luxelGetColor(world_pos + ivec3(u_io * (1 << depth)) + ivec3(v_io * (1 << depth)),d);
    vec3 hl = luxelGetColor(world_pos + ivec3(v_io * (1 << depth)),d);

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
}
