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
#include "platform/audio.h"
#include "span.h"
#include "sprite_trace.h"

#if !defined(__wasm__) && !defined(__linux__)
#include "win32/w_main.h"
#endif

#if 1

static ModelSprite model_sphere_slime = {
    .ellipsoid = {
        {
            .color = {FIXED_ONE / 3,FIXED_ONE,FIXED_ONE / 2},
            .ellipsoid.radius = {FIXED_ONE / 2,FIXED_ONE / 2,FIXED_ONE / 2},
        },
        {
            .position = {0,-FIXED_ONE / 3,FIXED_ONE / 3},
            .color = {FIXED_ONE,FIXED_ONE,FIXED_ONE},
            .ellipsoid.radius = {FIXED_ONE / 6,FIXED_ONE / 6,FIXED_ONE / 6},
        },
        {
            .position = {0,FIXED_ONE / 3,FIXED_ONE / 3},
            .color = {FIXED_ONE,FIXED_ONE,FIXED_ONE},
            .ellipsoid.radius = {FIXED_ONE / 6,FIXED_ONE / 6,FIXED_ONE / 6},
        },
    },
    .n_ellipsoid = 3,
};

static ModelSprite model_sphere_slime_chase = {
	.ellipsoid = {
        {
            .color = {FIXED_ONE / 3,FIXED_ONE,FIXED_ONE / 2},
            .ellipsoid.radius = {FIXED_ONE / 2,FIXED_ONE / 2,FIXED_ONE / 2},
        },
        {
            .position = {0,-FIXED_ONE / 3,FIXED_ONE / 3},
            .color = {FIXED_ONE / 2,FIXED_ONE / 2,FIXED_ONE},
            .ellipsoid.radius = {FIXED_ONE / 6,FIXED_ONE / 6,FIXED_ONE / 6},
        },
        {
            .position = {0,FIXED_ONE / 3,FIXED_ONE / 3},
            .color = {FIXED_ONE / 2,FIXED_ONE / 2,FIXED_ONE},
            .ellipsoid.radius = {FIXED_ONE / 6,FIXED_ONE / 6,FIXED_ONE / 6},
        },
    },
    .n_ellipsoid = 3,
};

#endif

static ModelSprite model_bolt = {
    .ellipsoid = {
        {
            .color = {FIXED_ONE,FIXED_ONE / 3,FIXED_ONE / 3},
            .ellipsoid.radius = {FIXED_ONE / 2,FIXED_ONE / 2,FIXED_ONE / 2},
        },
    },
    .n_ellipsoid = 1,
};

#if 0

static ModelSprite model_sphere_slime[] = {
	{
		.color = {FIXED_ONE,FIXED_ONE,FIXED_ONE},
		.radius = {FIXED_ONE / 2,FIXED_ONE / 2,FIXED_ONE / 2},
	},
    {
		.position = {0,-FIXED_ONE / 3,-FIXED_ONE / 3},
	    .color = {FIXED_ONE,FIXED_ONE,FIXED_ONE},
		.radius = {FIXED_ONE / 6,FIXED_ONE / 6,FIXED_ONE / 6},
	},
	{
		.position = {0,FIXED_ONE / 3,-FIXED_ONE / 3},
	    .color = {FIXED_ONE,FIXED_ONE,FIXED_ONE},
		.radius = {FIXED_ONE / 6,FIXED_ONE / 6,FIXED_ONE / 6},
	},
};

#endif

Entity* g_entity;

Entity* entityRayCollision(Entity* entity_list,Vec3 position,Vec3 direction){
	Entity* entity_closest = 0;
	for(Entity* entity = entity_list;entity;entity = entity->next_voxel){
		if(rayBoxIntersection(entity->position,vec3Shr(entity->hitbox,1),position,direction))
			entity_closest = entity;
	}
	return entity_closest;
}

Vec3 ellipsoidNormal(Vec3 pos,Vec3 ra){
    return vec3Normalize(vec3Div(pos,(vec3Mul(ra,ra))));
}



