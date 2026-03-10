#ifndef DDA_H
#define DDA_H

#include "vec3.h"

structure(Ray3){
	Vec3 pos;
	Vec3 dir;
	Vec3 delta;
	Vec3 side;

	Vec3 step;
	Vec3 square_pos;

	int square_side;
};

Ray3 initRay3(Vec3 position,Vec3 direction);

static void iterateRay3(Ray3* ray){
	if(ray->side.x < ray->side.y){
		if(ray->side.x < ray->side.z){
			ray->square_pos.x += ray->step.x;
			ray->side.x += ray->delta.x;
			ray->square_side = VEC3_X;
			return;
		}
		ray->square_pos.z += ray->step.z;
		ray->side.z += ray->delta.z;
		ray->square_side = VEC3_Z;
		return;
	}
	if(ray->side.y < ray->side.z){
		ray->square_pos.y += ray->step.y;
		ray->side.y += ray->delta.y;
		ray->square_side = VEC3_Y;
		return;
	}
	ray->square_pos.z += ray->step.z;
	ray->side.z += ray->delta.z;
	ray->square_side = VEC3_Z;
}

static void recalculateRay3(Ray3* ray){
	ray->side.x = fixedFract(ray->pos.x);
	ray->side.y = fixedFract(ray->pos.y);
	ray->side.z = fixedFract(ray->pos.z);

	ray->side.x = fixedMulR((ray->dir.x < 0 ? ray->side.x : FIXED_ONE - ray->side.x),ray->delta.x);
	ray->side.y = fixedMulR((ray->dir.y < 0 ? ray->side.y : FIXED_ONE - ray->side.y),ray->delta.y);
	ray->side.z = fixedMulR((ray->dir.z < 0 ? ray->side.z : FIXED_ONE - ray->side.z),ray->delta.z);

	ray->square_pos = (Vec3){ray->pos.x >> FIXED_PRECISION,ray->pos.y >> FIXED_PRECISION,ray->pos.z >> FIXED_PRECISION};
}

#endif