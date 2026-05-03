#include "entity.h"
#include "memory.h"
#include "draw.h"
#include "main.h"
#include "octree.h"
#include "lighting.h"
#include "physics.h"
#include "octree_render.h"
#include "staff.h"
#include "voxel_menu.h"
#include "audio.h"
#include "span.h"
#include "opengl.h"

#if !defined(__wasm__) && !defined(__linux__)
#include "win32/w_main.h"
#endif

static ModelEllipsoid model_sphere_slime[] = {
	{
		.color = {FIXED_ONE / 4,FIXED_ONE,FIXED_ONE / 4},
		.radius = {FIXED_ONE / 2,FIXED_ONE / 2,FIXED_ONE / 2},
	},
	{
		.position = {0,-FIXED_ONE / 3,-FIXED_ONE / 3},
		.color = {FIXED_ONE,FIXED_ONE / 4,FIXED_ONE / 4},
		.radius = {FIXED_ONE / 6,FIXED_ONE / 6,FIXED_ONE / 6},
	},
	{
		.position = {0,FIXED_ONE / 3,-FIXED_ONE / 3},
		.color = {FIXED_ONE,FIXED_ONE / 4,FIXED_ONE / 4},
		.radius = {FIXED_ONE / 6,FIXED_ONE / 6,FIXED_ONE / 6},
	},
};

Entity* g_entity;

Entity* entityRayCollision(Entity* entity_list,Vec3 position,Vec3 direction){
	Entity* entity_closest = 0;
	for(Entity* entity = entity_list;entity;entity = entity->next_voxel){
		if(!entity->hitable)
			continue;
		if(rayBoxIntersection(entity->position,vec3Shr(entity->hitbox,1),position,direction))
			entity_closest = entity;
	}
	return entity_closest;
}

Vec3 ellipsoidNormal(Vec3 pos,Vec3 ra){
    return vec3Normalize(vec3Div(pos,(vec3Mul(ra,ra))));
}

structure(Quaternion){
    int w;
    Vec3 v;
};

Quaternion quaternionCreate(Vec2 angle){
    Quaternion q;
    
    int cy = tCos(angle.y / 2);
    int sy = tSin(angle.y / 2);
    int cr = tCos(0);
    int sr = tSin(0);
    int cp = tCos(angle.x / 2);
    int sp = tSin(angle.x / 2);

    q.w = fixedMulR(fixedMulR(cy,cr),cp) + fixedMulR(fixedMulR(sy,sr),sp);
    q.v.a[0] = fixedMulR(fixedMulR(cy,sr),cp) - fixedMulR(fixedMulR(sy,cr),sp);
    q.v.a[1] = fixedMulR(fixedMulR(cy,cr),sp) + fixedMulR(fixedMulR(sy,sr),cp);
    q.v.a[2] = fixedMulR(fixedMulR(sy,cr),cp) - fixedMulR(fixedMulR(cy,sr),sp);

    return q;
}

Vec3 quaternionRotate(Quaternion q,Vec3 v){
    Vec3 result;

    int ww = fixedMulR(q.w,q.w);
    int xx = fixedMulR(q.v.a[0],q.v.a[0]);
    int yy = fixedMulR(q.v.a[1],q.v.a[1]);
    int zz = fixedMulR(q.v.a[2],q.v.a[2]);
    int wx = fixedMulR(q.w,q.v.a[0]);
    int wy = fixedMulR(q.w,q.v.a[1]);
    int wz = fixedMulR(q.w,q.v.a[2]);
    int xy = fixedMulR(q.v.a[0],q.v.a[1]);
    int xz = fixedMulR(q.v.a[0],q.v.a[2]);
    int yz = fixedMulR(q.v.a[1],q.v.a[2]);

    result.a[0] = fixedMulR(ww,v.a[0]) + fixedMulR(2*wy,v.a[2]) - fixedMulR(2*wz,v.a[1]) +
        fixedMulR(xx,v.a[0]) + fixedMulR(2 * xy,v.a[1]) + fixedMulR(2 * xz,v.a[2]) -
        fixedMulR(zz,v.a[0]) - fixedMulR(yy,v.a[0]);
    result.a[1] = fixedMulR(2*xy,v.a[0]) + fixedMulR(yy,v.a[1]) + fixedMulR(2*yz,v.a[2]) +
        fixedMulR(2*wz,v.a[0]) - fixedMulR(zz,v.a[1]) + fixedMulR(ww,v.a[1]) -
        fixedMulR(2*wx,v.a[2]) - fixedMulR(xx,v.a[1]);
    result.a[2] = fixedMulR(2*xz,v.a[0]) + fixedMulR(2*yz,v.a[1]) + fixedMulR(zz,v.a[2]) -
        fixedMulR(2*wy,v.a[0]) - fixedMulR(yy,v.a[2]) + fixedMulR(2*wx,v.a[1]) -
        fixedMulR(xx,v.a[2]) + fixedMulR(ww,v.a[2]);

    return result;
}

