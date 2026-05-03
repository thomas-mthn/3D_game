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
} EntityType;

structure(ModelEllipsoid){
	Vec3 position;
	Vec3 radius;
	Vec3 color;
};

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

    Vec3 hitbox;
	int size;
	int health;
	Texture texture_dynamic;
	Texture* texture;
    
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

	int move_angle;
    
	Vec3 render_position;
	Vec3 render_direction;
    
	Staff staff;
	SpellType pickup_type;
	int physics_friction_ground;
	int physics_friction_air;
	Vec3 windy;
	int attack_cooldown;
	String particle_string;

    int  n_model_sphere;    
	ModelEllipsoid* model_sphere;

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
int entitySpriteSize(Vec3 position,int size);
void entityTick(void);
void entityVoxelInsertSimulation(void);
void entityVoxelInsertRender(void);
void entityVoxelRemove(void);
Entity* entityRayCollision(Entity* entity_list,Vec3 position,Vec3 direction);
void entityDynamicLighting(void);

void entityHit(Entity* monster);

void ellipsoidModelGenerate(Vec3 position,Texture* texture,ModelEllipsoid* ellipsoids,int n_ellipsoid,Vec2 model_angle,bool angle_player);

#endif
