#include "physics.h"
#include "octree.h"
#include "main.h"
#include "entity.h"

#include "platform/audio.h"

void movementFly(void){
    if(g_voxel_interact)
        return;
    int speed = keyDown(KEY_LCONTROL) ? 0 : 2;
    if(keyDown(KEY_W)){
        g_player.entity->position.x -= realMulR(realShr(tCos(g_surface.angle.x),speed),g_time.delta);
        g_player.entity->position.y -= realMulR(realShr(tSin(g_surface.angle.x),speed),g_time.delta);
    }
    if(keyDown(KEY_S)){
        g_player.entity->position.x += realMulR(realShr(tCos(g_surface.angle.x),speed),g_time.delta);
        g_player.entity->position.y += realMulR(realShr(tSin(g_surface.angle.x),speed),g_time.delta);
    }
    if(keyDown(KEY_A)){
        g_player.entity->position.x += realMulR(realShr(tCos(g_surface.angle.x + FIXED_ONE / 4),speed),g_time.delta);
        g_player.entity->position.y += realMulR(realShr(tSin(g_surface.angle.x + FIXED_ONE / 4),speed),g_time.delta);
    }
    if(keyDown(KEY_D)){
        g_player.entity->position.x -= realMulR(realShr(tCos(g_surface.angle.x + FIXED_ONE / 4),speed),g_time.delta);
        g_player.entity->position.y -= realMulR(realShr(tSin(g_surface.angle.x + FIXED_ONE / 4),speed),g_time.delta);
    }
    if(keyDown(KEY_LSHIFT))
        g_player.entity->position.z -= realMulR(FIXED_ONE / (1 << speed),g_time.delta);
    if(keyDown(KEY_SPACE))
        g_player.entity->position.z += realMulR(FIXED_ONE / (1 << speed),g_time.delta);
}
#include "console.h"

static bool voxelCollision(Voxel* node,Voxel* query,Vec3 box_pos,Vec3 box_size){
    real block_size = depthToSize(node->depth);
    Vec3 block_pos = voxelWorldPos(node);
    if(node->type == VOXEL_PARENT){
        if(!intersectBoxCube(box_pos,box_size,block_pos,block_size))
            return false;
        
        bool result = false;
        
        for(int i = countof(node->child_s);i--;)
            result |= voxelCollision(node->child_s[i],query,box_pos,box_size);
        
        return result;
    }
    if(!intersectBoxCube(box_pos,box_size,block_pos,block_size))
        return false;
    return node == query;
}

static bool treeCollisionVoxelCheck(Vec3 pos,Vec3 size,Vec3 block_pos,real block_size,bool is_point){
    if(is_point){
#if 0
        debugPrint("position");
        printVec3(pos);
        debugPrint("voxel position");
        printVec3(block_pos);
        debugPrint("voxel size");
        printNumberNL(block_size);
#endif
        if(!intersectCubePoint(pos,block_pos,block_size))
            return false;
    }
    else{
        if(!intersectBoxCube(pos,size,block_pos,block_size))
            return false; 
    }
    return true;
}

static Vec3 torNormal(Vec3 pos,Vec2 tor){
    Vec3 t = vec3MulS(vec3MulS((Vec3){1.0,1.0,-1.0},tor.x),tor.x);
    return vec3Normalize(vec3Mul(pos,(vec3Sub(vec3Single(vec3Dot(pos,pos)-tor.y*tor.y),t))));
}

