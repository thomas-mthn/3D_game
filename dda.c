#include "dda.h"

Ray3 initRay3(Vec3 position,Vec3 direction){
    if(tAbs(direction.x) <= REAL_EPSILON)
        direction.x = direction.x < 0 ? -REAL_EPSILON : REAL_EPSILON;
    if(tAbs(direction.y) <= REAL_EPSILON)
        direction.y = direction.y < 0 ? -REAL_EPSILON : REAL_EPSILON;
    if(tAbs(direction.z) <= REAL_EPSILON)
        direction.z = direction.z < 0 ? -REAL_EPSILON : REAL_EPSILON;

    int scale = IS_FLOAT(real) ? 0x10000 : 1;
    
	Ray3 ray;
	ray.pos = (Vec3i){position.x * scale,position.y * scale,position.z * scale};
	ray.dir = (Vec3i){direction.x * scale,direction.y * scale,direction.z * scale};
    
    ray.delta = (Vec3i){	
        tReciprocal(tAbs(direction.x)) * scale,
        tReciprocal(tAbs(direction.y)) * scale,
        tReciprocal(tAbs(direction.z)) * scale
    };
	ray.step.x = direction.x < 0 ? -1 : 1;
	ray.step.y = direction.y < 0 ? -1 : 1;
	ray.step.z = direction.z < 0 ? -1 : 1;

	Vec3i fract_pos = {ray.pos.x & 0xFFFF,ray.pos.y & 0xFFFF,ray.pos.z & 0xFFFF};

	ray.side.x = fixedMulR((direction.x < 0 ? fract_pos.x : 0x10000 - fract_pos.x),ray.delta.x);
	ray.side.y = fixedMulR((direction.y < 0 ? fract_pos.y : 0x10000 - fract_pos.y),ray.delta.y);
	ray.side.z = fixedMulR((direction.z < 0 ? fract_pos.z : 0x10000 - fract_pos.z),ray.delta.z);

	ray.square_pos.x = ray.pos.x >> 16;
	ray.square_pos.y = ray.pos.y >> 16;
	ray.square_pos.z = ray.pos.z >> 16;

	return ray;
}

Ray2 initRay2(Vec2 position,Vec2 direction){
    if(tAbs(direction.x) <= REAL_EPSILON)
        direction.x = direction.x < 0 ? -REAL_EPSILON : REAL_EPSILON;
    if(tAbs(direction.y) <= REAL_EPSILON)
        direction.y = direction.y < 0 ? -REAL_EPSILON : REAL_EPSILON;
    
	Ray2 ray;
	ray.pos = position;
	ray.dir = direction;

	ray.delta = (Vec2){
        tReciprocal(tAbs(direction.x)),
        tReciprocal(tAbs(direction.y)),
	};
	ray.step.x = direction.x < 0 ? -1 : 1;
	ray.step.y = direction.y < 0 ? -1 : 1;

	Vec2 fract_pos = {tFract(position.x),tFract(position.y)};

	ray.side.x = realMulR((direction.x < 0 ? fract_pos.x : 0x10000 - fract_pos.x),ray.delta.x);
	ray.side.y = realMulR((direction.y < 0 ? fract_pos.y : 0x10000 - fract_pos.y),ray.delta.y);

	ray.square_pos.x = realToInt(position.x);
	ray.square_pos.y = realToInt(position.y);

	return ray;
}
