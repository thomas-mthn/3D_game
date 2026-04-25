#ifndef PHYSICS_H
#define PHYSICS_H

#include "langext.h"
#include "vec3.h"

#define PHYSICS_FRICTION_AIR    0x400
#define PHYSICS_FRICTION_GROUND 0x1800
#define PHYSICS_FRICTION_WATER  (FIXED_ONE - (FIXED_ONE >> 2))
#define PHYSICS_GRAVITY         (FIXED_ONE / 128)

#define PLAYER_SIZE (Vec3){FIXED_ONE,FIXED_ONE,FIXED_ONE + FIXED_ONE / 2 + FIXED_ONE / 4}

structure(CollisionFlags){
    bool ladder : 1;
    bool water : 1;
};

void movementFly(void);
void movementNormal(void);
bool boxTreeCollision(Vec3 pos,Vec3 size,int* max_height,CollisionFlags* flags);
Vec3 playerHitboxGet(void);

void physicsPointResolve(Vec3* position,Vec3* velocity);

#endif
