#include "geometry.h"
#include "octree.h"

bool intersectCubePoint(Vec3 point,Vec3 cube_position,int cube_size){
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

bool intersectBoxCube(Vec3 box_pos,Vec3 box_size,Vec3 cube_pos,int cube_size){
    bool x = tAbs(box_pos.x - cube_pos.x) <= (box_size.x + cube_size);
    bool y = tAbs(box_pos.y - cube_pos.y) <= (box_size.y + cube_size);
    bool z = tAbs(box_pos.z - cube_pos.z) <= (box_size.z + cube_size);
    return x && y && z;
}

int sdSegment(Vec2 p,Vec2 a,Vec2 b){
    Vec2 pa = vec2Sub(p,a);
    Vec2 ba = vec2Sub(b,a);
    int h = tClamp(fixedDivR(vec2Dot(pa,ba),vec2Dot(ba,ba)),0.0,FIXED_ONE);
    return vec2Length(vec2Sub(pa,vec2MulS(ba,h)));
}

int sdSquare(Vec3 p,Vec3 square_pos,int square_size,unsigned side){ 
	Vec2 axis = g_axis_table[side];

    int dx = tMax(tAbs(((int*)&p)[axis.x] - (((int*)&square_pos)[axis.x] + (square_size >> 1))) - (square_size >> 1),0);
    int dy = tMax(tAbs(((int*)&p)[axis.y] - (((int*)&square_pos)[axis.y] + (square_size >> 1))) - (square_size >> 1),0);

    int outsideDist = tSqrt(fixedMulR(dx,dx) + fixedMulR(dy,dy) + fixedMulR((((int*)&p)[side >> 1] - ((int*)&square_pos)[side >> 1]),(((int*)&p)[side >> 1] - ((int*)&square_pos)[side >> 1])));

    return outsideDist;
}

int sdPlane(Vec3 p,Vec3 n,int h){
	return vec3Dot(p,n) + h;
}

int sdVoxel(Vec3 point,Vec3 voxel_position,int voxel_size){
	Vec3 p = vec3Sub(vec3AddS(voxel_position,voxel_size / 2),point);
	Vec3 q = vec3SubS((Vec3){tAbs(p.x),tAbs(p.y),tAbs(p.z)},voxel_size);
	return vec3Length((Vec3){tMax(q.x,0),tMax(q.y,0),tMax(q.z,0)}) + tMin(tMax(q.x,tMax(q.y,q.z)),0);
}

int rayBoxIntersection(Vec3 box_position,Vec3 box_size,Vec3 ro,Vec3 rd){
	ro = vec3Sub(ro,box_position);
    Vec3 m = vec3Div(vec3Single(FIXED_ONE),rd);
    Vec3 n = vec3Mul(m,ro);
	Vec3 k = vec3Mul((Vec3){tAbs(m.x),tAbs(m.y),tAbs(m.z)},box_size);
    Vec3 t1 = vec3Sub((Vec3){-n.x,-n.y,-n.z},k);
    Vec3 t2 = vec3Add((Vec3){-n.x,-n.y,-n.z},k);
    int tN = tMax(tMax(t1.x,t1.y),t1.z);
    int tF = tMin(tMin(t2.x,t2.y),t2.z);
	if(tN > tF || tF < 0)
		return 0;
    return tN;
}

int rayVoxelIntersection(Voxel* voxel,Vec3 ro,Vec3 rd,Vec3* normal){
	Vec3 voxel_pos = (Vec3){
		voxel->position_x * FIXED_ONE * 2 >> voxel->depth,
		voxel->position_y * FIXED_ONE * 2 >> voxel->depth,
		voxel->position_z * FIXED_ONE * 2 >> voxel->depth,
	};
	ro = vec3Sub(ro,vec3AddS(voxel_pos,FIXED_ONE >> voxel->depth));
    Vec3 m = vec3Div(vec3Single(FIXED_ONE),rd);
    Vec3 n = vec3Mul(m,ro);
	Vec3 k = vec3MulS((Vec3){tAbs(m.x),tAbs(m.y),tAbs(m.z)},FIXED_ONE >> voxel->depth);
    Vec3 t1 = vec3Sub((Vec3){-n.x,-n.y,-n.z},k);
    Vec3 t2 = vec3Add((Vec3){-n.x,-n.y,-n.z},k);
    int tN = tMax(tMax(t1.x,t1.y),t1.z);
    int tF = tMin(tMin(t2.x,t2.y),t2.z);
	if(tN > tF || tF < 0)
		return 0;
	int cmp = tN > 0 ? tN : tF;
	if(tN > 0){
		normal->x = tN > t1.x ? 0 : FIXED_ONE;
		normal->y = tN > t1.y ? 0 : FIXED_ONE;
		normal->z = tN > t1.z ? 0 : FIXED_ONE;
	}
	else{
		normal->x = tF < t1.x ? 0 : FIXED_ONE;
		normal->y = tF < t1.y ? 0 : FIXED_ONE;
		normal->z = tF < t1.z ? 0 : FIXED_ONE;
	}
    return tN;
}

int rayPlaneIntersection(Vec3 pos,Vec3 dir,Plane plane){
	return -fixedDivR((vec3Dot(pos,plane.normal) + plane.distance),vec3Dot(dir,plane.normal));
}

int rayEllipsoidIntersection(Vec3 ray_position,Vec3 ray_direction,Vec3 sphere_position,Vec3 radius){
    Vec3 oc = vec3Sub(sphere_position,ray_position);

    Vec3 ocn = vec3Div(oc, radius);
    Vec3 rdn = vec3Div(ray_direction,radius);

    int a = vec3Dot(rdn,rdn);
    int b = 2 * vec3Dot(ocn,rdn);
    int c = vec3Dot(ocn,ocn) - FIXED_ONE;

    int discriminant = fixedMulR(b,b) - 4 * fixedMulR(a,c);
    if(discriminant < 0)
        return -1;
    else
        return fixedDivR((-b - tSqrt(discriminant)), (2 * a));
}

int raySphereIntersection(Vec3 ray_position,Vec3 ray_direction,Vec3 sphere_position,int radius){
    Vec3 oc = vec3Sub(sphere_position,ray_position);

    int a = vec3Dot(ray_direction,ray_direction);
    int b = 2 * vec3Dot(oc,ray_direction);
    int c = vec3Dot(oc,oc) - fixedMulR(radius,radius);
    
    int discriminant = fixedMulR(b,b) - 4 * fixedMulR(a,c);
    if(discriminant < 0)
        return -1;
    else
        return fixedDivR((-b - tSqrt(discriminant)),(2 * a));
}

PlaneCollision intersectBoxPlane(Vec3 box_position,Vec3 box_size,Plane plane){
    Vec3 e = {
        box_size.x / 2,
        box_size.y / 2,
        box_size.z / 2
    };
    Vec3 center = {
        box_position.x + e.x,
        box_position.y + e.y,
        box_position.z + e.z
    };

    int s = vec3Dot(plane.normal,center) + plane.distance;

    int r =
        fixedMulR(e.x,tAbs(plane.normal.x)) +
        fixedMulR(e.y,tAbs(plane.normal.y)) +
        fixedMulR(e.z,tAbs(plane.normal.z));

    if(tAbs(s) <= r)
        return PLANE_BETWEEN;

    return s > r ? PLANE_FRONT : PLANE_BACK;
}


