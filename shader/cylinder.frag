#version 330 core
out vec4 FragColor;

uniform vec3 cylinder_position;
uniform vec3 camera_position;
uniform vec2 resolution;
uniform vec4 tri;
uniform vec2 fov;
uniform float cylinder_radius;
uniform vec3 voxel_position;
uniform vec3 cylinder_direction;

uniform samplerCube cubemap;
uniform samplerCube cubemap2; 

float rayCylinderIntersection(vec3 ray_position,vec3 ray_direction,vec3 cylinder_position,vec3 axis,float radius){
    vec3 oc = ray_position - cylinder_position;

    vec3 d_perp  = ray_direction - axis * dot(ray_direction,axis);
    vec3 oc_perp = oc - axis * dot(oc,axis);

    float a = dot(d_perp,d_perp);
    float b = dot(oc_perp,d_perp);
    float c = dot(oc_perp,oc_perp) - radius * radius;
    float h = b * b - a * c;
    if(h < 0)
        return -1.0;
    h = sqrt(h);
    return (-b-h) / a;
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

    float distance = rayCylinderIntersection(camera_position,direction,cylinder_position,cylinder_direction,cylinder_radius / 2);
    if(distance < 0.0)
        discard;
    vec3 hit_position = camera_position + direction * distance;
    if(!intersectBoxPoint(hit_position,cylinder_position,cylinder_radius))
        discard;
    vec3 relative = hit_position - cylinder_position;
    float offset = dot(relative,cylinder_direction);
    vec3 normal = normalize(hit_position - (cylinder_position + cylinder_direction * offset));
    vec3 reflect_vector = reflect(direction,normal);
    vec3 color_1 = texture(cubemap,reflect_vector).rgb;
    vec3 color_2 = texture(cubemap2,reflect_vector).rgb;

    offset = offset / cylinder_radius;
    offset += 1.0;
    offset /= 2.0;

    FragColor.bgr = mix(color_2,color_1,offset);

    vec3 axis = cylinder_direction;

    vec3 tangent =
         abs(axis.y) < 0.999
         ? normalize(cross(vec3(0.0,1.0,0.0),axis))
         : normalize(cross(vec3(1.0,0.0,0.0),axis));

    vec3 bitangent = cross(axis,tangent);

    float height = dot(relative,axis);
    vec3 radial = relative - axis * height;

    float x = dot(radial,tangent);
    float y = dot(radial,bitangent);

    float u = (atan(y,x) + PI) / PI;

    bool b_u = fract(u * 8.0) < 0.5;
    bool b_v = fract(offset * 8.0) < 0.5;
#if 0
    if((int(b_u) ^ int(b_v)) == 0)
        FragColor.rgb /= 2.0;
#endif
}