void ellipsoidModelGenerate(Vec3 position,Texture* texture,ModelEllipsoid* ellipsoids,int n_ellipsoid,Vec2 model_angle,bool angle_player){
    ModelEllipsoid model_default = {.color = pixelColorToColor(0xFFFFFF),.radius = vec3Single(FIXED_ONE)};
	struct{
		Vec3 direction;
		Vec3 luminance;
	} ray_array[0x100];

	Vec3 direction = vec3Direction(g_surface.position,position);

    Vec2 angle;
    Quaternion quaternion = quaternionCreate((Vec2){g_surface.angle.y + FIXED_ONE / 2,g_surface.angle.x + FIXED_ONE / 2});
 
    if(angle_player){
        angle = getLookAngle(direction);
        angle = vec2Add(angle,model_angle);
    }
    else{
        angle = model_angle;
    }
    
    direction = getLookDirection(angle);

	for(int i = 0;i < countof(ray_array);i++){
		Vec3 ray_direction = vec3Normalize(fibonnaciSphereSample(i,countof(ray_array)));

		ray_array[i].direction = ray_direction;
		ray_array[i].luminance = rayLuminance(position,ray_direction);
	}
	int camera_distance = vec3Distance(g_surface.position,position);
	Vec3 camera_position = vec3MulS(direction,camera_distance);
	Vec3 ray_origin = vec3Sub(position,vec3MulS(direction,camera_distance));

	for(int i = 0;i < texture->size * texture->size;i++)
	    texture->pixel_data[i] = 0xFF000000;

	int tri[] = {tCos(angle.x),tSin(angle.x),tCos(angle.y),tSin(angle.y)};

	for(int i = 0;i < texture->size * texture->size;i++){
		int x = i / texture->size * FIXED_ONE * 2 / texture->size - FIXED_ONE;
		int y = i % texture->size * FIXED_ONE * 2 / texture->size - FIXED_ONE;
        
		Vec2 fov = {camera_distance,camera_distance};
		Vec3 direction = vec3Normalize(screenRayDirection(tri,x,y,fov.x,fov.y));
			
		int min_distance = INT_MAX;
		ModelEllipsoid* model_ellipsoid = 0;
        
        if(!ellipsoids){
            model_ellipsoid = &model_default;
            min_distance = rayEllipsoidIntersection(camera_position,direction,model_ellipsoid->position,model_ellipsoid->radius);
        }
        else{
			for(int j = 0;j < n_ellipsoid;j++){
				ModelEllipsoid* ellipsoid = ellipsoids + j;
				int distance = rayEllipsoidIntersection(camera_position,direction,ellipsoid->position,ellipsoid->radius);
				if(distance == -1 || distance > min_distance)	
					continue;
				min_distance = distance;
				model_ellipsoid = ellipsoid;
			}
        }
        
		if(min_distance == INT_MAX || min_distance == -1){
		    texture->pixel_data[i] = 0xFF000000;
			continue;
		}

		Vec3 color = model_ellipsoid ? model_ellipsoid->color : vec3Single(FIXED_ONE);

		Vec3 hit_position = vec3Add(ray_origin,vec3MulS(direction,min_distance));
		Vec3 reflect_vector = vec3Reflect(direction,vec3Normalize(vec3Div(vec3Direction(hit_position,position),model_ellipsoid->radius)));

        reflect_vector = quaternionRotate(quaternion,reflect_vector);
        
        texture->pixel_data[i] = colorToPixelColor(vec3Shl(reflect_vector,4));
        //continue;
        
		Vec3 color_acc = {0};
		for(int i = 0;i < countof(ray_array);i++){
			int strength = vec3Dot(ray_array[i].direction,reflect_vector);
			if(strength < 0)
				continue;
			color_acc = vec3Add(color_acc,vec3MulS(ray_array[i].luminance,strength));
		}
		color_acc = vec3DivS(color_acc,countof(ray_array));
		texture->pixel_data[i] = colorToPixelColor(vec3Mul(color,vec3MulS(vec3Shr(color_acc,10),g_exposure)));
	}
	/*
	DrawSurface surface_model = (DrawSurface){
		.width = entity->texture_dynamic.size,
		.height = entity->texture_dynamic.size,
		.backend = RENDER_BACKEND_SOFTWARE,
	};
	surfaceInit(&surface_model);
	tFree(surface_model.data);
	surface_model.data = entity->texture_dynamic.pixel_data;
		
	voxelModelRasterize(&surface_model,entity->player_angle,entity->luminance_3d,entity->model,camera_position,camera_distance);
	*/
	generateMipmaps(texture);
	textureUpdateGL(texture);
	//surface_model.data = 0;
	//surfaceDestroy(&surface_model);
}

void entityAdd(Entity* entity){
    if(entity->health <= 0)
        entity->health = 1;
    entity->next = g_entity;
	g_entity = entity;
}

Entity* entityCreate(Vec3 position,EntityType type){
	Entity* entity = tMallocZero(sizeof *entity);
    
	entity->position = position;
	entity->type = type;
	entity->health = 1;
	entity->physics_friction_ground = PHYSICS_FRICTION_GROUND;
	entity->physics_friction_air = PHYSICS_FRICTION_AIR;
	entity->color_emit = (Vec3){FIXED_ONE / 2,FIXED_ONE / 2,FIXED_ONE / 2};
    
	switch(type){
        case ENTITY_WEAPON:{
            entity->type = ENTITY_WEAPON;
            entity->no_gravity = true;
            entity->has_hitbox = true;
            entity->texture_dynamic = textureCreate(0x100);
            entity->hitbox = (Vec3){FIXED_ONE / 8,FIXED_ONE / 8,FIXED_ONE / 8};
            entity->render_position = entity->position;
        } break;
		case ENTITY_BOSS:{
			entity->hitable = true;
			entity->health = FIXED_ONE * 4;
			entity->texture_dynamic = textureCreate(0x100);
            entity->has_hitbox = true;
            entity->hitbox = (Vec3){FIXED_ONE * 2,FIXED_ONE * 2,FIXED_ONE * 2};
		} break;
		case ENTITY_ORB:{
            entity->bounce = true;
			entity->health = 0x200;
			entity->color_emit = (Vec3){FIXED_ONE / 3,FIXED_ONE,FIXED_ONE / 3};
			entity->hitable = true;
		} break;
		case ENTITY_BOMB:{
			entity->health = 0x100;
			entity->color_emit = (Vec3){FIXED_ONE,FIXED_ONE / 3,FIXED_ONE / 3};
		} break;
		case ENTITY_BOLT:{
            entity->bounce = true;
			entity->health = 0x200;
			entity->color_emit = (Vec3){FIXED_ONE / 3,FIXED_ONE / 3,FIXED_ONE};
		} break;
		case ENTITY_PICKUP:{
			entity->gravitate_player_freeze = 0x80;
            entity->has_hitbox = true;
            entity->hitbox = (Vec3){FIXED_ONE,FIXED_ONE,FIXED_ONE};
		} break;
		case ENTITY_STAFF:{
            entity->has_hitbox = true;
            entity->hitbox = (Vec3){FIXED_ONE,FIXED_ONE,FIXED_ONE};
			entity->gravitate_player_freeze = 0x80;
			entity->texture_dynamic = textureCreate(0x100);
            //ellipsoidModelGenerate(entity->position,&entity->texture_dynamic,entity->mo,g_entity_static[entity->type].n_model_sphere,(Vec2){entity->move_angle,0},true);
		} break;
		case ENTITY_SLIME:{
            entity->has_hitbox = true;
            entity->hitbox = (Vec3){FIXED_ONE / 2,FIXED_ONE / 2,FIXED_ONE / 2};
            entity->n_model_sphere = countof(model_sphere_slime);
            entity->model_sphere = model_sphere_slime;
			entity->health = FIXED_ONE / 2;
			entity->texture_dynamic = textureCreate(0x100);
			entity->hitable = true;
            entity->render_direction = vec3Direction(entity->position,g_surface.position);
		    ellipsoidModelGenerate(entity->position,&entity->texture_dynamic,entity->model_sphere,entity->n_model_sphere,(Vec2){entity->move_angle,0},true);
		} break;
		case ENTITY_ZOMBIE:{
            entity->hitbox = (Vec3){FIXED_ONE,FIXED_ONE,FIXED_ONE},
			entity->health = FIXED_ONE / 2 + FIXED_ONE / 4;
			entity->texture_dynamic = textureCreate(0x100);
			entity->hitable = true;
            entity->physics_stair = true;
			//ellipsoidModelGenerate(entity->position,&entity->texture_dynamic,g_entity_static[entity->type].model_sphere,g_entity_static[entity->type].n_model_sphere,(Vec2){entity->move_angle,0},true);
		} break;
		case ENTITY_MONSTER:{
            entity->hitbox = (Vec3){FIXED_ONE,FIXED_ONE,FIXED_ONE},
			entity->angle = (Vec2){tRnd() % FIXED_ONE,tRnd() % FIXED_ONE};
			entity->texture_dynamic = textureCreate(0x100);
			entity->health = FIXED_ONE;
			entity->pathfinding = tMallocZero(sizeof *entity->pathfinding);
			entity->pathfinding->route.n_positions = -1;
            entity->physics_stair = true;
            entity->hitable = true;
		    //ellipsoidModelGenerate(entity->position,&entity->texture_dynamic,g_entity_static[entity->type].model_sphere,g_entity_static[entity->type].n_model_sphere,(Vec2){entity->move_angle,0},true);
		} break;
		case ENTITY_PARTICLE:{
			entity->health = 0x8;
		} break;
	}
    entityAdd(entity);
	return entity;
}

