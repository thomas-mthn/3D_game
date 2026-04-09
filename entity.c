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

#if !defined(__wasm__) && !defined(__linux__)
#include "win32/w_draw_opengl.h"
#include "win32/w_main.h"
#endif

ModelSphere model_sphere_slime[] = {
	{
		.color = {FIXED_ONE / 4,FIXED_ONE,FIXED_ONE / 4},
		.radius = FIXED_ONE / 2,
	},
	{
		.position = {0,-FIXED_ONE / 3,-FIXED_ONE / 3},
		.color = {FIXED_ONE,FIXED_ONE / 4,FIXED_ONE / 4},
		.radius = FIXED_ONE / 6,
	},
	{
		.position = {0,FIXED_ONE / 3,-FIXED_ONE / 3},
		.color = {FIXED_ONE,FIXED_ONE / 4,FIXED_ONE / 4},
		.radius = FIXED_ONE / 6,
	},
};

EntityStatic g_entity_static[] = {
	[ENTITY_PICKUP] = {
		.emit = true,
		.has_hitbox = true,
		.hitbox = {FIXED_ONE,FIXED_ONE,FIXED_ONE},
	},
	[ENTITY_STAFF] = {
		.emit = true,
		.has_hitbox = true,
		.hitbox = {FIXED_ONE,FIXED_ONE,FIXED_ONE},
	},
	[ENTITY_BOSS] = {
		.has_hitbox = true,
		.hitbox = {FIXED_ONE * 2,FIXED_ONE * 2,FIXED_ONE * 2},
		.health = FIXED_ONE * 4,
	},
	[ENTITY_MONSTER] = {
		.hitbox = {FIXED_ONE,FIXED_ONE,FIXED_ONE},
	},
	[ENTITY_SLIME] = {
		.n_model_sphere = countof(model_sphere_slime),
		.model_sphere = model_sphere_slime,
		.hitbox = {FIXED_ONE,FIXED_ONE,FIXED_ONE},
	},
	[ENTITY_ZOMBIE] = {
		.hitbox = {FIXED_ONE,FIXED_ONE,FIXED_ONE},
	},
	[ENTITY_BOLT] = {
		.emit = true,
		.bounce = true,
	},
	[ENTITY_ORB] = {
		.emit = true,
		.bounce = true,
	},
	[ENTITY_BOMB] = {
		.bounce = false,
	},
};

Entity* g_entity;

Entity* entityRayCollision(Entity* entity_list,Vec3 position,Vec3 direction){
	Entity* entity_closest = 0;
	for(Entity* entity = entity_list;entity;entity = entity->next_voxel){
		EntityStatic* entity_s = g_entity_static + entity->type;
		if(!(entity->flags & ENTITY_FLAG_HITABLE))
			continue;
		if(rayBoxIntersection(entity->position,vec3ShrR(entity_s->hitbox,1),position,direction))
			entity_closest = entity;
	}
	return entity_closest;
}

static Voxel monster_model;
static Voxel slime_model;
static Voxel zombie_model;
static Voxel boss_model;

static Vec3 sampleFibonacciSphere(int i,int n){
    const int GOLDEN_ANGLE = 157286;

    int z = FIXED_ONE - ((i * 2 + 1) * FIXED_ONE) / n;

    int zz = fixedMulR(z,z);
    int r = tSqrt(FIXED_ONE - zz);

    int phi = fixedFract(i * GOLDEN_ANGLE);

    int x = fixedMulR(r,tCos(phi));
    int y = fixedMulR(r,tSin(phi));

    return (Vec3){x,y,z};
}

