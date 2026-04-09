#include "console.h"
#include "octree.h"
#include "octree_render.h"
#include "draw.h"
#include "vec2.h"
#include "main.h"
#include "lighting.h"
#include "voxel_gui.h"
#include "entity.h"
#include "span.h"

bool g_render_lines;
bool g_render_fog;

bool g_no_lighing;

static void voxelModelRasterizeSide(DrawSurface* surface,Vec2 model_angle,Vec3* luminance,Voxel* voxel,Vec3 block_pos,int side,Vec3 camera_position,int camera_distance){
    unsigned polygon_id = tRnd();
	Vec2 axis = g_axis_table[side];

	int size = depthToSize(voxel->depth) >> 8;

	Vec3 pos[4] = {block_pos,block_pos,block_pos,block_pos};

	((int*)&pos[1])[axis.y] += size;
	((int*)&pos[2])[axis.x] += size;
	((int*)&pos[3])[axis.x] += size;
	((int*)&pos[3])[axis.y] += size;

	Vec3 point_2[4];
	
	int tri[] = {tCos(model_angle.x),tSin(model_angle.x),tCos(model_angle.y),tSin(model_angle.y)};

	point_2[0] = pointToScreenRenderer(pos[0],tri,camera_position,vec2MulRS(g_options.fov,fixedMulR(camera_distance,FIXED_ONE * 2)));
	point_2[1] = pointToScreenRenderer(pos[1],tri,camera_position,vec2MulRS(g_options.fov,fixedMulR(camera_distance,FIXED_ONE * 2)));
	point_2[2] = pointToScreenRenderer(pos[2],tri,camera_position,vec2MulRS(g_options.fov,fixedMulR(camera_distance,FIXED_ONE * 2)));
	point_2[3] = pointToScreenRenderer(pos[3],tri,camera_position,vec2MulRS(g_options.fov,fixedMulR(camera_distance,FIXED_ONE * 2)));

	Vec3 d_point[] = {
		{point_2[0].x,-point_2[0].y,point_2[0].z},
		{point_2[1].x,-point_2[1].y,point_2[1].z},
		{point_2[3].x,-point_2[3].y,point_2[3].z},
		{point_2[2].x,-point_2[2].y,point_2[2].z}
	};

	VoxelStatic* voxel_s = g_voxel_static + voxel->type;

	//drawSquare(g_surface_model,0,0,FIXED_ONE,vec3ShlR(voxel_s->color,2));
	if(point_2[0].z <= 0 || point_2[1].z <= 0 || point_2[2].z <= 0 || point_2[3].z <= 0)
		return;
	/*
	Vec3 color_table[] = {
		{1 << 20,0,0},
		{1 << 20,0,0},
		{0,1 << 20,0},
		{0,1 << 20,0},
		{0,0,1 << 20},
		{0,0,1 << 20},
	};
	*/
	Vec3 color = luminance ? vec3ShlR(luminance[side],4) : vec3Single(FIXED_ONE << 4);

	if(voxel_s->texture){
		Vec2 texture_crd[4];
		if(voxel_s->flags & VOXEL_TEXTUREFILL){
			int size = FIXED_ONE >> 0;
			int texture_x = 0;
			int texture_y = 0;
			texture_crd[0] = (Vec2){texture_x,texture_y};
			texture_crd[1] = (Vec2){texture_x,texture_y + size};
			texture_crd[2] = (Vec2){texture_x + size,texture_y + size};
			texture_crd[3] = (Vec2){texture_x + size,texture_y};
		}
		else{
			int texture_x = ((int*)&block_pos)[axis.x] / 16;
			int texture_y = ((int*)&block_pos)[axis.y] / 16;
			fixedMul(&texture_x,voxel_s->texture_size);
			fixedMul(&texture_y,voxel_s->texture_size);
			int texture_size = fixedMulR(1 << 16,voxel_s->texture_size);
			texture_crd[0] = (Vec2){texture_x,texture_y}; 
			texture_crd[1] = (Vec2){texture_x,texture_y + texture_size}; 
			texture_crd[2] = (Vec2){texture_x + texture_size,texture_y + texture_size}; 
			texture_crd[3] = (Vec2){texture_x + texture_size,texture_y}; 
		}
		drawTexturePolygon3d(surface,voxel_s->texture,texture_crd,d_point,color,4);
	}
	else{
		drawPolygon3d(surface,d_point,vec3MulR(color,voxel_s->color));
	}
}

