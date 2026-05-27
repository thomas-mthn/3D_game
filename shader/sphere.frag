#version 330 core
out vec4 FragColor;

uniform vec3 sphere_position;
uniform vec3 camera_position;
uniform vec2 resolution;
uniform vec4 tri;
uniform vec2 fov;
uniform float sphere_radius;

uniform samplerCube cubemap; 

float raySphereIntersection(vec3 ray_position,vec3 ray_direction,vec3 sphere_position,float radius){
    vec3 oc = ray_position - sphere_position;

    float a = dot(ray_direction,ray_direction);
    float b = 2 * dot(oc,ray_direction);
    float c = dot(oc,oc) - radius * radius;
    
    float discriminant = b * b - 4 * (a * c);
    if(discriminant < 0)
        return -1.0;
    else
        return (-b - sqrt(discriminant)) / (2 * a);
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

void main(){
    vec2 uv = gl_FragCoord.yx / resolution;
    uv *= 2.0;
    uv -= 1.0;
    uv = -uv;
    vec3 direction = normalize(screenRayDirection(uv));
	FragColor.a = 1.0;

    float distance = raySphereIntersection(camera_position,direction,sphere_position,sphere_radius);
    if(distance < 0.0)
        discard;
    vec3 hit_position = camera_position + direction * distance;
    vec3 r = normalize(hit_position - sphere_position);
    vec3 normal = normalize(r);
    FragColor.bgr = texture(cubemap,reflect(direction,normal)).rgb;
}