static Collision boxTreeCollisionRecursive(Entity* entity,Vec3 pos,Vec3 velocity,Voxel* voxel){
    real voxel_size = realShr(depthToSize(voxel->depth),1);
    Vec3 voxel_pos = voxelWorldPosCenter(voxel);

    Vec3 voxel_size_water = {
        voxel_size,
        voxel_size,
        voxel_size - FIXED_ONE / 4,
    };
    Vec3 normal = {.z = FIXED_ONE};
    VoxelStatic* voxel_s = g_voxel_static + voxel->type;

    if(!treeCollisionVoxelCheck(pos,entity->hitbox,voxel_pos,voxel_size,!entity->has_hitbox))
        return (Collision){0};

    if(!entity->non_interactive && voxel_s->translucent){
#if 1
        for(Entity* other = voxel->entity_list;other;other = other->next_voxel){
            if(entity == other)
                continue;
            if(entity->type == ENTITY_WEAPON && other->type == ENTITY_PLAYER)
                continue;
            if(!intersectBoxBox(pos,entity->hitbox,other->position,other->hitbox))
                continue;
            return (Collision){.entity = other,.time = 0,.normal = normal,.type = COLLISION_ENTITY}; 
        }
#endif
    }
    if(voxel_s->slope){ 
        Vec3 u = voxel_s->slope_u;
        Vec3 v = voxel_s->slope_v;

        Vec3 slope_pos = voxel_pos;

        slope_pos = vec3Add(slope_pos,vec3MulS(voxel_s->slope_offset,voxel_size));

        Vec3 position = vec3Sub(pos,slope_pos);
            
        Vec3 normal = vec3Cross(u,v);
        Plane plane = {.normal = normal,0};

        PlaneCollision collision;
        if(entity->has_hitbox){
            collision = intersectBoxPlane(position,entity->hitbox,plane);
            PRINT_VAR(collision);
        }
        else{
            bool pre_side = vec3Dot(plane.normal,position) + plane.distance > 0;
            bool post_side = vec3Dot(plane.normal,vec3Sub(position,velocity)) + plane.distance > 0;

            if(pre_side != post_side)
                collision = PLANE_BETWEEN;
            else
                collision = pre_side ? PLANE_FRONT : PLANE_BACK;
        }
            
        if(collision == PLANE_BETWEEN && intersectBoxPlane(vec3Sub(position,velocity),entity->hitbox,plane) == PLANE_FRONT){
            real time = realDivR(vec3Dot(normal,position),vec3Dot(normal,velocity));
            time = tClamp(time - REAL_UNIT * 0x08,0,FIXED_ONE);
            return (Collision){.type = COLLISION_VOXEL,.normal = normal,.time = time,.voxel = voxel};
        }
        else if(collision == PLANE_FRONT){
            return (Collision){0};
        }
    }
    else{
        switch(voxel->type){
            case VOXEL_PARENT:{
                Collision collision = {.time = INT_MAX};

                if(!treeCollisionVoxelCheck(pos,entity->hitbox,voxel_pos,voxel_size,!entity->has_hitbox))
                    return (Collision){0};
                
                for(int i = countof(voxel->child_s);i--;){
                    Collision child = boxTreeCollisionRecursive(entity,pos,velocity,voxel->child_s[i]);
                    if(child.voxel && collision.time > child.time)
                        collision = child;
                    collision.water_covered = tMax(collision.water_covered,child.water_covered);
                }
                return collision;
            } break;
            case VOXEL_AIR:
                return (Collision){0};
            case VOXEL_CYLINDER:{
                if(!intersectCylinderBox(pos,entity->hitbox,voxel_pos,getLookDirection(voxel->cylinder_angle),depthToSize(voxel->depth) / 4))
                    return (Collision){0};
                real time = 0;
                real time_step = FIXED_ONE / 2;
                for(int i = 0;i < 4;i++){
                    Vec3 position = vec3Mix(entity->position,pos,time + time_step);
                    time_step /= 2;
                    if(intersectCylinderBox(position,entity->hitbox,voxel_pos,getLookDirection(voxel->cylinder_angle),depthToSize(voxel->depth) / 4))
                         continue;
                    time += time_step;
                }
                float offset = vec3Dot(vec3Sub(entity->position,voxel_pos),getLookDirection(voxel->cylinder_angle));
                Vec3 normal = vec3Normalize(vec3Sub(entity->position,(vec3Add(voxel_pos,vec3MulS(getLookDirection(voxel->cylinder_angle),offset)))));
                return (Collision){.type = COLLISION_VOXEL,.normal = normal,.time = time,.voxel = voxel};
            } break;
            case VOXEL_TORUS:{
                if(!intersectTorusBox(pos,entity->hitbox,vec3Add(voxel_pos,voxel->primitive_position),(Vec2){depthToSize(voxel->depth),depthToSize(voxel->depth) / 2}))
                    return (Collision){0};
                real time = 0;
                real time_step = FIXED_ONE / 2;
                for(int i = 0;i < 4;i++){
                    Vec3 position = vec3Mix(entity->position,pos,time + time_step);
                    time_step /= 2;
                    if(!intersectTorusBox(position,entity->hitbox,vec3Add(voxel_pos,voxel->primitive_position),(Vec2){depthToSize(voxel->depth) / 1,depthToSize(voxel->depth) / 2}))
                         continue;
                    time += time_step;
                }
                Vec3 normal = torNormal(entity->position,(Vec2){depthToSize(voxel->depth) / 1,depthToSize(voxel->depth) / 2});
                return (Collision){.type = COLLISION_VOXEL,.normal = normal,.time = time,.voxel = voxel};
            } break;
            case VOXEL_SPHERE:{
                if(!intersectBoxSphere(entity->position,entity->hitbox,voxel_pos,voxel_size))
                    return (Collision){0};
                Vec3 normal = vec3Direction(voxel_pos,entity->position);
                return (Collision){.type = COLLISION_VOXEL,.normal = normal,.time = FIXED_ONE,.voxel = voxel};
            } break;
            case VOXEL_PLANE:{
                Vec3 p_normal = getLookDirection(voxel->angle);
                Vec3 tangent = vec3Normalize(vec3Cross(p_normal,(Vec3){FIXED_ONE,0,0}));
                Vec3 u = tangent;
                Vec3 v = vec3Cross(p_normal,tangent);

                Vec3 slope_pos = voxel_pos;

                slope_pos = vec3Add(slope_pos,vec3MulS(voxel_s->slope_offset,voxel_size));

                Vec3 position = vec3Sub(pos,slope_pos);
            
                Vec3 normal = vec3Cross(u,v);
                Plane plane = {.normal = normal,voxel->distance};

                PlaneCollision collision;
                if(entity->has_hitbox){
                    collision = intersectBoxPlane(position,entity->hitbox,plane);
                }
                else{
                    bool pre_side = vec3Dot(plane.normal,position) + plane.distance > 0;
                    bool post_side = vec3Dot(plane.normal,vec3Sub(position,velocity)) + plane.distance > 0;

                    if(pre_side != post_side)
                        collision = PLANE_BETWEEN;
                    else
                        collision = pre_side ? PLANE_FRONT : PLANE_BACK;
                }
            
                if(collision == PLANE_BETWEEN && intersectBoxPlane(vec3Sub(position,velocity),entity->hitbox,plane) == PLANE_FRONT){
                    real time = realDivR(vec3Dot(normal,position),vec3Dot(normal,velocity));
                    time = tClamp(time - REAL_UNIT * 0x08,0,FIXED_ONE);
                    return (Collision){.type = COLLISION_VOXEL,.normal = normal,.time = time,.voxel = voxel};
                }
                else if(collision == PLANE_FRONT){
                    return (Collision){0};
                }
            } break;
            case VOXEL_MOVABLE:{
                Vec3 block_size_m = {
                    voxel_size,
                    voxel_size,
                    realMulR(voxel_size,voxel->opened ? voxel->animation : FIXED_ONE - voxel->animation),
                };
                if(!intersectBoxBox(pos,entity->hitbox,voxel_pos,block_size_m))
                    return (Collision){0};
            } break;
            case VOXEL_PRESSURE_PLATE:{
                if(!voxel->animation){
                    voxelLinkSignal(voxel);
                    voxelTickListAdd(voxel);
                }
                voxel->animation = 0x08;
            } break;
            case VOXEL_DOOR:{
                real door_size = realMulR(voxel_size,FIXED_ONE - voxel->animation) / 2;
                if(voxel->opened)
                    door_size = voxel_size / 2 - door_size;
                real door_size_inv = voxel_size / 2 - door_size;

                bool box_1 = intersectBoxBox(pos,entity->hitbox,voxel_pos,(Vec3){voxel_size,door_size,voxel_size});
                bool box_2 = intersectBoxBox(pos,entity->hitbox,(Vec3){voxel_pos.x,voxel_pos.y + voxel_size / 2 + door_size_inv,voxel_pos.z},(Vec3){voxel_size,door_size,voxel_size}); 

                if(!box_1 && !box_2)
                    return (Collision){0}; 
            } break;
            case VOXEL_WATER:{
                real covered = entity->position.z + entity->hitbox.z - (voxel_pos.z + voxel_size);
                return (Collision){.water_covered = FIXED_ONE - tClamp(realDivR(covered,entity->hitbox.z),0,FIXED_ONE)}; 
            } 
        }
    }

    Vec3 penetration;

    for(int i = 3;i--;){
        if(velocity.a[i] < 0)
            penetration.a[i] = voxel_pos.a[i] + voxel_size - (pos.a[i] - entity->hitbox.a[i]);
        else
            penetration.a[i] = pos.a[i] + entity->hitbox.a[i] - (voxel_pos.a[i] - voxel_size);
    }
    penetration = vec3Div(penetration,(Vec3){tAbs(velocity.x),tAbs(velocity.y),tAbs(velocity.z)});

    Vec3Axis hit_axis = VEC3_Z;

    for(Vec3Axis i = hit_axis;i--;){
        if(penetration.a[i] > FIXED_ONE || penetration.a[i] <= 0)
            continue;
        if(
           penetration.a[hit_axis] < penetration.a[i] ||
           penetration.a[hit_axis] > FIXED_ONE ||
           penetration.a[hit_axis] <= 0
           )
            hit_axis = i;
    }

    normal = g_normal_table[hit_axis << 1 | (velocity.a[hit_axis] <= 0)];

    real time = tClamp(FIXED_ONE - penetration.a[hit_axis] - REAL_UNIT * 0x08,0,FIXED_ONE);

    return (Collision){.type = COLLISION_VOXEL,.voxel = voxel,.time = time,.normal = normal}; 
}

