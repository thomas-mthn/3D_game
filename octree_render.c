#include "console.h"
#include "fixed.h"
#include "octree.h"
#include "octree_render.h"
#include "draw.h"
#include "vec2.h"
#include "main.h"
#include "lighting.h"
#include "voxel_gui.h"
#include "entity.h"
#include "span.h"
#include "draw_soft.h"

static void voxelModelRasterizeSide(DrawSurface* surface,Vec2 model_angle,Vec3* luminance,Voxel* voxel,Vec3 block_pos,int side,Vec3 camera_position,int camera_distance){
    unsigned polygon_id = tRnd();
	Vec2 axis = g_axis_table[side];

	int size = depthToSize(voxel->depth) >> 8;

	Vec3 pos[4] = {block_pos,block_pos,block_pos,block_pos};

	pos[1].a[axis.y] += size;
	pos[2].a[axis.x] += size;
	pos[3].a[axis.x] += size;
	pos[3].a[axis.y] += size;

	Vec3 point_2[4];
	
	int tri[] = {tCos(model_angle.x),tSin(model_angle.x),tCos(model_angle.y),tSin(model_angle.y)};

	point_2[0] = pointToScreenRenderer(pos[0],tri,camera_position,vec2MulS(g_options.fov,fixedMulR(camera_distance,FIXED_ONE * 2)));
	point_2[1] = pointToScreenRenderer(pos[1],tri,camera_position,vec2MulS(g_options.fov,fixedMulR(camera_distance,FIXED_ONE * 2)));
	point_2[2] = pointToScreenRenderer(pos[2],tri,camera_position,vec2MulS(g_options.fov,fixedMulR(camera_distance,FIXED_ONE * 2)));
	point_2[3] = pointToScreenRenderer(pos[3],tri,camera_position,vec2MulS(g_options.fov,fixedMulR(camera_distance,FIXED_ONE * 2)));

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
	Vec3 color = luminance ? vec3Shl(luminance[side],4) : vec3Single(FIXED_ONE << 4);

	if(voxel_s->texture){
		Vec2 texture_crd[4];
		if(voxel_s->texturefill){
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
		drawPolygon3d(surface,d_point,vec3Mul(color,voxel_s->color));
	}
}

void voxelModelRasterize(DrawSurface* surface,Vec2 model_angle,Vec3* luminance,Voxel* voxel,Vec3 camera_position,int camera_distance){
	int block_size = depthToSize(voxel->depth) >> 8;
	Vec3 block_pos = vec3Shr(voxelWorldPos(voxel),8);
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
		Vec3 pos = vec3Shr(voxelWorldPos(voxel),8);
		Vec3 rel_pos = vec3Sub(camera_position,pos);
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
		voxelModelRasterizeSide(surface,model_angle,luminance,voxel,vec3Add(block_pos,(Vec3){block_size,0,0}),1,camera_position,camera_distance);
	if(camera_position.y - block_pos.y < 0)
		voxelModelRasterizeSide(surface,model_angle,luminance,voxel,block_pos,2,camera_position,camera_distance);
	if(camera_position.y - block_pos.y - block_size > 0)
		voxelModelRasterizeSide(surface,model_angle,luminance,voxel,vec3Add(block_pos,(Vec3){0,block_size,0}),3,camera_position,camera_distance);
	if(camera_position.z - block_pos.z < 0)
		voxelModelRasterizeSide(surface,model_angle,luminance,voxel,block_pos,4,camera_position,camera_distance);
	if(camera_position.z - block_pos.z - block_size > 0)
		voxelModelRasterizeSide(surface,model_angle,luminance,voxel,vec3Add(block_pos,(Vec3){0,0,block_size}),5,camera_position,camera_distance);
}

static Vec3 triangleNormal(Vec3 a,Vec3 b,Vec3 c){
    Vec3 u = vec3Sub(b,a);
    Vec3 v = vec3Sub(c,a);
    return vec3Normalize(vec3Cross(u,v));
}

static DrawPrimitive* draw_list;

DrawPrimitive* primitiveToDraw(void){
    DrawPrimitive* polygon = memoryArenaAllocateZero(&g_arena_frame,sizeof *polygon);
    polygon->next = draw_list;
    draw_list = polygon;
    return polygon;
}

static void drawSlopeRecursive(Voxel* voxel,Vec3 block_pos,Vec3 u,Vec3 v,Vec2 coord,int depth){
    Vec3 normal = vec3Cross(u,v);
    
	Vec3 block_pos_t = block_pos;

    int size_u = depthToSize(voxel->depth) >> depth;
    int size_v = fixedMulR(depthToSize(voxel->depth),tSqrt(FIXED_ONE * 2)) >> depth;

    block_pos_t = vec3Add(block_pos_t,vec3MulS(u,fixedMulR(coord.x << FIXED_PRECISION,size_u)));
    block_pos_t = vec3Add(block_pos_t,vec3MulS(v,fixedMulR(coord.y << FIXED_PRECISION,size_v)));

	Vec3 pos[4] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

    pos[1] = vec3Add(pos[1],vec3MulS(v,size_v));
    pos[2] = vec3Add(pos[2],vec3MulS(u,size_u));
    pos[3] = vec3Add(pos[3],vec3MulS(u,size_u));
    pos[3] = vec3Add(pos[3],vec3MulS(v,size_v));

	Vec3 voxel_pos = voxelWorldPos(voxel);

	Vec3 square_pos = pos[0];
    square_pos = vec3Add(square_pos,vec3MulS(u,size_u));
    square_pos = vec3Add(square_pos,vec3MulS(v,size_v));

	Vec3 point[4] = {pointToScreen(pos[0]),pointToScreen(pos[1]),pointToScreen(pos[2]),pointToScreen(pos[3])};

	int distance_max = 0;
	int distance_max_index;

	for(int i = 0;i < 4;i++){
		int distance = vec3Distance(vec3Shr(g_surface.position,4),vec3Shr(pos[i],4));
		if(distance > distance_max){
			distance_max = distance;
			distance_max_index = i;
		}
	}
    
	int mipmap = mipmapGet(squarePointClosestPosition(pos[0],size_u,normal),normal,distance_max,FIXED_ONE);

	int split = 25 + -tClamp(mipmap,26 - LUXEL_MAX_MIPMAP,31) - voxel->depth;

	VoxelStatic* voxel_s = g_voxel_static + voxel->type;
    if(depth < split && !voxel_s->emiter){
#if 0
        if(sdSquare(vec3Shr(g_surface.position,4),vec3Shr(block_pos_t,4),size >> 4,side) > RENDER_DISTANCE)
            return;
#endif
        if(!squareInScreenSpace(pos))
            return;
#if 0
        if(occlusionBufferHidden(&g_surface,pos))
            return;
#endif   
        coord.x <<= 1;
        coord.y <<= 1;
        
        drawSlopeRecursive(voxel,block_pos,u,v,vec2Add(coord,(Vec2){0,0}),depth + 1);
        drawSlopeRecursive(voxel,block_pos,u,v,vec2Add(coord,(Vec2){0,1}),depth + 1);
        drawSlopeRecursive(voxel,block_pos,u,v,vec2Add(coord,(Vec2){1,0}),depth + 1);
        drawSlopeRecursive(voxel,block_pos,u,v,vec2Add(coord,(Vec2){1,1}),depth + 1);
        
        return;
    }
    Vec3 light_pos = pos[0];

    Vec3 luxel_pos[] = {vec3Shr(pos[0],mipmap),vec3Shr(pos[1],mipmap),vec3Shr(pos[2],mipmap),vec3Shr(pos[3],mipmap)};
	unsigned hash = luxelHashGet(luxel_pos[0],mipmap);
	Luxel* luxel = luxelGet(hash);
    
    Texture* texture = voxel_s->texture;
    Vec2 texture_crd[4];
    Vec3 luxel_colors[4] = {COLOR_WHITE,COLOR_WHITE,COLOR_WHITE,COLOR_WHITE};
    int c_index[] = {0,1,3,2};
	for(int i = 0;i < 4;i++){
		int x = i / 2;
		int y = i % 2;
		int index = c_index[i];
		if(!g_options.lighting_engine){
            luxel_colors[index] = texture ? vec3Single(FIXED_ONE << 4) : vec3Shl(voxel_s->color,4);
            continue;
		}
		Vec3 position = luxel_pos[i];

		luxel_colors[index] = luxelVoxelGet2(position,mipmap);

		unsigned dynamic_hash = luxelHashGet(position,mipmap);
		Luxel* luxel_dynamic = luxelDynamicGet(dynamic_hash);

		if(luxel_dynamic->hash == dynamic_hash)
			luxel_colors[index] = vec3Add(luxel_colors[index],luxel_dynamic->luminance);
		
		if(!voxel_s->texture)
			luxel_colors[index] = vec3Mul(luxel_colors[index],voxel_s->color);
		
		int f_x = tClamp((point[index].x + FIXED_ONE) * 16 / FIXED_ONE,0,31);
		int f_y = tClamp((point[index].y + FIXED_ONE) * 16 / FIXED_ONE,0,31);
		
		int distance = bitScanReverse(vec3Distance(vec3Shr(g_surface.position,4),vec3Shr(pos[index],4)) >> 4);
	}

    if(voxel_s->texturefill){
        int texture_size = FIXED_ONE >> depth;
        int texture_x = coord.x * texture_size;
        int texture_y = coord.y * texture_size;
        texture_crd[0] = (Vec2){texture_x,texture_y};
        texture_crd[1] = (Vec2){texture_x,texture_y + texture_size};
        texture_crd[2] = (Vec2){texture_x + texture_size,texture_y + texture_size};
        texture_crd[3] = (Vec2){texture_x + texture_size,texture_y};
    }
    else{
        int texture_x = vec3Dot(block_pos,u) / 16 + (coord.x << (25 - depth - voxel->depth) - 4);
        int texture_y = vec3Dot(block_pos,v) / 16 + (coord.y << (25 - depth - voxel->depth) - 4);
        fixedMul(&texture_x,voxel_s->texture_size);
        fixedMul(&texture_y,voxel_s->texture_size);
        int texture_size = fixedMulR(1 << (25 - depth - voxel->depth) - 4,voxel_s->texture_size);
        texture_crd[0] = (Vec2){texture_x,texture_y}; 
        texture_crd[1] = (Vec2){texture_x,texture_y + texture_size}; 
        texture_crd[2] = (Vec2){texture_x + texture_size,texture_y + texture_size}; 
        texture_crd[3] = (Vec2){texture_x + texture_size,texture_y}; 
    }
    DrawPrimitive* polygon = primitiveToDraw();
    for(int i = 4;i--;){
        polygon->luxel_colors[i] = luxel_colors[i];
        polygon->texture_crd[i] = texture_crd[i];
        polygon->position[i] = pos[i];
    }
    polygon->has_lighting = true;
    polygon->texture = texture;
}

structure(DrawSideFlags){
    bool triangle : 1;
    bool flip_x : 1;
    bool flip_y : 1;
};

static void drawSideRecursive(Voxel* voxel,Vec3 block_pos,int side,Vec2 coord,int depth,int surface_angle,Vec2 size,DrawSideFlags flags){
    Vec2 axis = g_axis_table[side];
	Vec3 block_pos_t = block_pos;
	block_pos_t.a[axis.x] += fixedMulR(coord.x << FIXED_PRECISION,size.x);
	block_pos_t.a[axis.y] += fixedMulR(coord.y << FIXED_PRECISION,size.y);

	Vec3 pos[4] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

	pos[1].a[axis.y] += size.y;
	pos[2].a[axis.x] += size.x;
	pos[3].a[axis.x] += size.x;
	pos[3].a[axis.y] += size.y;

	Vec3 voxel_pos = voxelWorldPos(voxel);

	Vec3 square_pos = pos[0];
	square_pos.a[axis.x] += size.x / 2;
	square_pos.a[axis.y] += size.y / 2;

	Vec3 point[4] = {pointToScreen(pos[0]),pointToScreen(pos[1]),pointToScreen(pos[2]),pointToScreen(pos[3])};

	int distance_max = 0;
	int distance_max_index;

	for(int i = 0;i < 4;i++){
		int distance = vec3Distance(vec3Shr(g_surface.position,4),vec3Shr(pos[i],4));
		if(distance > distance_max){
			distance_max = distance;
			distance_max_index = i;
		}
	}

	Vec3 normal = g_normal_table[side];
	int mipmap = mipmapGet(squarePointClosestPosition(pos[0],size.x,normal),normal,distance_max,surface_angle);

	int split = 25 + -tClamp(mipmap,26 - LUXEL_MAX_MIPMAP,31) - voxel->depth;

	VoxelStatic* voxel_s = g_voxel_static + voxel->type;
    
    if(voxel_s->rd_trace)
        split += 1;
    else if(g_surface.backend == RENDER_BACKEND_SOFTWARE)
        split -= 2;

    LightmapTree* lightmap = 0;

    if(depth < split && !voxel_s->emiter){
        Vec3 v_pos = vec3Shl((Vec3){voxel->position_x,voxel->position_y,voxel->position_z},depth);
        v_pos.a[axis.x] += coord.x;
        v_pos.a[axis.y] += coord.y;
        if(side & 1)
            v_pos.a[side >> 1] += (1 << depth) - 1;
		
        if(
           !voxel->opened &&
           !voxel->animation &&
           !squareVisible(v_pos,voxel->depth + depth,side,voxel->type) &&
           !(voxel_s->translucent)
           )
            return;
		
        if(sdSquare(vec3Shr(g_surface.position,4),vec3Shr(block_pos_t,4),size.x >> 4,side) > RENDER_DISTANCE)
            return;

        if(!squareInScreenSpace(pos))
            return;
#if 0
        if(occlusionBufferHidden(&g_surface,pos))
            return;
#endif   
        coord.x <<= 1;
        coord.y <<= 1;
        
        drawSideRecursive(voxel,block_pos,side,vec2Add(coord,(Vec2){0,0}),depth + 1,surface_angle,vec2Shr(size,1),flags);
        drawSideRecursive(voxel,block_pos,side,vec2Add(coord,(Vec2){0,1}),depth + 1,surface_angle,vec2Shr(size,1),flags);
        drawSideRecursive(voxel,block_pos,side,vec2Add(coord,(Vec2){1,0}),depth + 1,surface_angle,vec2Shr(size,1),flags);
        drawSideRecursive(voxel,block_pos,side,vec2Add(coord,(Vec2){1,1}),depth + 1,surface_angle,vec2Shr(size,1),flags);
        
        return;
    }
    if(flags.triangle){
        int tx = flags.flip_x ? (1 << depth) - coord.x - 1 : coord.x;
        int ty = flags.flip_y ? (1 << depth) - coord.y - 1 : coord.y;
        if((1 << depth) + ty - tx > (1 << depth))
            return;
    }
    //more aggressive pruning for software because drawing is more expensive
    if(g_surface.backend == RENDER_BACKEND_SOFTWARE){
        Vec3 v_pos = vec3Shl((Vec3){voxel->position_x,voxel->position_y,voxel->position_z},depth);
		v_pos.a[axis.x] += coord.x;
		v_pos.a[axis.y] += coord.y;
		if(side & 1)
			v_pos.a[side >> 1] += (1 << depth) - 1;
        		if(
           !voxel->opened &&
           !voxel->animation &&
           !squareVisible(v_pos,voxel->depth + depth,side,voxel->type) &&
           !(voxel_s->translucent)
        )
			return;
		
		if(sdSquare(vec3Shr(g_surface.position,4),vec3Shr(block_pos_t,4),size.x >> 4,side) > RENDER_DISTANCE)
			return;

		if(!squareInScreenSpace(pos))
			return;
        
        if(g_options.lighting_engine){
            lightmap = memoryArenaAllocateZero(&g_arena_frame,sizeof(*lightmap));
            lightmapTreeGenerate(lightmap,voxel,block_pos,side,coord,depth,surface_angle,size);
        }
    }
	for(int i = 0;i < 4;i++){
		if(pos[i].a[axis.x] < voxel_pos.a[axis.x])
			pos[i].a[axis.x] = voxel_pos.a[axis.x];
		if(pos[i].a[axis.y] < voxel_pos.a[axis.y])
			pos[i].a[axis.y] = voxel_pos.a[axis.y];
	}

	Vec3 point_2[4];
    
	point_2[0] = pointToScreenRenderer(pos[0],g_surface.rotation_matrix,g_surface.position,g_options.fov);
	point_2[1] = pointToScreenRenderer(pos[1],g_surface.rotation_matrix,g_surface.position,g_options.fov);
	point_2[2] = pointToScreenRenderer(pos[2],g_surface.rotation_matrix,g_surface.position,g_options.fov);
	point_2[3] = pointToScreenRenderer(pos[3],g_surface.rotation_matrix,g_surface.position,g_options.fov);

	mipmap = tClamp(mipmapGet(squarePointClosestPosition(pos[0],size.x,normal),normal,distance_max,surface_angle),26 - LUXEL_MAX_MIPMAP,31);

	Vec3 light_pos = pos[0];
    light_pos.a[axis.x] += size.x / 2;
    light_pos.a[axis.y] += size.y / 2;

	Vec3 luxel_pos = vec3Shr(light_pos,mipmap);
	unsigned hash = luxelHashGet(luxel_pos,mipmap);
	Luxel* luxel = luxelGet(hash);

	Vec3 luminance;
	if(voxel_s->emiter){
		luminance = voxel_s->side[side].custom ? voxel_s->side[side].color : voxel_s->color;
	}
	else if(g_options.lighting_engine && g_options.smooth_lighting){
		luminance = luxel->luminance;
		if(!voxel_s->texture)
			luminance = vec3Mul(luminance,voxel_s->side[side].custom ? voxel_s->side[side].color : voxel_s->color);
	}
    else{
		if(!voxel_s->texture)
			luminance = vec3Shl(voxel_s->side[side].custom ? voxel_s->side[side].color : voxel_s->color,4);
        else
            luminance = vec3Single(FIXED_ONE << 4);
    }

	luminance = vec3MulS(luminance,g_exposure);

	Vec3 d_point[] = {
		{point_2[0].x,point_2[0].y,point_2[0].z},
		{point_2[1].x,point_2[1].y,point_2[1].z},
		{point_2[3].x,point_2[3].y,point_2[3].z},
		{point_2[2].x,point_2[2].y,point_2[2].z}
	};

	if(voxel_s->emiter){
#if 0
        drawPolygon3d(&g_surface,pos,luminance);
#endif
        DrawPrimitive* polygon = primitiveToDraw();
        for(int i = countof(pos);i--;)
            polygon->position[i] = pos[i];
        polygon->luminance = luminance;
        return;
	}

	Vec3 luxel_positions[] = {
		luxel_pos,
		luxel_pos,
		luxel_pos,
		luxel_pos,
	};

    luxel_positions[1].a[axis.y] += 1;
    luxel_positions[2].a[axis.x] += 1;
    luxel_positions[3].a[axis.x] += 1;
    luxel_positions[3].a[axis.y] += 1;

	unsigned luxel_hashes[] = {
		0,
		luxelHashGet(luxel_positions[1],mipmap),
		luxelHashGet(luxel_positions[2],mipmap),
		luxelHashGet(luxel_positions[3],mipmap),
	};

	Vec3 luxel_colors[4];
    Texture* texture = voxel_s->side[side].custom ? voxel_s->side[side].texture : voxel_s->texture;
	int c_index[] = {0,1,3,2};
	for(int i = 0;i < 4;i++){
		int x = i / 2;
		int y = i % 2;
		int index = c_index[i];
		if(!g_options.lighting_engine){
            luxel_colors[index] = texture ? vec3Single(FIXED_ONE << 4) : vec3Shl(voxel_s->color,4);
            continue;
		}
		Vec3 position = luxel_pos;
	    position.a[axis.x] += x;
	    position.a[axis.y] += y;

		luxel_colors[index] = luxelVoxelGet(position,mipmap,axis);

		unsigned dynamic_hash = luxelHashGet(position,mipmap);
		Luxel* luxel_dynamic = luxelDynamicGet(dynamic_hash);

		if(luxel_dynamic->hash == dynamic_hash)
			luxel_colors[index] = vec3Add(luxel_colors[index],luxel_dynamic->luminance);
		
		if(!voxel_s->texture)
			luxel_colors[index] = vec3Mul(luxel_colors[index],voxel_s->color);
		
		int f_x = tClamp((point[index].x + FIXED_ONE) * 16 / FIXED_ONE,0,31);
		int f_y = tClamp((point[index].y + FIXED_ONE) * 16 / FIXED_ONE,0,31);
		
		int distance = bitScanReverse(vec3Distance(vec3Shr(g_surface.position,4),vec3Shr(pos[index],4)) >> 4);
	}

	for(int i = 0;i < 4;i++)
		luxel_colors[i] = vec3MulS(luxel_colors[i],g_exposure);
		
	//luminance = vec3ShrR(vec3AddR(vec3AddR(luxel_colors[0],luxel_colors[1]),vec3AddR(luxel_colors[2],luxel_colors[3])),2);

	if(g_options.gl_wireframe && voxel->type != VOXEL_MENU && g_surface.backend != RENDER_BACKEND_GL){
		drawLine(&g_surface,point_2[0].x,point_2[0].y,point_2[1].x,point_2[1].y,pixelColorToColor(0xFF00FF));
		drawLine(&g_surface,point_2[0].x,point_2[0].y,point_2[1].x,point_2[1].y,pixelColorToColor(0xFF00FF));
		drawLine(&g_surface,point_2[0].x,point_2[0].y,point_2[1].x,point_2[1].y,pixelColorToColor(0xFF00FF));
        drawLine(&g_surface,point_2[0].x,point_2[0].y,point_2[1].x,point_2[1].y,pixelColorToColor(0xFF00FF));
		return;
	}

    switch(voxel->type){
        case VOXEL_GLASS:{
            Vec3 relative = vec3Sub(light_pos,g_surface.position);
            Vec3 offset = vec3Direction(vec3Shr(g_surface.position,4),vec3Shr(light_pos,4));
            Vec3 position = light_pos;
            position.a[side >> 1] -= side % 2 ? 0x10 : -0x10;
            luminance = rayLuminance(position,offset);
            luminance = vec3Shl(luminance,4);
        } break;
        case VOXEL_MIRROR:{
            Vec3 relative = vec3Sub(light_pos,g_surface.position);
            Vec3 direction = vec3Normalize(vec3Reflect(vec3Shr(relative,8),normal));
            Vec3 position = light_pos;
            position.a[side >> 1] += side % 2 ? 0x10 : -0x10;
            if(tAbs(direction.x) < 4)
                direction.x = 4;
            if(tAbs(direction.y) < 4)
                direction.y = 4;
            if(tAbs(direction.z) < 4)
                direction.z = 4;
            luminance = rayLuminance(position,direction);
            luminance = vec3Shl(luminance,4);
                        DrawPrimitive* primitive = primitiveToDraw();
            primitive->has_lighting = true;
            primitive->luminance = luminance;
            for(int i = 4;i--;)
                primitive->position[i] = pos[i];  
        } return;
        case VOXEL_WATER:{
            for(int i = 0;i < countof(pos);i++){
                if(vec2Distance(voxel->splash_position,(Vec2){pos[i].x,pos[i].y}) > (g_time.tick - voxel->splash_tick) << 11)
                    continue;
                int height[4];

                int time = g_time.tick / 48;

                for(int j = 0;j < 4;j++){
                    Vec2 uv = {pos[i].a[axis.x] * (1 << j),pos[i].a[axis.y] * (1 << j)};
				
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

                    height[j] = fixedMulR(tMix(h1,h2,g_time.tick % 0x30 * 1536),fixedMulR(voxel->animation,voxel->animation));
                }

                pos[i].a[side >> 1] += height[0] + height[1] / 2 + height[2] / 4 + height[3] / 8;
            }

            Vec3 quad_normal = triangleNormal(vec3Shl(pos[3],2),vec3Shl(pos[1],2),vec3Shl(pos[0],2));
            Vec3 relative = vec3Sub(light_pos,g_surface.position);
            Vec3 direction = vec3Refract(vec3Normalize(vec3Shr(relative,8)),quad_normal,FIXED_ONE - FIXED_ONE / 4);
            Vec3 position = light_pos;
            position.a[side >> 1] -= side % 2 ? 0x10 : -0x10;

            Vec3 refraction = rayLuminance(position,direction);
            refraction = vec3Shl(refraction,4);

            relative = vec3Sub(light_pos,g_surface.position);
            direction = vec3Normalize(vec3Reflect(relative,quad_normal));
            position = light_pos;
            position.a[side >> 1] += side % 2 ? 0x10 : -0x10;
            if(tAbs(direction.x) < 4)
                direction.x = 4;
            if(tAbs(direction.y) < 4)
                direction.y = 4;
            if(tAbs(direction.z) < 4)
                direction.z = 4;
            Vec3 reflection = rayLuminance(position,direction);
            reflection = vec3Shl(reflection,4);

            luminance = vec3Mix(reflection,refraction,tClamp(tAbs(vec3Dot(vec3Direction(g_surface.position,light_pos),quad_normal)),0,FIXED_ONE));
            DrawPrimitive* primitive = primitiveToDraw();
            primitive->has_lighting = true;
            primitive->luminance = luminance;
            for(int i = 4;i--;)
                primitive->position[i] = pos[i];     
        } return;
        case VOXEL_DOOR:{
            Vec2 texture_crd[4];
            Vec2 scale = {
                (size.x << 8) / (depthToSize(voxel->depth) >> 8),
                (size.y << 8) / (depthToSize(voxel->depth) >> 8),
            };
            int texture_x = coord.x * scale.x;
            int texture_y = coord.y * scale.y;
            texture_crd[0] = (Vec2){texture_x,texture_y};
            texture_crd[1] = (Vec2){texture_x,texture_y + scale.y};
            texture_crd[2] = (Vec2){texture_x + scale.x,texture_y + scale.y};
            texture_crd[3] = (Vec2){texture_x + scale.x,texture_y};

            DrawPrimitive* polygon = primitiveToDraw();
            for(int i = 4;i--;){
                polygon->luxel_colors[i] = luxel_colors[i];
                polygon->texture_crd[i] = texture_crd[i];
                polygon->position[i] = pos[i];
            }
            polygon->has_lighting = true;
            polygon->texture = texture;
        } return;
    }
    if(g_options.textures && texture){
        Vec2 texture_crd[4];
        if(voxel_s->texturefill){
            int size = FIXED_ONE >> depth;
            int texture_x = coord.x * size;
            int texture_y = coord.y * size;
            texture_crd[0] = (Vec2){texture_x,texture_y};
            texture_crd[1] = (Vec2){texture_x,texture_y + size};
            texture_crd[2] = (Vec2){texture_x + size,texture_y + size};
            texture_crd[3] = (Vec2){texture_x + size,texture_y};
        }
        else{
            int texture_x = block_pos.a[axis.x] / 16 + (coord.x << (25 - depth - voxel->depth) - 4);
            int texture_y = block_pos.a[axis.y] / 16 + (coord.y << (25 - depth - voxel->depth) - 4);
            fixedMul(&texture_x,voxel_s->texture_size);
            fixedMul(&texture_y,voxel_s->texture_size);
            int texture_size = fixedMulR(1 << (25 - depth - voxel->depth) - 4,voxel_s->texture_size);
            texture_crd[0] = (Vec2){texture_x,texture_y}; 
            texture_crd[1] = (Vec2){texture_x,texture_y + texture_size}; 
            texture_crd[2] = (Vec2){texture_x + texture_size,texture_y + texture_size}; 
            texture_crd[3] = (Vec2){texture_x + texture_size,texture_y}; 
        }
        DrawPrimitive* polygon = primitiveToDraw();
        int tx = flags.flip_x ? (1 << depth) - coord.x - 1 : coord.x;
        int ty = flags.flip_y ? (1 << depth) - coord.y - 1 : coord.y;
        if(flags.triangle && (1 << depth) + ty - tx > (1 << depth) - 1){
            int i_table[][3] = {{0,2,3},{0,1,2},{1,2,3},{0,1,3}};
            for(int i = 3;i--;){
                polygon->luxel_colors[i] = luxel_colors[i_table[flags.flip_x | flags.flip_y << 1][i]];
                polygon->texture_crd[i] = texture_crd[i_table[flags.flip_x | flags.flip_y << 1][i]];
                polygon->position[i] = pos[i_table[flags.flip_x | flags.flip_y << 1][i]];
            }
            polygon->type = PRIMITIVE_TRIANGLE;
        }
        else{
            for(int i = 4;i--;){
                polygon->luxel_colors[i] = luxel_colors[i];
                polygon->texture_crd[i] = texture_crd[i];
                polygon->position[i] = pos[i];
            }
        }        
        polygon->has_lighting = true;
        polygon->texture = texture;
    }
    else{
        DrawPrimitive* polygon = primitiveToDraw();
        for(int i = 4;i--;){
            polygon->luxel_colors[i] = luxel_colors[i];
            polygon->position[i] = pos[i];
        }
        polygon->luminance = luminance;
        polygon->has_lighting = true;
        polygon->smooth_lighting = g_options.smooth_lighting && g_options.lighting_engine;
    }
}

static void drawSide(Voxel* voxel,Vec3 block_pos,int side,Vec2 size,bool occlude){
    Vec3 pos[4] = {block_pos,block_pos,block_pos,block_pos};
    Vec2 axis = g_axis_table[side];
    pos[1].a[axis.y] += size.y;
    pos[2].a[axis.x] += size.x;
    pos[3].a[axis.x] += size.x;
    pos[3].a[axis.y] += size.y;
    /*
    if(occlusionBufferHidden(&g_surface,pos))
        return;
    */
    switch(voxel->type){
        case VOXEL_STRING:{
            int size = 0x1000;
            drawGuiString(voxel,side,(Vec2){0x1000,FIXED_ONE - 0x800},voxel->string,size,0x400);
        } break;
        case VOXEL_CONSOLE:{
            consoleVoxelDraw(voxel,side);
        } break;
        case VOXEL_PRESSURE_PLATE:{
            int color = voxel->animation ? 0x40C040 : 0xC04040; 
            drawGuiRectangle(voxel,g_axis_table[side],block_pos,vec2Single(FIXED_ONE / 4),vec2Single(FIXED_ONE / 2),color,side);
        } break;
    }
    
    voxelGuiDraw(voxel,block_pos,side);
	
	if(voxel->type == VOXEL_CHEST && side != VEC3_Z * 2 && side != VEC3_Z * 2 + 1){
		if(voxel->chest_open){
			Vec3 color = vec3Mix(pixelColorToColor(0x804040),pixelColorToColor(0x408040),voxel->animation);
			int offset = (FIXED_ONE - voxel->animation);
			offset = tSqrt(offset);
			offset = offset / 8;
			drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2,FIXED_ONE / 3 + 0x5800 + offset},0x1000,colorToPixelColor(color),side);
		}
		else{
			drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2,FIXED_ONE / 3 + 0x5800},0x1000,0x408040,side);
		}
	}
	if(voxel->type == VOXEL_BOSS && side == VEC3_Y * 2){
		Vec3 color = pixelColorToColor(0x408040);
		if(g_boss)
			color = vec3Mix(pixelColorToColor(0x404080),pixelColorToColor(0x408040),voxel->animation);

		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2,FIXED_ONE / 2},0x4000,0x202020,side);
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 - 0x1800,FIXED_ONE / 2 + 0x1400},0xC00,colorToPixelColor(color),side);
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 + 0x1800,FIXED_ONE / 2 + 0x1400},0xC00,colorToPixelColor(color),side);
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 - 0x1800,FIXED_ONE / 2 + 0x1400},0x600,0x202020,side);
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 + 0x1800,FIXED_ONE / 2 + 0x1400},0x600,0x202020,side);

		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 - 0x600,FIXED_ONE / 2 - 0x1C00},0x800,colorToPixelColor(color),side);
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2,FIXED_ONE / 2 - 0x1C00},0x800,colorToPixelColor(color),side);
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 + 0x600,FIXED_ONE / 2 - 0x1C00},0x800,colorToPixelColor(color),side);
	}

    drawSideRecursive(voxel,block_pos,side,(Vec2){0},0,surfaceAngle(block_pos,g_normal_table[side]),size,(DrawSideFlags){0});

    if(occlude)
        occlusionBufferFill(&g_surface,pos);
}

