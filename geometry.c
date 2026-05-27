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

bool intersectBoxSphere(Vec3 box_pos,Vec3 box_size,Vec3 sphere_pos,real sphere_radius){
    real x = tMax(box_pos.x - box_size.x,tMin(sphere_pos.x,box_pos.x + box_size.x));
    real y = tMax(box_pos.y - box_size.y,tMin(sphere_pos.y,box_pos.y + box_size.y));
    real z = tMax(box_pos.z - box_size.z,tMin(sphere_pos.z, box_pos.z + box_size.z));

    real dx = sphere_pos.x - x;
    real dy = sphere_pos.y - y;
    real dz = sphere_pos.z - z;

    real dist2 =
        realMulR(dx, dx) +
        realMulR(dy, dy) +
        realMulR(dz, dz);

    return dist2 <= realMulR(sphere_radius, sphere_radius);
}

bool intersectTorusPoint(Vec3 p,real R,real r){
    real qx = vec2Length((Vec2){p.x,p.z}) - R;
    real qy = p.y;

    real f = realMulR(qx,qx) + realMulR(qy,qy);

    return tAbs(f - realMulR(r,r)) < REAL_EPSILON; // epsilon for float error
}

bool intersectCylinderPoint(Vec3 p,Vec3 position,Vec3 direction,real radius){
    Vec3 local = vec3Sub(p,position);

    // remove axial component
    Vec3 radial = vec3Sub(local,vec3MulS(direction,vec3Dot(local,direction)));

    return vec3Dot(radial,radial) <= realMulR(radius,radius);
}

bool intersectTorusBox(Vec3 box_position,Vec3 box_size,Vec3 torus_position,Vec2 torus_radius){
    Vec3 box_min = vec3Sub(
        box_position,
        vec3MulS(box_size,FIXED_ONE / 2)
    );

    Vec3 box_max = vec3Add(
        box_position,
        vec3MulS(box_size,FIXED_ONE / 2)
    );

    float R = torus_radius.x;
    float r = torus_radius.y;

    Vec3 p = {
        tClamp(torus_position.x, box_min.x, box_max.x),
        tClamp(torus_position.y, box_min.y, box_max.y),
        tClamp(torus_position.z, box_min.z, box_max.z),
    };

    Vec3 q = vec3Sub(p, torus_position);

    float xz = tSqrt(vec3Dot(
        (Vec3){q.x, 0, q.z},
        (Vec3){q.x, 0, q.z}
    )) - R;

    float dist2 =
        xz * xz +
        q.y * q.y;

    return dist2 <= r * r;
}

bool intersectCylinderBox(
    Vec3 box_position,
    Vec3 box_size,
    Vec3 cylinder_position,
    Vec3 cylinder_direction,
    real cylinder_radius
){
    
    Vec3 box_min = vec3Sub(box_position,vec3MulS(box_size,FIXED_ONE / 2));
    Vec3 box_max = vec3Add(box_position,vec3MulS(box_size,FIXED_ONE / 2));

    Vec3 closest = {
        tClamp(cylinder_position.x,box_min.x,box_max.x),
        tClamp(cylinder_position.y,box_min.y,box_max.y),
        tClamp(cylinder_position.z,box_min.z,box_max.z),
    };

    Vec3 delta = vec3Sub(closest,cylinder_position);

    Vec3 radial =
        vec3Sub(delta,vec3MulS(cylinder_direction,vec3Dot(delta,cylinder_direction)));

    real dist2 = vec3Dot(radial,radial);

    return dist2 <= realMulR(cylinder_radius,cylinder_radius);
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
		return -FIXED_ONE;
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
		return -FIXED_ONE;
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
		return -FIXED_ONE;
    return tF;
}

real rayCubeIntersectionPierce(Vec3 cube_position,real cube_size,Vec3 ro,Vec3 rd){
	ro = vec3Sub(ro,cube_position);
    
    Vec3 m = vec3Reciprocal(rd);
    Vec3 n = vec3Mul(m,ro);
	Vec3 k = vec3MulS((Vec3){tAbs(m.x),tAbs(m.y),tAbs(m.z)},cube_size);
    Vec3 t1 = vec3Sub((Vec3){-n.x,-n.y,-n.z},k);
    Vec3 t2 = vec3Add((Vec3){-n.x,-n.y,-n.z},k);
    real tN = tMax(tMax(t1.x,t1.y),t1.z);
    real tF = tMin(tMin(t2.x,t2.y),t2.z);
	if(tN > tF || tF < 0)
		return -FIXED_ONE;
    return tF - tN;
}