void entityDestroy(void){
	Entity* previous = 0;
	for(Entity* entity_i = g_entity;entity_i;){
		if(entity_i->health > 0){
			previous = entity_i;
			entity_i = entity_i->next;
			continue;
		}
		if(entity_i->type == ENTITY_BOSS)
			g_boss = 0;
		if(previous)
			previous->next = entity_i->next;
		else
			g_entity = entity_i->next;
		Entity* next = entity_i->next;
		if(entity_i->texture_dynamic.pixel_data)
            textureDestroy(entity_i->texture_dynamic);
		
		Entity* entity_d = entity_i;
		entity_i = entity_i->next;
		tFree(entity_d);
	}
}

void entityDestroyAll(void){
	for(Entity* entity = g_entity;entity;){
		if(entity->texture_dynamic.pixel_data){
			deleteTextureGL(entity->texture_dynamic.gl_id);
			tFree(entity->texture_dynamic.pixel_data);
		}
		Entity* copy = entity;
		entity = entity->next;
		tFree(copy);
	}
	g_entity = 0;
}

void entitySpawn(void){
	int angle = tRnd() % (FIXED_ONE * 16);
	Vec2 direction = vec2MulS(vec2Shl((Vec2){tCos(angle),tSin(angle)},5),tRnd() % (FIXED_ONE / 2) + FIXED_ONE / 2);
	Vec2 position = vec2Add((Vec2){g_surface.position.x,g_surface.position.y},direction);

	int z_count = 0;
	struct{
		int height;
		VoxelType voxel_type;
	} z_list[0x10];
#if 0
	for(int z = 0;z < 512 && z_count != countof(z_list);z++){
		if(boxTreeCollision((Vec3){position.x,position.y,z * FIXED_ONE},(Vec3){FIXED_ONE,FIXED_ONE,FIXED_ONE},0,0).collided)
			continue;
		Voxel* ground = treeRayTraceAndInit((Vec3){position.x,position.y,z * FIXED_ONE},(Vec3){0,0,-FIXED_ONE},0);
		if(!ground)
			continue;
		VoxelType voxel_type = ground->type;
		z_list[z_count].height = z;
		z_list[z_count].voxel_type = voxel_type;
		z_count += 1;
		while(z < 512 && !boxTreeCollision((Vec3){position.x,position.y,z * FIXED_ONE},(Vec3){FIXED_ONE,FIXED_ONE,FIXED_ONE},0,0).collided)
			z += 1;
	}
#endif
	if(!z_count)
		return;
	int random_index = tRnd() % z_count;
	EntityType type;
	switch(z_list[random_index].voxel_type){
		case VOXEL_GRASS:{
			EntityType monster_selection[] = {ENTITY_SLIME};
			type = monster_selection[tRnd() % countof(monster_selection)];
		} break;
		case VOXEL_STONE: case VOXEL_STONE_BRICK:{
			EntityType monster_selection[] = {ENTITY_ZOMBIE,ENTITY_MONSTER};
			type = monster_selection[tRnd() % countof(monster_selection)];
		} break;
		default:
			return;
	}
	entityCreate((Vec3){position.x,position.y,z_list[random_index].height * FIXED_ONE},type);
}
#include "console.h"
static void entityPhysics(Entity* entity){

	int max_height_x = 0;
	int max_height_y = 0;

    Collision collision;

    bool in_air = movementUpdate(entity);
	
	entity->on_ground = !in_air;
#if 1
	int friction = in_air ? entity->physics_friction_air : entity->physics_friction_ground;
    friction = fixedMulR(friction,g_time.delta);
    entity->velocity = vec3Sub(entity->velocity,vec3MulS(entity->velocity,friction));
#endif
	if(!entity->no_gravity)
		entity->velocity.z -= fixedMulR(PHYSICS_GRAVITY,g_time.delta);
    
	if(entity->is_windy){
		entity->velocity = vec3Add(entity->velocity,entity->windy);
	}
}

static bool entityTickSlime(Entity* slime){
	if(slime->on_ground && tRndChance(0x80)){
		int angle;
		if(!g_player.movement_fly && lineOfSight(g_surface.position,slime->position)){
			int distance = vec2Distance((Vec2){slime->position.x,slime->position.y},(Vec2){g_surface.position.x,g_surface.position.y}) * 8;
			Vec2 offset = vec2MulS((Vec2){g_player.entity.velocity.x,g_player.entity.velocity.y},distance);
			Vec2 direction = vec2Direction((Vec2){slime->position.x,slime->position.y},vec2Add((Vec2){g_surface.position.x,g_surface.position.y},offset));
			slime->velocity.x += (direction.x) >> 4;
			slime->velocity.y += (direction.y) >> 4;
			angle = tArcTan2(direction.y,direction.x);
		}
		else{
			angle = tRnd() % FIXED_ONE;
			slime->velocity.x += (tCos(angle)) >> 4;
			slime->velocity.y += (tSin(angle)) >> 4;
		}
		slime->move_angle = -angle + FIXED_ONE / 2;
		slime->velocity.z += FIXED_ONE / 6;
	}

	if(slime->attack_cooldown > 0)
		slime->attack_cooldown -= 1;
#if 0
	else if(intersectBoxBox(playerHitboxGet(),PLAYER_SIZE,slime->position,slime->hitbox)){
		Vec2 direction = vec2Direction((Vec2){g_surface.position.x,g_surface.position.y},(Vec2){slime->position.x,slime->position.y});
		slime->velocity.x += (direction.x) >> 4;
		slime->velocity.y += (direction.y) >> 4;
		slime->velocity.z += FIXED_ONE / 6;

		g_player.entity.velocity.x -= (direction.x) >> 4;
		g_player.entity.velocity.y -= (direction.y) >> 4;
		g_player.entity.velocity.z += FIXED_ONE / 6;

		g_player.entity.health -= FIXED_ONE / 4;

		slime->attack_cooldown = 0x80;
	}
#endif
	return true;
}

