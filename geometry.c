#include "geometry.h"
#include "octree.h"

bool pointBoxIntersection(Vec3 point,Vec3 box_position,Vec3 box_size){
	vec3Sub(&box_position,vec3ShrR(box_size,1));

	bool x = point.x > box_position.x && point.x < box_position.x + box_size.x;
	bool y = point.y > box_position.y && point.y < box_position.y + box_size.y;
	bool z = point.z > box_position.z && point.z < box_position.z + box_size.z;

	return x && y && z;
}

int rayBoxIntersection(Vec3 box_position,Vec3 box_size,Vec3 ro,Vec3 rd){
	ro = vec3SubR(ro,box_position);
    Vec3 m = vec3DivR(vec3Single(FIXED_ONE),rd);
    Vec3 n = vec3MulR(m,ro);
	Vec3 k = vec3MulR((Vec3){tAbs(m.x),tAbs(m.y),tAbs(m.z)},box_size);
    Vec3 t1 = vec3SubR((Vec3){-n.x,-n.y,-n.z},k);
    Vec3 t2 = vec3AddR((Vec3){-n.x,-n.y,-n.z},k);
    int tN = tMax(tMax(t1.x,t1.y),t1.z);
    int tF = tMin(tMin(t2.x,t2.y),t2.z);
	if(tN > tF || tF < 0)
		return 0;
    return tN;
}

int sdSegment(Vec2 p,Vec2 a,Vec2 b){
    Vec2 pa = vec2SubR(p,a);
    Vec2 ba = vec2SubR(b,a);
    int h = tClamp(fixedDivR(vec2Dot(pa,ba),vec2Dot(ba,ba)),0.0,FIXED_ONE);
    return vec2Length(vec2SubR(pa,vec2MulRS(ba,h)));
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
	Vec3 p = vec3SubR(vec3AddRS(voxel_position,voxel_size / 2),point);
	Vec3 q = vec3SubRS((Vec3){tAbs(p.x),tAbs(p.y),tAbs(p.z)},voxel_size);
	return vec3Length((Vec3){tMax(q.x,0),tMax(q.y,0),tMax(q.z,0)}) + tMin(tMax(q.x,tMax(q.y,q.z)),0);
}

int rayVoxelIntersection(Voxel* voxel,Vec3 ro,Vec3 rd,Vec3* normal){
	Vec3 voxel_pos = (Vec3){
		voxel->position_x * FIXED_ONE * 2 >> voxel->depth,
		voxel->position_y * FIXED_ONE * 2 >> voxel->depth,
		voxel->position_z * FIXED_ONE * 2 >> voxel->depth,
	};
	ro = vec3SubR(ro,vec3AddRS(voxel_pos,FIXED_ONE >> voxel->depth));
    Vec3 m = vec3DivR(vec3Single(FIXED_ONE),rd);
    Vec3 n = vec3MulR(m,ro);
	Vec3 k = vec3MulRS((Vec3){tAbs(m.x),tAbs(m.y),tAbs(m.z)},FIXED_ONE >> voxel->depth);
    Vec3 t1 = vec3SubR((Vec3){-n.x,-n.y,-n.z},k);
    Vec3 t2 = vec3AddR((Vec3){-n.x,-n.y,-n.z},k);
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

int raySphereIntersection(Vec3 ray_position,Vec3 ray_direction,Vec3 sphere_position,int radius){
    Vec3 oc = vec3SubR(sphere_position,ray_position);
    int a = vec3Dot(ray_direction,ray_direction);
    int b = 2 * vec3Dot(oc,ray_direction);
    int c = vec3Dot(oc,oc) - fixedMulR(radius,radius);
    int discriminant = fixedMulR(b,b) - 4 * fixedMulR(a,c);
    if(discriminant < 0)
        return -1;
    else
        return fixedDivR((-b - tSqrt(discriminant)),(2 * a));
}

bool boxBoxIntersect(Vec3 box1_pos,Vec3 box1_size,Vec3 box2_pos,Vec3 box2_size){
	Vec3 box_max = vec3AddR(box1_pos,box1_size);
	Vec3 cube_max = vec3AddR(box2_pos,box2_size);
	bool x = box1_pos.x <= cube_max.x && box_max.x >= box2_pos.x;
	bool y = box1_pos.y <= cube_max.y && box_max.y >= box2_pos.y;
	bool z = box1_pos.z <= cube_max.z && box_max.z >= box2_pos.z;
    return x && y && z;
}

bool boxCubeIntersect(Vec3 box_pos,Vec3 box_size,Vec3 cube_pos,int cube_size){
    return boxBoxIntersect(box_pos,box_size,cube_pos,vec3Single(cube_size));
}