real rayPlaneIntersection(Vec3 pos,Vec3 dir,Plane plane){
	return -realDivR((vec3Dot(pos,plane.normal) + plane.distance),vec3Dot(dir,plane.normal));
}

real rayEllipsoidIntersection(Vec3 ray_position,Vec3 ray_direction,Vec3 sphere_position,Vec3 radius){
    Vec3 oc = vec3Sub(ray_position,sphere_position);

    Vec3 ocn = vec3Div(oc,radius);
    Vec3 rdn = vec3Div(ray_direction,radius);

    real a = vec3Dot(rdn,rdn);
    real b = 2 * vec3Dot(ocn,rdn);
    real c = vec3Dot(ocn,ocn) - FIXED_ONE;

    real discriminant = realMulR(b,b) - 4 * realMulR(a,c);
    if(discriminant < 0)
        return -FIXED_ONE;
    else
        return realDivR((-b - tSqrt(discriminant)),(2 * a));
}

real rayCylinderIntersection(Vec3 ray_position,Vec3 ray_direction,Vec3 cylinder_position,Vec3 axis,real radius){
    Vec3 oc = vec3Sub(ray_position,cylinder_position);

    Vec3 d_perp  = vec3Sub(ray_direction,vec3MulS(axis,vec3Dot(ray_direction,axis)));
    Vec3 oc_perp = vec3Sub(oc,vec3MulS(axis,vec3Dot(oc,axis)));

    real a = vec3Dot(d_perp,d_perp);
    real b = vec3Dot(oc_perp,d_perp);
    real c = vec3Dot(oc_perp,oc_perp) - realMulR(radius,radius);
    real h = realMulR(b,b) - realMulR(a,c);
    if(h < 0)
        return -FIXED_ONE;
    h = tSqrt(h);
    return realDivR((-b-h),a);
}

bool rayTorusIntersection(Vec3 ro,Vec3 rd,Vec2 tor){
    real po = 1.0;
    real Ra2 = realMulR(tor.x,tor.x);
    real ra2 = realMulR(tor.y,tor.y);
    real m = vec3Dot(ro,ro);
    real n = vec3Dot(ro,rd);
    real k = (m + Ra2 - ra2) / 2.0;
    real k3 = n;
    real k2 = n*n - Ra2*vec2Dot((Vec2){rd.x,rd.y},(Vec2){rd.x,rd.y}) + k;
    real k1 = n*k - Ra2*vec2Dot((Vec2){rd.x,rd.y},(Vec2){ro.x,ro.y});
    real k0 = k*k - Ra2*vec2Dot((Vec2){ro.x,ro.y},(Vec2){ro.x,ro.y});
    
    if( tAbs(k3*(k3*k3-k2)+k1) < 0.01){
        po = -1.0;
        real tmp=k1; k1=k3; k3=tmp;
        k0 = 1.0/k0;
        k1 = k1*k0;
        k2 = k2*k0;
        k3 = k3*k0;
    }
    
    real c2 = k2*2.0 - 3.0*k3*k3;
    real c1 = k3*(k3*k3-k2)+k1;
    real c0 = k3*(k3*(c2+2.0*k2)-8.0*k1)+4.0*k0;
    c2 /= 3.0;
    c1 *= 2.0;
    c0 /= 3.0;
    real Q = c2*c2 + c0;
    real R = c2*c2*c2 - 3.0*c2*c0 + c1*c1;
    real h = R*R - Q*Q*Q;
    
    if(h >= 0.0){
        return true;
    }
    
    real sQ = tSqrt(Q);
    real w = sQ*tCos( tArcSin(-R/(sQ*Q) - FIXED_ONE / 4) / 3.0 );
    real d2 = -(w+c2);
    if( d2<0.0 )
        return false;
    return true;
}

real raySphereIntersection(Vec3 ray_position,Vec3 ray_direction,Vec3 sphere_position,real radius){
    Vec3 oc = vec3Sub(ray_position,sphere_position);

    real a = vec3Dot(ray_direction,ray_direction);
    real b = 2 * vec3Dot(oc,ray_direction);
    real c = vec3Dot(oc,oc) - realMulR(radius,radius);
    
    real discriminant = realMulR(b,b) - 4 * realMulR(a,c);
    if(discriminant < 0)
        return -FIXED_ONE;
    else
        return realDivR((-b - tSqrt(discriminant)),(2 * a));
}
