#ifndef DDA_H
#define DDA_H

#include "langext.h"
#include "vec3.h"
#include "vec2.h"

structure(Ray2){
	Vec2 pos;
	Vec2 dir;
	Vec2 delta;
	Vec2 side;

	Vec2i step;
	Vec2i square_pos;

	int square_side;
};

structure(Ray3){
	Vec3i pos;
	Vec3i dir;
	Vec3i delta;
	Vec3i side;

	Vec3i step;
	Vec3i square_pos;

	int square_side;
};

Ray3 initRay3(Vec3 position,Vec3 direction);
Ray2 initRay2(Vec2 position,Vec2 direction);

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

static void iterateRay2(Ray2* ray){
	if(ray->side.x < ray->side.y){
        ray->square_pos.x += ray->step.x;
        ray->side.x += ray->delta.x;
        ray->square_side = VEC3_X;
        return;
	}

	ray->square_pos.y += ray->step.y;
	ray->side.y += ray->delta.y;
	ray->square_side = VEC3_Y;
}

#endif
