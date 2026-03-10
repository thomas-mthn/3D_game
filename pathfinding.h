#ifndef PATHFINDING_H
#define PATHFINDING_H

#include "vec3.h"

#define PATH_FIND_SIZE 15

structure(Route){
	int  score;
	int  n_positions;
	Vec3 positions[0x40];
};

bool directPath(Vec3 position,Vec3 size,Vec3 destination);
bool pathFinding(Vec3 position,Vec3 size,Vec3 destination,Route* route_result);

#endif