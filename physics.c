#include "physics.h"
#include "octree.h"
#include "main.h"
#include "audio.h"
#include <stdbool.h>

void movementFly(void){
    if(g_voxel_interact)
        return;
	int speed = keyDown(KEY_LCONTROL) ? 2 : 4;
	if(keyDown(KEY_W)){
		g_surface.position.x -= fixedMulR(tCos(g_surface.angle.x) >> speed,g_delta);
		g_surface.position.y -= fixedMulR(tSin(g_surface.angle.x) >> speed,g_delta);
	}
	if(keyDown(KEY_S)){
		g_surface.position.x += fixedMulR(tCos(g_surface.angle.x) >> speed,g_delta);
        g_surface.position.y += fixedMulR(tSin(g_surface.angle.x) >> speed,g_delta);
	}
	if(keyDown(KEY_A)){
		g_surface.position.x += fixedMulR(tCos(g_surface.angle.x + FIXED_ONE / 4) >> speed,g_delta);
		g_surface.position.y += fixedMulR(tSin(g_surface.angle.x + FIXED_ONE / 4) >> speed,g_delta);
	}
	if(keyDown(KEY_D)){
		g_surface.position.x -= fixedMulR(tCos(g_surface.angle.x + FIXED_ONE / 4) >> speed,g_delta);
		g_surface.position.y -= fixedMulR(tSin(g_surface.angle.x + FIXED_ONE / 4) >> speed,g_delta);
	}
	if(keyDown(KEY_LSHIFT))
		g_surface.position.z -= fixedMulR(0x10000 >> speed,g_delta);
	if(keyDown(KEY_SPACE))
		g_surface.position.z += fixedMulR(0x10000 >> speed,g_delta);
}

