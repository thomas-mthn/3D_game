#version 330 core
out vec4 FragColor;
in vec3 world_pos_io;
in vec3 lightmap_pos_io;
in vec2 uv_io;
flat in vec3 vnormal_io;
flat in vec3 color_io;
flat in int lightmap_index_io;
flat in ivec3 normal_io;

uniform isampler2D lightmap;
uniform vec3 camera_position;
uniform vec3 camera_lm_position;

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

uint hash( uint x ) {
    x += ( x << 10u );
    x ^= ( x >>  6u );
    x += ( x <<  3u );
    x ^= ( x >> 11u );
    x += ( x << 15u );
    return x;
}
uint hash( uvec2 v ) { return hash( v.x ^ hash(v.y)                         ); }

float floatConstruct( uint m ) {
    const uint ieeeMantissa = 0x007FFFFFu;
    const uint ieeeOne      = 0x3F800000u;

    m &= ieeeMantissa;                 
    m |= ieeeOne;                    

    float  f = uintBitsToFloat( m );  
    return f - 1.0;                       
}
float random( float x ) { return floatConstruct(hash(floatBitsToUint(x))); }
float random( vec2  v ) { return floatConstruct(hash(floatBitsToUint(v))); }

float voronoiDistance(vec2 x){
    ivec2 p = ivec2(floor(x / 0x8000));
    vec2  f = fract(x / 0x8000);

    ivec2 mb;
    vec2 mr;

    float res = 8000.0;
    for( int j=-1; j<=1; j++ )
    for( int i=-1; i<=1; i++ )
    {
        ivec2 b = ivec2(i, j);
        vec2  r = vec2(b);
        r += random(p + b);
        r -= f;
        float d = dot(r,r);

        if( d < res )
        {
            res = d;
            mr = r;
            mb = b;
        }
    }

    res = 8000.0;
    for( int j=-2; j<=2; j++ )
    for( int i=-2; i<=2; i++ ){
        ivec2 b = mb + ivec2(i, j);
        vec2  r = vec2(b);
        r += random(p + b);
        r -= f;
        float d = dot(0.5*(mr+r), normalize(r-mr));

        res = min( res, d );
    }

    return res;
}

float getBorder(vec2 p){
    float d = voronoiDistance( p );

    return 1.0 - smoothstep(0.0,0.05,d);
}

void main(){
    ivec3 normal = normal_io;
    ivec3 world_pos = ivec3(world_pos_io);

    float d = (distance(camera_position,world_pos_io)) / 48.0;
    float dt = dot(normalize(world_pos_io - camera_position),vec3(normal) / 0x10000);
    float s_angle = abs(dt) / 2.0 + 0.5;
    s_angle = 1.0 / s_angle;
    d *= s_angle;
    int depth = floatExponent(d);
    int color_backup_filled = 0;

    vec3 color_backup[4];

    vec3 color[4];
    color[0] = vec3(0.0);
    color[1] = vec3(0.0);
    color[2] = vec3(0.0);
    color[3] = vec3(0.0);

    for(int j = 0;j < 2;j++){
        int offset = 1 << depth;

        ivec3 offsets[4];
        offsets[0] = ivec3(0.0);
        offsets[1] = ivec3(offset,0,0);
        offsets[2] = ivec3(offset,offset,0);
        offsets[3] = ivec3(0,offset,0);
        int i = 0;
        bool is_done = true;
        for(;i < 4;i++){
            vec4 result = luxelGetColor(ivec3(lightmap_pos_io) + offsets[i],d,normal_io);
            if(result.a < 0.5){
                is_done = false;
                continue;
            }
            if((color_backup_filled >> i & 1) == 0){
                color_backup_filled |= 1 << i;
                color_backup[i] = result.rgb;
            }
            color[i] = result.rgb;
        }
        if(is_done)
            break;
        if(j == 2){
            if(color_backup_filled == 15){
                for(int i = 0;i < 4;i++)
                    color[i] = color_backup[i];
                depth -= 2;
                break;
            }
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
    FragColor.rgb *= color_io;

    vec2 uv_brick;

    vec3 position[4];
    position[0] = lightmap_pos_io;
    position[1] = lightmap_pos_io + dFdx(lightmap_pos_io) * 0.5;
    position[2] = lightmap_pos_io + dFdy(lightmap_pos_io) * 0.5;
    position[3] = lightmap_pos_io + dFdx(lightmap_pos_io) * 0.5 + dFdy(lightmap_pos_io) * 0.5;

    vec3 filter = vec3(0.0);

    vec3 n = normalize(vnormal_io - lightmap_pos_io);

    for(int i = 0;i < 4;i++){
        float height = voronoiDistance(position[i].xy);
        #if 1
        if(height < 0.1){
            position[i] += -n * 0x4000 * (1.0 - height * 10.0);
        }
        #endif
        int x = int(position[i].x / 0x8000);
        int y = int(position[i].y / 0x8000);

        vec2 uv = fract(position[i].xy / 0x8000);

        float min_distance = 999999.0;
        int min_index;

        for(int j = 0;j < 9;j++){
            int cell_x = j / 3 - 1;
            int cell_y = j % 3 - 1;

            int cx = x + cell_x;
            int cy = y + cell_y;
            
            vec2 cell = vec2(
                float(cell_x),
                float(cell_y)
            );
            cell += random(vec2(cx,cy));
 
            int index = cx * 73856093 ^ cy * 19349663;
            float distance = distance(cell,uv);

            if(min_distance > distance){
                min_distance = distance;
                min_index = index; 
            }
        }
        #if 1
        vec3 adding = vec3(random(min_index)) * 0.5 + 0.5;
        height = voronoiDistance(position[i].xy);
        if(height < 0.1){
            adding *= height * 10.0;
        }
        filter += adding;
        #endif
    }
    filter *= 0.25;

    FragColor.rgb *= filter;

    //FragColor.rgb = vec3(abs(vnormal_io.z));

    //FragColor.rgb = normalize(vnormal_io - lightmap_pos_io);
}
