#include "physics.h"
#include "octree.h"
#include "main.h"
#include "audio.h"

void movementFly(void){
    if(g_voxel_interact)
        return;
    int speed = keyDown(KEY_LCONTROL) ? 0 : 2;
    if(keyDown(KEY_W)){
        g_player.entity.position.x -= fixedMulR(tCos(g_surface.angle.x) >> speed,g_time.delta);
        g_player.entity.position.y -= fixedMulR(tSin(g_surface.angle.x) >> speed,g_time.delta);
    }
    if(keyDown(KEY_S)){
        g_player.entity.position.x += fixedMulR(tCos(g_surface.angle.x) >> speed,g_time.delta);
        g_player.entity.position.y += fixedMulR(tSin(g_surface.angle.x) >> speed,g_time.delta);
    }
    if(keyDown(KEY_A)){
        g_player.entity.position.x += fixedMulR(tCos(g_surface.angle.x + FIXED_ONE / 4) >> speed,g_time.delta);
        g_player.entity.position.y += fixedMulR(tSin(g_surface.angle.x + FIXED_ONE / 4) >> speed,g_time.delta);
    }
    if(keyDown(KEY_D)){
        g_player.entity.position.x -= fixedMulR(tCos(g_surface.angle.x + FIXED_ONE / 4) >> speed,g_time.delta);
        g_player.entity.position.y -= fixedMulR(tSin(g_surface.angle.x + FIXED_ONE / 4) >> speed,g_time.delta);
    }
    if(keyDown(KEY_LSHIFT))
        g_player.entity.position.z -= fixedMulR(0x10000 >> speed,g_time.delta);
    if(keyDown(KEY_SPACE))
        g_player.entity.position.z += fixedMulR(0x10000 >> speed,g_time.delta);
}
#include "console.h"

