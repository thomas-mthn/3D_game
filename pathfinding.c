#include "pathfinding.h"
#include "vec2.h"
#include "memory.h"
#include "physics.h"

bool directPath(Vec3 position,Vec3 size,Vec3 destination){
#if 0
	int distance = vec3Distance(vec3Shr(position,4),vec3Shr(destination,4)) << 4;
	Vec3 direction = vec3Direction(position,destination);
	for(int i = 0;i < distance / FIXED_ONE;i++){
		if(boxTreeCollision(position,size,0,0).collided){
			position.z += FIXED_ONE;
			if(boxTreeCollision(position,size,0,0).collided)
				return false;
		}
		position.x += direction.x;
		position.y += direction.y;
	}
#endif
	return true;
}

typedef struct{
	int amount;
    int lowest_layer_index;
    Route routes[0x3F];
} Heap;

static bool heapInsert(Heap* heap,Route value){
	int index;
	if(heap->amount == countof(heap->routes)){
		index = heap->lowest_layer_index;
        heap->lowest_layer_index += 1;
        if(heap->lowest_layer_index >= countof(heap->routes))
            heap->lowest_layer_index = countof(heap->routes) / 2 + 1;
        if(heap->routes[index].score < value.score)
            return false;
    }
	else{
        index = heap->amount;
        heap->amount += 1;  
    }
    heap->routes[index] = value;
	while(index > 0 && heap->routes[(index - 1) / 2].score > heap->routes[index].score){
		Route temp = heap->routes[(index - 1) / 2];
		heap->routes[(index - 1) / 2] = heap->routes[index];
		heap->routes[index] = temp;
		index = (index - 1) / 2;
    }   
	return true;
}

static Route heapGet(Heap* heap){
	if(!heap->amount){
        Route candidate = {0};
		return candidate;
	}
	Route result = heap->routes[0];
	heap->amount -= 1;
	heap->routes[0] = heap->routes[heap->amount];

	int index = 0;
    while(index < heap->amount){
        int maxIndex = index;
        int left = index * 2 + 1;
        int right = index * 2 + 2;

        if(left < heap->amount && heap->routes[left].score < heap->routes[maxIndex].score)
            maxIndex = left;
        if(right < heap->amount && heap->routes[right].score < heap->routes[maxIndex].score)
            maxIndex = right;
        if(maxIndex == index) 
            break; 
		
		Route temp = heap->routes[index];
		heap->routes[index] = heap->routes[maxIndex];
		heap->routes[maxIndex] = temp;
        index = maxIndex; 
    }
	return result;
}

static bool routeIsUniquePosition(Route* route,Vec3 position){
	for(int i = 0;i < route->n_positions;i++){
		if(position.x == route->positions[i].x && position.y == route->positions[i].y && position.z == route->positions[i].z)
			return false;
	}
	return true;
}

unsigned g_pathfind_hashtable[0x40 * 0x40 * 2];

static bool tileAlreadyChecked(Vec3 position){
	unsigned hash = tHash(tHash(position.x) ^ position.y);
	unsigned index = hash % countof(g_pathfind_hashtable);

	while(g_pathfind_hashtable[index]){
		if(g_pathfind_hashtable[index] == hash)
			return true;
		index += 1;
		index %= countof(g_pathfind_hashtable);
	}
	return false;
}

bool pathFinding(Vec3 position,Vec3 size,Vec3 destination,Route* route_result){
	for(int i = 0;i < countof(g_pathfind_hashtable);i++)
		g_pathfind_hashtable[i] = 0;

	position = vec3Shr(position,PATH_FIND_SIZE);
	destination = vec3Shr(destination,PATH_FIND_SIZE);

	position.z += 1;
	
	Heap* route_list = virtualAllocate(sizeof *route_list);
	heapInsert(route_list,(Route){
		.positions = {position},
		.n_positions = 1,
		.score = tAbs(position.x - destination.x) + tAbs(position.y - destination.y)
	});
	
	for(int i = 1;i < countof(route_list->routes);i++)
		route_list->routes[i].score = INT_MAX;

    bool result = false;
    
	for(int c = 0;c < 0x400;c++){
		if(!route_list->amount)
		    goto exit;
		Route route = heapGet(route_list);

		Vec3 offset[] = {
			{1,0,0},
			{-1,0,0},
			{0,1,0},
			{0,-1,0},
		};
		for(int i = 0;i < countof(offset);i++){
			Route route2 = route;
			Vec3 position_forward = vec3Add(route.positions[route.n_positions - 1],offset[i]);
		
			if(tileAlreadyChecked(position_forward))
				continue;
#if 0
			if(boxTreeCollision(vec3Shl(position_forward,PATH_FIND_SIZE),vec3Single(FIXED_ONE * 2),0,0).collided){
				position_forward.z += 1;
				if(boxTreeCollision(vec3Shl(position_forward,PATH_FIND_SIZE),vec3Single(FIXED_ONE * 2),0,0).collided)
					continue;
			}
#endif
			route2.positions[route2.n_positions++] = position_forward;
			if(
				position_forward.x == destination.x && 
				position_forward.y == destination.y
			){
				for(int j = 0;j < route2.n_positions;j++)
					route.positions[j] = route2.positions[route2.n_positions - j - 1];
				route.n_positions = route2.n_positions - 1;
				*route_result = route;
                result = true;
                goto exit;
			}
			if(route2.n_positions == countof(route.positions))
				goto exit;
			int score = (vec2Distance((Vec2){position_forward.x << 8,position_forward.y << 8},(Vec2){destination.x << 8,destination.y << 8}) >> 8) + route2.n_positions;
			route2.score = score;
			
			if(heapInsert(route_list,route2)){
				unsigned hash = tHash(tHash(position_forward.x) ^ position_forward.y);
				unsigned index = hash % countof(g_pathfind_hashtable);
				while(g_pathfind_hashtable[index]){
					index += 1;
					index %= countof(g_pathfind_hashtable);
				}
				g_pathfind_hashtable[index] = hash;
			}
		}
	}
 exit:
    virtualFree(route_list,sizeof *route_list);
	return result;
}