void physicsPointResolve(Vec3* position,Vec3* velocity){
    bool collide[3];
    
    collide[VEC3_X] = voxelPositionGet(vec3Add(*position,(Vec3){velocity->x,0,0}))->type != VOXEL_AIR;
    collide[VEC3_Y] = voxelPositionGet(vec3Add(*position,(Vec3){0,velocity->y,0}))->type != VOXEL_AIR;
    collide[VEC3_Z] = voxelPositionGet(vec3Add(*position,(Vec3){0,0,velocity->z}))->type != VOXEL_AIR;

    for(int i = countof(collide);i--;){
        if(collide[i]){
            velocity->a[i] = 0;
        }
        else{
            position->a[i] += velocity->a[i];
        }
    }
}

#define JUMP_HEIGHT (FIXED_ONE)
#define WALK_SPEED (FIXED_ONE / 2)
#define DIAGONAL 0x0000B53C
#define BHOP_BONUS (FIXED_ONE / 128)

void movementUpdate(Entity* entity){
    real time = FIXED_ONE;
    bool in_air = true;
    Vec3 velocity_delta = vec3MulS(entity->velocity,g_time.delta);
    Collision collision_latest;
    
    for(int i = 0x20;;i--){
        Vec3 vel_itt = vec3MulS(velocity_delta,time);
        Collision collision = boxTreeCollisionRecursive(entity,vec3Add(entity->position,vel_itt),vel_itt,&g_world.voxel);        

        if(!collision.voxel || !i){
            if(i)
                entity->position = vec3Add(entity->position,vec3MulS(velocity_delta,time));
            break;
        }
        collision_latest = collision;
        time -= realMulR(collision.time,time);
#if 0
        PRINT_VAR(i);
        PRINT_VEC3(velocity_delta);
        PRINT_VEC3(entity->position);
#endif
        if(collision.type == COLLISION_ENTITY){
            if(collision.entity == entity->parent){
                entity->position = vec3Add(entity->position,vec3MulS(velocity_delta,time));
                continue;
            }
            switch(entity->type){
                case ENTITY_BOLT:{
                    if(entity->health < 0)
                        break;
                    Entity* collider = collision.entity;
                    if(collider->type == ENTITY_SLIME){
                        entityHit(collision.entity);
                        entity->health = -REAL_EPSILON;
                    }
                } break;
                case ENTITY_WEAPON:{
                    if(entity->attack_cooldown < REAL_EPSILON)
                        break;
                
                    if(collision.type == COLLISION_ENTITY){
                        entityHit(collision.entity);
                        entity->attack_cooldown = 0;
                        break;
                    }
    
                    RayHit hit = rayHitPosition(g_surface.position,vec3Direction(g_player.entity->position,entity->position));

                    if(!hit.voxel)
                        break;

                    VoxelStatic* voxel_s = g_voxel_static + hit.voxel->type;
                    audioPlay(entity->position,AUDIO_PUNCH_HIT);
                } break;
                case ENTITY_SLIME:{
                    Entity* collider = collision.entity;
                
                    if(collider->type == ENTITY_PLAYER){
                        Vec2 direction = vec2Direction((Vec2){collider->position.x,collider->position.y},(Vec2){entity->position.x,entity->position.y});
                        entity->velocity.x += direction.x / 0x80;
                        entity->velocity.y += direction.y / 0x80;
                        entity->velocity.z += FIXED_ONE / 0x80;

                        collider->velocity.x -= direction.x / 0x80;
                        collider->velocity.y -= direction.y / 0x80;
                        collider->velocity.z += FIXED_ONE / 0x80;
#if 0
                        collider->health -= FIXED_ONE / 4;
#endif
                        entity->attack_cooldown = 0x80;
                    }
                } break;
            }
            continue;
        }
        if(tAbs(collision.normal.z) < REAL_EPSILON){
            real upper_z = entity->position.z - entity->hitbox.z;
            real height_delta = voxelWorldPos(collision.voxel).z + depthToSize(collision.voxel->depth) - upper_z;
            if(height_delta < FIXED_ONE - FIXED_ONE / 3){
                entity->position.z += height_delta;
                continue;
            }
        }
        entity->position = vec3Add(entity->position,vec3MulS(vel_itt,collision.time));
        if(collision.time == 0){
            Vec3 epsilon = vec3Shr(collision.normal,8);
            velocity_delta = vec3Add(velocity_delta,epsilon);
            entity->velocity = vec3Add(entity->velocity,epsilon);
        }
        else{
            if(entity->bounce){
                velocity_delta = vec3Reflect(velocity_delta,collision.normal);
                entity->velocity = vec3Reflect(entity->velocity,collision.normal);
                entity->velocity = vec3MulS(entity->velocity,entity->bounciness);
            }
            else{
                velocity_delta = vec3Sub(velocity_delta,vec3MulS(collision.normal,vec3Dot(velocity_delta,collision.normal)));
                entity->velocity = vec3Sub(entity->velocity,vec3MulS(collision.normal,vec3Dot(entity->velocity,collision.normal)));
            }
        }
#if 0
        debugPrint("velocity post: ");
        printVec3(velocity);
#endif       
        if(collision.normal.z > 0)
            in_air = false;
    }

    real friction = in_air ? entity->physics_friction_air : entity->physics_friction_ground;
    friction = tMix(friction,entity->physics_friction_ground,collision_latest.water_covered);
    friction = realMulR(friction,g_time.delta);
    entity->velocity = vec3Sub(entity->velocity,vec3MulS(entity->velocity,friction));

    if(!entity->no_gravity)
		entity->velocity.z -= realMulR(PHYSICS_GRAVITY,g_time.delta);

    entity->on_ground = !in_air;

    if(entity->type == ENTITY_PLAYER){
        Vec2 wish_dir = {0};
        if(keyDown(KEY_W)){
            wish_dir.x -= tCos(g_surface.angle.x);
            wish_dir.y -= tSin(g_surface.angle.x);
        }
        if(keyDown(KEY_S)){
            wish_dir.x += tCos(g_surface.angle.x);
            wish_dir.y += tSin(g_surface.angle.x);
        }
        if(keyDown(KEY_D)){
            wish_dir.x -= tCos(g_surface.angle.x + FIXED_ONE / 4);
            wish_dir.y -= tSin(g_surface.angle.x + FIXED_ONE / 4);
        }
        if(keyDown(KEY_A)){
            wish_dir.x += tCos(g_surface.angle.x + FIXED_ONE / 4);
            wish_dir.y += tSin(g_surface.angle.x + FIXED_ONE / 4);
        }
        if(wish_dir.x && wish_dir.y)
            wish_dir = vec2Normalize(wish_dir);
    
        wish_dir = vec2MulS(wish_dir,WALK_SPEED);

        if(in_air)
            wish_dir = vec2MulS(wish_dir,FIXED_ONE / 8);

        g_player.entity->velocity.x += realMulR(wish_dir.x,g_time.delta);
        g_player.entity->velocity.y += realMulR(wish_dir.y,g_time.delta);

        if(!IS_FLOAT(real)){
            if(g_player.entity->velocity.x < 0)
                g_player.entity->velocity.x += 1;
            if(g_player.entity->velocity.y < 0)
                g_player.entity->velocity.y += 1;
        }

        if(!in_air && keyDown(KEY_SPACE)){
            audioPlay(vec3Add(g_player.entity->position,(Vec3){0,0,-FIXED_ONE}),AUDIO_JUMP);
            g_player.entity->velocity = vec3Add(g_player.entity->velocity,vec3MulS(collision_latest.normal,JUMP_HEIGHT));
        }
        if(collision_latest.water_covered > REAL_EPSILON){
            if(keyDown(KEY_LSHIFT))
                g_player.entity->velocity.z -= FIXED_ONE / 8;
            if(keyDown(KEY_SPACE))
                g_player.entity->velocity.z += FIXED_ONE / 8;
        }
    }

}

void movementNormal(void){
    if(g_voxel_interact)
        return;

    movementUpdate(g_player.entity);
#if 0
    if(!in_air){
        static bool pressed;
    }
    
	if(collision_flags.ladder){
		g_player.entity->velocity.z = 0;
		if(keyDown(KEY_LSHIFT))
			g_player.entity->velocity.z -= FIXED_ONE / 16;
		else
			g_player.entity->velocity.z += FIXED_ONE / 16;
	}
#endif
}