void voxelModelRasterize(DrawSurface* surface,Vec2 model_angle,Vec3* luminance,Voxel* voxel,Vec3 camera_position,int camera_distance){
	int block_size = depthToSize(voxel->depth) >> 8;
	Vec3 block_pos = vec3ShrR(voxelWorldPos(voxel),8);
	if(voxel->type == VOXEL_PARENT){
		Vec3 point[] = {
			{block_pos.x + 0,block_pos.y + 0,block_pos.z + 0},
			{block_pos.x + 0,block_pos.y + 0,block_pos.z + block_size},
			{block_pos.x + 0.,block_pos.y + block_size,block_pos.z + 0},
			{block_pos.x + 0,block_pos.y + block_size,block_pos.z + block_size},
			{block_pos.x + block_size,block_pos.y + 0,block_pos.z + 0},
			{block_pos.x + block_size,block_pos.y + 0,block_pos.z + block_size},
			{block_pos.x + block_size,block_pos.y + block_size,block_pos.z + 0},
			{block_pos.x + block_size,block_pos.y + block_size,block_pos.z + block_size},
		};
		int order[][8] = {
			{0,1,2,4,3,5,6,7},
			{1,0,3,5,2,7,4,6},
			{2,0,3,6,1,4,7,5},
			{3,1,2,7,0,5,6,4},
			{4,5,6,0,7,1,2,3},
			{5,7,4,1,6,3,0,2},
			{6,4,7,2,5,0,3,1},
			{7,5,6,3,4,1,2,0},
		};
		Vec3 pos = vec3ShrR(voxelWorldPos(voxel),8);
		Vec3 rel_pos = vec3SubR(camera_position,pos);
		Vec3 i_pos = (Vec3){rel_pos.x * 2 / block_size,rel_pos.y * 2 / block_size,rel_pos.z * 2 / block_size};
		int order_id = (i_pos.z <= 0) << 2 | (i_pos.y <= 0) << 1 | (i_pos.x <= 0) << 0;
		for(int i = 0;i < 8;i++){
			int index = order[order_id][i];
			voxelModelRasterize(surface,model_angle,luminance,voxel->child_s[index],camera_position,camera_distance);
		}
		return;
	}
	if(voxel->type == VOXEL_AIR)
		return;

	if(camera_position.x - block_pos.x < 0)
		voxelModelRasterizeSide(surface,model_angle,luminance,voxel,block_pos,0,camera_position,camera_distance);
	if(camera_position.x - block_pos.x - block_size > 0)
		voxelModelRasterizeSide(surface,model_angle,luminance,voxel,vec3AddR(block_pos,(Vec3){block_size,0,0}),1,camera_position,camera_distance);
	if(camera_position.y - block_pos.y < 0)
		voxelModelRasterizeSide(surface,model_angle,luminance,voxel,block_pos,2,camera_position,camera_distance);
	if(camera_position.y - block_pos.y - block_size > 0)
		voxelModelRasterizeSide(surface,model_angle,luminance,voxel,vec3AddR(block_pos,(Vec3){0,block_size,0}),3,camera_position,camera_distance);
	if(camera_position.z - block_pos.z < 0)
		voxelModelRasterizeSide(surface,model_angle,luminance,voxel,block_pos,4,camera_position,camera_distance);
	if(camera_position.z - block_pos.z - block_size > 0)
		voxelModelRasterizeSide(surface,model_angle,luminance,voxel,vec3AddR(block_pos,(Vec3){0,0,block_size}),5,camera_position,camera_distance);
}

static Vec3 lightingGet(Vec3 position,int mipmap,Vec2 axis){
	unsigned hash_main = luxelHashGet(position,mipmap);
	Luxel* luxel_main = luxelGet(hash_main); 

	if(luxel_main->hash == hash_main && luxel_main->n_sample > 0x100)
		return luxel_main->luminance;

	unsigned hash = luxelHashGet(vec3ShrR(position,1),mipmap + 1);
	Luxel* luxel = luxelGet(hash);

	if(luxel->hash == hash && luxel->n_sample > 0x100)
		return luxel->luminance;

	Vec3 neighbour = position;
	((int*)&neighbour)[axis.x] += 1;

	hash = luxelHashGet(neighbour,mipmap);
	luxel = luxelGet(hash);

	if(luxel->hash == hash && luxel->n_sample > 0x100)
		return luxel->luminance;

	return luxel_main->luminance;
}