static bool entityTickZombie(Entity* zombie){
	if(!zombie->on_ground)
		return true;

	if(!g_player.movement_fly && lineOfSight(g_surface.position,zombie->position)){
		int distance = vec2Distance((Vec2){zombie->position.x,zombie->position.y},(Vec2){g_surface.position.x,g_surface.position.y}) * 8;
		Vec2 offset = vec2MulS((Vec2){g_player.entity.velocity.x,g_player.entity.velocity.y},distance);
		Vec2 direction = vec2Direction((Vec2){zombie->position.x,zombie->position.y},vec2Add((Vec2){g_surface.position.x,g_surface.position.y},offset));
		zombie->velocity.x += direction.x >> 8;
		zombie->velocity.y += direction.y >> 8;

		zombie->move_angle = tArcTan2(direction.y,direction.x);
		zombie->is_moving = true;
	}
	else{
		zombie->velocity.x += (tCos(zombie->move_angle)) >> 10;
		zombie->velocity.y += (tSin(zombie->move_angle)) >> 10;
	}
	if(tRndChance(0x100)){
		zombie->is_moving = tRndChance(2);
		zombie->move_angle = tRnd() % FIXED_ONE;
	}
	return true;
}

static bool entityTickMonster(Entity* entity){
	if(!entity->on_ground)
		return true;
	bool down_z = entity->velocity.z < 0;
	
	if(!g_player.movement_fly && entity->pathfinding->cooldown <= 0){
		if(vec3Distance(entity->position,g_surface.position) < FIXED_ONE * 32){
			if(directPath(entity->position,entity->hitbox,g_surface.position)){
				entity->pathfinding->state = ENTITY_PATHFIND_DIRECT;
				entity->pathfinding->direct_position = g_surface.position;
			}
			else if(pathFinding(entity->position,entity->hitbox,g_surface.position,&entity->pathfinding->route)){
				entity->pathfinding->state = ENTITY_PATHFIND_ROUTE;
			}
		}
		entity->pathfinding->cooldown = tRnd() % 0x40 + 0x40;
	}
	switch(entity->pathfinding->state){
		case ENTITY_PATHFIND_DIRECT:{
			entity->velocity = vec3Add(entity->velocity,vec3Shr(vec3Direction(entity->position,entity->pathfinding->direct_position),8));
		} break;
		case ENTITY_PATHFIND_IDLE:{
			int r = tRnd();
			entity->velocity = vec3Add(entity->velocity,vec3Shr((Vec3){tCos(r),tSin(r),0},8));
		} break;
		case ENTITY_PATHFIND_ROUTE:{
			if(entity->pathfinding->route.n_positions < 0){
				entity->pathfinding->state = ENTITY_PATHFIND_IDLE;
				break;
			}
			Vec3 route_position = vec3Shl(entity->pathfinding->route.positions[entity->pathfinding->route.n_positions],PATH_FIND_SIZE);
		
			Vec2 direction = vec2Direction((Vec2){entity->position.x,entity->position.y},(Vec2){route_position.x,route_position.y});
			entity->velocity.x += direction.x >> 8;
			entity->velocity.y += direction.y >> 8;
			
			Vec3 relative_position = vec3Sub(entity->position,route_position);
			int distance = vec3Dot(relative_position,relative_position);
			if(entity->pathfinding->distance_route_node <= distance){
				entity->pathfinding->route.n_positions -= 1;
				entity->pathfinding->distance_route_node = INT_MAX;
			}
			else{
				entity->pathfinding->distance_route_node = distance;
			}
		} break;
	}
	entity->pathfinding->cooldown -= 1;
	return true;
}

static bool entityTickParticle(Entity* entity){
    if(g_options.lighting_engine){
        int n_sample = 0x40;
        int n_fibbonaci = 0x400;
        Vec3 lum_acc = {0};
        for(int i = n_sample;i--;){
            Vec3 direction = fibonnaciSphereSample(entity->n_luminance_sample % n_fibbonaci,n_fibbonaci);
            lum_acc = vec3Add(lum_acc,vec3Shl(rayLuminance(entity->position,direction),4));
            entity->n_luminance_sample += 1;
        }
        lum_acc.x /= n_sample;
        lum_acc.y /= n_sample;
        lum_acc.z /= n_sample;

        entity->luminance = vec3Mix(entity->luminance,lum_acc,FIXED_ONE / 16);
    }
    
	entity->health -= 1;
	if(entity->health <= 0)
		return false;
	return true;
}

static bool entityTickPickup(Entity* entity){
	Vec3 player_middle = vec3Add(g_surface.position,(Vec3){0,0,-FIXED_ONE / 2 - FIXED_ONE / 3});
	Vec3 relative = vec3Sub(entity->position,player_middle);
	if(vec3Dot(relative,relative) < FIXED_ONE * 16 && !entity->gravitate_player_freeze && !inventoryFull()){
		entity->position = vec3Add(entity->position,vec3Shr(vec3Direction(entity->position,player_middle),4));
		if(vec3Dot(relative,relative) < FIXED_ONE / 64){
			g_pickup_collected = true;
			audioPlay(vec3Add(g_surface.position,(Vec3){0,0,-FIXED_ONE}),AUDIO_ITEM_GAINED);
			int i = 0;
			InventorySlot* slot = g_inventory;
			while(slot->type)
				slot += 1;
			slot->type = INVENTORY_SPELL;
			slot->spell_type = entity->pickup_type;
			return false;
		}
	}
	if(entity->gravitate_player_freeze)
		entity->gravitate_player_freeze -= 1;
	return true;
}

static Entity* boltMonsterCollision(Voxel* voxel,Vec3 position){
	if(voxel->type == VOXEL_PARENT){
		for(int i = 0;i < 8;i++){
			Entity* collision = boltMonsterCollision(voxel->child_s[i],position);
			if(collision)
				return collision;
		}
		return 0;
	}
	for(Entity* entity = (voxel->entity_list);entity;entity = entity->next_voxel){
		if(!entity->hitable)
			continue;
		if(intersectBoxPoint(position,entity->position,entity->hitbox))
			return entity;
	}
	return 0;
}

void entityHit(Entity* entity){
	entity->health -= FIXED_ONE / 100 * 30;
	Vec3 death_position = entity->position;

	Vec2 knockback = vec2Direction((Vec2){g_surface.position.x,g_surface.position.y},(Vec2){entity->position.x,entity->position.y});
	entity->velocity.x += knockback.x / 8;
	entity->velocity.y += knockback.y / 8;
	entity->velocity.z += FIXED_ONE / 6;
	for(int i = 0x100;i--;){
		Vec3 velocity = (Vec3){tRnd() % FIXED_ONE * 2 - FIXED_ONE,tRnd() % FIXED_ONE * 2 - FIXED_ONE,tRnd() % FIXED_ONE * 2};
		if(vec2Dot((Vec2){velocity.x,velocity.y},(Vec2){velocity.x,velocity.y}) > FIXED_ONE)
			continue;
		Entity* particle = entityCreate(death_position,ENTITY_PARTICLE);
		particle->color = (Vec3){
            tRnd() % FIXED_ONE / 8,
            tRnd() % FIXED_ONE / 8,
            tRnd() % FIXED_ONE / 2 + FIXED_ONE / 2,
        };
		particle->health = tRnd() % 0x80 + 0x80;
		particle->size = FIXED_ONE / 16;
		particle->velocity = vec3Shr(vec3Mix(velocity,vec3Direction(g_surface.position,death_position),FIXED_ONE / 2),3);
	}
}