static bool voxelCollision(Voxel* node,Voxel* query,Vec3 box_pos,Vec3 box_size){
    int block_size = depthToSize(node->depth);
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

static bool treeCollisionVoxelCheck(Vec3 pos,Vec3 size,Vec3 block_pos,int block_size,bool is_point){
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

static Collision boxTreeCollisionRecursive(Entity* entity,Vec3 pos,Vec3 velocity,Voxel* voxel,int* max_height){
    int block_size = depthToSize(voxel->depth) >> 1;
    Vec3 block_pos = voxelWorldPosCenter(voxel);

    Vec3 block_size_water = {
        block_size,
        block_size,
        block_size - FIXED_ONE / 4,
    };
    Vec3 normal = {.z = FIXED_ONE};
    VoxelStatic* voxel_s = g_voxel_static + voxel->type;

    if(voxel_s->translucent){
        for(Entity* other = voxel->entity_list;other;other = other->next_voxel){
            if(entity == other)
                continue;
            if(!intersectBoxBox(pos,entity->hitbox,other->position,other->hitbox))
                continue;
            return (Collision){.entity = entity,.time = 0,.normal = normal,.type = COLLISION_ENTITY}; 
        }
    }

    if(voxel_s->slope){
        if(!treeCollisionVoxelCheck(pos,entity->hitbox,block_pos,block_size,!entity->has_hitbox))
            return (Collision){0};
        
        Vec3 u = voxel_s->slope_u;
        Vec3 v = voxel_s->slope_v;

        Vec3 voxel_pos = block_pos;

        voxel_pos = vec3Add(voxel_pos,vec3MulS(voxel_s->slope_offset,block_size));

        Vec3 position = vec3Sub(pos,voxel_pos);
            
        Vec3 normal = vec3Cross(u,v);
        Plane plane = {.normal = normal,0};

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
            int time = 0;
            Vec3 prev_pos = vec3Sub(pos,velocity);
            for(int i = 0;i < 8;i++){
                int offset = FIXED_ONE >> i + 1;
                    
                Vec3 test_pos = vec3Mix(prev_pos,pos,time + offset);
                test_pos = vec3Sub(test_pos,voxel_pos);
                if(intersectBoxPlane(test_pos,entity->hitbox,plane) != PLANE_BETWEEN)
                    time += offset;
            }
            return (Collision){.normal = normal,.time = time,.voxel = voxel};
        }
        else if(collision == PLANE_FRONT){
            return (Collision){0};
        }
    }
    else{
        switch(voxel->type){
            case VOXEL_PARENT:{
                Collision collision = {.time = INT_MAX};

                if(!treeCollisionVoxelCheck(pos,entity->hitbox,block_pos,block_size,!entity->has_hitbox))
                    return (Collision){0};
                
                for(int i = countof(voxel->child_s);i--;){
                    Collision child = boxTreeCollisionRecursive(entity,pos,velocity,voxel->child_s[i],max_height);
                    if(child.voxel && collision.time > child.time)
                        collision = child;
                }
                return collision;
            } break;
            case VOXEL_AIR:
                return (Collision){0}; 
            case VOXEL_MOVABLE:{
                Vec3 block_size_m = {
                    block_size,
                    block_size,
                    fixedMulR(block_size,voxel->opened ? voxel->animation : FIXED_ONE - voxel->animation),
                };
                if(!intersectBoxBox(pos,entity->hitbox,block_pos,block_size_m))
                    return (Collision){0};
            } break;
            case VOXEL_PRESSURE_PLATE:{
                if(!intersectBoxCube(pos,entity->hitbox,block_pos,block_size))
                    return (Collision){0}; 
                if(!voxel->animation){
                    voxelLinkSignal(voxel);
                    voxelTickListAdd(voxel);
                }
                voxel->animation = 0x08;
            } break;
            case VOXEL_DOOR:{
                int door_size = fixedMulR(block_size,FIXED_ONE - voxel->animation) / 2;
                if(voxel->opened)
                    door_size = block_size / 2 - door_size;
                int door_size_inv = block_size / 2 - door_size;

                bool box_1 = intersectBoxBox(pos,entity->hitbox,block_pos,(Vec3){block_size,door_size,block_size});
                bool box_2 = intersectBoxBox(pos,entity->hitbox,(Vec3){block_pos.x,block_pos.y + block_size / 2 + door_size_inv,block_pos.z},(Vec3){block_size,door_size,block_size}); 

                if(!box_1 && !box_2)
                    return (Collision){0}; 
            } break;
            case VOXEL_WATER:{
                if(!intersectBoxBox(pos,entity->hitbox,block_pos,block_size_water))
                    return (Collision){0}; 
                if(!voxel->animation){
                    voxel->splash_position = (Vec2){pos.x,pos.y};
                    voxel->splash_tick = g_time.tick;
                    voxelTickListAdd(voxel);
                    voxel->animation = FIXED_ONE;
                }
                else if(voxel->collision_tick != g_time.tick && voxel->collision_tick != g_time.tick - 1){
                    voxel->splash_position = (Vec2){pos.x,pos.y};
                    voxel->splash_tick = g_time.tick;
                    voxel->animation = FIXED_ONE;
                }
                else{
                    voxel->animation = tMax(voxel->animation,FIXED_ONE / 2);
                }
                voxel->collision_tick = g_time.tick;
            } return (Collision){0}; 
            default:{
                if(!treeCollisionVoxelCheck(pos,entity->hitbox,block_pos,block_size,!entity->has_hitbox))
                    return (Collision){0};
            } 
        }
    }
    if(max_height){
        Vec3 voxel_position = voxelWorldPos(voxel);
        int voxel_size;
        if(voxel->type == VOXEL_MOVABLE)
            voxel_size = fixedMulR(block_size,voxel->opened ? voxel->animation : FIXED_ONE - voxel->animation);
        else
            voxel_size = block_size;
        *max_height = tMax(*max_height,voxel_position.z + voxel_size);
    }
    for(int i = 3;i--;){
        int table[][2] = {{1,2},{0,2},{0,1}};
        Vec3 v = vec3Shl(velocity,1);
        
        v.a[table[i][0]] = 0;
        v.a[table[i][1]] = 0;
        
        Vec3 test_pos = vec3Sub(pos,v);
        
        if(!voxelCollision(&g_voxel,voxel,test_pos,entity->hitbox)){
            normal = g_normal_table[i << 1 | (velocity.a[i] <= 0)];
            break;
        }
    }
    int time = 0;
    Vec3 prev_pos = vec3Sub(pos,velocity);
    for(int i = 0;i < 8;i++){
        int offset = FIXED_ONE >> i + 1;
        Vec3 test_pos = vec3Mix(prev_pos,pos,time + offset);
        
        if(!treeCollisionVoxelCheck(test_pos,entity->hitbox,block_pos,block_size,!entity->has_hitbox))
            time += offset;
    }
    return (Collision){.voxel = voxel,.time = time,.normal = normal}; 
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

bool movementUpdate(Entity* entity){
    int max_height_x = 0;
    int max_height_y = 0;
    int time = FIXED_ONE;
    bool in_air = true;
    Vec3 velocity_delta = vec3MulS(entity->velocity,g_time.delta);
    for(int i = 0x20;;i--){
        Vec3 vel_itt = vec3MulS(velocity_delta,time);
        Collision collision = boxTreeCollisionRecursive(entity,vec3Add(entity->position,vel_itt),vel_itt,&g_voxel,&max_height_x);        

        if(!collision.voxel || !i){
            if(i)
                entity->position = vec3Add(entity->position,vec3MulS(velocity_delta,time));
            break;
        }
        
        entity->position = vec3Add(entity->position,vec3MulS(vel_itt,collision.time));
            
        time -= fixedMulR(collision.time,time);
#if 0
        debugPrint("\nitt: ");
        printNumberNL(i);
        debugPrint("normal: ");
        printVec3(collision.normal);
        debugPrint("time: ");
        printNumberNL(collision.time);
        debugPrint("velocity: ");
        printVec3(*velocity);
        debugPrint("time global: ");
        printNumberNL(time);
#endif
        switch(entity->type){
            case ENTITY_WEAPON:{
                if(!entity->attack_cooldown)
                    break;
                
                entity->attack_cooldown = 0;
                
                if(collision.type == COLLISION_ENTITY){
                    Vec3 direction = vec3Direction(entity->position,collision.entity->position);
                    collision.entity->velocity = vec3Add(collision.entity->velocity,direction);
                    collision.entity->velocity.z += FIXED_ONE * 256;
                    break;
                }
    
                RayHit hit = rayHitPosition(g_surface.position,vec3Direction(g_player.entity.position,entity->position));

                if(!hit.voxel)
                    break;

                VoxelStatic* voxel_s = g_voxel_static + hit.voxel->type;
    
                for(int i = 0x20;i--;){
                    Entity* particle = entityCreate(vec3Mix(entity->position,g_surface.position,FIXED_ONE / 16),ENTITY_PARTICLE);
                    particle->health = tRnd() % 0x80 + 0x80;
                    particle->size = FIXED_ONE / 16;
                    particle->velocity = vec3Shr(vec3Rnd(),2);
                    if(voxel_s->texture){
                        particle->texture = voxel_s->texture;
                        particle->texture_size = FIXED_ONE * 16 / voxel_s->texture->size;
                        particle->texture_offset = (Vec2){tRnd() % FIXED_ONE,tRnd() % FIXED_ONE};
                    }
                    particle->color = COLOR_WHITE;
                }	
                audioPlay(entity->position,AUDIO_PUNCH_HIT);
            } break;
        }
        if(collision.time == 0){
            Vec3 epsilon = vec3Shr(collision.normal,8);
            velocity_delta = vec3Add(velocity_delta,epsilon);
            entity->velocity = vec3Add(entity->velocity,epsilon);
        }
        else{
            if(entity->bounce){
                velocity_delta = vec3Reflect(velocity_delta,collision.normal);
                entity->velocity = vec3Reflect(entity->velocity,collision.normal);
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
    return in_air;
}

#define JUMP_HEIGHT (FIXED_ONE)
#define WALK_SPEED (FIXED_ONE / 2)
#define DIAGONAL 0x0000B53C
#define BHOP_BONUS (FIXED_ONE / 128)

void movementNormal(void){
    if(g_voxel_interact)
        return;
    int max_height_x = 0;
    int max_height_y = 0;
    bool down_z = g_player.entity.velocity.z < 0;
    CollisionFlags collision_flags = {0};
    Vec3 velocity = vec3MulS(g_player.entity.velocity,g_time.delta);

    bool in_air = true;
    int time = FIXED_ONE;

    in_air = movementUpdate(&g_player.entity);

    if(!in_air){
        static bool pressed;
        if(keyDown(KEY_SPACE)){
            audioPlay(vec3Add(g_player.entity.position,(Vec3){0,0,-FIXED_ONE}),AUDIO_JUMP);
            g_player.entity.velocity.z = 0;
            g_player.entity.velocity.z += JUMP_HEIGHT;
            down_z = false;
            pressed = false;
        }
    }
	if(collision_flags.ladder){
		g_player.entity.velocity.z = 0;
		if(keyDown(KEY_LSHIFT))
			g_player.entity.velocity.z -= FIXED_ONE / 16;
		else
			g_player.entity.velocity.z += FIXED_ONE / 16;
	}
	if(collision_flags.water){
		g_player.entity.velocity = vec3MulS(g_player.entity.velocity,PHYSICS_FRICTION_WATER);
		if(keyDown(KEY_LSHIFT))
			g_player.entity.velocity.z -= FIXED_ONE / 64;
		if(keyDown(KEY_SPACE))
			g_player.entity.velocity.z += FIXED_ONE / 64;
	}
	static int moved;
	if(!in_air){
		moved += vec3Dot(g_player.entity.velocity,g_player.entity.velocity);
		if(moved > FIXED_ONE / 8){
			audioPlay(vec3Add(g_player.entity.position,(Vec3){0,0,-FIXED_ONE}),AUDIO_FOOTSTEP);
			moved = 0;
		}
	}
	else{
		moved = 0;
	}
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
    wish_dir = vec2Normalize(wish_dir);
    
    wish_dir = vec2MulS(wish_dir,WALK_SPEED);

    if(in_air)
        wish_dir = vec2MulS(wish_dir,FIXED_ONE / 8);

    g_player.entity.velocity.x += fixedMulR(wish_dir.x,g_time.delta);
    g_player.entity.velocity.y += fixedMulR(wish_dir.y,g_time.delta);
    
    int friction = in_air ? PHYSICS_FRICTION_AIR : PHYSICS_FRICTION_GROUND;
    friction = fixedMulR(friction,g_time.delta);
    g_player.entity.velocity = vec3Sub(g_player.entity.velocity,vec3MulS(g_player.entity.velocity,friction));

	if(g_player.entity.velocity.x < 0)
		g_player.entity.velocity.x += 1;
	if(g_player.entity.velocity.y < 0)
		g_player.entity.velocity.y += 1;
	g_player.entity.velocity.z -= fixedMulR(PHYSICS_GRAVITY,g_time.delta);
}