static void entityRender3dGenerateSprite(Entity* entity){
	struct{
		Vec3 direction;
		Vec3 luminance;
	} ray_array[0x100];

	Vec3 direction = vec3Direction(g_position,entity->position);
	Vec2 angle = getLookAngle(direction);
	angle.x += entity->move_angle;
	direction = getLookDirection(angle);

	for(int i = 0;i < countof(ray_array);i++){
		Vec3 ray_direction = vec3Normalize(sampleFibonacciSphere(i,countof(ray_array)));

		ray_array[i].direction = ray_direction;
		ray_array[i].luminance = rayLuminance(entity->position,ray_direction);
	}
	int camera_distance = vec3Distance(g_position,entity->position);
	Vec3 camera_position = vec3MulRS(direction,camera_distance);
	Vec3 ray_origin = vec3SubR(entity->position,vec3MulRS(direction,camera_distance));

	for(int i = 0;i < entity->texture_dynamic.size * entity->texture_dynamic.size;i++)
		entity->texture_dynamic.pixel_data[i] = 0xFF000000;

	int tri[] = {tCos(angle.x),tSin(angle.x),tCos(angle.y),tSin(angle.y)};

	for(int i = 0;i < entity->texture_dynamic.size * entity->texture_dynamic.size;i++){
		int x = i / entity->texture_dynamic.size * FIXED_ONE * 2 / entity->texture_dynamic.size - FIXED_ONE;
		int y = i % entity->texture_dynamic.size * FIXED_ONE * 2 / entity->texture_dynamic.size - FIXED_ONE;
		Vec2 fov = {camera_distance,camera_distance};
		Vec3 direction = vec3Normalize(screenRayDirection(tri,x,y,fov.x,fov.y));
			
		int min_distance = INT_MAX;
		ModelSphere* model_sphere = 0;
		ModelSphere* model_sphere_array = g_entity_static[entity->type].model_sphere;

		if(!model_sphere_array){
			min_distance = raySphereIntersection(camera_position,direction,(Vec3){0,0,0},FIXED_ONE >> 1);
		}
		else{
			for(int i = 0;i < g_entity_static[entity->type].n_model_sphere;i++){
				ModelSphere* sphere = model_sphere_array + i;
				int distance = raySphereIntersection(camera_position,direction,sphere->position,sphere->radius);
				if(distance == -1 || distance > min_distance)	
					continue;
				min_distance = distance;
				model_sphere = sphere;
			}
		}
		if(min_distance == INT_MAX || min_distance == -1){
			entity->texture_dynamic.pixel_data[i] = 0xFF000000;
			continue;
		}

		Vec3 color = model_sphere ? model_sphere->color : vec3Single(FIXED_ONE);

		Vec3 hit_position = vec3AddR(ray_origin,vec3MulRS(direction,min_distance));
		Vec3 reflect_vector = vec3Reflect(direction,vec3Direction(hit_position,entity->position));
		Vec3 color_acc = {0};
		for(int i = 0;i < countof(ray_array);i++){
			int strength = vec3Dot(ray_array[i].direction,reflect_vector);
			if(strength < 0)
				continue;
			vec3Add(&color_acc,vec3MulRS(ray_array[i].luminance,strength));
		}
		vec3DivS(&color_acc,countof(ray_array));
		entity->texture_dynamic.pixel_data[i] = colorToPixelColor(vec3MulR(color,vec3MulRS(vec3ShrR(color_acc,10),g_exposure)));
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
	generateMipmaps(&entity->texture_dynamic);
#if !defined(__wasm__) && !defined(__linux__)
	textureUpdateGL(&entity->texture_dynamic);
#endif
	//surface_model.data = 0;
	//surfaceDestroy(&surface_model);
	entity->render_angle = angle;
	entity->render_position = entity->position;
}
	
Entity* entityCreate(Vec3 position,EntityType type){
	Entity* entity = tMallocZero(sizeof(Entity));
	entity->position = position;
	entity->type = type;
	entity->health = 1;
	entity->physics_friction_ground = PHYSICS_FRICTION_GROUND;
	entity->physics_friction_air = PHYSICS_FRICTION_AIR;
	entity->color_emit = (Vec3){FIXED_ONE / 2,FIXED_ONE / 2,FIXED_ONE / 2};
	switch(type){
		case ENTITY_BOSS:{
			entity->flags = ENTITY_FLAG_HITABLE;
			entity->model = &boss_model;
			entity->health = g_entity_static[type].health;
			entity->texture_dynamic = (Texture){.size = 0x100,.pixel_data = tMalloc(0x100 * 0x100 * sizeof(unsigned) * 2)};
		} break;
		case ENTITY_ORB:{
			entity->health = 0x200;
			entity->color_emit = (Vec3){FIXED_ONE / 3,FIXED_ONE,FIXED_ONE / 3};
			entity->flags = ENTITY_FLAG_NO_GRAVITY;
		} break;
		case ENTITY_BOMB:{
			entity->health = 0x100;
			entity->color_emit = (Vec3){FIXED_ONE,FIXED_ONE / 3,FIXED_ONE / 3};
		} break;
		case ENTITY_BOLT:{
			entity->health = 0x200;
			entity->color_emit = (Vec3){FIXED_ONE / 3,FIXED_ONE / 3,FIXED_ONE};
		} break;
		case ENTITY_PICKUP:{
			entity->gravitate_player_freeze = 0x80;
		} break;
		case ENTITY_STAFF:{
			entity->gravitate_player_freeze = 0x80;
			entity->texture_dynamic = (Texture){.size = 0x100,.pixel_data = tMalloc(0x100 * 0x100 * sizeof(unsigned) * 2)};
			entityRender3dGenerateSprite(entity);
		} break;
		case ENTITY_SLIME:{
			entity->model = &slime_model;
			entity->health = FIXED_ONE / 2;
			entity->texture_dynamic = (Texture){.size = 0x100,.pixel_data = tMalloc(0x100 * 0x100 * sizeof(unsigned) * 2)};
			entity->flags = ENTITY_FLAG_HITABLE;
			entityRender3dGenerateSprite(entity);
		} break;
		case ENTITY_ZOMBIE:{
			entity->model = &zombie_model;
			entity->health = FIXED_ONE / 2 + FIXED_ONE / 4;
			entity->texture_dynamic = (Texture){.size = 0x100,.pixel_data = tMalloc(0x100 * 0x100 * sizeof(unsigned) * 2)};
			entity->flags = ENTITY_FLAG_PHYSICS_STAIR | ENTITY_FLAG_HITABLE;
			entityRender3dGenerateSprite(entity);
		} break;
		case ENTITY_MONSTER:{
			entity->model = &monster_model;
			entity->angle = (Vec2){tRnd() % FIXED_ONE,tRnd() % FIXED_ONE};
			entity->texture_dynamic = (Texture){.size = 0x100,.pixel_data = tMalloc(0x100 * 0x100 * sizeof(unsigned) * 2)};
			entity->health = FIXED_ONE;
			entity->pathfinding = tMallocZero(sizeof *entity->pathfinding);
			entity->pathfinding->route.n_positions = -1;
			entity->flags = ENTITY_FLAG_PHYSICS_STAIR | ENTITY_FLAG_HITABLE;
			entityRender3dGenerateSprite(entity);
		} break;
		case ENTITY_PARTICLE:{
			entity->health = 0x8;
		} break;
	}
	if(g_entity)
		entity->next = g_entity;
	g_entity = entity;
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
		if(entity_i->texture_dynamic.pixel_data){
#if !defined(__wasm__) && !defined(__linux__)
			deleteTextureGL(entity_i->texture_dynamic.gl_id);
#endif
			tFree(entity_i->texture_dynamic.pixel_data);
		}
		Entity* entity_d = entity_i;
		entity_i = entity_i->next;
		tFree(entity_d);
	}
}

