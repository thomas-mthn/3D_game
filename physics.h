#ifndef PHYSICS_H
#define PHYSICS_H

#include "langext.h"
#include "vec3.h"

#define PHYSICS_FRICTION_AIR    (FIXED_ONE - (FIXED_ONE >> 8))
#define PHYSICS_FRICTION_GROUND (FIXED_ONE - (FIXED_ONE >> 4))
#define PHYSICS_FRICTION_WATER  (FIXED_ONE - (FIXED_ONE >> 2))
#define PHYSICS_GRAVITY         (FIXED_ONE / 128)

#define PLAYER_SIZE (Vec3){FIXED_ONE,FIXED_ONE,FIXED_ONE + FIXED_ONE / 2 + FIXED_ONE / 4}

typedef enum{
	COLLISION_FLAG_LADDER = (1 << 0),
	COLLISION_FLAG_WATER  = (1 << 1),
} CollisionFlags;

void movementFly(void);
void movementNormal(void);
bool boxTreeCollision(Vec3 pos,Vec3 size,int* max_height,CollisionFlags* flags);
Vec3 playerHitboxGet(void);

#endif
