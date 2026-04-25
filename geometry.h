#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "vec3.h"
#include "vec2.h"

structure(Voxel);

structure(Plane){
	Vec3 normal;
	int distance;
};

//collision
bool pointBoxIntersection(Vec3 point,Vec3 box_position,Vec3 box_size);
bool boxBoxIntersect(Vec3 box_pos,Vec3 box_size,Vec3 cube_pos,Vec3 cube_size);
bool boxCubeIntersect(Vec3 box_pos,Vec3 box_size,Vec3 cube_pos,int cube_size);

//SDF
int sdSegment(Vec2 p,Vec2 a,Vec2 b);
int sdSquare(Vec3 p,Vec3 square_pos,int square_size,unsigned side);
int sdPlane(Vec3 p,Vec3 n,int h);
int sdVoxel(Vec3 point,Vec3 voxel_position,int voxel_size);

//ray tracing
int rayVoxelIntersection(Voxel* voxel,Vec3 ro,Vec3 rd,Vec3* normal);
int rayBoxIntersection(Vec3 box_position,Vec3 box_size,Vec3 ro,Vec3 rd);
int rayPlaneIntersection(Vec3 pos,Vec3 dir,Plane plane);
int rayEllipsoidIntersection(Vec3 ray_position,Vec3 ray_direction,Vec3 ellipsoid_position,Vec3 ra);
int raySphereIntersection(Vec3 ray_position,Vec3 ray_direction,Vec3 sphere_position,int radius);

#endif