static void spellAdjectiveParticleSpawn(Entity* entity){
	if(entity->adj_damage && tRndChance(16)){
		Entity* particle = entityCreate(entity->position,ENTITY_PARTICLE);
		particle->color = (Vec3){FIXED_ONE << 4,FIXED_ONE,FIXED_ONE};
		particle->health = tRnd() % 0x80 + 0x80;
		particle->size = FIXED_ONE / 16;
		particle->velocity = vec3Shr(vec3Rnd(),4);
		particle->particle_string = (String)STRING_LITERAL("#+");
	}
	if(entity->adj_speed && tRndChance(16)){
		Entity* particle = entityCreate(entity->position,ENTITY_PARTICLE);
		particle->color = (Vec3){FIXED_ONE,FIXED_ONE,FIXED_ONE << 4};
		particle->health = tRnd() % 0x80 + 0x80;
		particle->size = FIXED_ONE / 16;
		particle->velocity = vec3Shr(vec3Rnd(),4);
		particle->particle_string = (String)STRING_LITERAL(">>");
	}
}

static bool entityTickBolt(Entity* entity){
	entity->health -= 1;
	if(entity->health <= 0)
		return false;
	Entity* entity_collided = boltMonsterCollision(&g_voxel,entity->position);
	spellAdjectiveParticleSpawn(entity);
	if(entity_collided){
		entityHit(entity_collided);
		return false;
	}
	return true;
}

static bool entityTickStaff(Entity* entity){
	Vec3 player_middle = vec3Add(g_surface.position,(Vec3){0,0,-FIXED_ONE / 2 - FIXED_ONE / 3});
	Vec3 relative = vec3Sub(entity->position,player_middle);
	if(vec3Dot(relative,relative) < FIXED_ONE * 256 && !g_equipped_staff && !entity->gravitate_player_freeze){
		entity->position = vec3Add(entity->position,vec3Shr(vec3Direction(entity->position,player_middle),4));
		if(vec3Dot(relative,relative) < FIXED_ONE / 64){
			g_equipped = entity->staff;
			g_equipped_staff = true;
			staffEditorCreateMenu(&g_equipped);
			audioPlay(vec3Add(g_surface.position,(Vec3){0,0,-FIXED_ONE}),AUDIO_ITEM_GAINED);
			return false;
		}
	}
	if(entity->gravitate_player_freeze)
		entity->gravitate_player_freeze -= 1;
	return true;
}

static bool entityTickBomb(Entity* entity){
	entity->health -= 1;
	if(entity->health <= 0){
		if(lineOfSight(entity->position,g_surface.position)){
			int distance = vec3Distance(g_surface.position,entity->position);
			int shock = fixedDivR(FIXED_ONE,fixedMulR(distance,distance));
			g_player.entity.velocity = vec3Add(g_player.entity.velocity,vec3MulS(vec3Direction(entity->position,g_surface.position),shock));
			g_player.entity.health -= shock * 8;
		}
		for(Entity* entity_other = g_entity;entity_other;entity_other = entity_other->next){
			if(entity == entity_other)
				continue;
			if(!lineOfSight(entity->position,entity_other->position))
				continue;
			int distance = vec3Distance(entity_other->position,entity->position);
			int shock = fixedMulR(distance,distance);
			entity_other->velocity = vec3Add(entity_other->velocity,vec3MulS(vec3Direction(entity->position,entity_other->position),fixedDivR(FIXED_ONE * 4,fixedMulR(distance,distance))));
			entity_other->health -= shock / 8;
		}
		for(int i = 0x100;i--;){ 
			Entity* particle = entityCreate(entity->position,ENTITY_PARTICLE);
			particle->health = tRnd() % 0x400;
			particle->size = FIXED_ONE / 4 + tRnd() % (FIXED_ONE / 4);
			particle->velocity = vec3Shr(vec3Rnd(),2);
			particle->color = pixelColorToColor(0x808080);
			particle->no_gravity =true;
            particle->is_windy = true;
			particle->windy = vec3Shr(vec3MulS(vec3Rnd(),tRnd() % FIXED_ONE),11);
			particle->physics_friction_air = (FIXED_ONE - (FIXED_ONE >> 4));
			particle->texture = g_textures + TEXTURE_SMOKE;
			particle->texture_offset = (Vec2){tRnd() % FIXED_ONE,tRnd() % FIXED_ONE};
			particle->texture_size = FIXED_ONE / 4;
		}
		for(int i = 0x10;i--;){
			Entity* particle = entityCreate(entity->position,ENTITY_PARTICLE);
			particle->health = tRnd() % 0x40 + 0x40;
			particle->size = FIXED_ONE / 16;
			particle->velocity = vec3Shr(vec3Rnd(),2);
			particle->color = vec3Shl(pixelColorToColor(0x204080),4);
			particle->color_emit = vec3Shr(particle->color,4);
		}
		audioPlay(entity->position,AUDIO_EXPLOSION);
		return false;
	}
	spellAdjectiveParticleSpawn(entity);
	Entity* entity_collided = boltMonsterCollision(&g_voxel,entity->position);
	if(entity_collided){
		//monsterHit(entity_collided);
		return false;
	}
	return true;
}

static bool entityTickBoss(Entity* boss){
	if(boss->on_ground && tRndChance(0x80)){
		int angle;
		if(!g_player.movement_fly && lineOfSight(g_surface.position,boss->position)){
			int distance = vec2Distance((Vec2){boss->position.x,boss->position.y},(Vec2){g_surface.position.x,g_surface.position.y}) * 8;
			Vec2 offset = vec2MulS((Vec2){g_player.entity.velocity.x,g_player.entity.velocity.y},distance);
			Vec2 direction = vec2Direction((Vec2){boss->position.x,boss->position.y},vec2Add((Vec2){g_surface.position.x,g_surface.position.y},offset));
			boss->velocity.x += (direction.x) >> 4;
			boss->velocity.y += (direction.y) >> 4;
			angle = tArcTan2(direction.y,direction.x);
		}
		else{
			angle = tRnd() % FIXED_ONE;
			boss->velocity.x += (tCos(angle)) >> 4;
			boss->velocity.y += (tSin(angle)) >> 4;
		}
		boss->move_angle = -angle + FIXED_ONE / 2;
		boss->velocity.z += FIXED_ONE / 6;
	}

	if(boss->attack_cooldown > 0)
		boss->attack_cooldown -= 1;
#if 0
	else if(intersectBoxBox(playerHitboxGet(),PLAYER_SIZE,boss->position,boss->hitbox)){
		Vec2 direction = vec2Direction((Vec2){g_surface.position.x,g_surface.position.y},(Vec2){boss->position.x,boss->position.y});
		boss->velocity.x += (direction.x) >> 4;
		boss->velocity.y += (direction.y) >> 4;
		boss->velocity.z += FIXED_ONE / 6;

		g_player.entity.velocity.x -= (direction.x) >> 4;
		g_player.entity.velocity.y -= (direction.y) >> 4;
		g_player.entity.velocity.z += FIXED_ONE / 6;

		g_player.entity.health -= FIXED_ONE / 4;

		boss->attack_cooldown = 0x80;
	}
#endif
	return true;
}