static void drawSideRecursive(Voxel* voxel,Vec3 block_pos,int side,Vec2 coord,int depth,int surface_angle){
	Vec2 axis = g_axis_table[side];
	Vec3 block_pos_t = block_pos;
	int size = depthToSize(voxel->depth + depth);
	((int*)&block_pos_t)[axis.x] += fixedMulR(coord.x << FIXED_PRECISION,size);
	((int*)&block_pos_t)[axis.y] += fixedMulR(coord.y << FIXED_PRECISION,size);

	Vec3 pos[4] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

	((int*)&pos[1])[axis.y] += size;
	((int*)&pos[2])[axis.x] += size;
	((int*)&pos[3])[axis.x] += size;
	((int*)&pos[3])[axis.y] += size;

	Vec3 voxel_pos = voxelWorldPos(voxel);

	Vec3 square_pos = pos[0];
	((int*)&square_pos)[axis.x] += size / 2;
	((int*)&square_pos)[axis.y] += size / 2;

	Vec3 point[4] = {pointToScreen(pos[0]),pointToScreen(pos[1]),pointToScreen(pos[2]),pointToScreen(pos[3])};

	int distance_max = 0;
	int distance_max_index;

	for(int i = 0;i < 4;i++){
		int distance = vec3Distance(vec3ShrR(g_position,4),vec3ShrR(pos[i],4));
		if(distance > distance_max){
			distance_max = distance;
			distance_max_index = i;
		}
	}

	Vec3 cube_c = pos[0];

	((int*)&cube_c)[axis.x] = tClamp(((int*)&g_position)[axis.x],((int*)&pos[0])[axis.x],((int*)&pos[3])[axis.x]);
	((int*)&cube_c)[axis.y] = tClamp(((int*)&g_position)[axis.y],((int*)&pos[0])[axis.y],((int*)&pos[3])[axis.y]);

	Vec3 normal = g_normal_table[side];
	int mipmap = mipmapGet(squarePointClosestPosition(pos[0],size,normal),normal,distance_max,surface_angle);

	int split = 25 + -tClamp(mipmap,26 - LUXEL_MAX_MIPMAP,31) - voxel->depth;

	VoxelStatic* voxel_s = g_voxel_static + voxel->type;

	if(depth < split && !(voxel_s->flags & VOXEL_EMITER)){
		Vec3 v_pos = vec3ShlR((Vec3){voxel->position_x,voxel->position_y,voxel->position_z},depth);
		((int*)&v_pos)[axis.x] += coord.x;
		((int*)&v_pos)[axis.y] += coord.y;
		if(side & 1)
			((int*)&v_pos)[side >> 1] += (1 << depth) - 1;
		
		if(!voxel->opened && !voxel->animation && !squareVisible(v_pos,voxel->depth + depth,side,voxel->type))
			return;
		
		if(sdSquare(vec3ShrR(g_position,4),vec3ShrR(block_pos_t,4),size >> 4,side) > RENDER_DISTANCE)
			return;

		if(!squareInScreenSpace(pos))
			return;
		
		coord.x <<= 1;
		coord.y <<= 1;
		drawSideRecursive(voxel,block_pos,side,vec2AddR(coord,(Vec2){0,0}),depth + 1,surface_angle);
		drawSideRecursive(voxel,block_pos,side,vec2AddR(coord,(Vec2){0,1}),depth + 1,surface_angle);
		drawSideRecursive(voxel,block_pos,side,vec2AddR(coord,(Vec2){1,0}),depth + 1,surface_angle);
		drawSideRecursive(voxel,block_pos,side,vec2AddR(coord,(Vec2){1,1}),depth + 1,surface_angle);
		return;
	}

	for(int i = 0;i < 4;i++){
		if(((int*)&pos[i])[axis.x] < ((int*)&voxel_pos)[axis.x])
			((int*)&pos[i])[axis.x] = ((int*)&voxel_pos)[axis.x];
		if(((int*)&pos[i])[axis.y] < ((int*)&voxel_pos)[axis.y])
			((int*)&pos[i])[axis.y] = ((int*)&voxel_pos)[axis.y];
	}

	Vec3 point_2[4];
    
	point_2[0] = pointToScreenRenderer(pos[0],g_tri,g_position,g_options.fov);
	point_2[1] = pointToScreenRenderer(pos[1],g_tri,g_position,g_options.fov);
	point_2[2] = pointToScreenRenderer(pos[2],g_tri,g_position,g_options.fov);
	point_2[3] = pointToScreenRenderer(pos[3],g_tri,g_position,g_options.fov);

	mipmap = tClamp(mipmapGet(squarePointClosestPosition(pos[0],size,normal),normal,distance_max,surface_angle),26 - LUXEL_MAX_MIPMAP,31);

	Vec3 light_pos = pos[0];
	((int*)&light_pos)[axis.x] += size / 2;
	((int*)&light_pos)[axis.y] += size / 2;

	Vec3 luxel_pos = vec3ShrR(light_pos,mipmap);
	unsigned hash = luxelHashGet(luxel_pos,mipmap);
	Luxel* luxel = luxelGet(hash);

	Vec3 luminance;
	if(voxel_s->flags & VOXEL_EMITER){
		luminance = voxel_s->side[side].custom ? voxel_s->side[side].color : voxel_s->color;
	}
	else{
		luminance = luxel->luminance;
		if(!voxel_s->texture)
			vec3Mul(&luminance,voxel_s->side[side].custom ? voxel_s->side[side].color : voxel_s->color);
	}

	vec3MulS(&luminance,g_exposure);

	Vec3 d_point[] = {
		{point_2[0].x,point_2[0].y,point_2[0].z},
		{point_2[1].x,point_2[1].y,point_2[1].z},
		{point_2[3].x,point_2[3].y,point_2[3].z},
		{point_2[2].x,point_2[2].y,point_2[2].z}
	};

	if(voxel_s->flags & VOXEL_EMITER){
        drawPolygon3d(&g_surface,pos,luminance);
		return;
	}

	Vec3 luxel_positions[] = {
		luxel_pos,
		luxel_pos,
		luxel_pos,
		luxel_pos,
	};

	((int*)&luxel_positions[1])[axis.y] += 1;
	((int*)&luxel_positions[2])[axis.x] += 1;
	((int*)&luxel_positions[3])[axis.x] += 1;
	((int*)&luxel_positions[3])[axis.y] += 1;

	unsigned luxel_hashes[] = {
		0,
		luxelHashGet(luxel_positions[1],mipmap),
		luxelHashGet(luxel_positions[2],mipmap),
		luxelHashGet(luxel_positions[3],mipmap),
	};

	Vec3 luxel_colors[4];
	
	int c_index[] = {0,1,3,2};
	for(int i = 0;i < 4;i++){
		int x = i / 2;
		int y = i % 2;
		int index = c_index[i];
		if(g_no_lighing){
			luxel_colors[index] = vec3Single(FIXED_ONE << 4);
			continue;
		}
		Vec3 position = luxel_pos;
		((int*)&position)[axis.x] += x;
		((int*)&position)[axis.y] += y;

		luxel_colors[index] = lightingGet(position,mipmap,axis);

		unsigned dynamic_hash = luxelHashGet(position,mipmap);
		Luxel* luxel_dynamic = luxelDynamicGet(dynamic_hash);

		if(luxel_dynamic->hash == dynamic_hash)
			vec3Add(&luxel_colors[index],luxel_dynamic->luminance);
		
		if(!voxel_s->texture)
			vec3Mul(luxel_colors + index,voxel_s->color);
		
		int f_x = tClamp((point[index].x + FIXED_ONE) * 16 / FIXED_ONE,0,31);
		int f_y = tClamp((point[index].y + FIXED_ONE) * 16 / FIXED_ONE,0,31);
		
		int distance = bitScanReverse(vec3Distance(vec3ShrR(g_position,4),vec3ShrR(pos[index],4)) >> 4);
	}

	for(int i = 0;i < 4;i++){
		vec3MulS(luxel_colors + i,g_exposure);
	}
		
	//luminance = vec3ShrR(vec3AddR(vec3AddR(luxel_colors[0],luxel_colors[1]),vec3AddR(luxel_colors[2],luxel_colors[3])),2);

	if(g_render_lines && voxel->type != VOXEL_MENU && g_surface.backend != RENDER_BACKEND_GL){
		drawLine(&g_surface,point_2[0].x,point_2[0].y,point_2[1].x,point_2[1].y,pixelColorToColor(0xFF00FF));
		drawLine(&g_surface,point_2[0].x,point_2[0].y,point_2[1].x,point_2[1].y,pixelColorToColor(0xFF00FF));
		drawLine(&g_surface,point_2[0].x,point_2[0].y,point_2[1].x,point_2[1].y,pixelColorToColor(0xFF00FF));
		drawLine(&g_surface,point_2[0].x,point_2[0].y,point_2[1].x,point_2[1].y,pixelColorToColor(0xFF00FF));
		return;
	}

	Texture* texture = voxel_s->side[side].custom ? voxel_s->side[side].texture : voxel_s->texture;

	if(texture){
		Vec2 texture_crd[4];
		if(voxel_s->flags & VOXEL_TEXTUREFILL){
			int size = FIXED_ONE >> depth;
			int texture_x = coord.x * size;
			int texture_y = coord.y * size;
			texture_crd[0] = (Vec2){texture_x,texture_y};
			texture_crd[1] = (Vec2){texture_x,texture_y + size};
			texture_crd[2] = (Vec2){texture_x + size,texture_y + size};
			texture_crd[3] = (Vec2){texture_x + size,texture_y};
		}
		else{
			int texture_x = ((int*)&block_pos)[axis.x] / 16 + (coord.x << (25 - depth - voxel->depth) - 4);
			int texture_y = ((int*)&block_pos)[axis.y] / 16 + (coord.y << (25 - depth - voxel->depth) - 4);
			fixedMul(&texture_x,voxel_s->texture_size);
			fixedMul(&texture_y,voxel_s->texture_size);
			int texture_size = fixedMulR(1 << (25 - depth - voxel->depth) - 4,voxel_s->texture_size);
			texture_crd[0] = (Vec2){texture_x,texture_y}; 
			texture_crd[1] = (Vec2){texture_x,texture_y + texture_size}; 
			texture_crd[2] = (Vec2){texture_x + texture_size,texture_y + texture_size}; 
			texture_crd[3] = (Vec2){texture_x + texture_size,texture_y}; 
		}
		
        if(g_smooth_lighting)
            drawColoredTexturePolygon3d(&g_surface,texture,texture_crd,d_point,luxel_colors);
        else
            drawTexturePolygon3d(&g_surface,texture,texture_crd,d_point,luminance,4);
			
	}
	else{
        if(g_smooth_lighting)
            drawColoredPolygon3d(&g_surface,d_point,luxel_colors);
        else
            drawPolygon3d(&g_surface,d_point,luminance);
	}
}

