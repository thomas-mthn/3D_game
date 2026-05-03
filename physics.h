#ifndef PHYSICS_H
#define PHYSICS_H

#include "langext.h"
#include "vec3.h"

#define PHYSICS_FRICTION_AIR    0x1000
#define PHYSICS_FRICTION_GROUND 0x10000
#define PHYSICS_FRICTION_WATER  (FIXED_ONE - (FIXED_ONE >> 2))
#define PHYSICS_GRAVITY         (FIXED_ONE / 8)

#define PLAYER_SIZE (Vec3){FIXED_ONE / 2,FIXED_ONE / 2,(FIXED_ONE + FIXED_ONE / 2 + FIXED_ONE / 4) / 2}

structure(Voxel);
structure(Entity);

structure(CollisionFlags){
    bool ladder : 1;
    bool water : 1;
};

structure(Collision){
    Vec3 normal;
    int time;
    enum{
        COLLISION_NONE,
        COLLISION_VOXEL,
        COLLISION_ENTITY,
    } type;
    union{
        Voxel* voxel;
        Entity* entity;
    };
};

structure(MovementFlags){
    bool point : 1;
    bool bounce : 1;
};

void movementFly(void);
void movementNormal(void);
bool movementUpdate(Entity* entity);

void physicsPointResolve(Vec3* position,Vec3* velocity);

#endif
