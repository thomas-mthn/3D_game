#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "vec3.h"
#include "vec2.h"

structure(Voxel);

structure(Plane){
	Vec3 normal;
	real distance;
};

typedef enum{
    PLANE_FRONT,
    PLANE_BACK,
    PLANE_BETWEEN,
} PlaneCollision;

//collision
bool intersectBoxPoint(Vec3 point,Vec3 box_position,Vec3 box_size);
bool intersectCubePoint(Vec3 point,Vec3 cube_position,real cube_size);
bool intersectBoxBox(Vec3 box_pos,Vec3 box_size,Vec3 cube_pos,Vec3 cube_size);
bool intersectBoxCube(Vec3 box_pos,Vec3 box_size,Vec3 cube_pos,real cube_size);
bool intersectBoxSphere(Vec3 box_pos,Vec3 box_size,Vec3 sphere_pos,real sphere_radius);
bool intersectTorusPoint(Vec3 p,real R,real r);
bool intersectCylinderPoint(Vec3 p,Vec3 position,Vec3 direction,real radius);
bool intersectTorusBox(Vec3 box_position,Vec3 box_size,Vec3 torus_position,Vec2 torus_radius);
bool intersectCylinderBox(
    Vec3 box_position,
    Vec3 box_size,
    Vec3 cylinder_position,
    Vec3 cylinder_direction,
    real cylinder_radius
);
PlaneCollision intersectBoxPlane(Vec3 box_position,Vec3 box_size,Plane plane);
PlaneCollision intersectCubePlane(Vec3 cube_position,real cube_size,Plane plane);

//SDF
real sdSegment(Vec2 p,Vec2 a,Vec2 b);
real sdSquare(Vec3 p,Vec3 square_pos,real square_size,int side);
real sdPlane(Vec3 p,Vec3 n,real h);
real sdVoxel(Vec3 point,Vec3 voxel_position,real voxel_size);
real sdVoxelSquare(Vec3 point,Vec3 voxel_position,real voxel_size);
real sdSquareSquare(Vec3 p,Vec3 square_pos,real square_size,int side);

//ray tracing
real rayBoxIntersection(Vec3 box_position,Vec3 box_size,Vec3 ro,Vec3 rd);
real rayCubeIntersection(Vec3 cube_position,real cube_size,Vec3 ro,Vec3 rd);
real rayCubeIntersectionInside(Vec3 cube_position,real cube_size,Vec3 ro,Vec3 rd);
real rayCubeIntersectionPierce(Vec3 cube_position,real cube_size,Vec3 ro,Vec3 rd);
real rayPlaneIntersection(Vec3 pos,Vec3 dir,Plane plane);
real rayEllipsoidIntersection(Vec3 ray_position,Vec3 ray_direction,Vec3 ellipsoid_position,Vec3 ra);
real raySphereIntersection(Vec3 ray_position,Vec3 ray_direction,Vec3 sphere_position,real radius);
real rayCylinderIntersection(Vec3 ray_position,Vec3 ray_direction,Vec3 cylinder_position,Vec3 axis,real radius);

bool rayTorusIntersection(Vec3 ro,Vec3 rd,Vec2 tor);

#endif