void entityAdd(Entity* entity){
    if(entity->health <= 0)
        entity->health = 1;
    entity->next = g_entity;
	g_entity = entity;
}

static void modelInit(Entity* entity){
    entity->texture_dynamic = textureCreate(0x100);
    entity->render_position = entity->position;
    entity->render_direction = vec3Direction(entity->position,g_surface.position);
    ellipsoidModelCubemapGenerate(&entity->cubemap,entity->position,0);
    ellipsoidModelGenerate(&entity->cubemap,entity->position,entity->hitbox,&entity->texture_dynamic,entity->model_sphere,(Vec2){entity->move_angle,0},true,0,FIXED_ONE);
}

Entity* entityCreate(Vec3 position,EntityType type){
	Entity* entity = tMallocZero(sizeof *entity);

	entity->position = position;
	entity->type = type;
	entity->health = FIXED_ONE;
	entity->physics_friction_ground = PHYSICS_FRICTION_GROUND;
	entity->physics_friction_air = PHYSICS_FRICTION_AIR;
	entity->color_emit = (Vec3){FIXED_ONE / 2,FIXED_ONE / 2,FIXED_ONE / 2};
    
	switch(type){
        case ENTITY_PLAYER:{
            entity->health = FIXED_ONE;
            entity->has_hitbox = true;
            entity->hitbox = PLAYER_SIZE;
        } break;
        case ENTITY_WEAPON:{
            entity->type = ENTITY_WEAPON;
            entity->no_gravity = true;
            entity->has_hitbox = true;
            entity->texture_dynamic = textureCreate(0x100);
            entity->hitbox = (Vec3){FIXED_ONE / 8,FIXED_ONE / 8,FIXED_ONE / 8};
            modelInit(entity);
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
            entity->render_direction = vec3Direction(entity->position,g_surface.position);
		    ellipsoidModelGenerate(&entity->cubemap,entity->position,entity->hitbox,&entity->texture_dynamic,entity->model_sphere,(Vec2){entity->move_angle,0},true,0,FIXED_ONE);
		} break;
		case ENTITY_BOMB:{
			entity->health = 0x100;
			entity->color_emit = (Vec3){FIXED_ONE,FIXED_ONE / 3,FIXED_ONE / 3};
		} break;
		case ENTITY_BOLT:{
            entity->bounce = true;
            entity->bounciness = REAL_UNIT * 0xE0;
			entity->health = 0x200;
			entity->color_emit = (Vec3){FIXED_ONE / 3,FIXED_ONE / 3,FIXED_ONE};
            entity->render_direction = vec3Direction(entity->position,g_surface.position);
            entity->size = REAL_UNIT * 0x40;
            entity->bounce = true;
            entity->model_sphere = &model_bolt;
		    //ellipsoidModelGenerate(&entity->cubemap,entity->position,&entity->texture_dynamic,entity->model_sphere,(Vec2){entity->move_angle,0},true,0);
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
		    modelInit(entity);
		} break;
		case ENTITY_SLIME:{
            entity->has_hitbox = true;
            entity->hitbox = (Vec3){FIXED_ONE / 2,FIXED_ONE / 2,FIXED_ONE / 2};
            entity->model_sphere = &model_sphere_slime;
			entity->health = 100;
			entity->hitable = true;
            modelInit(entity);
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
            entity->hitbox = (Vec3){FIXED_ONE,FIXED_ONE,FIXED_ONE};
            entity->angle = (Vec2){realRandom(FIXED_ONE),realRandom(FIXED_ONE)};
			entity->texture_dynamic = textureCreate(0x100);
			entity->health = FIXED_ONE;
			entity->pathfinding = tMallocZero(sizeof *entity->pathfinding);
			entity->pathfinding->route.n_positions = -1;
            entity->physics_stair = true;
            entity->hitable = true;
		    //ellipsoidModelGenerate(entity->position,&entity->texture_dynamic,g_entity_static[entity->type].model_sphere,g_entity_static[entity->type].n_model_sphere,(Vec2){entity->move_angle,0},true);
		} break;
		case ENTITY_PARTICLE:{
            entity->non_interactive = true;
		} break;
	}
    entityAdd(entity);
	return entity;
}

