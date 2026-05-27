#include "sprite_trace.h"
#include "main.h"
#include "lighting.h"
#include "texture.h"
#include "opengl.h"
#include "octree_render.h"

void ellipsoidModelCubemapGenerate(Cubemap* cubemap,Vec3 position,int reflect_factor){
    struct{
		Vec3 direction;
		Vec3 luminance;
	} ray_array[0x100];
    for(int i = 0;i < countof(ray_array);i++){
		Vec3 ray_direction = vec3Normalize(fibonnaciSphereSample(i,countof(ray_array)));

		ray_array[i].direction = ray_direction;
		ray_array[i].luminance = rayLuminance(position,ray_direction,(RayLuminanceFlag){0});
	}
    cubemap->size = 0x10;
    
    for(Side i = SIDE_COUNT;i--;){
        if(!cubemap->textures[i].pixel_data)
            cubemap->textures[i] = textureCreate(cubemap->size);

        for(int j = cubemap->size * cubemap->size;j--;){
            int x = j / cubemap->size;
            int y = j % cubemap->size;

            Vec3 direction = vec3Normalize(cubemapDirectionGet(cubemap,i,x,y));
            
            Vec3 color_acc = {0};
            real strength_total = 0;
            for(int i = 0;i < countof(ray_array);i++){
                real strength = vec3Dot(ray_array[i].direction,direction);
                if(strength < 0)
                    continue;
                for(int j = reflect_factor;j--;)
                    strength = realMulR(strength,strength);
                strength_total += strength;
                color_acc = vec3Add(color_acc,vec3MulS(ray_array[i].luminance,strength));
            }
            cubemap->textures[i].pixel_data[j] = colorToPixelColor(vec3DivS(color_acc,strength_total));
            int color_table[] = {
                0xFFFF00,
                0xFF0000,
                0x00FF00,
                0x0000FF,
                0xFF00FF,
                0x00FFFF,
            };
            //cubemap.textures[i].pixel_data[j] = color_table[i];
            //cubemap.textures[i].pixel_data[j] = x * 0x100 / cubemap.size << 16 | y * 0x100 / cubemap.size << 8;
        }
    }

    if(cubemap->gl_id)
        openglUpdateCubemap(cubemap);
}

static Vec3 cylinderNormal(Vec3 p,Vec3 c,Vec3 axis){
    Vec3 oc = vec3Sub(p,c);
    Vec3 proj = vec3MulS(axis,vec3Dot(oc, axis));

    Vec3 n = vec3Sub(oc,proj);
    return vec3Normalize(n);
}