static Vec3 triangleNormal(Vec3 a,Vec3 b,Vec3 c){
    Vec3 u = vec3SubR(b,a);
    Vec3 v = vec3SubR(c,a);
    return vec3Normalize(vec3Cross(u,v));
}

static void drawSideRecursiveTraced(Voxel* voxel,Vec3 block_pos,int side,Vec2 coord,int depth,int surface_angle){
	if(depth > 11)
		return;
	Vec2 axis = g_axis_table[side];
	Vec3 block_pos_t = block_pos;
	int size = depthToSize(voxel->depth + depth);
	((int*)&block_pos_t)[axis.x] += fixedMulR(coord.x << FIXED_PRECISION,size);
	((int*)&block_pos_t)[axis.y] += fixedMulR(coord.y << FIXED_PRECISION,size);

	Vec3 pos[4] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

	((int*)&pos[1])[axis.y] += size;
	((int*)&pos[2])[axis.x] += size;
	((int*)&pos[3])[axis.x] += size;
	((int*)&pos[3])[axis.y] += size;

	Vec3 voxel_pos = voxelWorldPos(voxel);

	Vec3 square_pos = pos[0];
	((int*)&square_pos)[axis.x] += size / 2;
	((int*)&square_pos)[axis.y] += size / 2;

	Vec3 point[4] = {pointToScreen(pos[0]),pointToScreen(pos[1]),pointToScreen(pos[2]),pointToScreen(pos[3])};

	int distance_max = 0;
	int distance_max_index;

	for(int i = 0;i < 4;i++){
		int distance = vec3Distance(vec3ShrR(g_position,4),vec3ShrR(pos[i],4));
		if(distance > distance_max){
			distance_max = distance;
			distance_max_index = i;
		}
	}

	Vec3 cube_c = pos[0];

	((int*)&cube_c)[axis.x] = tClamp(((int*)&g_position)[axis.x],((int*)&pos[0])[axis.x],((int*)&pos[3])[axis.x]);
	((int*)&cube_c)[axis.y] = tClamp(((int*)&g_position)[axis.y],((int*)&pos[0])[axis.y],((int*)&pos[3])[axis.y]);

	Vec3 normal = g_normal_table[side];
	int mipmap = mipmapGet(squarePointClosestPosition(pos[0],size,normal),normal,distance_max,surface_angle);

	int split = 26 + -mipmap - voxel->depth;

	VoxelStatic* voxel_s = g_voxel_static + voxel->type;

	if(depth < split && !(voxel_s->flags & VOXEL_EMITER)){
		Vec3 v_pos = vec3ShlR((Vec3){voxel->position_x,voxel->position_y,voxel->position_z},depth);
		((int*)&v_pos)[axis.x] += coord.x;
		((int*)&v_pos)[axis.y] += coord.y;
		if(side & 1)
			((int*)&v_pos)[side >> 1] += (1 << depth) - 1;
		
		if(!voxel->opened && !voxel->animation && !squareVisible(v_pos,voxel->depth + depth,side,voxel->type))
			return;
		
		if(!squareInScreenSpace(pos))
			return;
		
		coord.x <<= 1;
		coord.y <<= 1;
		drawSideRecursiveTraced(voxel,block_pos,side,vec2AddR(coord,(Vec2){0,0}),depth + 1,surface_angle);
		drawSideRecursiveTraced(voxel,block_pos,side,vec2AddR(coord,(Vec2){0,1}),depth + 1,surface_angle);
		drawSideRecursiveTraced(voxel,block_pos,side,vec2AddR(coord,(Vec2){1,0}),depth + 1,surface_angle);
		drawSideRecursiveTraced(voxel,block_pos,side,vec2AddR(coord,(Vec2){1,1}),depth + 1,surface_angle);
		return;
	}

	for(int i = 0;i < 4;i++){
		if(((int*)&pos[i])[axis.x] < ((int*)&voxel_pos)[axis.x])
			((int*)&pos[i])[axis.x] = ((int*)&voxel_pos)[axis.x];
		if(((int*)&pos[i])[axis.y] < ((int*)&voxel_pos)[axis.y])
			((int*)&pos[i])[axis.y] = ((int*)&voxel_pos)[axis.y];
	}

	Vec3 point_2[4];
	
	point_2[0] = pointToScreenRenderer(pos[0],g_tri,g_position,g_options.fov);
	point_2[1] = pointToScreenRenderer(pos[1],g_tri,g_position,g_options.fov);
	point_2[2] = pointToScreenRenderer(pos[2],g_tri,g_position,g_options.fov);
	point_2[3] = pointToScreenRenderer(pos[3],g_tri,g_position,g_options.fov);

	mipmap = tClamp(mipmapGet(squarePointClosestPosition(pos[0],size,normal),normal,distance_max,surface_angle),26 - LUXEL_MAX_MIPMAP,31);

	Vec3 light_pos = pos[0];
	((int*)&light_pos)[axis.x] += size / 2;
	((int*)&light_pos)[axis.y] += size / 2;

	Vec3 luminance;
	if(voxel->type == VOXEL_MIRROR){
		Vec3 relative = vec3SubR(light_pos,g_position);
		Vec3 direction = vec3Normalize(vec3Reflect(vec3ShrR(relative,8),normal));
		Vec3 position = light_pos;
		((int*)&position)[side >> 1] += side % 2 ? 0x10 : -0x10;
		if(tAbs(direction.x) < 4)
			direction.x = 4;
		if(tAbs(direction.y) < 4)
			direction.y = 4;
		if(tAbs(direction.z) < 4)
			direction.z = 4;
		luminance = rayLuminance(position,direction);
		luminance = vec3ShlR(luminance,4);
	}
	else if(voxel->type == VOXEL_GLASS){
		Vec3 relative = vec3SubR(light_pos,g_position);
		Vec3 offset = vec3Direction(vec3ShrR(g_position,4),vec3ShrR(light_pos,4));
		Vec3 position = light_pos;
		((int*)&position)[side >> 1] -= side % 2 ? 0x10 : -0x10;
		luminance = rayLuminance(position,offset);
		luminance = vec3ShlR(luminance,4);
	}
	else if(voxel->type == VOXEL_WATER){
		for(int i = 0;i < countof(pos);i++){
			if(vec2Distance(voxel->splash_position,(Vec2){pos[i].x,pos[i].y}) > (g_tick - voxel->splash_tick) << 11)
				continue;
			int height[4];

			int time = g_tick / 48;

			for(int j = 0;j < 4;j++){
				Vec2 uv = {((int*)&pos[i])[axis.x] * (1 << j),((int*)&pos[i])[axis.y] * (1 << j)};
				
				int v00 = tHash(tHash(tHash(fixedToInt(uv.x)) ^ fixedToInt(uv.y)) ^ time) % FIXED_ONE;
				int v01 = tHash(tHash(tHash(fixedToInt(uv.x)) ^ fixedToInt(uv.y) + 1) ^ time) % FIXED_ONE;

				int v10 = tHash(tHash(tHash(fixedToInt(uv.x) + 1) ^ fixedToInt(uv.y)) ^ time) % FIXED_ONE;
				int v11 = tHash(tHash(tHash(fixedToInt(uv.x) + 1) ^ fixedToInt(uv.y) + 1) ^ time) % FIXED_ONE;

				int h1 = (bilinearScalar(uv,(int[]){v00,v01,v10,v11}) >> 1) - FIXED_ONE / 4;

				v00 = tHash(tHash(tHash(fixedToInt(uv.x)) ^ fixedToInt(uv.y)) ^ time + 1) % FIXED_ONE;
				v01 = tHash(tHash(tHash(fixedToInt(uv.x)) ^ fixedToInt(uv.y) + 1) ^ time + 1) % FIXED_ONE;

				v10 = tHash(tHash(tHash(fixedToInt(uv.x) + 1) ^ fixedToInt(uv.y)) ^ time + 1) % FIXED_ONE;
				v11 = tHash(tHash(tHash(fixedToInt(uv.x) + 1) ^ fixedToInt(uv.y) + 1) ^ time + 1) % FIXED_ONE;

				int h2 = (bilinearScalar(uv,(int[]){v00,v01,v10,v11}) >> 1) - FIXED_ONE / 4;

				height[j] = fixedMulR(tMix(h1,h2,g_tick % 0x30 * 1536),fixedMulR(voxel->animation,voxel->animation));
			}

			((int*)(pos + i))[side >> 1] += height[0] + height[1] / 2 + height[2] / 4 + height[3] / 8;
		}

		Vec3 quad_normal = triangleNormal(vec3ShlR(pos[3],2),vec3ShlR(pos[1],2),vec3ShlR(pos[0],2));
		Vec3 relative = vec3SubR(light_pos,g_position);
		Vec3 direction = vec3Refract(vec3Normalize(vec3ShrR(relative,8)),quad_normal,FIXED_ONE - FIXED_ONE / 4);
		Vec3 position = light_pos;
		((int*)&position)[side >> 1] -= side % 2 ? 0x10 : -0x10;

		Vec3 refraction = rayLuminance(position,direction);
		refraction = vec3ShlR(refraction,4);

		point_2[0] = pointToScreenRenderer(pos[0],g_tri,g_position,g_options.fov);
		point_2[1] = pointToScreenRenderer(pos[1],g_tri,g_position,g_options.fov);
		point_2[2] = pointToScreenRenderer(pos[2],g_tri,g_position,g_options.fov);
		point_2[3] = pointToScreenRenderer(pos[3],g_tri,g_position,g_options.fov);

		//Vec3 offset_refraction = vec3Normalize(vec3AddR(normal,vec3Normalize((Vec3){tCos(position.x + g_tick * 0x400),tCos(position.y),FIXED_ONE})));
		//refraction = vec3Mix(refraction,offset_refraction,voxel->animation);

		relative = vec3SubR(light_pos,g_position);
		direction = vec3Normalize(vec3Reflect(relative,quad_normal));
		position = light_pos;
		((int*)&position)[side >> 1] += side % 2 ? 0x10 : -0x10;
		if(tAbs(direction.x) < 4)
			direction.x = 4;
		if(tAbs(direction.y) < 4)
			direction.y = 4;
		if(tAbs(direction.z) < 4)
			direction.z = 4;
		Vec3 reflection = rayLuminance(position,direction);
		reflection = vec3ShlR(reflection,4);

		//Vec3 offset_reflection = vec3Normalize(vec3AddR(normal,vec3Normalize((Vec3){tCos(position.x + g_tick * 0x400),tCos(position.y),FIXED_ONE})));

		//reflection = vec3Mix(reflection,offset_reflection,voxel->animation);

		luminance = vec3Mix(reflection,refraction,tClamp(tAbs(vec3Dot(vec3Direction(g_position,light_pos),quad_normal)),0,FIXED_ONE));
	}
	for(int i = 0;i < 4;i++){
		if(!point_2[i].z)
			return;
	}
	vec3MulS(&luminance,g_exposure);

	Vec3 d_point[] = {
		{point_2[0].x,point_2[0].y,point_2[0].z},
		{point_2[1].x,point_2[1].y,point_2[1].z},
		{point_2[3].x,point_2[3].y,point_2[3].z},
		{point_2[2].x,point_2[2].y,point_2[2].z}
	};

	if(voxel_s->flags & VOXEL_EMITER){
		drawPolygon3d(&g_surface,d_point,luminance);
		return;
	}

	if(g_render_lines && voxel->type != VOXEL_MENU && g_surface.backend != RENDER_BACKEND_GL){
		drawLine(&g_surface,point_2[0].x,point_2[0].y,point_2[1].x,point_2[1].y,pixelColorToColor(0xFF00FF));
		drawLine(&g_surface,point_2[0].x,point_2[0].y,point_2[1].x,point_2[1].y,pixelColorToColor(0xFF00FF));
		drawLine(&g_surface,point_2[0].x,point_2[0].y,point_2[1].x,point_2[1].y,pixelColorToColor(0xFF00FF));
		drawLine(&g_surface,point_2[0].x,point_2[0].y,point_2[1].x,point_2[1].y,pixelColorToColor(0xFF00FF));
		return;
	}

	Texture* texture = voxel_s->side[side].custom ? voxel_s->side[side].texture : voxel_s->texture;

    if(texture){
		Vec2 texture_crd[4];
		if(voxel_s->flags & VOXEL_TEXTUREFILL){
			int size = FIXED_ONE >> depth;
			int texture_x = coord.x * size;
			int texture_y = coord.y * size;
			texture_crd[0] = (Vec2){texture_x,texture_y};
			texture_crd[1] = (Vec2){texture_x,texture_y + size};
			texture_crd[2] = (Vec2){texture_x + size,texture_y + size};
			texture_crd[3] = (Vec2){texture_x + size,texture_y};
		}
		else{
			int texture_x = ((int*)&block_pos)[axis.x] / 16 + (coord.x << (25 - depth - voxel->depth) - 4);
			int texture_y = ((int*)&block_pos)[axis.y] / 16 + (coord.y << (25 - depth - voxel->depth) - 4);
			fixedMul(&texture_x,voxel_s->texture_size);
			fixedMul(&texture_y,voxel_s->texture_size);
			int texture_size = fixedMulR(1 << (25 - depth - voxel->depth) - 4,voxel_s->texture_size);
			texture_crd[0] = (Vec2){texture_x,texture_y}; 
			texture_crd[1] = (Vec2){texture_x,texture_y + texture_size}; 
			texture_crd[2] = (Vec2){texture_x + texture_size,texture_y + texture_size}; 
			texture_crd[3] = (Vec2){texture_x + texture_size,texture_y}; 
		}
        drawTexturePolygon3d(&g_surface,texture,texture_crd,d_point,luminance,4);
	}
	else{
		if(g_surface.backend == RENDER_BACKEND_SOFTWARE){
			spanQuad3dLightingAdd(&g_surface,d_point,(Vec3[]){luminance,luminance,luminance,luminance});
		}
		else{
			drawPolygon3d(&g_surface,d_point,luminance);
		}
	}
}