static void entityResourcesFree(Entity* entity){
    if(entity->texture_dynamic.pixel_data)
        textureDestroy(entity->texture_dynamic);
    if(entity->cubemap.textures[0].pixel_data){
        for(int i = countof(entity->cubemap.textures);i--;)
            textureDestroy(entity->cubemap.textures[i]);
    }
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

        entityResourcesFree(entity_i);
        
		Entity* entity_d = entity_i;
		entity_i = entity_i->next;
		tFree(entity_d);
	}
}

void entityDestroyAll(void){
	for(Entity* entity = g_entity;entity;){
        entityResourcesFree(entity);
		Entity* copy = entity;
		entity = entity->next;
		tFree(copy);
	}
	g_entity = 0;
}

void entitySpawn(void){
	real angle = realRandom(FIXED_ONE * 16);
	Vec2 direction = vec2MulS(vec2Shl((Vec2){tCos(angle),tSin(angle)},5),realRandom(FIXED_ONE / 2) + FIXED_ONE / 2);
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

static bool entityTickSlime(Entity* slime){
	if(slime->on_ground && slime->attack_cooldown < 0){
		real angle;
        bool chase = true;
        chase &= lineOfSight(g_surface.position,slime->position);
        chase &= !g_player.movement_fly;
        if(slime->model_sphere != &model_sphere_slime_chase)
            chase &= vec3DistanceSquare(g_surface.position,slime->position) < FIXED_ONE * 0x100;
		if(chase){
            Vec2 pos_2d = {slime->position.x,slime->position.y};
			Vec2 direction = vec2Direction(pos_2d,(Vec2){g_surface.position.x,g_surface.position.y});
			slime->velocity.x += realShr((direction.x),1);
			slime->velocity.y += realShr((direction.y),1);
			angle = tArcTan2(direction.y,direction.x);
            slime->model_sphere = &model_sphere_slime_chase;
		}
		else{
			angle = realRandom(FIXED_ONE);
			slime->velocity.x += realShr((tCos(angle)),1);
			slime->velocity.y += realShr((tSin(angle)),1);
            slime->model_sphere = &model_sphere_slime;
		}
		slime->move_angle = -angle + FIXED_ONE / 2;
		slime->velocity.z += FIXED_ONE / 6;
        slime->attack_cooldown = realRandom(FIXED_ONE * 0x10) + FIXED_ONE * 0x10;
	}
    slime->attack_cooldown -= g_time.delta;
	return true;
}

static bool entityTickZombie(Entity* zombie){
	if(!zombie->on_ground)
		return true;

	if(!g_player.movement_fly && lineOfSight(g_surface.position,zombie->position)){
		real distance = vec2Distance((Vec2){zombie->position.x,zombie->position.y},(Vec2){g_surface.position.x,g_surface.position.y}) * 8;
		Vec2 offset = vec2MulS((Vec2){g_player.entity->velocity.x,g_player.entity->velocity.y},distance);
		Vec2 direction = vec2Direction((Vec2){zombie->position.x,zombie->position.y},vec2Add((Vec2){g_surface.position.x,g_surface.position.y},offset));
		zombie->velocity.x += realShr(direction.x,8);
		zombie->velocity.y += realShr(direction.y,8);

		zombie->move_angle = tArcTan2(direction.y,direction.x);
		zombie->is_moving = true;
	}
	else{
		zombie->velocity.x += realShr((tCos(zombie->move_angle)),10);
		zombie->velocity.y += realShr((tSin(zombie->move_angle)),10);
	}
	if(tRndChance(0x100)){
		zombie->is_moving = tRndChance(2);
		zombie->move_angle = realRandom(FIXED_ONE);
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
#if 0
			Vec3 route_position = vec3Shl(entity->pathfinding->route.positions[entity->pathfinding->route.n_positions],PATH_FIND_SIZE);
		
			Vec2 direction = vec2Direction((Vec2){entity->position.x,entity->position.y},(Vec2){route_position.x,route_position.y});
			entity->velocity.x += realShr(direction.x,8);
			entity->velocity.y += realShr(direction.y,8);
			
			Vec3 relative_position = vec3Sub(entity->position,route_position);
			real distance = vec3Dot(relative_position,relative_position);
			if(entity->pathfinding->distance_route_node <= distance){
				entity->pathfinding->route.n_positions -= 1;
				entity->pathfinding->distance_route_node = INT_MAX;
			}
			else{
				entity->pathfinding->distance_route_node = distance;
			}
#endif
		} break;
	}
	entity->pathfinding->cooldown -= 1;
	return true;
}