void ellipsoidModelGenerate(Cubemap* cubemap,Vec3 position,Vec3 size,Texture* texture,ModelSprite* model,Vec2 model_angle,bool angle_player,real projection_size,real scale){
    static ModelSprite model_default = {
        .ellipsoid = {
            {
                .color = {FIXED_ONE,FIXED_ONE,FIXED_ONE},
                .ellipsoid.radius = {FIXED_ONE / 2,FIXED_ONE / 2,FIXED_ONE / 2},
            }
        },
        .n_ellipsoid = 1,
    };
    if(!model)
        model = &model_default;

	Vec3 direction = vec3Direction(g_surface.position,position);
    direction = vec3Epsilon(direction);

    Vec2 q_angle = {-g_surface.angle.y + FIXED_ONE / 2,-g_surface.angle.x + FIXED_ONE / 2};
    Quaternion quaternion = quaternionCreate(angle_player ? (Vec2){model_angle.y,model_angle.x} : q_angle);
    
    Vec2 angle;
    Vec2 fov_offset;
    if(angle_player){
        angle = getLookAngle(direction);

        Vec2 delta_angle = vec2Sub(angle,getLookAngle(getLookDirection(g_surface.angle)));
        
        angle = vec2Add(angle,model_angle);
        angle = vec2Sub(angle,(Vec2){tSin(delta_angle.x / 0x10),tSin(delta_angle.y / 0x8)});
    }
    else{
        angle = model_angle;
    }
    
    direction = getLookDirection(angle);

	real camera_distance = vec3Distance(g_surface.position,position);
	Vec3 camera_position = vec3MulS(direction,-camera_distance);
	Vec3 ray_origin = vec3Sub(position,vec3MulS(direction,camera_distance));
    ray_origin = g_surface.position;
	for(int i = 0;i < texture->size * texture->size;i++)
	    texture->pixel_data[i] = 0xFF000000;

    Vec3 screen_position = pointToScreen(position);

    real delta = realDivR(projection_size,texture->size);
    Vec2 delta_fov = vec2MulS(g_surface.fov,delta);
    delta_fov.x *= g_surface.height * 2;
    delta_fov.y *= g_surface.width * 2;
    //delta_fov = vec2Single(delta);
    
	for(int i = 0;i < texture->size * texture->size;i++){
		real x = (-realMulR(intToReal(i / texture->size),delta_fov.x) + realMulR(intToReal(texture->size / 2),delta_fov.x)) / g_surface.height;
		real y = (-realMulR(intToReal(i % texture->size),delta_fov.y) + realMulR(intToReal(texture->size / 2),delta_fov.y)) / g_surface.width;
        
		Vec3 direction = vec3Normalize(screenRayDirection(g_surface.rotation_matrix,screen_position.x + x,screen_position.y + y,g_surface.fov.x,g_surface.fov.y));
			
		real min_distance = REAL_MAX;

        TracePrimitive* ellipsoid = 0;

        for(int j = 0;j < model->n_ellipsoid;j++){
            real distance;
            Vec3 model_position = vec3Add(model->ellipsoid[j].position,position);
            Vec3 relative_position = vec3Sub(ray_origin,position);

            relative_position = quaternionRotate(quaternion,relative_position);
            Vec3 direction_local = quaternionRotate(quaternion,direction);

            switch(model->ellipsoid[j].type){
                case MODEL_ELLIPSOID:{
                    distance = rayEllipsoidIntersection(relative_position,direction_local,model->ellipsoid[j].position,vec3MulS(model->ellipsoid[j].ellipsoid.radius,scale));
                } break;
                case MODEL_CYLINDER:{
                    distance = rayCylinderIntersection(ray_origin,direction,model_position,model->ellipsoid[j].cylinder.axis,model->ellipsoid[j].cylinder.radius);
                } break;
            }
            if(distance < 0 || distance > min_distance)	
                continue;
            min_distance = distance;
            ellipsoid = model->ellipsoid + j;
        }

		if(!ellipsoid){
		    texture->pixel_data[i] = 0xFF000000;
            //texture->pixel_data[i] = colorToPixelColor((Vec3){tFractU((direction.x + FIXED_ONE) * 0x10),tFractU((direction.y + FIXED_ONE) * 0x10),tFractU((direction.z + FIXED_ONE) * 0x10)});
			continue;
		}

		Vec3 hit_position = vec3Add(ray_origin,vec3MulS(direction,min_distance));

        if(model->is_voxel && !intersectBoxPoint(hit_position,position,size)){
            texture->pixel_data[i] = 0xFF000000;
			continue;
        }
        
		Vec3 reflect_vector;
        Vec3 color_acc = {0};
        switch(ellipsoid->type){
            case MODEL_ELLIPSOID:{
                Vec3 relative = vec3Direction(hit_position,position);
                relative = quaternionRotate(quaternion,relative);
                Vec3 normal = vec3Normalize(vec3Div(relative,ellipsoid->ellipsoid.radius));
                
                reflect_vector = vec3Reflect(direction,normal);
                //reflect_vector = quaternionRotate(quaternion,reflect_vector);
                color_acc = pixelColorToColor(cubemapColorGetBilinear(cubemap,reflect_vector));
                //color_acc = reflect_vector;
            } break;
            case MODEL_CYLINDER:{
                Vec3 relative = vec3Sub(hit_position,position);
                real offset = vec3Dot(relative,ellipsoid->cylinder.axis);
                Vec3 normal = vec3Direction(hit_position,vec3Add(position,vec3MulS(ellipsoid->cylinder.axis,offset)));
                reflect_vector = vec3Reflect(direction,normal);
                
                offset = realDivR(offset,size.x);
                offset += FIXED_ONE;
                offset /= 2;
                Vec3 color_1 = pixelColorToColor(cubemapColorGetBilinear(cubemap + 1,reflect_vector));
                Vec3 color_2 = pixelColorToColor(cubemapColorGetBilinear(cubemap,reflect_vector));
                color_acc = vec3Mix(color_1,color_2,offset);

                Vec3 axis = ellipsoid->cylinder.axis;

                Vec3 tangent =
                    tAbs(axis.y) < FIXED_ONE - REAL_EPSILON
                    ? vec3Normalize(vec3Cross((Vec3){0,1,0},axis))
                    : vec3Normalize(vec3Cross((Vec3){1,0,0},axis));

                Vec3 bitangent = vec3Cross(axis,tangent);

                relative = vec3Sub(hit_position,position);

                real height = vec3Dot(relative,axis);
                Vec3 radial = vec3Sub(relative,vec3MulS(axis,height));

                real x = vec3Dot(radial,tangent);
                real y = vec3Dot(radial,bitangent);

                real u = tArcTan2(y,x);

                bool b_u = tFractU(realMulR(u,FIXED_ONE * 8)) < 0.5;
                bool b_v = tFractU(realMulR(offset,FIXED_ONE * 8)) < 0.5;

                if(b_u ^ b_v)
                    color_acc = vec3MulS(color_acc,FIXED_ONE / 2);
                //color_acc = vec3Single(offset);
            } break;
        }
        
		texture->pixel_data[i] = colorToPixelColor(vec3Mul(color_acc,ellipsoid->color));
	}
	generateMipmaps(texture);
	textureUpdateGL(texture);
}

void spriteRender3d(Vec3 position,real entity_size,Texture* texture){
	Vec3 point = pointToScreen(position);
	if(!point.z)
		return;
	real size = spriteSize(position,entity_size);
	Vec2 points[] = {
		{point.x - realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
		{point.x - realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
		{point.x + realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
		{point.x + realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
	};
   
    DrawPrimitive* primitive = primitiveToDraw();
    primitive->texture = texture;
    primitive->is_sprite = true;
    for(int i = 4;i--;){
        primitive->position_sprite[i] = points[i];
        primitive->texture_crd[i] = g_texture_coordinates_fill[i];
    }
}