static void drawBox(Voxel* voxel,Vec3 block_pos,Vec3 size){
    int distance = vec3Distance(g_surface.position,vec3Add(block_pos,vec3Shr(size,1)));
        
    bool occlude = distance < depthToSize(voxel->depth) * 8;
        
	if(g_surface.position.x - block_pos.x < 0)
		drawSide(voxel,block_pos,0,(Vec2){size.y,size.z},occlude);
	if(g_surface.position.x - block_pos.x - size.x > 0)
		drawSide(voxel,vec3Add(block_pos,(Vec3){size.x,0,0}),1,(Vec2){size.y,size.z},occlude);
	if(g_surface.position.y - block_pos.y < 0)
		drawSide(voxel,block_pos,2,(Vec2){size.x,size.z},occlude);
	if(g_surface.position.y - block_pos.y - size.y > 0)
		drawSide(voxel,vec3Add(block_pos,(Vec3){0,size.y,0}),3,(Vec2){size.x,size.z},occlude);
	if(g_surface.position.z - block_pos.z < 0)
		drawSide(voxel,block_pos,4,(Vec2){size.y,size.z},occlude);
	if(g_surface.position.z - block_pos.z - size.z > 0)
		drawSide(voxel,vec3Add(block_pos,(Vec3){0,0,size.z}),5,(Vec2){size.y,size.z},occlude);
}