static bool entityTickParticle(Entity* entity){
	entity->lifetime -= g_time.delta;
	if(entity->lifetime < 0)
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
#if 1
	for(Entity* entity = (voxel->entity_list);entity;entity = entity->next_voxel){
		if(!entity->hitable)
			continue;
		if(intersectBoxPoint(position,entity->position,entity->hitbox))
			return entity;
	}
#endif
	return 0;
}

void entityHit(Entity* entity){
	entity->health -= 30;
	Vec3 death_position = entity->position;

	Vec2 knockback = vec2Direction((Vec2){g_surface.position.x,g_surface.position.y},(Vec2){entity->position.x,entity->position.y});
	entity->velocity.x += knockback.x / 2;
	entity->velocity.y += knockback.y / 2;
	entity->velocity.z += REAL_UNIT * 0xD0;
	for(int i = 0x100;i--;){
        Vec3 from_player = vec3Direction(g_surface.position,death_position);
		Vec3 velocity = vec3Add(vec3Rnd(),from_player);
        
		Entity* particle = entityCreate(death_position,ENTITY_PARTICLE);

		particle->color = (Vec3){
            realRandom(FIXED_ONE / 8),
            realRandom(FIXED_ONE / 8),
            realRandom(FIXED_ONE / 2) + FIXED_ONE / 2,
        };
        
		particle->health = tRnd() % 0x80 + 0x80;
		particle->size = FIXED_ONE / 32 + realRandom(FIXED_ONE / 32);
		particle->velocity = vec3Mix(velocity,from_player,FIXED_ONE / 2);
        particle->velocity = vec3Shl(particle->velocity,1);
        particle->circle = true;
        particle->bounce = true;
        particle->bounciness = REAL_UNIT * 0x80;
        particle->lifetime = FIXED_ONE * 0x10 + realRandom(FIXED_ONE * 0x10);
	}
}

