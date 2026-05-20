#include "geometry.h"
#include "octree.h"

bool intersectCubePoint(Vec3 point,Vec3 cube_position,real cube_size){
    bool x = tAbs(cube_position.x - point.x) <= (cube_size);
    bool y = tAbs(cube_position.y - point.y) <= (cube_size);
    bool z = tAbs(cube_position.z - point.z) <= (cube_size);
	return x && y && z;
}

bool intersectBoxPoint(Vec3 point,Vec3 box_position,Vec3 box_size){
    bool x = tAbs(box_position.x - point.x) <= (box_size.x);
    bool y = tAbs(box_position.y - point.y) <= (box_size.y);
    bool z = tAbs(box_position.z - point.z) <= (box_size.z);
	return x && y && z;
}

bool intersectBoxBox(Vec3 box1_pos,Vec3 box1_size,Vec3 box2_pos,Vec3 box2_size){
    bool x = tAbs(box1_pos.x - box2_pos.x) <= (box1_size.x + box2_size.x);
    bool y = tAbs(box1_pos.y - box2_pos.y) <= (box1_size.y + box2_size.y);
    bool z = tAbs(box1_pos.z - box2_pos.z) <= (box1_size.z + box2_size.z);
    return x && y && z;
}

bool intersectBoxCube(Vec3 box_pos,Vec3 box_size,Vec3 cube_pos,real cube_size){
    bool x = tAbs(box_pos.x - cube_pos.x) <= (box_size.x + cube_size);
    bool y = tAbs(box_pos.y - cube_pos.y) <= (box_size.y + cube_size);
    bool z = tAbs(box_pos.z - cube_pos.z) <= (box_size.z + cube_size);
    return x && y && z;
}

real sdSegment(Vec2 p,Vec2 a,Vec2 b){
    Vec2 pa = vec2Sub(p,a);
    Vec2 ba = vec2Sub(b,a);
    real h = tClamp(realDivR(vec2Dot(pa,ba),vec2Dot(ba,ba)),0.0,FIXED_ONE);
    return vec2Length(vec2Sub(pa,vec2MulS(ba,h)));
}

real sdSquare(Vec3 p,Vec3 square_pos,real square_size,int side){ 
	Vec2i axis = g_axis_table[side];

    real dx = tMax(tAbs(p.a[axis.x] - (square_pos.a[axis.x] + (realShr(square_size,1)))) - (realShr(square_size,1)),0);
    real dy = tMax(tAbs(p.a[axis.y] - (square_pos.a[axis.y] + (realShr(square_size,1)))) - (realShr(square_size,1)),0);

    real outsideDist = tSqrt(realMulR(dx,dx) + realMulR(dy,dy) + realMulR((p.a[side >> 1] - square_pos.a[side >> 1]),(p.a[side >> 1] - square_pos.a[side >> 1])));

    return outsideDist;
}

real sdSquareSquare(Vec3 p,Vec3 square_pos,real square_size,int side){ 
	Vec2i axis = g_axis_table[side];

    real dx = tMax(tAbs(p.a[axis.x] - (square_pos.a[axis.x] + (realShr(square_size,1)))) - (realShr(square_size,1)),0);
    real dy = tMax(tAbs(p.a[axis.y] - (square_pos.a[axis.y] + (realShr(square_size,1)))) - (realShr(square_size,1)),0);

    real outsideDist = realMulR(dx,dx) + realMulR(dy,dy) + realMulR((p.a[side >> 1] - square_pos.a[side >> 1]),(p.a[side >> 1] - square_pos.a[side >> 1]));

    return outsideDist;
}

real sdPlane(Vec3 p,Vec3 n,real h){
	return vec3Dot(p,n) + h;
}

real sdVoxel(Vec3 point,Vec3 voxel_position,real voxel_size){
	Vec3 p = vec3Sub(voxel_position,point);
	Vec3 q = vec3SubS((Vec3){tAbs(p.x),tAbs(p.y),tAbs(p.z)},voxel_size);
	return vec3Length((Vec3){tMax(q.x,0),tMax(q.y,0),tMax(q.z,0)}) + tMin(tMax(q.x,tMax(q.y,q.z)),0);
}

real sdVoxelSquare(Vec3 point,Vec3 voxel_position,real voxel_size){
	Vec3 p = vec3Sub(voxel_position,point);
	Vec3 q = vec3SubS((Vec3){tAbs(p.x),tAbs(p.y),tAbs(p.z)},voxel_size);
	return vec3LengthSquare((Vec3){tMax(q.x,0),tMax(q.y,0),tMax(q.z,0)}) + tMin(tMax(q.x,tMax(q.y,q.z)),0);
}

