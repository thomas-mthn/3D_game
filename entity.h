#ifndef ENTITY_H
#define ENTITY_H

#include "vec3.h"
#include "vec2.h"
#include "texture.h"
#include "pathfinding.h"
#include "staff.h"
#include "string.h"

structure(Voxel);

typedef enum{
	ENTITY_MONSTER,
	ENTITY_SLIME,
	ENTITY_ZOMBIE,
	ENTITY_PARTICLE,
	ENTITY_PICKUP,
	ENTITY_BOLT,
	ENTITY_ORB,
	ENTITY_BOMB,
	ENTITY_STAFF,
	ENTITY_BOSS,
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
	enum{
		ENTITY_FLAG_EMIT          = 1 << 0,
		ENTITY_FLAG_NO_GRAVITY    = 1 << 1,
		ENTITY_FLAG_PHYSICS_STAIR = 1 << 2,
		ENTITY_FLAG_HITABLE       = 1 << 3,
		ENTITY_FLAG_WINDY         = 1 << 4,
	} flags;
	int size;
	int health;
	Texture texture_dynamic;
	Texture* texture;
	Voxel* model;
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
	bool is_moving;
	int move_angle;
	Vec3 render_position;
	Vec2 render_angle;
	Staff staff;
	SpellType pickup_type;
	int physics_friction_ground;
	int physics_friction_air;
	bool on_ground;
	Vec3 windy;
	int attack_cooldown;
	bool adj_speed;
	bool adj_damage;
	String particle_string;
};

structure(ModelSphere){
	Vec3 position;
	int radius;
	Vec3 color;
};

structure(EntityStatic){
	bool has_hitbox;
	Vec3 hitbox;
	bool emit;
	bool bounce;
	Vec3 color;
	int  health;
	int  n_model_sphere;
	ModelSphere* model_sphere;
};

extern EntityStatic g_entity_static[];
extern Entity* g_entity;

Entity* entityCreate(Vec3 position,EntityType type);
void entityDestroy(void);
void entityDestroyAll(void);
void entitySpawn(void);

void entityDraw(Entity* entity);
void entityDrawHitbox(void);

void entityInit(void);
void entityTick(void);
void entityVoxelInsert(void);
void entityVoxelRemove(void);
Entity* entityRayCollision(Entity* entity_list,Vec3 position,Vec3 direction);
void entityDynamicLighting(void);

void entityHit(Entity* monster);

#endif