static void spellAdjectiveParticleSpawn(Entity* entity){
	if(entity->adj_damage && tRndChance(16)){
		Entity* particle = entityCreate(entity->position,ENTITY_PARTICLE);
		particle->color = (Vec3){FIXED_ONE * 16,FIXED_ONE,FIXED_ONE};
		particle->health = tRnd() % 0x80 + 0x80;
		particle->size = FIXED_ONE / 16;
		particle->velocity = vec3Shr(vec3Rnd(),4);
		particle->particle_string = (String)STRING_LITERAL("#+");
	}
	if(entity->adj_speed && tRndChance(16)){
		Entity* particle = entityCreate(entity->position,ENTITY_PARTICLE);
		particle->color = (Vec3){FIXED_ONE,FIXED_ONE,FIXED_ONE * 16};
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
	Entity* entity_collided = boltMonsterCollision(&g_world.voxel,entity->position);
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
			real distance = vec3Distance(g_surface.position,entity->position);
			real shock = realDivR(FIXED_ONE,realMulR(distance,distance));
			g_player.entity->velocity = vec3Add(g_player.entity->velocity,vec3MulS(vec3Direction(entity->position,g_surface.position),shock));
			g_player.entity->health -= shock * 8;
		}
		for(Entity* entity_other = g_entity;entity_other;entity_other = entity_other->next){
			if(entity == entity_other)
				continue;
			if(!lineOfSight(entity->position,entity_other->position))
				continue;
			real distance = vec3Distance(entity_other->position,entity->position);
			real shock = realMulR(distance,distance);
			entity_other->velocity = vec3Add(entity_other->velocity,vec3MulS(vec3Direction(entity->position,entity_other->position),realDivR(FIXED_ONE * 4,realMulR(distance,distance))));
			entity_other->health -= shock / 8;
		}
		for(int i = 0x100;i--;){ 
			Entity* particle = entityCreate(entity->position,ENTITY_PARTICLE);
			particle->health = tRnd() % 0x400;
			particle->size = FIXED_ONE / 4 + realRandom(FIXED_ONE / 4);
			particle->velocity = vec3Shr(vec3Rnd(),2);
			particle->color = pixelColorToColor(0x808080);
			particle->no_gravity =true;
            particle->is_windy = true;
			particle->windy = vec3Shr(vec3MulS(vec3Rnd(),realRandom(FIXED_ONE)),11);
			particle->physics_friction_air = (FIXED_ONE - (FIXED_ONE / 16));
			particle->texture = g_textures + TEXTURE_SMOKE;
			particle->texture_offset = (Vec2){realRandom(FIXED_ONE),realRandom(FIXED_ONE)};
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
	Entity* entity_collided = boltMonsterCollision(&g_world.voxel,entity->position);
	if(entity_collided){
		//monsterHit(entity_collided);
		return false;
	}
	return true;
}

static bool entityTickBoss(Entity* boss){
	if(boss->on_ground && tRndChance(0x80)){
		real angle;
		if(!g_player.movement_fly && lineOfSight(g_surface.position,boss->position)){
			int distance = vec2Distance((Vec2){boss->position.x,boss->position.y},(Vec2){g_surface.position.x,g_surface.position.y}) * 8;
			Vec2 offset = vec2MulS((Vec2){g_player.entity->velocity.x,g_player.entity->velocity.y},distance);
			Vec2 direction = vec2Direction((Vec2){boss->position.x,boss->position.y},vec2Add((Vec2){g_surface.position.x,g_surface.position.y},offset));
			boss->velocity.x += realShr((direction.x),4);
			boss->velocity.y += realShr((direction.y),4);
			angle = tArcTan2(direction.y,direction.x);
		}
		else{
            angle = realRandom(FIXED_ONE);
			boss->velocity.x += realShr((tCos(angle)),4);
			boss->velocity.y += realShr((tSin(angle)),4);
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

static real punchAnimationOffset(real animation){
	real value = realMulR(realMulR(animation,animation),animation);
	return tSin(value / 2);
}

static bool entityTickWeapon(Entity* entity){
    Vec3 direction_x = getLookDirection(vec2Add(g_surface.angle,(Vec2){0,0}));
    Vec3 direction_y = getLookDirection(vec2Add(g_surface.angle,(Vec2){0,FIXED_ONE / 4}));
    Vec3 direction_z = vec3Cross(direction_x,direction_y);

    Vec3 wish_position = g_surface.position;
            
    wish_position = vec3Add(wish_position,vec3MulS(direction_x,REAL_UNIT * 0x140));
    wish_position = vec3Add(wish_position,vec3MulS(direction_y,REAL_UNIT * 0x100));
    wish_position = vec3Add(wish_position,vec3MulS(direction_z,REAL_UNIT * 0x100));
    
    wish_position = vec3Mix(entity->position,wish_position,FIXED_ONE / 2);

    if(g_equipped_staff){
        Vec3 recoil_position = vec3Add(g_surface.position,vec3Shl(getLookDirection(g_surface.angle),1));
        real percentage = 0;
        if(g_time.time > g_shoot_timestamp && g_time.time < g_delay_timestamp){
            int rel_end = g_delay_timestamp - g_shoot_timestamp;
            int relative = g_time.time - g_shoot_timestamp;

            percentage = realDivR(intToReal(relative / 0x100),intToReal(rel_end / 0x100) + REAL_EPSILON);
        }
        wish_position.z -= tSin(percentage / 2) / 4;
    }
    
    Vec3 punch_position = vec3Add(g_surface.position,vec3Shl(getLookDirection(g_surface.angle),1)); 
    wish_position = vec3Mix(wish_position,punch_position,punchAnimationOffset(entity->attack_cooldown));
 
    entity->velocity = vec3Shl(vec3Sub(wish_position,entity->position),3);

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
        movementUpdate(entity);
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

static void entityRender3d(Entity* entity,real entity_size){
	Vec3 point = pointToScreen(entity->position);
	if(!point.z)
		return;
	real size = spriteSize(entity->position,entity_size);

    real discrepancy = vec3DistanceSquare(entity->render_position,entity->position);
    Vec3 to_player = vec3Direction(entity->position,g_surface.position);

    if(discrepancy > FIXED_ONE){
        textureDestroy(entity->texture_dynamic);
        int texture_size = realShr(realToInt(size * 0x100) * tMax(g_surface.window_height,g_surface.window_width),10);
        entity->texture_dynamic = textureCreate(tMin(0x100,texture_size));
        Vec2 angle = (Vec2){entity->move_angle,0};
        ellipsoidModelCubemapGenerate(&entity->cubemap,entity->position,0);
		ellipsoidModelGenerate(&entity->cubemap,entity->position,entity->hitbox,&entity->texture_dynamic,entity->model_sphere,angle,true,size,entity_size);
        entity->render_direction = to_player;
        entity->render_position = entity->position;
    }
    else{
        discrepancy = vec3DistanceSquare(entity->render_direction,to_player);
            
        if(discrepancy > REAL_UNIT * 0x10){
            textureDestroy(entity->texture_dynamic);
            int texture_size = realShr(realToInt(size * 0x100) * tMax(g_surface.window_height,g_surface.window_width),10);
            entity->texture_dynamic = textureCreate(tMin(0x100,texture_size));
            Vec2 angle = (Vec2){entity->move_angle,0};
            ellipsoidModelGenerate(&entity->cubemap,entity->position,entity->hitbox,&entity->texture_dynamic,entity->model_sphere,angle,true,size,entity_size);
            entity->render_direction = to_player;
        }
    }
    spriteRender3d(entity->position,entity_size,&entity->texture_dynamic);
}

void entityDraw(Entity* entity){
	switch(entity->type){
        case ENTITY_WEAPON:{
            static ModelSprite model_hand = {
                .ellipsoid = {
                    {
                        .color = {FIXED_ONE,FIXED_ONE,FIXED_ONE},
                        .ellipsoid.radius = {FIXED_ONE,FIXED_ONE / 4,FIXED_ONE / 4},
                    },
                },
                .n_ellipsoid = 1,
            };
            static ModelSprite model_staff = {
                .ellipsoid = {
                    {
                        .color = {FIXED_ONE,FIXED_ONE / 2,FIXED_ONE / 4},
                        .ellipsoid.radius = {FIXED_ONE,FIXED_ONE / 4,FIXED_ONE / 4},
                    },
                },
                .n_ellipsoid = 1,
            };
            Vec3 position = entity->position;
            real size = spriteSize(position,FIXED_ONE);
            ModelSprite* model = g_equipped_staff ? &model_staff : &model_hand;

            real discrepancy = vec3DistanceSquare(entity->render_position,entity->position);

            if(discrepancy > FIXED_ONE){
                ellipsoidModelCubemapGenerate(&entity->cubemap,entity->position,0);
                ellipsoidModelGenerate(&entity->cubemap,position,(Vec3){0},&entity->texture_dynamic,model,(Vec2){REAL_UNIT * 12,-REAL_UNIT * 12},false,size,FIXED_ONE);
                entity->render_position = entity->position;
                entity->render_direction = getLookDirection(g_surface.angle);
            }
            else{
                discrepancy = vec3DistanceSquare(entity->render_direction,getLookDirection(g_surface.angle));
            
                if(discrepancy > REAL_UNIT * 0x10){
                    ellipsoidModelGenerate(&entity->cubemap,position,(Vec3){0},&entity->texture_dynamic,model,(Vec2){REAL_UNIT * 12,-REAL_UNIT * 12},false,size,FIXED_ONE);
                    entity->render_direction = getLookDirection(g_surface.angle);
                }
            }

            Vec3 point = pointToScreen(position);
            if(!point.z)
                break;
            Vec2 points[] = {
                {point.x - realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
                {point.x - realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
                {point.x + realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
                {point.x + realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
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
			real size = spriteSize(entity->position,FIXED_ONE / 8);
			Vec2 points[] = {
				{point.x - realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
				{point.x + realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
				{point.x + realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
				{point.x - realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
			};
			drawEllipses(&g_surface,point.x,point.y,realMulR(size * 4,g_surface.fov.x),realMulR(size * 4,g_surface.fov.y),vec3MulS(vec3Single(1 << 14),g_exposure));
			drawEllipses(&g_surface,point.x - size / 2,point.y + size / 2,realMulR(size,g_surface.fov.x),realMulR(size,g_surface.fov.y),vec3MulS(vec3Single(1 << 18),g_exposure));
			drawRectangle(&g_surface,point.x - size * 2 - size / 4,point.y - size / 2,realMulR(size,g_surface.fov.x),realMulR(size,g_surface.fov.y) * 4,vec3Single(1 << 16));
			real fuse = realMulR(realMulR(size,g_surface.fov.x),entity->health);
			drawRectangle(&g_surface,point.x - size * 2 - size / 4 - fuse / 2,point.y,realMulR(fuse,g_surface.fov.x),realMulR(size,g_surface.fov.y),pixelColorToColor(0x83B2EB));
			drawRectangle(&g_surface,point.x - size * 2 - size / 4 - fuse / 2 - size / 2,point.y,realMulR(size,g_surface.fov.x),realMulR(size,g_surface.fov.y),pixelColorToColor(0x1050FF));
		} break;
		case ENTITY_BOLT: case ENTITY_ORB:{
            /*
			Vec3 point = pointToScreen(entity->position);
			if(!point.z)
				return;
			real size = entitySpriteSize(entity->position,FIXED_ONE / 8);
            if(g_surface.backend == RENDER_BACKEND_SOFTWARE){
                spanEllipsesAdd(&g_surface,point.x,point.y,realMulR(size,g_surface.fov.x),realMulR(size,g_surface.fov.y),vec3MulS(entity->color_emit,g_exposure));
                return;
            }
            DrawPrimitive* polygon = primitiveToDraw();
            polygon->type = PRIMITIVE_ELLIPSIS;
            polygon->is_sprite = true;
            polygon->sprite_size = (Vec2){realMulR(size,g_surface.fov.x),realMulR(size,g_surface.fov.y)};
            polygon->position_sprite[0] = (Vec2){point.x,point.y};
            polygon->luminance = entity->color_emit;
            */
            entityRender3d(entity,entity->size);
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
            if(g_options.lighting_engine){
                int n_sample = 0x10;
                int n_fibbonaci = 0x100;
                Vec3 lum_acc = {0};
                for(int i = n_sample;i--;){
                    int fibbonaci_index = i * 16 % (n_fibbonaci + entity->n_luminance_sample);
                    Vec3 direction = fibonnaciSphereSample(fibbonaci_index,n_fibbonaci);
                    lum_acc = vec3Add(lum_acc,vec3Shr(rayLuminance(entity->position,direction,(RayLuminanceFlag){0}),4));
                    
                }
                lum_acc.x /= n_sample;
                lum_acc.y /= n_sample;
                lum_acc.z /= n_sample;
                
                entity->n_luminance_sample += 1;
                
                entity->luminance = vec3Mix(entity->luminance,lum_acc,FIXED_ONE / 16);
            }
			Vec3 point = pointToScreen(entity->position);
			if(!point.z)
				return;
			real size = spriteSize(entity->position,entity->size);
            size = realMulR(size,tMin(entity->lifetime / 4,FIXED_ONE));
            Vec2 points[] = {
				{point.x + realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
				{point.x + realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
				{point.x - realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
				{point.x - realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
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
                primitive->type = entity->circle ? PRIMITIVE_ELLIPSIS : PRIMITIVE_QUAD;
                if(entity->circle){
                    primitive->sprite_size.x = realMulR(size,g_surface.fov.x);
                    primitive->sprite_size.y = realMulR(size,g_surface.fov.y);
                    primitive->position_sprite[0] = (Vec2){point.x,point.y};
                }
                else{
                    for(int i = 4;i--;)
                        primitive->position_sprite[i] = points[i];
                }
			}
		} break;
		case ENTITY_PICKUP:{
			Vec3 point = pointToScreen(entity->position);
			if(!point.z)
				return;
			real size = spriteSize(entity->position,FIXED_ONE / 2);
			Vec2 points[] = {
				{point.x - realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
				{point.x + realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
				{point.x + realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
				{point.x - realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
			};

            DrawPrimitive* primitive = primitiveToDraw();
            primitive->is_sprite = true;
            primitive->type = PRIMITIVE_FRAME;
            primitive->position_sprite[0] = (Vec2){point.x - realMulR(size / 2,g_surface.fov.x),point.y - realMulR(size / 2,g_surface.fov.y)};
            primitive->sprite_size = (Vec2){realMulR(size,g_surface.fov.x),realMulR(size,g_surface.fov.y)};
            primitive->luminance = pixelColorToColor(0x808080);
            primitive->thickness = realShr(realMulR(size,g_surface.fov.x),4);
#if 0
			drawFrame(&g_surface,point.x - fixedMulR(size / 2,g_surface.fov.x),point.y - fixedMulR(size / 2,g_surface.fov.y),fixedMulR(size,g_surface.fov.x),fixedMulR(size,g_surface.fov.y),pixelColorToColor(0x808080),fixedMulR(size,g_surface.fov.x) >> 4);
#endif
            switch(entity->pickup_type){
				case SPELL_BOLT:{
					drawEllipses(&g_surface,point.x,point.y,realMulR(size / 3,g_surface.fov.x),realMulR(size / 3,g_surface.fov.y),vec3MulS(pixelColorToColor(0xFF0000),g_exposure));
				} break;
				case SPELL_BOMB:{
					drawEllipses(&g_surface,point.x,point.y,realMulR(size / 3,g_surface.fov.x),realMulR(size / 3,g_surface.fov.y),vec3MulS(pixelColorToColor(0x0000FF),g_exposure));
				} break;
				case SPELL_ORB:{
					drawEllipses(&g_surface,point.x,point.y,realMulR(size / 3,g_surface.fov.x),realMulR(size / 3,g_surface.fov.y),vec3MulS(pixelColorToColor(0x00FF00),g_exposure));
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
            int size = 0x80;
            Vec3 point = pointToScreen(entity->position);
            Vec2 points[] = {
				{point.x + realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
				{point.x + realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
				{point.x - realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
				{point.x - realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
			};
            drawPolygon(&g_surface,points,4,pixelColorToColor(0x00FF00));
        }
	}	
}

void entityDynamicLighting(void){
	for(Entity* entity = g_entity;entity;entity = entity->next){
        if(g_options.rd_dshadow && !entity->non_interactive)
            lightingEntityShadow(&g_world.voxel,entity);
		if(!entity->emit)
			continue;
        lightingEntityDynamic(&g_world.voxel,entity);
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
			real distance = vec3Distance(entity->position,g_surface.position);
			if(vec3Distance(voxel->entity_list->position,g_surface.position) > distance){
				entity->next_voxel = voxel->entity_list;
				voxel->entity_list = entity;
			}
			else{
				Entity* prev = voxel->entity_list;
				Entity* entity_l = prev->next_voxel;
				
				while(entity_l && vec3Distance(entity_l->position,g_surface.position) < distance){
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