#include "font.h"

static void drawConsoleLine(Voxel* voxel,int side,char* buffer,int buffer_index,int buffer_size,int offset_y){
    int draw_length = 0;
    int buffer_length = buffer_size - buffer_index;
    for(int i = 0;i < buffer_length;i++){
        if(draw_length > 0xE0000){
            buffer_length = i;
            break;
        }
        draw_length += g_vector_font[buffer[buffer_index + i]].width;
    }
    Vec2 uv = {0x1000,offset_y * 0x1000};
    String draw_string = {.data = buffer + buffer_index,.size = buffer_length};
    drawString3D(voxel,side,uv,draw_string,0x1000,0x400);
}

static void drawSide(Voxel* voxel,Vec3 block_pos,int side){
    unsigned polygon_id = tRnd();
	if(g_voxel_static[voxel->type].flags & VOXEL_TRANSLUCENT)
		drawSideRecursiveTraced(voxel,block_pos,side,(Vec2){0},0,surfaceAngle(block_pos,g_normal_table[side]));
	else
		drawSideRecursive(voxel,block_pos,side,(Vec2){0},0,surfaceAngle(block_pos,g_normal_table[side]));
    
    if(voxel->type == VOXEL_STRING){
        int size = 0x1000;
        drawString3D(voxel,side,(Vec2){0x1000,FIXED_ONE - 0x800},voxel->string,size,0x400);
    }
    if(voxel->type == VOXEL_CONSOLE){
        int offset_y = 2;
        char buffer[0x100];
        int buffer_index = countof(buffer) - 1;
        for(ConsoleContent* content = g_console_content;content;content = content->next){
            String string = {.data = content->string_data,.size = content->string_length};
            for(int i = content->string_length;i--;){
                if(content->string_data[i] == '\n'){
                    drawConsoleLine(voxel,side,buffer,buffer_index,countof(buffer),offset_y);
                    offset_y += 1;
                    buffer_index = countof(buffer) - 1;
                    if(offset_y == 17)
                        goto end_console;
                    continue;
                }
                buffer[buffer_index--] = content->string_data[i];
            }
        }
        drawConsoleLine(voxel,side,buffer,buffer_index,countof(buffer),offset_y);
    end_console:
        String input = {.data = g_console_buffer,.size = g_console_buffer_index};
        drawString3D(voxel,side,(Vec2){0x1000,0x1000},input,0x1000,0x400);
    }
    
    voxelGuiDraw(voxel,block_pos,side);
	
	if(voxel->type == VOXEL_CHEST && side != VEC3_Z * 2 && side != VEC3_Z * 2 + 1){
		if(voxel->chest_open){
			Vec3 color = vec3Mix(pixelColorToColor(0x404080),pixelColorToColor(0x408040),voxel->animation);
			int offset = (FIXED_ONE - voxel->animation);
			offset = tSqrt(offset);
			offset = offset / 8;
			drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2,FIXED_ONE / 3 + 0x5800 + offset},0x1000,colorToPixelColor(color));
		}
		else{		
			drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2,FIXED_ONE / 3 + 0x5800},0x1000,0x408040);
		}
	}
	if(voxel->type == VOXEL_BOSS && side == VEC3_Y * 2){
		Vec3 color = pixelColorToColor(0x408040);
		if(g_boss)
			color = vec3Mix(pixelColorToColor(0x404080),pixelColorToColor(0x408040),voxel->animation);

		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2,FIXED_ONE / 2},0x4000,0x202020);
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 - 0x1800,FIXED_ONE / 2 + 0x1400},0xC00,colorToPixelColor(color));
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 + 0x1800,FIXED_ONE / 2 + 0x1400},0xC00,colorToPixelColor(color));
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 - 0x1800,FIXED_ONE / 2 + 0x1400},0x600,0x202020);
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 + 0x1800,FIXED_ONE / 2 + 0x1400},0x600,0x202020);

		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 - 0x600,FIXED_ONE / 2 - 0x1C00},0x800,colorToPixelColor(color));
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2,FIXED_ONE / 2 - 0x1C00},0x800,colorToPixelColor(color));
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 + 0x600,FIXED_ONE / 2 - 0x1C00},0x800,colorToPixelColor(color));
	}
}