void entityDestroyAll(void){
	for(Entity* entity = g_entity;entity;){
		if(entity->texture_dynamic.pixel_data){
#if !defined(__wasm__) && !defined(__linux__)
			deleteTextureGL(entity->texture_dynamic.gl_id);
#endif
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
	Vec2 direction = vec2MulRS(vec2ShlR((Vec2){tCos(angle),tSin(angle)},5),tRnd() % (FIXED_ONE / 2) + FIXED_ONE / 2);
	Vec2 position = vec2AddR((Vec2){g_position.x,g_position.y},direction);

	int z_count = 0;
	struct{
		int height;
		VoxelType voxel_type;
	} z_list[0x10];

	for(int z = 0;z < 512 && z_count != countof(z_list);z++){
		if(boxTreeCollision((Vec3){position.x,position.y,z * FIXED_ONE},(Vec3){FIXED_ONE,FIXED_ONE,FIXED_ONE},0,0))
			continue;
		Voxel* ground = treeRayTraceAndInit((Vec3){position.x,position.y,z * FIXED_ONE},(Vec3){0,0,-FIXED_ONE},0);
		if(!ground)
			continue;
		VoxelType voxel_type = ground->type;
		z_list[z_count].height = z;
		z_list[z_count].voxel_type = voxel_type;
		z_count += 1;
		while(z < 512 && !boxTreeCollision((Vec3){position.x,position.y,z * FIXED_ONE},(Vec3){FIXED_ONE,FIXED_ONE,FIXED_ONE},0,0))
			z += 1;
	}
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

static void entityPhysicsResolve(Entity* entity,int axis,int* max_height){
	EntityStatic* entity_s = g_entity_static + entity->type;
	int height_difference;
	if((entity->flags & ENTITY_FLAG_PHYSICS_STAIR) && max_height)
		height_difference = *max_height - entity->position.z + entity_s->hitbox.z / 2;

	if((entity->flags & ENTITY_FLAG_PHYSICS_STAIR) && max_height && height_difference < FIXED_ONE){
		entity->position.z += height_difference;
	}
	else{
		if(entity_s->bounce){
			((int*)&entity->position)[axis] -= ((int*)&entity->velocity)[axis];
			((int*)&entity->velocity)[axis] = -((int*)&entity->velocity)[axis];
		}
		else{
			((int*)&entity->position)[axis] -= ((int*)&entity->velocity)[axis];
			((int*)&entity->velocity)[axis] = 0;
		}
	}
}

static void entityPhysics(Entity* entity){
	EntityStatic* entity_s = g_entity_static + entity->type;
	bool voxel_x;
	bool voxel_y;
	bool voxel_z;

	int max_height_x = 0;
	int max_height_y = 0;

	if(entity_s->hitbox.x){
		voxel_x = boxTreeCollision(vec3AddR(entity->position,(Vec3){entity->velocity.x,0,0}),entity_s->hitbox,&max_height_x,0);
		voxel_y = boxTreeCollision(vec3AddR(entity->position,(Vec3){0,entity->velocity.y,0}),entity_s->hitbox,&max_height_y,0);
		voxel_z = boxTreeCollision(vec3AddR(entity->position,(Vec3){0,0,entity->velocity.z}),entity_s->hitbox,0,0);
	}
	else{
		voxel_x = voxelPositionGet(vec3AddR(entity->position,(Vec3){entity->velocity.x,0,0}))->type != VOXEL_AIR;
		voxel_y = voxelPositionGet(vec3AddR(entity->position,(Vec3){0,entity->velocity.y,0}))->type != VOXEL_AIR;
		voxel_z = voxelPositionGet(vec3AddR(entity->position,(Vec3){0,0,entity->velocity.z}))->type != VOXEL_AIR;
	}
	entity->on_ground = voxel_z;
	vec3Add(&entity->position,entity->velocity);
	
	if(voxel_x)
		entityPhysicsResolve(entity,VEC3_X,&max_height_x);
	if(voxel_y)
		entityPhysicsResolve(entity,VEC3_Y,&max_height_y);
	if(voxel_z)
		entityPhysicsResolve(entity,VEC3_Z,0);

	int friction = voxel_z ? entity->physics_friction_ground : entity->physics_friction_air;
	vec3MulS(&entity->velocity,friction);

	if(!(entity->flags & ENTITY_FLAG_NO_GRAVITY))
		entity->velocity.z -= PHYSICS_GRAVITY;
	if(entity->flags & ENTITY_FLAG_WINDY){
		vec3Add(&entity->velocity,entity->windy);
	}
}

static bool entityTickSlime(Entity* slime){
	if(slime->on_ground && tRndChance(0x80)){
		int angle;
		if(!g_movement_fly && lineOfSight(g_position,slime->position)){
			int distance = vec2Distance((Vec2){slime->position.x,slime->position.y},(Vec2){g_position.x,g_position.y}) * 8;
			Vec2 offset = vec2MulRS((Vec2){g_velocity.x,g_velocity.y},distance);
			Vec2 direction = vec2Direction((Vec2){slime->position.x,slime->position.y},vec2AddR((Vec2){g_position.x,g_position.y},offset));
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
	EntityStatic* slime_s = g_entity_static + slime->type;

	if(slime->attack_cooldown > 0)
		slime->attack_cooldown -= 1;
	else if(boxBoxIntersect(playerHitboxGet(),PLAYER_SIZE,slime->position,slime_s->hitbox)){
		Vec2 direction = vec2Direction((Vec2){g_position.x,g_position.y},(Vec2){slime->position.x,slime->position.y});
		slime->velocity.x += (direction.x) >> 4;
		slime->velocity.y += (direction.y) >> 4;
		slime->velocity.z += FIXED_ONE / 6;

		g_velocity.x -= (direction.x) >> 4;
		g_velocity.y -= (direction.y) >> 4;
		g_velocity.z += FIXED_ONE / 6;

		g_health -= FIXED_ONE / 4;

		slime->attack_cooldown = 0x80;
	}
	return true;
}

static bool entityTickZombie(Entity* zombie){
	if(!zombie->on_ground)
		return true;
	EntityStatic* entity_s = g_entity_static + zombie->type;

	if(!g_movement_fly && lineOfSight(g_position,zombie->position)){
		int distance = vec2Distance((Vec2){zombie->position.x,zombie->position.y},(Vec2){g_position.x,g_position.y}) * 8;
		Vec2 offset = vec2MulRS((Vec2){g_velocity.x,g_velocity.y},distance);
		Vec2 direction = vec2Direction((Vec2){zombie->position.x,zombie->position.y},vec2AddR((Vec2){g_position.x,g_position.y},offset));
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
	
	if(!g_movement_fly && entity->pathfinding->cooldown <= 0){
		EntityStatic* entity_s = g_entity_static + entity->type;
		if(vec3Distance(entity->position,g_position) < FIXED_ONE * 32){
			if(directPath(entity->position,entity_s->hitbox,g_position)){
				entity->pathfinding->state = ENTITY_PATHFIND_DIRECT;
				entity->pathfinding->direct_position = g_position;
			}
			else if(pathFinding(entity->position,entity_s->hitbox,g_position,&entity->pathfinding->route)){
				entity->pathfinding->state = ENTITY_PATHFIND_ROUTE;
			}
		}
		entity->pathfinding->cooldown = tRnd() % 0x40 + 0x40;
	}
	switch(entity->pathfinding->state){
		case ENTITY_PATHFIND_DIRECT:{
			vec3Add(&entity->velocity,vec3ShrR(vec3Direction(entity->position,entity->pathfinding->direct_position),8));
		} break;
		case ENTITY_PATHFIND_IDLE:{
			int r = tRnd();
			vec3Add(&entity->velocity,vec3ShrR((Vec3){tCos(r),tSin(r),0},8));
		} break;
		case ENTITY_PATHFIND_ROUTE:{
			if(entity->pathfinding->route.n_positions < 0){
				entity->pathfinding->state = ENTITY_PATHFIND_IDLE;
				break;
			}
			Vec3 route_position = vec3ShlR(entity->pathfinding->route.positions[entity->pathfinding->route.n_positions],PATH_FIND_SIZE);
		
			Vec2 direction = vec2Direction((Vec2){entity->position.x,entity->position.y},(Vec2){route_position.x,route_position.y});
			entity->velocity.x += direction.x >> 8;
			entity->velocity.y += direction.y >> 8;
			
			Vec3 relative_position = vec3SubR(entity->position,route_position);
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
	entity->health -= 1;
	if(entity->health <= 0)
		return false;
	return true;
}

static bool entityTickPickup(Entity* entity){
	Vec3 player_middle = vec3AddR(g_position,(Vec3){0,0,-FIXED_ONE / 2 - FIXED_ONE / 3});
	Vec3 relative = vec3SubR(entity->position,player_middle);
	if(vec3Dot(relative,relative) < FIXED_ONE * 16 && !entity->gravitate_player_freeze && !inventoryFull()){
		vec3Add(&entity->position,vec3ShrR(vec3Direction(entity->position,player_middle),4));
		if(vec3Dot(relative,relative) < FIXED_ONE / 64){
			g_pickup_collected = true;
			audioPlay(vec3AddR(g_position,(Vec3){0,0,-FIXED_ONE}),AUDIO_ITEM_GAINED);
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
		if(!(entity->flags & ENTITY_FLAG_HITABLE))
			continue;
		EntityStatic* entity_s = g_entity_static + entity->type;
		if(pointBoxIntersection(position,entity->position,entity_s->hitbox))
			return entity;
	}
	return 0;
}

void entityHit(Entity* entity){
	entity->health -= FIXED_ONE / 100 * 30;
	Vec3 death_position = entity->position;

	Vec2 knockback = vec2Direction((Vec2){g_position.x,g_position.y},(Vec2){entity->position.x,entity->position.y});
	entity->velocity.x += knockback.x / 8;
	entity->velocity.y += knockback.y / 8;
	entity->velocity.z += FIXED_ONE / 6;
	for(int i = 0x100;i--;){
		Vec3 velocity = (Vec3){tRnd() % FIXED_ONE * 2 - FIXED_ONE,tRnd() % FIXED_ONE * 2 - FIXED_ONE,tRnd() % FIXED_ONE * 2};
		if(vec2Dot((Vec2){velocity.x,velocity.y},(Vec2){velocity.x,velocity.y}) > FIXED_ONE)
			continue;
		Entity* particle = entityCreate(death_position,ENTITY_PARTICLE);
		particle->color = (Vec3){tRnd() % FIXED_ONE / 2 + FIXED_ONE / 2,tRnd() % FIXED_ONE / 8,tRnd() % FIXED_ONE / 8};
		particle->health = tRnd() % 0x80 + 0x80;
		particle->size = FIXED_ONE / 16;
		particle->velocity = vec3ShrR(vec3Mix(velocity,vec3Direction(g_position,death_position),FIXED_ONE / 2),3);
	}
}

static void spellAdjectiveParticleSpawn(Entity* entity){
	if(entity->adj_damage && tRndChance(16)){
		Entity* particle = entityCreate(entity->position,ENTITY_PARTICLE);
		particle->color = (Vec3){FIXED_ONE << 4,FIXED_ONE,FIXED_ONE};
		particle->health = tRnd() % 0x80 + 0x80;
		particle->size = FIXED_ONE / 16;
		particle->velocity = vec3ShrR(vec3Rnd(),4);
		particle->particle_string = (String)STRING_LITERAL("#+");
	}
	if(entity->adj_speed && tRndChance(16)){
		Entity* particle = entityCreate(entity->position,ENTITY_PARTICLE);
		particle->color = (Vec3){FIXED_ONE,FIXED_ONE,FIXED_ONE << 4};
		particle->health = tRnd() % 0x80 + 0x80;
		particle->size = FIXED_ONE / 16;
		particle->velocity = vec3ShrR(vec3Rnd(),4);
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
	Vec3 player_middle = vec3AddR(g_position,(Vec3){0,0,-FIXED_ONE / 2 - FIXED_ONE / 3});
	Vec3 relative = vec3SubR(entity->position,player_middle);
	if(vec3Dot(relative,relative) < FIXED_ONE * 16 && !g_equipped_staff && !entity->gravitate_player_freeze){
		vec3Add(&entity->position,vec3ShrR(vec3Direction(entity->position,player_middle),4));
		if(vec3Dot(relative,relative) < FIXED_ONE / 64){
			g_equipped = entity->staff;
			g_equipped_staff = true;
			staffEditorCreateMenu(&g_equipped);
			audioPlay(vec3AddR(g_position,(Vec3){0,0,-FIXED_ONE}),AUDIO_ITEM_GAINED);
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
		if(lineOfSight(entity->position,g_position)){
			int distance = vec3Distance(g_position,entity->position);
			int shock = fixedDivR(FIXED_ONE,fixedMulR(distance,distance));
			vec3Add(&g_velocity,vec3MulRS(vec3Direction(entity->position,g_position),shock));
			g_health -= shock * 8;
		}
		for(Entity* entity_other = g_entity;entity_other;entity_other = entity_other->next){
			if(entity == entity_other)
				continue;
			if(!lineOfSight(entity->position,entity_other->position))
				continue;
			int distance = vec3Distance(entity_other->position,entity->position);
			int shock = fixedMulR(distance,distance);
			vec3Add(&entity_other->velocity,vec3MulRS(vec3Direction(entity->position,entity_other->position),fixedDivR(FIXED_ONE * 4,fixedMulR(distance,distance))));
			entity_other->health -= shock / 8;
		}
		for(int i = 0x100;i--;){ 
			Entity* particle = entityCreate(entity->position,ENTITY_PARTICLE);
			particle->health = tRnd() % 0x400;
			particle->size = FIXED_ONE / 4 + tRnd() % (FIXED_ONE / 4);
			particle->velocity = vec3ShrR(vec3Rnd(),2);
			particle->color = pixelColorToColor(0x808080);
			particle->flags = ENTITY_FLAG_NO_GRAVITY | ENTITY_FLAG_WINDY;
			particle->windy = vec3ShrR(vec3MulRS(vec3Rnd(),tRnd() % FIXED_ONE),11);
			particle->physics_friction_air = (FIXED_ONE - (FIXED_ONE >> 4));
			particle->texture = g_textures + TEXTURE_SMOKE;
			particle->texture_offset = (Vec2){tRnd() % FIXED_ONE,tRnd() % FIXED_ONE};
			particle->texture_size = FIXED_ONE / 4;
		}
		for(int i = 0x10;i--;){
			Entity* particle = entityCreate(entity->position,ENTITY_PARTICLE);
			particle->health = tRnd() % 0x40 + 0x40;
			particle->size = FIXED_ONE / 16;
			particle->velocity = vec3ShrR(vec3Rnd(),2);
			particle->color = vec3ShlR(pixelColorToColor(0x204080),4);
			particle->color_emit = vec3ShrR(particle->color,4);
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
		if(!g_movement_fly && lineOfSight(g_position,boss->position)){
			int distance = vec2Distance((Vec2){boss->position.x,boss->position.y},(Vec2){g_position.x,g_position.y}) * 8;
			Vec2 offset = vec2MulRS((Vec2){g_velocity.x,g_velocity.y},distance);
			Vec2 direction = vec2Direction((Vec2){boss->position.x,boss->position.y},vec2AddR((Vec2){g_position.x,g_position.y},offset));
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
	EntityStatic* slime_s = g_entity_static + boss->type;

	if(boss->attack_cooldown > 0)
		boss->attack_cooldown -= 1;
	else if(boxBoxIntersect(playerHitboxGet(),PLAYER_SIZE,boss->position,slime_s->hitbox)){
		Vec2 direction = vec2Direction((Vec2){g_position.x,g_position.y},(Vec2){boss->position.x,boss->position.y});
		boss->velocity.x += (direction.x) >> 4;
		boss->velocity.y += (direction.y) >> 4;
		boss->velocity.z += FIXED_ONE / 6;

		g_velocity.x -= (direction.x) >> 4;
		g_velocity.y -= (direction.y) >> 4;
		g_velocity.z += FIXED_ONE / 6;

		g_health -= FIXED_ONE / 4;

		boss->attack_cooldown = 0x80;
	}
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
	};
	Entity* previous = 0;
	for(Entity* entity = g_entity;entity;entity = entity->next){
		if(entity->type >= countof(entity_tick) || !entity_tick[entity->type])
			continue;
		entityPhysics(entity);
		if(!entity_tick[entity->type](entity))
			entity->health = 0;
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
	int voxel_depth = 6;
	int voxel_n_row = 1 << voxel_depth;
	for(int i = 0;i < voxel_n_row * voxel_n_row * voxel_n_row;i++){
		Vec3 voxel_position = {
			i / voxel_n_row / voxel_n_row,
			i / voxel_n_row % voxel_n_row,
			i % voxel_n_row 
		};
		Vec3 relative_pos = {
			 (voxel_position.x * FIXED_ONE * 2 / voxel_n_row - FIXED_ONE),
			 (voxel_position.y * FIXED_ONE * 2 / voxel_n_row - FIXED_ONE),
			 (voxel_position.z * FIXED_ONE * 2 / voxel_n_row - FIXED_ONE),
		};
		Vec3 relative_pos2;
		relative_pos2 = vec3AddR(relative_pos,(Vec3){FIXED_ONE / 2,FIXED_ONE / 2,-FIXED_ONE / 2});
		if(vec3Dot(relative_pos2,relative_pos2) < FIXED_ONE / 8){
			voxelSet(&monster_model,voxel_position,voxel_depth,VOXEL_BLOCK_BLUE);
			continue;
		}
		relative_pos2 = vec3AddR(relative_pos,(Vec3){FIXED_ONE / 2,-FIXED_ONE / 2,-FIXED_ONE / 2});
		if(vec3Dot(relative_pos2,relative_pos2) < FIXED_ONE / 8){
			voxelSet(&monster_model,voxel_position,voxel_depth,VOXEL_BLOCK_BLUE);
			continue;
		}
		relative_pos2 = relative_pos;
		if(vec3Dot(relative_pos2,relative_pos2) < FIXED_ONE){
			voxelSet(&monster_model,voxel_position,voxel_depth,VOXEL_BLOCK);
		}
	}

	voxel_depth = 6;
	voxel_n_row = 1 << voxel_depth;
	for(int i = 0;i < voxel_n_row * voxel_n_row * voxel_n_row;i++){
		Vec3 voxel_position = {
			i / voxel_n_row / voxel_n_row,
			i / voxel_n_row % voxel_n_row,
			i % voxel_n_row 
		};
		Vec3 relative_pos = {
			 (voxel_position.x * FIXED_ONE * 2 / voxel_n_row - FIXED_ONE),
			 (voxel_position.y * FIXED_ONE * 2 / voxel_n_row - FIXED_ONE),
			 (voxel_position.z * FIXED_ONE * 2 / voxel_n_row - FIXED_ONE),
		};
		Vec3 relative_pos2;
		relative_pos2 = vec3AddR(relative_pos,(Vec3){FIXED_ONE / 2,FIXED_ONE / 2,-FIXED_ONE / 2});
		if(vec3Dot(relative_pos2,relative_pos2) < FIXED_ONE / 8){
			voxelSet(&slime_model,voxel_position,voxel_depth,VOXEL_BLOCK_GREEN);
			continue;
		}
		relative_pos2 = vec3AddR(relative_pos,(Vec3){FIXED_ONE / 2,-FIXED_ONE / 2,-FIXED_ONE / 2});
		if(vec3Dot(relative_pos2,relative_pos2) < FIXED_ONE / 8){
			voxelSet(&slime_model,voxel_position,voxel_depth,VOXEL_BLOCK_GREEN);
			continue;
		}
		relative_pos2 = relative_pos;
		if(vec3Dot(relative_pos2,relative_pos2) < FIXED_ONE){
			voxelSet(&slime_model,voxel_position,voxel_depth,VOXEL_BLOCK);
		}
	}

	voxel_depth = 6;
	voxel_n_row = 1 << voxel_depth;
	for(int i = 0;i < voxel_n_row * voxel_n_row * voxel_n_row;i++){
		Vec3 voxel_position = {
			i / voxel_n_row / voxel_n_row,
			i / voxel_n_row % voxel_n_row,
			i % voxel_n_row 
		};
		Vec3 relative_pos = {
			 (voxel_position.x * FIXED_ONE * 2 / voxel_n_row - FIXED_ONE),
			 (voxel_position.y * FIXED_ONE * 2 / voxel_n_row - FIXED_ONE),
			 (voxel_position.z * FIXED_ONE * 2 / voxel_n_row - FIXED_ONE),
		};
		Vec3 relative_pos2;
		relative_pos2 = vec3AddR(relative_pos,(Vec3){FIXED_ONE / 2,FIXED_ONE / 2,-FIXED_ONE / 2});
		if(vec3Dot(relative_pos2,relative_pos2) < FIXED_ONE / 8){
			voxelSet(&zombie_model,voxel_position,voxel_depth,VOXEL_BLOCK_RED);
			continue;
		}
		relative_pos2 = vec3AddR(relative_pos,(Vec3){FIXED_ONE / 2,-FIXED_ONE / 2,-FIXED_ONE / 2});
		if(vec3Dot(relative_pos2,relative_pos2) < FIXED_ONE / 8){
			voxelSet(&zombie_model,voxel_position,voxel_depth,VOXEL_BLOCK_RED);
			continue;
		}
		relative_pos2 = relative_pos;
		if(vec3Dot(relative_pos2,relative_pos2) < FIXED_ONE){
			voxelSet(&zombie_model,voxel_position,voxel_depth,VOXEL_BLOCK);
		}
	}

	voxel_depth = 6;
	voxel_n_row = 1 << voxel_depth;
	for(int i = 0;i < voxel_n_row * voxel_n_row * voxel_n_row;i++){
		Vec3 voxel_position = {
			i / voxel_n_row / voxel_n_row,
			i / voxel_n_row % voxel_n_row,
			i % voxel_n_row 
		};
		Vec3 relative_pos = {
			 (voxel_position.x * FIXED_ONE * 2 / voxel_n_row - FIXED_ONE),
			 (voxel_position.y * FIXED_ONE * 2 / voxel_n_row - FIXED_ONE),
			 (voxel_position.z * FIXED_ONE * 2 / voxel_n_row - FIXED_ONE),
		};
		Vec3 relative_pos2;
		relative_pos2 = vec3AddR(relative_pos,(Vec3){FIXED_ONE / 2,FIXED_ONE / 2,-FIXED_ONE / 2});
		if(vec3Dot(relative_pos2,relative_pos2) < FIXED_ONE / 8){
			voxelSet(&boss_model,voxel_position,voxel_depth,VOXEL_BLOCK_RED);
			continue;
		}
		relative_pos2 = vec3AddR(relative_pos,(Vec3){FIXED_ONE / 2,-FIXED_ONE / 2,-FIXED_ONE / 2});
		if(vec3Dot(relative_pos2,relative_pos2) < FIXED_ONE / 8){
			voxelSet(&boss_model,voxel_position,voxel_depth,VOXEL_BLOCK_RED);
			continue;
		}
		relative_pos2 = relative_pos;
		if(vec3Dot(relative_pos2,relative_pos2) < FIXED_ONE){
			voxelSet(&boss_model,voxel_position,voxel_depth,VOXEL_BLOCK);
		}
	}
}

static int entitySpriteSize(Vec3 position,int size){
	Vec3 plane_normal = getLookDirection(g_angle);
	int plane_distance = -vec3Dot(plane_normal,g_position);
	return fixedDivR(size,sdPlane(position,plane_normal,plane_distance));
}

static void entityDynamicLightingSide(Voxel* voxel,Entity* entity,Vec3 block_pos,int side,Vec2 coord,int depth,int distance_max,int surface_angle){
	Vec3 normal = g_normal_table[side];
	Vec2 axis = g_axis_table[side];
	Vec3 luxel_position = block_pos;
	int size = depthToSize(voxel->depth) / (1 << depth);
	((int*)&luxel_position)[axis.x] += fixedMulR(coord.x << FIXED_PRECISION,size);
	((int*)&luxel_position)[axis.y] += fixedMulR(coord.y << FIXED_PRECISION,size);
	
	Vec3 square_pos = luxel_position;
	((int*)&square_pos)[axis.x] += size / 2;
	((int*)&square_pos)[axis.y] += size / 2;

	int mipmap = tClamp(mipmapGet(squarePointClosestPosition(square_pos,size,normal),normal,distance_max,surface_angle),26 - LUXEL_MAX_MIPMAP,31);

	Vec3 luxel_pos = vec3ShrR(luxel_position,mipmap);
	unsigned hash = luxelHashGet(luxel_pos,mipmap);
	Luxel* luxel = luxelDynamicGet(hash);

	Vec3 position = square_pos;
	((int*)&position)[side >> 1] += side & 1 ? 0x10 : -0x10;

	VoxelStatic* voxel_s = g_voxel_static + voxel->type;

	int distance = vec3Distance(entity->position,position);
	int intensity = fixedDivR(FIXED_ONE << 2,fixedMulR(distance,distance));
	fixedMul(&intensity,tMax(vec3Dot(normal,vec3Direction(position,entity->position)),0));

	vec3Add(&luxel->luminance,vec3MulRS(entity->color_emit,intensity));
	//luxel->luminance = (Vec3){position.x % (FIXED_ONE << 4),position.y % (FIXED_ONE << 4),position.z % (FIXED_ONE << 4)};
	luxel->hash = hash;
}

static void entityDynamicLightingSideRecursive(Voxel* voxel,Entity* entity,Vec3 block_pos,int side,Vec2 coord,int depth,int surface_angle){
	Vec2 axis = g_axis_table[side];
	Vec3 block_pos_t = block_pos;
	int size = depthToSize(voxel->depth) / (1 << depth);
	((int*)&block_pos_t)[axis.x] += fixedMulR(coord.x << FIXED_PRECISION,size);
	((int*)&block_pos_t)[axis.y] += fixedMulR(coord.y << FIXED_PRECISION,size);

	Vec3 positions[4] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

	((int*)&positions[1])[axis.y] += size;
	((int*)&positions[2])[axis.x] += size;
	((int*)&positions[3])[axis.x] += size;
	((int*)&positions[3])[axis.y] += size;

	int distance_max = 0;
	int distance_max_index;

	Vec3* position_farthest;

	for(int i = 0;i < countof(positions);i++){
		int distance = vec3Dot(vec3ShrR(g_position,4),vec3ShrR(positions[i],4));
		if(distance > distance_max){
			distance_max = distance;
			position_farthest = positions + i;
		}
	}

	distance_max = vec3Distance(vec3ShrR(g_position,4),vec3ShrR(*position_farthest,4));

	Vec3 normal = g_normal_table[side];

	int mipmap = tClamp(mipmapGet(squarePointClosestPosition(positions[0],size,normal),normal,distance_max,surface_angle),26 - LUXEL_MAX_MIPMAP,31);

	int split = 25 + -mipmap - voxel->depth;

	if(depth < split){
		Vec3 v_pos = vec3ShlR((Vec3){voxel->position_x,voxel->position_y,voxel->position_z},depth);
		((int*)&v_pos)[axis.x] += coord.x;
		((int*)&v_pos)[axis.y] += coord.y;
		if(side & 1)
			((int*)&v_pos)[side >> 1] += (1 << depth) - 1;
		
		if(sdVoxel(entity->position,positions[0],size) > FIXED_ONE * 4)
			return;

		coord.x <<= 1;
		coord.y <<= 1;
		entityDynamicLightingSideRecursive(voxel,entity,block_pos,side,vec2AddR(coord,(Vec2){0,0}),depth + 1,surface_angle);
		entityDynamicLightingSideRecursive(voxel,entity,block_pos,side,vec2AddR(coord,(Vec2){0,1}),depth + 1,surface_angle);
		entityDynamicLightingSideRecursive(voxel,entity,block_pos,side,vec2AddR(coord,(Vec2){1,0}),depth + 1,surface_angle);
		entityDynamicLightingSideRecursive(voxel,entity,block_pos,side,vec2AddR(coord,(Vec2){1,1}),depth + 1,surface_angle);
		return;
	}
	entityDynamicLightingSide(voxel,entity,block_pos,side,coord,depth,distance_max,surface_angle);
}

static void entityDynamicLightingSidePre(Voxel* voxel,Entity* entity,Vec3 block_pos,int side){
	entityDynamicLightingSideRecursive(voxel,entity,block_pos,side,(Vec2){0,0},0,surfaceAngle(block_pos,g_normal_table[side]));
}

static void entityDynamicLightingRecursive(Voxel* voxel,Entity* entity){
	int block_size = depthToSize(voxel->depth);
	Vec3 block_pos = voxelWorldPos(voxel);
	if(voxel->type == VOXEL_PARENT){
		if(sdVoxel(entity->position,block_pos,block_size) > FIXED_ONE * 4)
			return;
		for(int i = 0;i < countof(voxel->child_s);i++)
			entityDynamicLightingRecursive(voxel->child_s[i],entity);
			
		return;
	}
	if(voxel->type == VOXEL_AIR || voxel->type == VOXEL_MIRROR || g_voxel_static[voxel->type].flags & VOXEL_EMITER)
		return;
	entityDynamicLightingSidePre(voxel,entity,block_pos,0);
	entityDynamicLightingSidePre(voxel,entity,vec3AddR(block_pos,(Vec3){block_size,0,0}),1);
	entityDynamicLightingSidePre(voxel,entity,block_pos,2);
	entityDynamicLightingSidePre(voxel,entity,vec3AddR(block_pos,(Vec3){0,block_size,0}),3);
	entityDynamicLightingSidePre(voxel,entity,block_pos,4);
	entityDynamicLightingSidePre(voxel,entity,vec3AddR(block_pos,(Vec3){0,0,block_size}),5);
}

static void entityRender3d(Entity* entity,int entity_size){
	for(int i = 0x40;i--;){
		Vec3 direction = getLookDirection((Vec2){tRnd() % FIXED_ONE,tRnd() % FIXED_ONE});
		Vec3 luminance = rayLuminance(entity->position,direction);
		for(int i = 0;i < 3;i++){
			int offset = vec3Dot(direction,g_normal_table[i * 2]) < 0;
			entity->n_luminance_sample_3d[i * 2 + offset] += 1;
			entity->luminance_3d[i * 2 + offset] = vec3Mix(entity->luminance_3d[i * 2 + offset],luminance,FIXED_ONE / tMin(entity->n_luminance_sample_3d[i * 2 + offset],0x400));
		}
	}
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
	/*
	int r = tRnd();
	for(int i = 0;i < 0x100 * 0x100;i++)
		entity->texture_dynamic.pixel_data[i] = colorToPixelColor(vec3ShlR(vec3Direction(entity->position,g_position),4));
	*/
	
	static int r_angle;
	r_angle += 128;

	Vec3 direction = vec3Direction(g_position,entity->position);
	Vec2 rotation = vec2Rotate((Vec2){direction.x,direction.y},entity->move_angle);

	Vec2 angle = getLookAngle(direction);

	int change_factor = vec2Distance(angle,entity->render_angle) / 0x10 + vec3Distance(entity->position,entity->render_position) / 0x100;

	if(change_factor > 0x100)
		entityRender3dGenerateSprite(entity);
	if(g_surface.backend == RENDER_BACKEND_SOFTWARE){
        spanSpriteAdd(&g_surface,&entity->texture_dynamic,points);
        return;
    }
	drawTexturePolygon(&g_surface,&entity->texture_dynamic,g_texture_coordinates_fill,points,vec3Single(FIXED_ONE << 4),4);
}

void entityDraw(Entity* entity){
	switch(entity->type){
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
			drawEllipses(&g_surface,point.x,point.y,fixedMulR(size * 4,g_options.fov.x),fixedMulR(size * 4,g_options.fov.y),vec3MulRS(vec3Single(1 << 14),g_exposure));
			drawEllipses(&g_surface,point.x - size / 2,point.y + size / 2,fixedMulR(size,g_options.fov.x),fixedMulR(size,g_options.fov.y),vec3MulRS(vec3Single(1 << 18),g_exposure));
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
                spanEllipsesAdd(&g_surface,point.x,point.y,fixedMulR(size,g_options.fov.x),fixedMulR(size,g_options.fov.y),vec3MulRS(entity->color_emit,g_exposure));
                return;
            }
			drawEllipses(&g_surface,point.x,point.y,fixedMulR(size,g_options.fov.x),fixedMulR(size,g_options.fov.y),vec3MulRS(entity->color_emit,g_exposure));
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
				entity->luminance = vec3Mix(entity->luminance,vec3ShlR(luminance,4),tMin(FIXED_ONE / entity->n_luminance_sample,0x400));
			}
			Vec3 point = pointToScreen(entity->position);
			if(!point.z)
				return;
			int size = entitySpriteSize(entity->position,entity->size);
			if(entity->health < 0x80)
				fixedMul(&size,entity->health * FIXED_ONE / 0x80);
			Vec2 points[] = {
				{point.x + fixedMulR(size,g_options.fov.x),point.y + fixedMulR(size,g_options.fov.y)},
				{point.x + fixedMulR(size,g_options.fov.x),point.y - fixedMulR(size,g_options.fov.y)},
				{point.x - fixedMulR(size,g_options.fov.x),point.y - fixedMulR(size,g_options.fov.y)},
				{point.x - fixedMulR(size,g_options.fov.x),point.y + fixedMulR(size,g_options.fov.y)},
			};
			Vec3 color = vec3MulRS(vec3MulR(entity->luminance,entity->color),g_exposure);
			if(entity->texture){
                if(g_surface.backend == RENDER_BACKEND_SOFTWARE){
                    //spanQuadAdd(&g_surface,points,color);
                    
                    //spanSpriteAdd(&g_surface,entity->texture,points);
                }
                else{
                    Vec2 texture_coordinates[] = {
                        {entity->texture_offset.x,entity->texture_offset.y},
                        {entity->texture_offset.x,entity->texture_offset.y + entity->texture_size},
                        {entity->texture_offset.x + entity->texture_size,entity->texture_offset.y + entity->texture_size},
                        {entity->texture_offset.x + entity->texture_size,entity->texture_offset.y},
                    };
                    drawTexturePolygon(&g_surface,entity->texture,texture_coordinates,points,color,4);
                }
			}
			else if(entity->particle_string.data){
				drawString(&g_surface,point.x,point.y,entity->particle_string,size,pixelColorToColor(0xFFFFFF),0x800);
			}
			else{
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
			drawFrame(&g_surface,point.x - fixedMulR(size / 2,g_options.fov.x),point.y - fixedMulR(size / 2,g_options.fov.y),fixedMulR(size,g_options.fov.x),fixedMulR(size,g_options.fov.y),pixelColorToColor(0x808080),fixedMulR(size,g_options.fov.x) >> 4);
			switch(entity->pickup_type){
				case SPELL_BOLT:{
					drawEllipses(&g_surface,point.x,point.y,fixedMulR(size / 3,g_options.fov.x),fixedMulR(size / 3,g_options.fov.y),vec3MulRS(pixelColorToColor(0xFF0000),g_exposure));
				} break;
				case SPELL_BOMB:{
					drawEllipses(&g_surface,point.x,point.y,fixedMulR(size / 3,g_options.fov.x),fixedMulR(size / 3,g_options.fov.y),vec3MulRS(pixelColorToColor(0x0000FF),g_exposure));
				} break;
				case SPELL_ORB:{
					drawEllipses(&g_surface,point.x,point.y,fixedMulR(size / 3,g_options.fov.x),fixedMulR(size / 3,g_options.fov.y),vec3MulRS(pixelColorToColor(0x00FF00),g_exposure));
				} break;
			}
			//drawTexturePolygon(g_surface,g_textures + TEXTURE_PICKUP,g_texture_coordinates_fill,points,vec3Single(1 << 20),4);
		} break;
	}
}

void entityDrawHitbox(void){
	for(Entity* entity = g_entity;entity;entity = entity->next){
		Vec3 hitbox = g_entity_static[entity->type].hitbox;
		if(!hitbox.x)
			continue;

		boxQuadWireframeDraw(vec3SubR(entity->position,vec3ShrR(hitbox,1)),hitbox,0x00FF00);
	}	
}

void entityDynamicLighting(void){
	for(Entity* entity = g_entity;entity;entity = entity->next){
		if(!g_entity_static[entity->type].emit && !(entity->flags & ENTITY_FLAG_EMIT))
			continue;
		entityDynamicLightingRecursive(&g_voxel,entity);
	}	
}

void entityVoxelInsert(void){
	for(Entity* entity = g_entity;entity;entity = entity->next){
		Voxel* voxel = voxelPositionGet(entity->position);
		if(voxel->type != VOXEL_AIR)
			continue;
		if(voxel->entity_list){
			int distance = vec3Distance(entity->position,g_position);
			if(vec3Distance(voxel->entity_list->position,g_position) < distance){
				entity->next_voxel = voxel->entity_list;
				voxel->entity_list = entity;
			}
			else{
				Entity* prev = voxel->entity_list;
				Entity* entity_l = prev->next_voxel;
				
				while(entity_l && vec3Distance(entity_l->position,g_position) > distance){
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
			voxelTickListAdd(voxel);
			voxel->entity_list = entity;
			entity->next_voxel = 0;
		}
	}
}

void entityVoxelRemove(void){
	for(Entity* entity = g_entity;entity;entity = entity->next)
		entity->next_voxel = 0;
}