static int punchAnimationOffset(int animation){
	int value = fixedMulR(fixedMulR(animation,animation),animation);
	return tCos(tAbs(FIXED_ONE / 2 - value)) + FIXED_ONE;
}
#include "console.h"
static bool entityTickWeapon(Entity* entity){
    Vec3 direction_x = getLookDirection(vec2Add(g_surface.angle,(Vec2){0,0}));
    Vec3 direction_y = getLookDirection(vec2Add(g_surface.angle,(Vec2){0,FIXED_ONE / 4}));
    Vec3 direction_z = vec3Cross(direction_x,direction_y);

    Vec3 wish_position = g_surface.position;

    Vec3 fist = {FIXED_ONE + punchAnimationOffset(entity->attack_cooldown),FIXED_ONE,FIXED_ONE};
            
    wish_position = vec3Add(wish_position,vec3MulS(direction_x,fist.x));
    wish_position = vec3Add(wish_position,vec3MulS(direction_y,fist.y));
    wish_position = vec3Add(wish_position,vec3MulS(direction_z,fist.z));

    wish_position = vec3Sub(wish_position,g_player.entity.velocity);

    wish_position = vec3Mix(entity->position,wish_position,FIXED_ONE / 2);

    entity->velocity = vec3Shl(vec3Sub(wish_position,entity->position),4);

    entity->attack_cooldown = tMax(entity->attack_cooldown - g_time.delta / 8,0);
    
    return true;
}

void entityTick(void){
	bool (*entity_tick[])(Entity*) = {
		[ENTITY_STAFF] = entityTickStaff,
		[ENTITY_BOLT] = entityTickBolt,
		[ENTITY_BOMB] = entityTickBomb,
		[ENTITY_ORB] = entityTickBolt,
		[ENTITY_MONSTER] = entityTickMonster,
		[ENTITY_PARTICLE] = entityTickParticle,
		[ENTITY_PICKUP] = entityTickPickup,
		[ENTITY_ZOMBIE] = entityTickZombie,
		[ENTITY_SLIME] = entityTickSlime,
		[ENTITY_BOSS] = entityTickBoss,
        [ENTITY_WEAPON] = entityTickWeapon,
	};
	Entity* previous = 0;
	for(Entity* entity = g_entity;entity;entity = entity->next){
		if(entity->type >= countof(entity_tick) || !entity_tick[entity->type])
			continue;
		if(!entity_tick[entity->type](entity))
			entity->health = 0;
        entityPhysics(entity);
		previous = entity;
	}
}

void entityInit(void){
	/*
#if !defined(__wasm__) && !defined(__linux__)
	g_monster_model = win32LoadModel("model/monster.octvxl");
#endif
	*/
	//voxelSet()
}

int entitySpriteSize(Vec3 position,int size){
	Vec3 plane_normal = getLookDirection(g_surface.angle);
	int plane_distance = -vec3Dot(plane_normal,g_surface.position);
	return fixedDivR(size,sdPlane(position,plane_normal,plane_distance));
}

static void entityRender3d(Entity* entity,int entity_size){
#if 0
	for(int i = 0x40;i--;){
		Vec3 direction = getLookDirection((Vec2){tRnd() % FIXED_ONE,tRnd() % FIXED_ONE});
		Vec3 luminance = rayLuminance(entity->position,direction);
		for(int i = 0;i < 3;i++){
			int offset = vec3Dot(direction,g_normal_table[i * 2]) < 0;
			entity->n_luminance_sample_3d[i * 2 + offset] += 1;
			entity->luminance_3d[i * 2 + offset] = vec3Mix(entity->luminance_3d[i * 2 + offset],luminance,FIXED_ONE / tMin(entity->n_luminance_sample_3d[i * 2 + offset],0x400));
		}
	}
#endif
	Vec3 point = pointToScreen(entity->position);
	if(!point.z)
		return;
	int size = entitySpriteSize(entity->position,entity_size);
	Vec2 points[] = {
		{point.x - fixedMulR(size,g_options.fov.y),point.y - fixedMulR(size,g_options.fov.x)},
		{point.x - fixedMulR(size,g_options.fov.y),point.y + fixedMulR(size,g_options.fov.x)},
		{point.x + fixedMulR(size,g_options.fov.y),point.y + fixedMulR(size,g_options.fov.x)},
		{point.x + fixedMulR(size,g_options.fov.y),point.y - fixedMulR(size,g_options.fov.x)},
	};

    Vec3 to_player = vec3Direction(entity->position,g_surface.position);
    int discrepancy = vec3LengthSquare(vec3Sub(entity->render_direction,to_player));
    
	if(discrepancy > 0x100){
		ellipsoidModelGenerate(entity->position,&entity->texture_dynamic,entity->model_sphere,entity->n_model_sphere,(Vec2){entity->move_angle,0},true);
        entity->render_direction = to_player;
    }
   
    DrawPrimitive* primitive = primitiveToDraw();
    primitive->texture = &entity->texture_dynamic;
    primitive->is_sprite = true;
    for(int i = 4;i--;){
        primitive->position_sprite[i] = points[i];
        primitive->texture_crd[i] = g_texture_coordinates_fill[i];
    }
}