void octreeDraw(Voxel* voxel){
	int block_size = depthToSize(voxel->depth);
	Vec3 block_pos = voxelWorldPos(voxel);
	if(voxel->type == VOXEL_PARENT){
		Vec3 point[] = {
			{block_pos.x + 0,block_pos.y + 0,block_pos.z + 0},
			{block_pos.x + 0,block_pos.y + 0,block_pos.z + block_size},
			{block_pos.x + 0.,block_pos.y + block_size,block_pos.z + 0},
			{block_pos.x + 0,block_pos.y + block_size,block_pos.z + block_size},
			{block_pos.x + block_size,block_pos.y + 0,block_pos.z + 0},
			{block_pos.x + block_size,block_pos.y + 0,block_pos.z + block_size},
			{block_pos.x + block_size,block_pos.y + block_size,block_pos.z + 0},
			{block_pos.x + block_size,block_pos.y + block_size,block_pos.z + block_size},
		};
		if(!cubeInScreenSpace(point))
			return;
		if(sdVoxel(vec3ShrR(g_position,4),vec3ShrR(block_pos,4),block_size >> 4) > RENDER_DISTANCE)
			return;
		int order[][8] = {
			{0,1,2,4,3,5,6,7},
			{1,0,3,5,2,7,4,6},
			{2,0,3,6,1,4,7,5},
			{3,1,2,7,0,5,6,4},
			{4,5,6,0,7,1,2,3},
			{5,7,4,1,6,3,0,2},
			{6,4,7,2,5,0,3,1},
			{7,5,6,3,4,1,2,0},
		};
		Vec3 pos = voxelWorldPos(voxel);
		Vec3 rel_pos = vec3SubR(g_position,pos);
		Vec3 i_pos = (Vec3){rel_pos.x * 2 / block_size,rel_pos.y * 2 / block_size,rel_pos.z * 2 / block_size};
		int order_id = (i_pos.z <= 0) << 2 | (i_pos.y <= 0) << 1 | (i_pos.x <= 0) << 0;
		for(int i = 0;i < 8;i++){
			int index = order[order_id][i];
			octreeDraw(voxel->child_s[index]);
		}
		return;
	}
	switch(voxel->type){
		case VOXEL_AIR:{
			for(Entity* entity = voxel->entity_list;entity;entity = entity->next_voxel)
				entityDraw(entity);
		} return;
		case VOXEL_MOVABLE:{
			if(voxel->opened)
				block_pos.z -= fixedMulR(block_size,FIXED_ONE - voxel->animation);
			else
				block_pos.z -= fixedMulR(block_size,voxel->animation);
		} break;
		case VOXEL_WATER:{
			block_pos.z -= FIXED_ONE / 4;
			if(g_position.z - block_pos.z - block_size > 0)
				drawSide(voxel,vec3AddR(block_pos,(Vec3){0,0,block_size}),5);
		} return;
	}
	if(g_position.x - block_pos.x < 0)
		drawSide(voxel,block_pos,0);
	if(g_position.x - block_pos.x - block_size > 0)
		drawSide(voxel,vec3AddR(block_pos,(Vec3){block_size,0,0}),1);
	if(g_position.y - block_pos.y < 0)
		drawSide(voxel,block_pos,2);
	if(g_position.y - block_pos.y - block_size > 0)
		drawSide(voxel,vec3AddR(block_pos,(Vec3){0,block_size,0}),3);
	if(g_position.z - block_pos.z < 0)
		drawSide(voxel,block_pos,4);
	if(g_position.z - block_pos.z - block_size > 0)
		drawSide(voxel,vec3AddR(block_pos,(Vec3){0,0,block_size}),5);
}
