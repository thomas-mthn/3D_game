#ifndef ENTITY_H
#define ENTITY_H

#include "vec3.h"
#include "vec2.h"
#include "texture.h"
#include "pathfinding.h"
#include "staff.h"
#include "string.h"

structure(Voxel);
structure(ModelSprite);

typedef enum{
	ENTITY_MONSTER = 1,
	ENTITY_SLIME,
	ENTITY_ZOMBIE,
	ENTITY_PARTICLE,
	ENTITY_PICKUP,
	ENTITY_BOLT,
	ENTITY_ORB,
	ENTITY_BOMB,
	ENTITY_STAFF,
	ENTITY_BOSS,
    ENTITY_WEAPON,
    ENTITY_PLAYER,
} EntityType;

structure(Entity){
	Entity* next;
	Entity* next_voxel;
	EntityType type;
	Vec3 velocity;
	Vec3 position;
	Vec2 angle;
	union{
		struct{
			int n_luminance_sample;
			Vec3 luminance;
		};
		struct{
			int n_luminance_sample_3d[6];
			Vec3 luminance_3d[6];
		};
	};
	Vec3 color;
	Vec3 color_emit;
    real bounciness;
    
    bool emit : 1;
    bool no_gravity : 1;
    bool physics_stair : 1;
    bool hitable : 1;
    bool is_windy : 1;
    bool adj_speed : 1;
	bool adj_damage : 1;
    bool on_ground : 1;
    
    bool is_moving : 1;
	bool bounce : 1;
    bool has_hitbox : 1;
    bool non_interactive : 1;
    bool circle : 1;
    bool particle_shrink : 1;

    Entity* parent;

    Vec3 hitbox;
	real size;
	real health;
    real lifetime;
	Texture texture_dynamic;
	Texture* texture;
    Cubemap cubemap;
    
	Vec2 texture_offset;
	int texture_size;
	int gravitate_player_freeze;
	struct{
		enum{
			ENTITY_PATHFIND_IDLE,
			ENTITY_PATHFIND_DIRECT,
			ENTITY_PATHFIND_ROUTE,
		} state;
		Route route;
		Vec3 direct_position;
		int cooldown;
		int distance_route_node;
	}* pathfinding;

	real move_angle;
    
	Vec3 render_position;
	Vec3 render_direction;
    
	Staff staff;
	SpellType pickup_type;
	real physics_friction_ground;
	real physics_friction_air;
	Vec3 windy;
	real attack_cooldown;
	String particle_string;

	ModelSprite* model_sphere;

    Voxel* inside;
};

extern Entity* g_entity;

Entity* entityCreate(Vec3 position,EntityType type);
void entityAdd(Entity* entity);
void entityDestroy(void);
void entityDestroyAll(void);
void entitySpawn(void);

void entityDraw(Entity* entity);
void entityDrawHitbox(void);

void entityInit(void);
void entityTick(void);
void entityVoxelInsertSimulation(void);
void entityVoxelInsertRender(void);
void entityVoxelRemove(void);
Entity* entityRayCollision(Entity* entity_list,Vec3 position,Vec3 direction);
void entityDynamicLighting(void);

void entityHit(Entity* monster);

#endif