void entityDraw(Entity* entity){
	switch(entity->type){
        case ENTITY_WEAPON:{
            ModelEllipsoid model_hand[] = {
                {
                    .color = {FIXED_ONE,FIXED_ONE,FIXED_ONE},
                    .radius = {FIXED_ONE,FIXED_ONE / 4,FIXED_ONE / 4},
                },
            };
            ModelEllipsoid model_staff[] = {
                {
                    .color = {FIXED_ONE,FIXED_ONE / 2,FIXED_ONE / 4},
                    .radius = {FIXED_ONE,FIXED_ONE / 4,FIXED_ONE / 4},
                },
            };
            Vec3 position = entity->position;
            int size = entitySpriteSize(position,FIXED_ONE);
            ModelEllipsoid* model = g_equipped_staff ? model_staff : model_hand;
            int n_model = g_equipped_staff ? countof(model_staff) : countof(model_hand);

            int discrepancy = vec3DistanceSquare(entity->position,entity->render_position);
            
            if(discrepancy > 0x4000){
                ellipsoidModelGenerate(position,&entity->texture_dynamic,model,n_model,(Vec2){2500,-4000},false);
                entity->render_position = entity->position;
            }
            
            Vec3 point = pointToScreen(position);
            if(!point.z)
                break;
            Vec2 points[] = {
                {point.x - fixedMulR(size,g_options.fov.y),point.y - fixedMulR(size,g_options.fov.x)},
                {point.x - fixedMulR(size,g_options.fov.y),point.y + fixedMulR(size,g_options.fov.x)},
                {point.x + fixedMulR(size,g_options.fov.y),point.y + fixedMulR(size,g_options.fov.x)},
                {point.x + fixedMulR(size,g_options.fov.y),point.y - fixedMulR(size,g_options.fov.x)},
            };
            DrawPrimitive* polygon = primitiveToDraw();
            polygon->texture = &entity->texture_dynamic;
            polygon->is_sprite = true;
            for(int i = 4;i--;){
                polygon->position_sprite[i] = points[i];
                polygon->texture_crd[i] = g_texture_coordinates_fill[i];
            }
        } break;
		case ENTITY_BOMB:{
			Vec3 point = pointToScreen(entity->position);
			if(!point.z)
				return;
			int size = entitySpriteSize(entity->position,FIXED_ONE / 8);
			Vec2 points[] = {
				{point.x - fixedMulR(size,g_options.fov.x),point.y - fixedMulR(size,g_options.fov.y)},
				{point.x + fixedMulR(size,g_options.fov.x),point.y - fixedMulR(size,g_options.fov.y)},
				{point.x + fixedMulR(size,g_options.fov.x),point.y + fixedMulR(size,g_options.fov.y)},
				{point.x - fixedMulR(size,g_options.fov.x),point.y + fixedMulR(size,g_options.fov.y)},
			};
			drawEllipses(&g_surface,point.x,point.y,fixedMulR(size * 4,g_options.fov.x),fixedMulR(size * 4,g_options.fov.y),vec3MulS(vec3Single(1 << 14),g_exposure));
			drawEllipses(&g_surface,point.x - size / 2,point.y + size / 2,fixedMulR(size,g_options.fov.x),fixedMulR(size,g_options.fov.y),vec3MulS(vec3Single(1 << 18),g_exposure));
			drawRectangle(&g_surface,point.x - size * 2 - size / 4,point.y - size / 2,fixedMulR(size,g_options.fov.x),fixedMulR(size,g_options.fov.y) * 4,vec3Single(1 << 16));
			int fuse = fixedMulR(fixedMulR(size,g_options.fov.x),entity->health << 10);
			drawRectangle(&g_surface,point.x - size * 2 - size / 4 - fuse / 2,point.y,fixedMulR(fuse,g_options.fov.x),fixedMulR(size,g_options.fov.y),pixelColorToColor(0x83B2EB));
			drawRectangle(&g_surface,point.x - size * 2 - size / 4 - fuse / 2 - size / 2,point.y,fixedMulR(size,g_options.fov.x),fixedMulR(size,g_options.fov.y),pixelColorToColor(0x1050FF));
		} break;
		case ENTITY_BOLT: case ENTITY_ORB:{
			Vec3 point = pointToScreen(entity->position);
			if(!point.z)
				return;
			int size = entitySpriteSize(entity->position,FIXED_ONE / 8);
            if(g_surface.backend == RENDER_BACKEND_SOFTWARE){
                spanEllipsesAdd(&g_surface,point.x,point.y,fixedMulR(size,g_options.fov.x),fixedMulR(size,g_options.fov.y),vec3MulS(entity->color_emit,g_exposure));
                return;
            }
            DrawPrimitive* polygon = primitiveToDraw();
            polygon->type = PRIMITIVE_ELLIPSIS;
            polygon->is_sprite = true;
            polygon->sprite_size = (Vec2){fixedMulR(size,g_options.fov.x),fixedMulR(size,g_options.fov.y)};
            polygon->position_sprite[0] = (Vec2){point.x,point.y};
            polygon->luminance = entity->color_emit;
        } break;
		case ENTITY_STAFF:{
			entityRender3d(entity,FIXED_ONE);
		} break;
		case ENTITY_BOSS:{
			entityRender3d(entity,FIXED_ONE * 2);
		} break;
		case ENTITY_MONSTER: case ENTITY_SLIME: case ENTITY_ZOMBIE:{
			entityRender3d(entity,FIXED_ONE);
		} break;
		case ENTITY_PARTICLE:{
			for(int i = 0x40;i--;){
				Vec3 luminance = rayLuminance(entity->position,getLookDirection((Vec2){tRnd() % FIXED_ONE,tRnd() % FIXED_ONE}));
				entity->n_luminance_sample += 1;
				entity->luminance = vec3Mix(entity->luminance,vec3Shl(luminance,4),tMin(FIXED_ONE / entity->n_luminance_sample,0x400));
			}
			Vec3 point = pointToScreen(entity->position);
			if(!point.z)
				return;
			int size = entitySpriteSize(entity->position,entity->size);
            Vec2 points[] = {
				{point.x + fixedMulR(size,g_options.fov.x),point.y + fixedMulR(size,g_options.fov.y)},
				{point.x + fixedMulR(size,g_options.fov.x),point.y - fixedMulR(size,g_options.fov.y)},
				{point.x - fixedMulR(size,g_options.fov.x),point.y - fixedMulR(size,g_options.fov.y)},
				{point.x - fixedMulR(size,g_options.fov.x),point.y + fixedMulR(size,g_options.fov.y)},
			};
			Vec3 color = vec3MulS(vec3Mul(entity->luminance,entity->color),g_exposure);
			if(entity->texture){
                Vec2 texture_coordinates[] = {
                    {entity->texture_offset.x,entity->texture_offset.y},
                    {entity->texture_offset.x,entity->texture_offset.y + entity->texture_size},
                    {entity->texture_offset.x + entity->texture_size,entity->texture_offset.y + entity->texture_size},
                    {entity->texture_offset.x + entity->texture_size,entity->texture_offset.y},
                };
                DrawPrimitive* primitive = primitiveToDraw();
                primitive->is_sprite = true;
                primitive->texture = entity->texture;
                primitive->has_lighting = true;
                primitive->luminance = entity->luminance;
                for(int i = 4;i--;){
                    primitive->texture_crd[i] = texture_coordinates[i];
                    primitive->position_sprite[i] = points[i];
                }
			}
			else if(entity->particle_string.data){
                drawString(&g_surface,point.x,point.y,entity->particle_string,size,COLOR_WHITE);
			}
			else{
                DrawPrimitive* primitive = primitiveToDraw();
                primitive->luminance = color;
                primitive->is_sprite = true;
                for(int i = 4;i--;)
                    primitive->position_sprite[i] = points[i];
#if 0
                if(g_surface.backend == RENDER_BACKEND_SOFTWARE){
                    Vec2 points[] = {
                        {point.x + fixedMulR(size,g_options.fov.x),-point.y + fixedMulR(size,g_options.fov.y)},
                        {point.x + fixedMulR(size,g_options.fov.x),-point.y - fixedMulR(size,g_options.fov.y)},
                        {point.x - fixedMulR(size,g_options.fov.x),-point.y - fixedMulR(size,g_options.fov.y)},
                        {point.x - fixedMulR(size,g_options.fov.x),-point.y + fixedMulR(size,g_options.fov.y)},
                    };
                    spanQuadAdd(&g_surface,points,color);
                }
                else{
                    drawPolygon(&g_surface,points,4,color);
                }
#endif
			}
		} break;
		case ENTITY_PICKUP:{
			Vec3 point = pointToScreen(entity->position);
			if(!point.z)
				return;
			int size = entitySpriteSize(entity->position,FIXED_ONE / 2);
			Vec2 points[] = {
				{point.x - fixedMulR(size,g_options.fov.x),point.y - fixedMulR(size,g_options.fov.y)},
				{point.x + fixedMulR(size,g_options.fov.x),point.y - fixedMulR(size,g_options.fov.y)},
				{point.x + fixedMulR(size,g_options.fov.x),point.y + fixedMulR(size,g_options.fov.y)},
				{point.x - fixedMulR(size,g_options.fov.x),point.y + fixedMulR(size,g_options.fov.y)},
			};

            DrawPrimitive* primitive = primitiveToDraw();
            primitive->is_sprite = true;
            primitive->type = PRIMITIVE_FRAME;
            primitive->position_sprite[0] = (Vec2){point.x - fixedMulR(size / 2,g_options.fov.x),point.y - fixedMulR(size / 2,g_options.fov.y)};
            primitive->sprite_size = (Vec2){fixedMulR(size,g_options.fov.x),fixedMulR(size,g_options.fov.y)};
            primitive->luminance = pixelColorToColor(0x808080);
            primitive->thickness = fixedMulR(size,g_options.fov.x) >> 4;
#if 0
			drawFrame(&g_surface,point.x - fixedMulR(size / 2,g_options.fov.x),point.y - fixedMulR(size / 2,g_options.fov.y),fixedMulR(size,g_options.fov.x),fixedMulR(size,g_options.fov.y),pixelColorToColor(0x808080),fixedMulR(size,g_options.fov.x) >> 4);
#endif
            switch(entity->pickup_type){
				case SPELL_BOLT:{
					drawEllipses(&g_surface,point.x,point.y,fixedMulR(size / 3,g_options.fov.x),fixedMulR(size / 3,g_options.fov.y),vec3MulS(pixelColorToColor(0xFF0000),g_exposure));
				} break;
				case SPELL_BOMB:{
					drawEllipses(&g_surface,point.x,point.y,fixedMulR(size / 3,g_options.fov.x),fixedMulR(size / 3,g_options.fov.y),vec3MulS(pixelColorToColor(0x0000FF),g_exposure));
				} break;
				case SPELL_ORB:{
					drawEllipses(&g_surface,point.x,point.y,fixedMulR(size / 3,g_options.fov.x),fixedMulR(size / 3,g_options.fov.y),vec3MulS(pixelColorToColor(0x00FF00),g_exposure));
				} break;
			}
			//drawTexturePolygon(g_surface,g_textures + TEXTURE_PICKUP,g_texture_coordinates_fill,points,vec3Single(1 << 20),4);
		} break;
	}
}