//horror function 
static void slopeDraw(Voxel* voxel,Vec3 block_pos,int block_size){
    Vec3Axis table[][3] = {{VEC3_Y,VEC3_Z},{VEC3_X,VEC3_Z},{VEC3_X,VEC3_Y}};

    VoxelStatic* voxel_s = g_voxel_static + voxel->type;
    
    Vec3Axis axis = voxel_s->slope_axis;
    bool flip_x = voxel_s->slope_flip_x;
    bool flip_y = voxel_s->slope_flip_y;

    Vec3 u = voxel_s->slope_u;
    Vec3 v = voxel_s->slope_v;
    
    Vec3 normal = vec3Cross(u,v);
    Vec3 position = block_pos;
    position = vec3Add(position,vec3MulS(voxel_s->slope_offset,block_size));
    
    
    Plane plane = {.normal = normal,vec3Dot(normal,position)};

    if(vec3Dot(g_surface.position,plane.normal) - plane.distance > 0)
        drawSlopeRecursive(voxel,position,u,v,(Vec2){0},0);

    int value = (flip_x ^ flip_y) ? g_surface.position.a[table[axis][0]] : g_surface.position.a[table[axis][0]] - block_size;
    if(value - block_pos.a[table[axis][0]] < 0 == (flip_x ^ flip_y)){
        Vec3 position = block_pos;
        position.a[table[axis][0]] += block_size  * !(flip_x ^ flip_y);
        drawSide(voxel,position,table[axis][0] << 1 | (flip_x ^ flip_y),(Vec2){block_size,block_size},false);
    }
    if(g_surface.position.a[axis] - block_pos.a[axis] - block_size > 0){
        Vec3 offset = {0};
        offset.a[axis] = block_size;
        drawSideRecursive(voxel,vec3Add(block_pos,offset),axis << 1 | 1,(Vec2){0},0,FIXED_ONE,(Vec2){block_size,block_size},(DrawSideFlags){.triangle = true,.flip_x = flip_x ^ flip_y,.flip_y = flip_y});
    }
    if(g_surface.position.a[axis] - block_pos.a[axis] < 0){
        drawSideRecursive(voxel,block_pos,axis << 1,(Vec2){0},0,FIXED_ONE,(Vec2){block_size,block_size},(DrawSideFlags){.triangle = true,.flip_x = flip_x ^ flip_y,.flip_y = flip_y});
    }

    value = flip_y ? g_surface.position.a[table[axis][1]] : g_surface.position.a[table[axis][1]] - block_size;
    if(value - block_pos.a[table[axis][1]] < 0 == !flip_y){
        Vec3 position = block_pos;
        position.a[table[axis][1]] += block_size  * flip_y;
        drawSide(voxel,position,table[axis][1] << 1 | flip_y,(Vec2){block_size,block_size},true);
    }
}