real rayBoxIntersection(Vec3 box_position,Vec3 box_size,Vec3 ro,Vec3 rd){
	ro = vec3Sub(ro,box_position);
    Vec3 m = vec3Reciprocal(rd);
    Vec3 n = vec3Mul(m,ro);
	Vec3 k = vec3Mul((Vec3){tAbs(m.x),tAbs(m.y),tAbs(m.z)},box_size);
    Vec3 t1 = vec3Sub((Vec3){-n.x,-n.y,-n.z},k);
    Vec3 t2 = vec3Add((Vec3){-n.x,-n.y,-n.z},k);
    real tN = tMax(tMax(t1.x,t1.y),t1.z);
    real tF = tMin(tMin(t2.x,t2.y),t2.z);
	if(tN > tF || tF < 0)
		return 0;
    return tN;
}

real rayCubeIntersection(Vec3 cube_position,real cube_size,Vec3 ro,Vec3 rd){
	ro = vec3Sub(ro,cube_position);
    
    Vec3 m = vec3Reciprocal(rd);
    Vec3 n = vec3Mul(m,ro);
	Vec3 k = vec3MulS((Vec3){tAbs(m.x),tAbs(m.y),tAbs(m.z)},cube_size);
    Vec3 t1 = vec3Sub((Vec3){-n.x,-n.y,-n.z},k);
    Vec3 t2 = vec3Add((Vec3){-n.x,-n.y,-n.z},k);
    real tN = tMax(tMax(t1.x,t1.y),t1.z);
    real tF = tMin(tMin(t2.x,t2.y),t2.z);
	if(tN > tF || tF < 0)
		return 0;
    return tN;
}

real rayCubeIntersectionInside(Vec3 cube_position,real cube_size,Vec3 ro,Vec3 rd){
	ro = vec3Sub(ro,cube_position);
    
    Vec3 m = vec3Reciprocal(rd);
    Vec3 n = vec3Mul(m,ro);
	Vec3 k = vec3MulS((Vec3){tAbs(m.x),tAbs(m.y),tAbs(m.z)},cube_size);
    Vec3 t1 = vec3Sub((Vec3){-n.x,-n.y,-n.z},k);
    Vec3 t2 = vec3Add((Vec3){-n.x,-n.y,-n.z},k);
    real tN = tMax(tMax(t1.x,t1.y),t1.z);
    real tF = tMin(tMin(t2.x,t2.y),t2.z);
	if(tN > tF || tF < 0)
		return 0;
    return tF;
}

real rayPlaneIntersection(Vec3 pos,Vec3 dir,Plane plane){
	return -realDivR((vec3Dot(pos,plane.normal) + plane.distance),vec3Dot(dir,plane.normal));
}

real rayEllipsoidIntersection(Vec3 ray_position,Vec3 ray_direction,Vec3 sphere_position,Vec3 radius){
    Vec3 oc = vec3Sub(sphere_position,ray_position);

    Vec3 ocn = vec3Div(oc,radius);
    Vec3 rdn = vec3Div(ray_direction,radius);

    real a = vec3Dot(rdn,rdn);
    real b = 2 * vec3Dot(ocn,rdn);
    real c = vec3Dot(ocn,ocn) - FIXED_ONE;

    real discriminant = realMulR(b,b) - 4 * realMulR(a,c);
    if(discriminant < 0)
        return -1;
    else
        return realDivR((-b - tSqrt(discriminant)),(2 * a));
}

real raySphereIntersection(Vec3 ray_position,Vec3 ray_direction,Vec3 sphere_position,real radius){
    Vec3 oc = vec3Sub(sphere_position,ray_position);

    real a = vec3Dot(ray_direction,ray_direction);
    real b = 2 * vec3Dot(oc,ray_direction);
    real c = vec3Dot(oc,oc) - realMulR(radius,radius);
    
    real discriminant = realMulR(b,b) - 4 * realMulR(a,c);
    if(discriminant < 0)
        return -1;
    else
        return realDivR((-b - tSqrt(discriminant)),(2 * a));
}

PlaneCollision intersectBoxPlane(Vec3 box_position,Vec3 box_size,Plane plane){
    Vec3 center = {
        box_position.x,
        box_position.y,
        box_position.z
    };

    real s = vec3Dot(plane.normal,center) + plane.distance;

    real r =
        realMulR(box_size.x,tAbs(plane.normal.x)) +
        realMulR(box_size.y,tAbs(plane.normal.y)) +
        realMulR(box_size.z,tAbs(plane.normal.z));

    if(tAbs(s) <= r)
        return PLANE_BETWEEN;

    return s > r ? PLANE_FRONT : PLANE_BACK;
}

PlaneCollision intersectCubePlane(Vec3 cube_position,real cube_size,Plane plane){
    Vec3 center = {
        cube_position.x,
        cube_position.y,
        cube_position.z
    };

    real s = vec3Dot(plane.normal,center) + plane.distance;

    real r =
        realMulR(cube_size,tAbs(plane.normal.x)) +
        realMulR(cube_size,tAbs(plane.normal.y)) +
        realMulR(cube_size,tAbs(plane.normal.z));

    if(tAbs(s) <= r)
        return PLANE_BETWEEN;

    return s > r ? PLANE_FRONT : PLANE_BACK;
}