static bool boxTreeCollisionRecursive(Vec3 pos,Vec3 size,Voxel* voxel,int* max_height,CollisionFlags* flags){
	int block_size = depthToSize(voxel->depth);
	Vec3 block_pos = voxelWorldPos(voxel);

	Vec3 block_size_water = {
		block_size,
		block_size,
		block_size - FIXED_ONE / 4,
	};
    
	if(flags){
		if(voxel->type == VOXEL_LADDER)
 			flags->ladder = true;
		if(voxel->type == VOXEL_WATER && boxBoxIntersect(pos,size,block_pos,block_size_water))
		    flags->water = true;
	}
    
    switch(voxel->type){
        case VOXEL_PARENT:{
            bool collision = false;

            if(!boxCubeIntersect(pos,size,block_pos,block_size))
                return false;

            for(int i = 0;i < 8;i++)
                collision |= boxTreeCollisionRecursive(pos,size,voxel->child_s[i],max_height,flags);
            return collision;
        } break;
        case VOXEL_AIR:
            return false;
        case VOXEL_MOVABLE:{
            Vec3 block_size_m = {
                block_size,
                block_size,
                fixedMulR(block_size,voxel->opened ? voxel->animation : FIXED_ONE - voxel->animation),
            };
            if(!boxBoxIntersect(pos,size,block_pos,block_size_m))
                return false;
        } break;
        case VOXEL_PRESSURE_PLATE:{
            if(!boxCubeIntersect(pos,size,block_pos,block_size))
                return false;
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

            bool box_1 = boxBoxIntersect(pos,size,block_pos,(Vec3){block_size,door_size,block_size});
            bool box_2 = boxBoxIntersect(pos,size,(Vec3){block_pos.x,block_pos.y + block_size / 2 + door_size_inv,block_pos.z},(Vec3){block_size,door_size,block_size}); 

            if(!box_1 && !box_2)
                return false;
        } break;
        case VOXEL_WATER:{
            if(!boxBoxIntersect(pos,size,block_pos,block_size_water))
                return false;
            if(!voxel->animation){
                voxel->splash_position = (Vec2){pos.x,pos.y};
                voxel->splash_tick = g_tick;
                voxelTickListAdd(voxel);
                voxel->animation = FIXED_ONE;
            }
            else if(voxel->collision_tick != g_tick && voxel->collision_tick != g_tick - 1){
                voxel->splash_position = (Vec2){pos.x,pos.y};
                voxel->splash_tick = g_tick;
                voxel->animation = FIXED_ONE;
            }
            else{
                voxel->animation = tMax(voxel->animation,FIXED_ONE / 2);
            }
            voxel->collision_tick = g_tick;
        } return false;
        default:{
            if(!boxCubeIntersect(pos,size,block_pos,block_size))
                return false;
        } break;
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
	return true;
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

bool boxTreeCollision(Vec3 pos,Vec3 size,int* max_height,CollisionFlags* flags){
	pos = vec3Sub(pos,vec3Shr(size,1));
	return boxTreeCollisionRecursive(pos,size,&g_voxel,max_height,flags);
}

Vec3 playerHitboxGet(void){
	return vec3Add(g_surface.position,(Vec3){.z = -(FIXED_ONE / 2 + FIXED_ONE / 4)});
}

#define JUMP_HEIGHT (FIXED_ONE / 4)
#define WALK_SPEED (FIXED_ONE / 320)
#define DIAGONAL 0x0000B53C
#define BHOP_BONUS (FIXED_ONE / 128)

static int powFixed(int base,int dt){
    int k = fixedMulR(FIXED_ONE - base,dt);
    return FIXED_ONE - k;
}
#include "console.h"
void movementNormal(void){
    if(g_voxel_interact)
        return;
	int max_height_x = 0;
	int max_height_y = 0;
	bool hit[3];
	bool down_z = g_velocity.z < 0;
	CollisionFlags collision_flags = {0};
	Vec3 hitbox = playerHitboxGet();

    Vec3 velocity = vec3MulS(g_velocity,g_delta);
    
	hit[VEC3_X] = boxTreeCollision(vec3Add(hitbox,(Vec3){velocity.x,0,0}),PLAYER_SIZE,&max_height_x,&collision_flags);
	hit[VEC3_Y] = boxTreeCollision(vec3Add(hitbox,(Vec3){0,velocity.y,0}),PLAYER_SIZE,&max_height_y,&collision_flags);
	hit[VEC3_Z] = boxTreeCollision(vec3Add(hitbox,(Vec3){0,0,velocity.z}),PLAYER_SIZE,0,0);
	g_surface.position = vec3Add(g_surface.position,velocity);

	if(hit[VEC3_X]){
		int height_difference = max_height_x - g_surface.position.z + PLAYER_SIZE.z / 2 + FIXED_ONE / 2 + FIXED_ONE / 4;
	
		if(height_difference < FIXED_ONE - FIXED_ONE / 3){
			g_surface.position.z += height_difference;
		}
		else{
			g_surface.position.x -= velocity.x;
			g_velocity.x = 0;
		}
	}
	if(hit[VEC3_Y]){
		int height_difference = max_height_y - g_surface.position.z + PLAYER_SIZE.z / 2 + FIXED_ONE / 2 + FIXED_ONE / 4;
	
		if(height_difference < FIXED_ONE - FIXED_ONE / 3){
			g_surface.position.z += height_difference;
		}
		else{
			g_surface.position.y -= velocity.y;
			g_velocity.y = 0;
		}
	}
	if(hit[VEC3_Z]){
		if(g_velocity.z < -FIXED_ONE / 4)
			audioPlay(vec3Add(g_surface.position,(Vec3){0,0,-FIXED_ONE}),AUDIO_LAND);

		//fall damage
		if(g_velocity.z < -FIXED_ONE / 3)
			g_health -= (-g_velocity.z - FIXED_ONE / 3) * 3;

		g_surface.position.z -= velocity.z;
		g_velocity.z = 0;
		if(down_z){
			static bool pressed;
			if(keyDown(KEY_SPACE)){
				audioPlay(vec3Add(g_surface.position,(Vec3){0,0,-FIXED_ONE}),AUDIO_JUMP);
				g_velocity.z = 0;
				g_velocity.z += JUMP_HEIGHT;
                down_z = false;
				pressed = false;
			}
			else
				pressed = true;
		}
	}
	if(collision_flags.ladder){
		g_velocity.z = 0;
		if(keyDown(KEY_LSHIFT))
			g_velocity.z -= FIXED_ONE / 16;
		else
			g_velocity.z += FIXED_ONE / 16;
	}
	if(collision_flags.water){
		g_velocity = vec3MulS(g_velocity,PHYSICS_FRICTION_WATER);
		if(keyDown(KEY_LSHIFT))
			g_velocity.z -= FIXED_ONE / 64;
		if(keyDown(KEY_SPACE))
			g_velocity.z += FIXED_ONE / 64;
	}
	bool in_air = (!hit[VEC3_Z] || hit[VEC3_Z] && !down_z) && !collision_flags.water;
	static int moved;
	if(!in_air){
		moved += vec3Dot(g_velocity,g_velocity);
		if(moved > FIXED_ONE / 8){
			audioPlay(vec3Add(g_surface.position,(Vec3){0,0,-FIXED_ONE}),AUDIO_FOOTSTEP);
			moved = 0;
		}
	}
	else{
		moved = 0;
	}
	if(keyDown(KEY_W)){
		int mod = keyDown(KEY_D) || keyDown(KEY_A) ? fixedMulR(WALK_SPEED,DIAGONAL) : WALK_SPEED;
		if(in_air)
			fixedMul(&mod,FIXED_ONE / 16);
		g_velocity.x -= fixedMulR(fixedMulR(tCos(g_surface.angle.x),mod * 2),g_delta);
		g_velocity.y -= fixedMulR(fixedMulR(tSin(g_surface.angle.x),mod * 2),g_delta);
	}
	if(keyDown(KEY_S)){
		int mod = keyDown(KEY_D) || keyDown(KEY_A) ? fixedMulR(WALK_SPEED,DIAGONAL) : WALK_SPEED;
		if(in_air)
			fixedMul(&mod,FIXED_ONE / 16);
		g_velocity.x += fixedMulR(fixedMulR(tCos(g_surface.angle.x),mod * 2),g_delta);
		g_velocity.y += fixedMulR(fixedMulR(tSin(g_surface.angle.x),mod * 2),g_delta);
	}
	if(keyDown(KEY_D)){
		int mod = keyDown(KEY_S) || keyDown(KEY_W) ? fixedMulR(WALK_SPEED,DIAGONAL) : WALK_SPEED;
		if(in_air)
			fixedMul(&mod,FIXED_ONE / 16);
		g_velocity.x -= fixedMulR(fixedMulR(tCos(g_surface.angle.x + FIXED_ONE / 4),mod * 2),g_delta);
		g_velocity.y -= fixedMulR(fixedMulR(tSin(g_surface.angle.x + FIXED_ONE / 4),mod * 2),g_delta);
	}
	if(keyDown(KEY_A)){
		int mod = keyDown(KEY_S) || keyDown(KEY_W) ? fixedMulR(WALK_SPEED,DIAGONAL) : WALK_SPEED;
		if(in_air)
			fixedMul(&mod,FIXED_ONE / 16);
		g_velocity.x += fixedMulR(fixedMulR(tCos(g_surface.angle.x + FIXED_ONE / 4),mod * 2),g_delta);
		g_velocity.y += fixedMulR(fixedMulR(tSin(g_surface.angle.x + FIXED_ONE / 4),mod * 2),g_delta);
	}
    
    int friction = !hit[VEC3_Z] || hit[VEC3_Z] && !down_z ? PHYSICS_FRICTION_AIR : PHYSICS_FRICTION_GROUND;
    friction = fixedMulR(friction,g_delta);
    g_velocity = vec3Sub(g_velocity,vec3MulS(g_velocity,friction));
    
	if(g_velocity.x < 0)
		g_velocity.x += 1;
	if(g_velocity.y < 0)
		g_velocity.y += 1;
	g_velocity.z -= fixedMulR(PHYSICS_GRAVITY,g_delta);

	static int delta_angle;
	static int prev_angle;

	delta_angle += (g_surface.angle.x - prev_angle) * 0x10;
	
	bool key_a = keyDown(KEY_A);
	bool key_d = keyDown(KEY_D);

	if(!keyDown(KEY_W)){
		if(delta_angle < 0 && key_a && !key_d && in_air){
			int boost = tClamp(FIXED_ONE / 4 - tAbs(-delta_angle - FIXED_ONE),0,FIXED_ONE);

			Vec2 angle = g_surface.angle;
			angle.x -= FIXED_ONE / 16;

			if(!keyDown(KEY_W)){
				g_velocity.x += fixedMulR(fixedMulR(fixedMulR(getLookDirection(angle).x,BHOP_BONUS),boost << 2),g_delta);
				g_velocity.y += fixedMulR(fixedMulR(fixedMulR(getLookDirection(angle).y,BHOP_BONUS),boost << 2),g_delta);
			}
		}
		if(delta_angle > 0 && key_d && !key_a && in_air){
			int boost = tClamp(FIXED_ONE / 4 - tAbs(delta_angle - FIXED_ONE),0,FIXED_ONE);

			Vec2 angle = g_surface.angle;
			angle.x += FIXED_ONE / 16;

			if(!keyDown(KEY_W)){
				g_velocity.x += fixedMulR(fixedMulR(fixedMulR(getLookDirection(angle).x,BHOP_BONUS),boost << 2),g_delta);
				g_velocity.y += fixedMulR(fixedMulR(fixedMulR(getLookDirection(angle).y,BHOP_BONUS),boost << 2),g_delta);
			}
		}
	}

	prev_angle = g_surface.angle.x;
	fixedMul(&delta_angle,FIXED_ONE - (FIXED_ONE >> 4));
}