void octreeDraw(Voxel* voxel){
	int block_size = depthToSize(voxel->depth);
	Vec3 block_pos = voxelWorldPos(voxel);
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
    
    if(voxel->type == VOXEL_PARENT){
        if(!cubeInScreenSpace(point))
            return;
        
        if(sdVoxel(vec3Shr(g_surface.position,4),vec3Shr(block_pos,4),block_size >> 4) > RENDER_DISTANCE)
            return;

        bool visible = false;
        bool inside = true;

        Vec3 position_table[] = {
            block_pos,
            vec3Add(block_pos,(Vec3){block_size,0,0}),
            block_pos,
            vec3Add(block_pos,(Vec3){0,block_size,0}),
            block_pos,
            vec3Add(block_pos,(Vec3){0,0,block_size}),
        };

        bool backface[] = {
            g_surface.position.x - block_pos.x < 0,
            g_surface.position.x - block_pos.x - block_size > 0,
            g_surface.position.y - block_pos.y < 0,
            g_surface.position.y - block_pos.y - block_size > 0,
            g_surface.position.z - block_pos.z < 0,
            g_surface.position.z - block_pos.z - block_size > 0,
        };

        for(int i = countof(position_table);i--;){
            if(!backface[i])
                continue;
            inside = false;
            Vec3 position = position_table[i];
            Vec3 pos[4] = {position,position,position,position};
            Vec2 axis = g_axis_table[i];
            pos[1].a[axis.y] += block_size;
            pos[2].a[axis.x] += block_size;
            pos[3].a[axis.x] += block_size;
            pos[3].a[axis.y] += block_size;

            visible |= !occlusionBufferHidden(&g_surface,pos);
            if(visible)
                break;
        }
        
        if(!inside && !visible)
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
		Vec3 rel_pos = vec3Sub(g_surface.position,pos);
		Vec3 i_pos = (Vec3){rel_pos.x * 2 / block_size,rel_pos.y * 2 / block_size,rel_pos.z * 2 / block_size};
		int order_id = (i_pos.z <= 0) << 2 | (i_pos.y <= 0) << 1 | (i_pos.x <= 0) << 0;
		for(int i = 0;i < 8;i++){
			int index = order[order_id][7 - i];
			octreeDraw(voxel->child_s[index]);
		}
		return;
	}
    if(g_options.rd_octree_wireframe){
        int color_table[] = {
            0xFF0000,
            0x00FF00,
            0x0000FF,
            0xFF00FF,
            0x00FFFF,
            0xFFFF00,
        };
        int color = color_table[voxel->depth % countof(color_table)];
        boxQuadWireframeDraw(voxelWorldPos(voxel),vec3Single(depthToSize(voxel->depth)),color,true);
    }
    if(g_voxel_static[voxel->type].translucent){
        for(Entity* entity = voxel->entity_list;entity;entity = entity->next_voxel)
            entityDraw(entity);
    }
    if(g_voxel_static[voxel->type].slope){
        slopeDraw(voxel,block_pos,block_size);
    }
    else{
        switch(voxel->type){
            case VOXEL_AIR:
                return;
            case VOXEL_MOVABLE:{
                if(voxel->opened)
                    block_pos.z -= fixedMulR(block_size,FIXED_ONE - voxel->animation);
                else
                    block_pos.z -= fixedMulR(block_size,voxel->animation);
            } break;
            case VOXEL_DOOR:{
                int door_size = fixedMulR(block_size,FIXED_ONE - voxel->animation) / 2;
                if(voxel->opened)
                    door_size = block_size / 2 - door_size;
                int door_size_inv = block_size / 2 - door_size;
                int door_cove = fixedMulR(block_size,0x1000);
                Vec3 size = {block_size - door_cove * 2,door_size,block_size};
                if(g_surface.position.y < block_pos.y + block_size / 2){
                    drawBox(voxel,vec3Add(block_pos,(Vec3){door_cove,0,0}),size);
                    drawBox(voxel,(Vec3){block_pos.x + door_cove,block_pos.y + block_size / 2 + door_size_inv,block_pos.z},size);
                }
                else{
                    drawBox(voxel,(Vec3){block_pos.x + door_cove,block_pos.y + block_size / 2 + door_size_inv,block_pos.z},size);
                    drawBox(voxel,vec3Add(block_pos,(Vec3){door_cove,0,0}),size);
                }
            } break;
            case VOXEL_WATER:{
                block_pos.z -= FIXED_ONE / 4;
                if(g_surface.position.z - block_pos.z - block_size > 0)
                    drawSide(voxel,vec3Add(block_pos,(Vec3){0,0,block_size}),5,(Vec2){block_size,block_size},false);
            } break;
            default:{
                drawBox(voxel,block_pos,vec3Single(block_size));
            } break;
        }
    }
}

