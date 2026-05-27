#version 330 core
out vec4 FragColor;

uniform vec3 sphere_position;
uniform vec3 camera_position;
uniform vec2 resolution;
uniform vec4 tri;
uniform vec2 fov;
uniform float sphere_radius;
uniform vec3 voxel_position;
uniform vec3 cylinder_direction;

uniform samplerCube cubemap;
uniform samplerCube cubemap2; 

float torIntersect(vec3 ro,vec3 rd,vec2 tor){
    float po = 1.0;
    float Ra2 = tor.x*tor.x;
    float ra2 = tor.y*tor.y;
    float m = dot(ro,ro);
    float n = dot(ro,rd);
    float k = (m + Ra2 - ra2)/2.0;
    float k3 = n;
    float k2 = n*n - Ra2*dot(rd.xy,rd.xy) + k;
    float k1 = n*k - Ra2*dot(rd.xy,ro.xy);
    float k0 = k*k - Ra2*dot(ro.xy,ro.xy);
    
    if( abs(k3*(k3*k3-k2)+k1) < 0.01 )
    {
        po = -1.0;
        float tmp=k1; k1=k3; k3=tmp;
        k0 = 1.0/k0;
        k1 = k1*k0;
        k2 = k2*k0;
        k3 = k3*k0;
    }
    
    float c2 = k2*2.0 - 3.0*k3*k3;
    float c1 = k3*(k3*k3-k2)+k1;
    float c0 = k3*(k3*(c2+2.0*k2)-8.0*k1)+4.0*k0;
    c2 /= 3.0;
    c1 *= 2.0;
    c0 /= 3.0;
    float Q = c2*c2 + c0;
    float R = c2*c2*c2 - 3.0*c2*c0 + c1*c1;
    float h = R*R - Q*Q*Q;
    
    if(h >= 0.0){
        h = sqrt(h);
        float v = sign(R+h)*pow(abs(R+h),1.0/3.0); // cube root
        float u = sign(R-h)*pow(abs(R-h),1.0/3.0); // cube root
        vec2 s = vec2( (v+u)+4.0*c2, (v-u)*sqrt(3.0));
        float y = sqrt(0.5*(length(s)+s.x));
        float x = 0.5*s.y/y;
        float r = 2.0*c1/(x*x+y*y);
        float t1 =  x - r - k3; t1 = (po<0.0)?2.0/t1:t1;
        float t2 = -x - r - k3; t2 = (po<0.0)?2.0/t2:t2;
        float t = 1e20;
        if( t1>0.0 ) t=t1;
        if( t2>0.0 ) t=min(t,t2);
        return t;
    }
    
    float sQ = sqrt(Q);
    float w = sQ*cos( acos(-R/(sQ*Q)) / 3.0 );
    float d2 = -(w+c2); if( d2<0.0 ) return -1.0;
    float d1 = sqrt(d2);
    float h1 = sqrt(w - 2.0*c2 + c1/d1);
    float h2 = sqrt(w - 2.0*c2 - c1/d1);
    float t1 = -d1 - h1 - k3; t1 = (po<0.0)?2.0/t1:t1;
    float t2 = -d1 + h1 - k3; t2 = (po<0.0)?2.0/t2:t2;
    float t3 =  d1 - h2 - k3; t3 = (po<0.0)?2.0/t3:t3;
    float t4 =  d1 + h2 - k3; t4 = (po<0.0)?2.0/t4:t4;
    float t = 1e20;
    if( t1>0.0 ) t=t1;
    if( t2>0.0 ) t=min(t,t2);
    if( t3>0.0 ) t=min(t,t3);
    if( t4>0.0 ) t=min(t,t4);
    return t;
}

vec3 torNormal(vec3 pos,vec2 tor){
    return normalize(pos*(dot(pos,pos)-tor.y*tor.y - tor.x*tor.x*vec3(1.0,1.0,-1.0)));
}

vec3 screenRayDirection(vec2 uv){
	vec3 ray_ang;
	
	float pixel_offset_x = uv.x * (1.0 / fov.x);
	float pixel_offset_y = uv.y * (1.0 / fov.y);
	ray_ang.x = tri.x * tri.z;
	ray_ang.y = tri.y * tri.z;
	ray_ang.x -= tri.x * tri.a * pixel_offset_x;
	ray_ang.y -= tri.y * tri.a * pixel_offset_x;
	ray_ang.y += tri.x * pixel_offset_y;
	ray_ang.x -= tri.y * pixel_offset_y;
	ray_ang.z = tri.a + tri.z * pixel_offset_x;
	return ray_ang;
}

bool intersectBoxPoint(vec3 point,vec3 box_position,float box_size){
    bool x = abs(box_position.x - point.x) <= (box_size);
    bool y = abs(box_position.y - point.y) <= (box_size);
    bool z = abs(box_position.z - point.z) <= (box_size);
	return x && y && z;
}

#define PI 3.1415926538

void main(){
    vec2 uv = gl_FragCoord.yx / resolution;
    uv *= 2.0;
    uv -= 1.0;
    uv = -uv;
    vec3 direction = normalize(screenRayDirection(uv));
	FragColor.a = 1.0;

    float distance = torIntersect(camera_position - sphere_position,direction,vec2(sphere_radius,sphere_radius / 2));
    if(distance < 0.0)
        discard;
    vec3 hit_position = camera_position + direction * distance;

    if(!intersectBoxPoint(hit_position,voxel_position,sphere_radius))
        discard;

    vec3 normal = torNormal(hit_position - sphere_position,vec2(sphere_radius,sphere_radius / 2));

    FragColor.rgb = vec3(0.5);
    FragColor.bgr = texture(cubemap,reflect(direction,normal)).rgb;
}