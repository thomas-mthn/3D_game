#include "dda.h"

Ray3 initRay3(Vec3 position,Vec3 direction){
	Ray3 ray;
	ray.pos = position;
	ray.dir = direction;
	ray.square_side = 0;
	ray.delta = (Vec3){	
		fixedDivR(FIXED_ONE,tAbs(direction.x)),
		fixedDivR(FIXED_ONE,tAbs(direction.y)),
		fixedDivR(FIXED_ONE,tAbs(direction.z))
	};
	ray.step.x = direction.x < 0 ? -1 : 1;
	ray.step.y = direction.y < 0 ? -1 : 1;
	ray.step.z = direction.z < 0 ? -1 : 1;

	Vec3 fract_pos = {fixedFract(position.x),fixedFract(position.y),fixedFract(position.z)};

	ray.side.x = fixedMulR((direction.x < 0 ? fract_pos.x : FIXED_ONE - fract_pos.x),ray.delta.x);
	ray.side.y = fixedMulR((direction.y < 0 ? fract_pos.y : FIXED_ONE - fract_pos.y),ray.delta.y);
	ray.side.z = fixedMulR((direction.z < 0 ? fract_pos.z : FIXED_ONE - fract_pos.z),ray.delta.z);

	ray.square_pos.x = position.x >> FIXED_PRECISION;
	ray.square_pos.y = position.y >> FIXED_PRECISION;
	ray.square_pos.z = position.z >> FIXED_PRECISION;

	return ray;
}

Ray2 initRay2(Vec2 position,Vec2 direction){
	Ray2 ray;
	ray.pos = position;
	ray.dir = direction;
	ray.square_side = 0;

	ray.delta = (Vec2){	
		fixedDivR(FIXED_ONE,tAbs(direction.x)),
		fixedDivR(FIXED_ONE,tAbs(direction.y)),
	};
	ray.step.x = direction.x < 0 ? -1 : 1;
	ray.step.y = direction.y < 0 ? -1 : 1;

	Vec2 fract_pos = {fixedFract(position.x),fixedFract(position.y)};

	ray.side.x = fixedMulR((direction.x < 0 ? fract_pos.x : FIXED_ONE - fract_pos.x),ray.delta.x);
	ray.side.y = fixedMulR((direction.y < 0 ? fract_pos.y : FIXED_ONE - fract_pos.y),ray.delta.y);

	ray.square_pos.x = position.x >> FIXED_PRECISION;
	ray.square_pos.y = position.y >> FIXED_PRECISION;

	return ray;
}