void octreeDrawList(void){
    for(DrawPrimitive* primitive = draw_list;primitive;primitive = primitive->next){
        switch(primitive->type){
            case PRIMITIVE_CIRCLE:{
                drawCircle3d(&g_surface,primitive->position,vec3MulS(primitive->luminance,g_exposure));
            } break;
            case PRIMITIVE_LINE:{
                drawLine(&g_surface,primitive->position[0].x,primitive->position[0].y,primitive->position[1].x,primitive->position[1].y,vec3MulS(primitive->luminance,g_exposure));
            } break;
            case PRIMITIVE_TRIANGLE:{
                if(primitive->has_lighting){
                    drawColoredTexturePolygon3d(
                        &g_surface,
                        primitive->texture,
                        primitive->texture_crd,
                        primitive->position,
                        primitive->luxel_colors,
                        primitive->lightmap,
                        3
                    );
                }
                else{
                    drawTexturePolygon3d(&g_surface,primitive->texture,primitive->texture_crd,primitive->position,COLOR_WHITE,3);
                }
            } break;
            case PRIMITIVE_QUAD:{
                if(primitive->texture){
                    if(primitive->is_sprite){
                        Vec3 luminance = vec3MulS(primitive->luminance,g_exposure);
                        if(primitive->has_lighting)
                            drawTexturePolygon(&g_surface,primitive->texture,primitive->texture_crd,primitive->position_sprite,luminance,4);
                        else
                            drawTexturePolygon(&g_surface,primitive->texture,primitive->texture_crd,primitive->position_sprite,COLOR_WHITE,4);
                    }
                    else{
                        if(primitive->has_lighting){
                            drawColoredTexturePolygon3d(
                                &g_surface,
                                primitive->texture,
                                primitive->texture_crd,
                                primitive->position,
                                primitive->luxel_colors,
                                primitive->lightmap,
                                4
                            );
                        }
                        else{
                            drawTexturePolygon3d(&g_surface,primitive->texture,primitive->texture_crd,primitive->position,COLOR_WHITE,4);
                        }
                    }
                }
                else if(primitive->has_lighting){
                    if(primitive->smooth_lighting)
                        drawColoredPolygon3d(&g_surface,primitive->position,primitive->luxel_colors,primitive->lightmap);
                    else
                        drawPolygon3d(&g_surface,primitive->position,primitive->luminance);
                }
                else{
                    if(primitive->is_sprite)
                        drawPolygon(&g_surface,primitive->position_sprite,4,primitive->luminance);
                    else
                        drawPolygon3d(&g_surface,primitive->position,primitive->luminance);
                }
            } break;
            case PRIMITIVE_ELLIPSIS:{
                Vec3 luminance = vec3MulS(primitive->luminance,g_exposure);
                drawEllipses(&g_surface,primitive->position_sprite[0].x,primitive->position_sprite[0].y,primitive->sprite_size.x,primitive->sprite_size.y,luminance);
            } break;
            case PRIMITIVE_FRAME:{
                Vec3 luminance = vec3MulS(primitive->luminance,g_exposure);
                drawFrame(&g_surface,primitive->position_sprite[0].x,primitive->position_sprite[0].y,primitive->sprite_size.x,primitive->sprite_size.y,luminance,primitive->thickness);
            } break;
        }
    }
    draw_list = 0;
}