void entityDrawHitbox(void){
	for(Entity* entity = g_entity;entity;entity = entity->next){
        if(entity->has_hitbox){
            Vec3 dst = vec3Add(entity->position,getLookDirection((Vec2){entity->move_angle,0}));
            drawLine3d(&g_surface,entity->position,dst,0xFF0000);
            boxQuadWireframeDraw(vec3Sub(entity->position,entity->hitbox),vec3Shl(entity->hitbox,1),0x00FF00,false);
        }
        else{
            int size = 0x100;
            Vec3 point = pointToScreen(entity->position);
            Vec2 points[] = {
				{point.x + fixedMulR(size,g_options.fov.x),point.y + fixedMulR(size,g_options.fov.y)},
				{point.x + fixedMulR(size,g_options.fov.x),point.y - fixedMulR(size,g_options.fov.y)},
				{point.x - fixedMulR(size,g_options.fov.x),point.y - fixedMulR(size,g_options.fov.y)},
				{point.x - fixedMulR(size,g_options.fov.x),point.y + fixedMulR(size,g_options.fov.y)},
			};
            drawPolygon(&g_surface,points,4,pixelColorToColor(0x00FF00));
        }
	}	
}

void entityDynamicLighting(void){
	for(Entity* entity = g_entity;entity;entity = entity->next){
		if(!entity->emit)
			continue;
        lightingEntityDynamic(&g_voxel,entity);
	}	
}

void entityVoxelInsertSimulation(void){
    for(Entity* entity = g_entity;entity;entity = entity->next){
		Voxel* voxel = voxelPositionGet(entity->position);
        
		if(!g_voxel_static[voxel->type].translucent || entity->non_interactive)
			continue;
        
		if(voxel->entity_list){
            entity->next_voxel = voxel->entity_list;
            voxel->entity_list = entity;
		}
		else{
			voxel->entity_list = entity;
			entity->next_voxel = 0;
		}
        entity->inside = voxel;
	}
}

void entityVoxelInsertRender(void){
	for(Entity* entity = g_entity;entity;entity = entity->next){
		Voxel* voxel = voxelPositionGet(entity->position);
		if(!g_voxel_static[voxel->type].translucent)
			continue;
		if(voxel->entity_list){
			int distance = vec3Distance(entity->position,g_surface.position);
			if(vec3Distance(voxel->entity_list->position,g_surface.position) < distance){
				entity->next_voxel = voxel->entity_list;
				voxel->entity_list = entity;
			}
			else{
				Entity* prev = voxel->entity_list;
				Entity* entity_l = prev->next_voxel;
				
				while(entity_l && vec3Distance(entity_l->position,g_surface.position) > distance){
					prev = entity_l;
					entity_l = entity_l->next_voxel;
				}
				if(entity_l){
					entity->next_voxel = entity_l;
					prev->next_voxel = entity;
				}
				else{
					prev->next_voxel = entity;
					entity->next_voxel = 0;
				}
			}
		}
		else{
			voxel->entity_list = entity;
			entity->next_voxel = 0;
		}
        entity->inside = voxel;
	}
}

void entityVoxelRemove(void){
	for(Entity* entity = g_entity;entity;entity = entity->next){
        if(!entity->inside)
            continue;
        entity->inside->entity_list = 0;
		entity->next_voxel = 0;
    }
}
