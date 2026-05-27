#include "octree_render.h"
#include "console.h"
#include "octree.h"
#include "draw.h"
#include "vec2.h"
#include "main.h"
#include "lighting.h"
#include "voxel_gui.h"
#include "entity.h"
#include "span.h"
#include "draw_soft.h"
#include "sprite_trace.h"

static void voxelModelRasterizeSide(DrawSurface* surface,Vec2 model_angle,Vec3* luminance,Voxel* voxel,Vec3 block_pos,int side,Vec3 camera_position,real camera_distance){
    unsigned polygon_id = tRnd();
	Vec2i axis = g_axis_table[side];

	real size = realShr(depthToSize(voxel->depth),8);

	Vec3 pos[4] = {block_pos,block_pos,block_pos,block_pos};

	pos[1].a[axis.y] += size;
	pos[2].a[axis.x] += size;
	pos[3].a[axis.x] += size;
	pos[3].a[axis.y] += size;

	Vec3 point_2[4];
	
	real tri[] = {tCos(model_angle.x),tSin(model_angle.x),tCos(model_angle.y),tSin(model_angle.y)};

	point_2[0] = pointToScreenRenderer(pos[0],tri,camera_position,vec2MulS(g_surface.fov,realMulR(camera_distance,FIXED_ONE * 2)));
	point_2[1] = pointToScreenRenderer(pos[1],tri,camera_position,vec2MulS(g_surface.fov,realMulR(camera_distance,FIXED_ONE * 2)));
	point_2[2] = pointToScreenRenderer(pos[2],tri,camera_position,vec2MulS(g_surface.fov,realMulR(camera_distance,FIXED_ONE * 2)));
	point_2[3] = pointToScreenRenderer(pos[3],tri,camera_position,vec2MulS(g_surface.fov,realMulR(camera_distance,FIXED_ONE * 2)));

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
	Vec3 color = luminance ? vec3Shl(luminance[side],4) : vec3Single(FIXED_ONE / 16);

	if(voxel_s->texture){
		Vec2 texture_crd[4];
		if(voxel_s->texturefill){
			real size = FIXED_ONE;
			int texture_x = 0;
			int texture_y = 0;
			texture_crd[0] = (Vec2){texture_x,texture_y};
			texture_crd[1] = (Vec2){texture_x,texture_y + size};
			texture_crd[2] = (Vec2){texture_x + size,texture_y + size};
			texture_crd[3] = (Vec2){texture_x + size,texture_y};
		}
		else{
			real texture_x = block_pos.a[axis.x] / 16;
			real texture_y = block_pos.a[axis.y] / 16;
			realMul(&texture_x,voxel_s->texture_size);
			realMul(&texture_y,voxel_s->texture_size);
			real texture_size = realMulR(1 << 16,voxel_s->texture_size);
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

void voxelModelRasterize(DrawSurface* surface,Vec2 model_angle,Vec3* luminance,Voxel* voxel,Vec3 camera_position,real camera_distance){
	real block_size = realShr(depthToSize(voxel->depth),8);
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

static void drawSlopeRecursive(Voxel* voxel,Vec3 block_pos,Vec3 u,Vec3 v,Vec2i coord,int depth){
    Vec3 normal = vec3Cross(u,v);
    
	Vec3 block_pos_t = block_pos;

    real size_u = realShr(depthToSize(voxel->depth),depth);
    real size_v = realShr(realMulR(depthToSize(voxel->depth),tSqrt(FIXED_ONE * 2)),depth);

    block_pos_t = vec3Add(block_pos_t,vec3MulS(u,realMulR(intToReal(coord.x),size_u)));
    block_pos_t = vec3Add(block_pos_t,vec3MulS(v,realMulR(intToReal(coord.y),size_v)));

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

	real distance_max = 0;
	int distance_max_index;

	for(int i = 0;i < 4;i++){
		int distance = vec3Distance(vec3Shr(g_surface.position,4),vec3Shr(pos[i],4));
		if(distance > distance_max){
			distance_max = distance;
			distance_max_index = i;
		}
	}
    
	int mipmap = mipmapGet(squarePointClosestPosition(pos[0],size_u,normal),normal,distance_max,FIXED_ONE);
    mipmap = tClamp(mipmap,0,31);

	int split = 25 + -mipmap - voxel->depth;

	VoxelStatic* voxel_s = g_voxel_static + voxel->type;
    if(depth > 8 && depth < split && !voxel_s->emiter){
#if 0
        if(sdSquareSquare(vec3Shr(g_surface.position,4),vec3Shr(block_pos_t,4),size >> 4,side) > RENDER_DISTANCE)
            return;
#endif
        if(!squareInScreenSpace(g_view_plane,pos))
            return;
#if 0
        if(occlusionBufferHidden(&g_surface,pos))
            return;
#endif   
        coord.x <<= 1;
        coord.y <<= 1;
        
        drawSlopeRecursive(voxel,block_pos,u,v,(Vec2i){coord.x + 0,coord.y + 0},depth + 1);
        drawSlopeRecursive(voxel,block_pos,u,v,(Vec2i){coord.x + 0,coord.y + 1},depth + 1);
        drawSlopeRecursive(voxel,block_pos,u,v,(Vec2i){coord.x + 1,coord.y + 0},depth + 1);
        drawSlopeRecursive(voxel,block_pos,u,v,(Vec2i){coord.x + 1,coord.y + 1},depth + 1);
        
        return;
    }
    Vec3 light_pos = pos[0];

    Vec3 luxel_pos[] = {vec3Shr(pos[0],mipmap),vec3Shr(pos[1],mipmap),vec3Shr(pos[2],mipmap),vec3Shr(pos[3],mipmap)};

    
    Texture* texture = voxel_s->texture;
    Vec2 texture_crd[4];
    Vec3 luxel_colors[4] = {COLOR_WHITE,COLOR_WHITE,COLOR_WHITE,COLOR_WHITE};
    int c_index[] = {0,1,3,2};
	for(int i = 0;i < 4;i++){
		int x = i / 2;
		int y = i % 2;
		int index = c_index[i];
		if(!g_options.lighting_engine){
            luxel_colors[index] = texture ? vec3Single(FIXED_ONE * 16) : vec3Shl(voxel_s->color,4);
            continue;
		}
		Vec3 position = luxel_pos[i];

		luxel_colors[index] = lightingPositionLuminanceGet(position,mipmap,normal);
#if 0
		unsigned dynamic_hash = luxelHashGet(position,mipmap);
		Luxel* luxel_dynamic = luxelDynamicGet(dynamic_hash);

		if(luxel_dynamic->hash == dynamic_hash)
			luxel_colors[index] = vec3Add(luxel_colors[index],luxel_dynamic->luminance);
#endif
		if(!voxel_s->texture)
			luxel_colors[index] = vec3Mul(luxel_colors[index],voxel_s->color);
		
		real f_x = tClamp((point[index].x + FIXED_ONE) * 16 / FIXED_ONE,0,31);
		real f_y = tClamp((point[index].y + FIXED_ONE) * 16 / FIXED_ONE,0,31);
		
		real distance = realShr(bitScanReverse(vec3Distance(vec3Shr(g_surface.position,4),vec3Shr(pos[index],4))),4);
	}

    if(voxel_s->texturefill){
        real texture_size = realShr(FIXED_ONE,depth);
        real texture_x = coord.x * texture_size;
        real texture_y = coord.y * texture_size;
        texture_crd[0] = (Vec2){texture_x,texture_y};
        texture_crd[1] = (Vec2){texture_x,texture_y + texture_size};
        texture_crd[2] = (Vec2){texture_x + texture_size,texture_y + texture_size};
        texture_crd[3] = (Vec2){texture_x + texture_size,texture_y};
    }
    else{
        real texture_x = vec3Dot(block_pos,u) / 16 + (coord.x << (25 - depth - voxel->depth) - 4);
        real texture_y = vec3Dot(block_pos,v) / 16 + (coord.y << (25 - depth - voxel->depth) - 4);
        realMul(&texture_x,voxel_s->texture_size);
        realMul(&texture_y,voxel_s->texture_size);
        real texture_size = realMulR(1 << (25 - depth - voxel->depth) - 4,voxel_s->texture_size);
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

static LightmapTree* trace_buffer;

static void drawSidePart(Voxel* voxel,Vec3 block_pos,Side side,Vec2 size,real distance_max,real surface_angle,Vec2i coord,int depth,DrawSideFlags flags){
    if(flags.triangle){
        real tx = flags.flip_x ? (1 << depth) - coord.x - 1 : coord.x;
        real ty = flags.flip_y ? (1 << depth) - coord.y - 1 : coord.y;
        if((1 << depth) + ty - tx > (1 << depth))
            return;
    }
    Vec2i axis = g_axis_table[side];
    Vec3 normal = g_normal_table[side];
    VoxelStatic* voxel_s = g_voxel_static + voxel->type;
    
    Vec3 point_2[4];

    Vec3 block_pos_t = block_pos;
	block_pos_t.a[axis.x] += realMulR(intToReal(coord.x),size.x);
	block_pos_t.a[axis.y] += realMulR(intToReal(coord.y),size.y);

	Vec3 pos[4] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

	pos[1].a[axis.y] += size.y;
	pos[2].a[axis.x] += size.x;
	pos[3].a[axis.x] += size.x;
	pos[3].a[axis.y] += size.y;

    
	point_2[0] = pointToScreenRenderer(pos[0],g_surface.rotation_matrix,g_surface.position,g_surface.fov);
	point_2[1] = pointToScreenRenderer(pos[1],g_surface.rotation_matrix,g_surface.position,g_surface.fov);
	point_2[2] = pointToScreenRenderer(pos[2],g_surface.rotation_matrix,g_surface.position,g_surface.fov);
	point_2[3] = pointToScreenRenderer(pos[3],g_surface.rotation_matrix,g_surface.position,g_surface.fov);

	int mipmap = mipmapGet(squarePointClosestPosition(pos[0],size.x,normal),normal,distance_max,surface_angle);

	Vec3 light_pos = pos[0];
    light_pos.a[axis.x] += size.x / 2;
    light_pos.a[axis.y] += size.y / 2;

	Vec3 luxel_pos = vec3Shr(light_pos,mipmap);
    
	Vec3 luminance;
	if(voxel_s->emiter){
		luminance = voxel_s->side[side].custom ? voxel_s->side[side].color : voxel_s->color;
	}
	else if(g_options.lighting_engine && g_options.smooth_lighting){
		luminance = lightingPositionLuminanceGet(luxel_pos,mipmap,normal);
		if(!voxel_s->texture)
			luminance = vec3Mul(luminance,voxel_s->side[side].custom ? voxel_s->side[side].color : voxel_s->color);
	}
    else{
		if(!voxel_s->texture)
			luminance = vec3Shl(voxel_s->side[side].custom ? voxel_s->side[side].color : voxel_s->color,4);
        else
            luminance = vec3Single(FIXED_ONE * 16);
    }

	luminance = vec3MulS(luminance,g_exposure);

	Vec3 d_point[] = {
		{point_2[0].x,point_2[0].y,point_2[0].z},
		{point_2[1].x,point_2[1].y,point_2[1].z},
		{point_2[3].x,point_2[3].y,point_2[3].z},
		{point_2[2].x,point_2[2].y,point_2[2].z}
	};

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

	Vec3 luxel_colors[4];
    Texture* texture = voxel_s->side[side].custom ? voxel_s->side[side].texture : voxel_s->texture;
	int c_index[] = {0,1,3,2};
	for(int i = 0;i < 4;i++){
		int x = i / 2;
		int y = i % 2;
		int index = c_index[i];
		if(!g_options.lighting_engine){
            luxel_colors[index] = texture ? vec3Single(FIXED_ONE * 16) : vec3Shl(voxel_s->color,4);
            continue;
		}
		Vec3 position = luxel_pos;
	    position.a[axis.x] += IS_FLOAT(real) ? x / 0x10000 : x;
	    position.a[axis.y] += IS_FLOAT(real) ? y / 0x10000 : y;

		luxel_colors[index] = lightingPositionLuminanceGet(position,mipmap,normal);
#if 0
		unsigned dynamic_hash = luxelHashGet(position,mipmap);
		Luxel* luxel_dynamic = luxelDynamicGet(dynamic_hash);

		if(luxel_dynamic->hash == dynamic_hash)
			luxel_colors[index] = vec3Add(luxel_colors[index],luxel_dynamic->luminance);
#endif
		if(!voxel_s->texture)
			luxel_colors[index] = vec3Mul(luxel_colors[index],voxel_s->color);
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
        case VOXEL_MIRROR: case VOXEL_GLASS: case VOXEL_WATER:{
            DrawPrimitive* primitive = primitiveToDraw();
            primitive->has_lighting = true;
            primitive->smooth_lighting = true;

            int l_table[] = {0,1,3,2};
            
            for(int i = 4;i--;){
                Vec2 uv = {
                    intToReal((coord.x + l_table[i] / 2)) / (1 << depth),
                    intToReal((coord.y + l_table[i] % 2)) / (1 << depth),
                };
                            
                primitive->luxel_colors[i] = lightmapGet(trace_buffer,uv);
                primitive->position[i] = pos[i];
            }
        } return;
        case VOXEL_DOOR:{
            Vec2 texture_crd[4];
            Vec2 scale = {
                realShl(size.x,8) / realShr(depthToSize(voxel->depth),8),
                realShl(size.y,8) / realShr(depthToSize(voxel->depth),8),
            };
            real texture_x = coord.x * scale.x;
            real texture_y = coord.y * scale.y;
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
            real size = realShr(FIXED_ONE,depth);
            real texture_x = coord.x * size;
            real texture_y = coord.y * size;
            texture_crd[0] = (Vec2){texture_x,texture_y};
            texture_crd[1] = (Vec2){texture_x,texture_y + size};
            texture_crd[2] = (Vec2){texture_x + size,texture_y + size};
            texture_crd[3] = (Vec2){texture_x + size,texture_y};
        }
        else{
            real texture_size = realDivR(FIXED_ONE * 0x10,1 << voxel->depth + depth);
            real texture_x = block_pos.a[axis.x] / 16 + texture_size * coord.x;
            real texture_y = block_pos.a[axis.y] / 16 + texture_size * coord.y;
            realMul(&texture_x,voxel_s->texture_size);
            realMul(&texture_y,voxel_s->texture_size);
            realMul(&texture_size,voxel_s->texture_size);
            texture_crd[0] = (Vec2){texture_x,texture_y}; 
            texture_crd[1] = (Vec2){texture_x,texture_y + texture_size}; 
            texture_crd[2] = (Vec2){texture_x + texture_size,texture_y + texture_size}; 
            texture_crd[3] = (Vec2){texture_x + texture_size,texture_y}; 
        }
        DrawPrimitive* polygon = primitiveToDraw();
        real tx = flags.flip_x ? (1 << depth) - coord.x - 1 : coord.x;
        real ty = flags.flip_y ? (1 << depth) - coord.y - 1 : coord.y;
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

static void drawSideRecursive(Voxel* voxel,Vec3 block_pos,int side,Vec2i coord,int depth,real surface_angle,Vec2 size,DrawSideFlags flags){
    Vec2i axis = g_axis_table[side];
	Vec3 block_pos_t = block_pos;
	block_pos_t.a[axis.x] += realMulR(intToReal(coord.x),size.x);
	block_pos_t.a[axis.y] += realMulR(intToReal(coord.y),size.y);

	Vec3 pos[4] = {block_pos_t,block_pos_t,block_pos_t,block_pos_t};

	pos[1].a[axis.y] += size.y;
	pos[2].a[axis.x] += size.x;
	pos[3].a[axis.x] += size.x;
	pos[3].a[axis.y] += size.y;

	Vec3 voxel_pos = voxelWorldPos(voxel);

	Vec3 point[4] = {pointToScreen(pos[0]),pointToScreen(pos[1]),pointToScreen(pos[2]),pointToScreen(pos[3])};

	real distance_max = 0;
	int distance_max_index;

	for(int i = 0;i < 4;i++){
		real distance = vec3Distance(vec3Shr(g_surface.position,4),vec3Shr(pos[i],4));
		if(distance > distance_max){
			distance_max = distance;
			distance_max_index = i;
		}
	}

	Vec3 normal = g_normal_table[side];
	int mipmap = mipmapGet(squarePointClosestPosition(pos[0],size.x,normal),normal,distance_max,surface_angle);

	int split = 25 + -mipmap - voxel->depth;

    split -= 1;
    
	VoxelStatic* voxel_s = g_voxel_static + voxel->type;
    
    if(voxel_s->rd_trace)
        split += 1;
    else if(g_surface.backend == RENDER_BACKEND_SOFTWARE)
        split -= 2;

    LightmapTree* lightmap = 0;

    Vec3i v_pos = {voxel->position_x << depth,voxel->position_y << depth,voxel->position_z << depth};
    v_pos.a[axis.x] += coord.x;
    v_pos.a[axis.y] += coord.y;
    if(side & 1)
        v_pos.a[side >> 1] += (1 << depth) - 1;
        
    if(!squareVisible(v_pos,voxel->depth + depth,side,voxel->type))
        return;
        
    if(sdSquareSquare(vec3Shr(g_surface.position,4),vec3Shr(block_pos_t,4),realShr(size.x,4),side) > RENDER_DISTANCE)
        return;

    if(!squareInScreenSpace(g_view_plane,pos))
        return;
#if 0
    if(occlusionBufferHidden(&g_surface,pos))
        return;
#endif   
    
    if(depth < split && !voxel_s->emiter){
        coord.x <<= 1;
        coord.y <<= 1;
        
        drawSideRecursive(voxel,block_pos,side,(Vec2i){coord.x + 0,coord.y + 0},depth + 1,surface_angle,vec2Shr(size,1),flags);
        drawSideRecursive(voxel,block_pos,side,(Vec2i){coord.x + 0,coord.y + 1},depth + 1,surface_angle,vec2Shr(size,1),flags);
        drawSideRecursive(voxel,block_pos,side,(Vec2i){coord.x + 1,coord.y + 0},depth + 1,surface_angle,vec2Shr(size,1),flags);
        drawSideRecursive(voxel,block_pos,side,(Vec2i){coord.x + 1,coord.y + 1},depth + 1,surface_angle,vec2Shr(size,1),flags);
        
        return;
    }
    if(flags.triangle){
        real tx = flags.flip_x ? (1 << depth) - coord.x - 1 : coord.x;
        real ty = flags.flip_y ? (1 << depth) - coord.y - 1 : coord.y;
        if((1 << depth) + ty - tx > (1 << depth))
            return;
    }
    //more aggressive pruning for software because drawing is more expensive
    if(g_surface.backend == RENDER_BACKEND_SOFTWARE){
        Vec3i v_pos = {voxel->position_x << depth,voxel->position_y << depth,voxel->position_z << depth};
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
		
        if(sdSquareSquare(vec3Shr(g_surface.position,4),vec3Shr(block_pos_t,4),realShr(size.x,4),side) > RENDER_DISTANCE)
            return;
                
		if(!squareInScreenSpace(g_view_plane,pos))
			return;
        
        if(g_options.lighting_engine){
            lightmap = memoryArenaAllocateZero(&g_arena_frame,sizeof *lightmap);
            lightmapTreeGenerate(lightmap,voxel,block_pos,side,coord,depth,surface_angle,size);
        }
    }
	for(int i = 0;i < 4;i++){
		if(pos[i].a[axis.x] < voxel_pos.a[axis.x])
			pos[i].a[axis.x] = voxel_pos.a[axis.x];
		if(pos[i].a[axis.y] < voxel_pos.a[axis.y])
			pos[i].a[axis.y] = voxel_pos.a[axis.y];
	}
    coord.x <<= 1;
    coord.y <<= 1;

    size = vec2Shr(size,1);
    
    drawSidePart(voxel,block_pos,side,size,distance_max,surface_angle,(Vec2i){coord.x + 0,coord.y + 0},depth + 1,flags);
    drawSidePart(voxel,block_pos,side,size,distance_max,surface_angle,(Vec2i){coord.x + 0,coord.y + 1},depth + 1,flags);
    drawSidePart(voxel,block_pos,side,size,distance_max,surface_angle,(Vec2i){coord.x + 1,coord.y + 0},depth + 1,flags);
    drawSidePart(voxel,block_pos,side,size,distance_max,surface_angle,(Vec2i){coord.x + 1,coord.y + 1},depth + 1,flags);
}

static void drawSide(Voxel* voxel,Vec3 block_pos,Side side,Vec2 size,bool occlude){
    Vec3 pos[4] = {block_pos,block_pos,block_pos,block_pos};
    Vec2i axis = g_axis_table[side];
    VoxelStatic* voxel_s = g_voxel_static + voxel->type;
    pos[1].a[axis.y] += size.y;
    pos[2].a[axis.x] += size.x;
    pos[3].a[axis.x] += size.x;
    pos[3].a[axis.y] += size.y;
#if 1
    
    if(voxel_s->rd_trace){
        trace_buffer = memoryArenaAllocateZero(&g_arena_frame,sizeof *trace_buffer);
        lightmapTreeGenerate(trace_buffer,voxel,block_pos,side,(Vec2i){0},0,surfaceAngle(block_pos,g_normal_table[side]),size);
    }
    /*
    if(occlusionBufferHidden(&g_surface,pos))
        return;
    */

    switch(voxel->type){
        case VOXEL_STRING:{
            real size = REAL_UNIT * 0x10;
            drawGuiString(voxel,side,(Vec2){REAL_UNIT * 0x10,FIXED_ONE - REAL_UNIT * 0x08},voxel->string,size,REAL_UNIT * 4,0xFFFFFF);
        } break;
        case VOXEL_CONSOLE:{
            consoleVoxelDraw(voxel,side);
        } break;
        case VOXEL_PRESSURE_PLATE:{
            int color = voxel->animation ? 0x40C040 : 0xC04040; 
            drawGuiRectangle(voxel,g_axis_table[side],block_pos,vec2Single(FIXED_ONE / 4),vec2Single(FIXED_ONE / 2),color,side);
        } break;
    }
    
    int n_gui = voxel_s->side[side].custom ? voxel_s->side[side].n_gui : voxel_s->n_gui;
    VoxelGuiElement* gui = voxel_s->side[side].custom ? voxel_s->side[side].gui : voxel_s->gui;
    voxelGuiDraw(voxel,block_pos,side,gui,n_gui);
    
    if(voxel == g_voxel_interact)
        voxelGuiDraw(voxel,block_pos,side,voxel_s->gui_interact,voxel_s->n_gui_interact);

	if(voxel->type == VOXEL_CHEST && side != VEC3_Z * 2 && side != VEC3_Z * 2 + 1){
		if(voxel->chest_open){
			Vec3 color = vec3Mix(pixelColorToColor(0x804040),pixelColorToColor(0x408040),voxel->animation);
			real offset = (FIXED_ONE - voxel->animation);
			offset = tSqrt(offset);
			offset = offset / 8;
			drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2,FIXED_ONE / 3 + REAL_UNIT * 0x58 + offset},REAL_UNIT * 0x10,colorToPixelColor(color),side);
		}
		else{
			drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2,FIXED_ONE / 3 + REAL_UNIT * 0x58},REAL_UNIT * 0x10,0x408040,side);
		}
	}

	if(voxel->type == VOXEL_BOSS && side == VEC3_Y * 2){
#if 0
		Vec3 color = pixelColorToColor(0x408040);
		if(g_boss)
			color = vec3Mix(pixelColorToColor(0x404080),pixelColorToColor(0x408040),voxel->animation);

		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2,FIXED_ONE / 2},FIXED_ONE / 8,0x202020,side);
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 - 0x1800,FIXED_ONE / 2 + 0x1400},0xC00,colorToPixelColor(color),side);
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 + 0x1800,FIXED_ONE / 2 + 0x1400},0xC00,colorToPixelColor(color),side);
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 - 0x1800,FIXED_ONE / 2 + 0x1400},0x600,0x202020,side);
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 + 0x1800,FIXED_ONE / 2 + 0x1400},0x600,0x202020,side);

		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 - 0x600,FIXED_ONE / 2 - 0x1C00},0x800,colorToPixelColor(color),side);
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2,FIXED_ONE / 2 - 0x1C00},0x800,colorToPixelColor(color),side);
		drawGuiCircle(voxel,g_axis_table[side],block_pos,(Vec2){FIXED_ONE / 2 + 0x600,FIXED_ONE / 2 - 0x1C00},0x800,colorToPixelColor(color),side);
#endif
	}
#endif
    if(voxel_s->emiter){
        DrawPrimitive* polygon = primitiveToDraw();
        for(int i = countof(pos);i--;)
            polygon->position[i] = pos[i];
        if(voxel->type == VOXEL_CUSTOM_EMIT)
            polygon->luminance = vec3MulS(voxel->color,intToReal(1 << voxel->emit_pow));
        else
            polygon->luminance = voxel_s->color;
        return;
	}
    if(g_options.gl_qlightmap && !g_voxel_static[voxel->type].rd_trace){
        Vec3i v_pos = {voxel->position_x,voxel->position_y,voxel->position_z};

        if(voxel->depth > 1 && !squareVisible(v_pos,voxel->depth,side,voxel->type))
            return;

        Vec2 texture_crd[4];
        DrawPrimitive* primitive = primitiveToDraw();
        Texture* texture = g_voxel_static[voxel->type].texture;
        if(voxel->type == VOXEL_CUSTOM && voxel->has_texture)
            texture = g_textures + voxel->texture_id;
        else
            primitive->procedural_texture = voxel->procedural_texture;
        
        if(voxel->type == VOXEL_STONE_BRICK){
            primitive->procedural_texture = PROCTEXT_BRICK;
        }
        else if(voxel->type == VOXEL_STONE){
            primitive->procedural_texture = PROCTEXT_VORONOI;
        }
        else if(texture){
            if(voxel_s->texturefill){
                texture_crd[0] = g_texture_coordinates_fill[0];
                texture_crd[1] = g_texture_coordinates_fill[1];
                texture_crd[2] = g_texture_coordinates_fill[2];
                texture_crd[3] = g_texture_coordinates_fill[3];
            }
            else{
                real texture_x = block_pos.a[axis.x] / 16;
                real texture_y = block_pos.a[axis.y] / 16;
                realMul(&texture_x,voxel_s->texture_size);
                realMul(&texture_y,voxel_s->texture_size);
                real texture_size = realMulR(realShr(FIXED_ONE,voxel->depth) * 16,voxel_s->texture_size);
                texture_crd[0] = (Vec2){texture_x,texture_y}; 
                texture_crd[1] = (Vec2){texture_x,texture_y + texture_size}; 
                texture_crd[2] = (Vec2){texture_x + texture_size,texture_y + texture_size}; 
                texture_crd[3] = (Vec2){texture_x + texture_size,texture_y}; 
            }
            primitive->texture = texture;
        }
        primitive->gpu_lightmap = true;
        primitive->side = side;
        primitive->normal = g_normal_table[side];

        if(voxel->type == VOXEL_CUSTOM)
            primitive->luminance = voxel->color;
        else if(texture)
             primitive->luminance = COLOR_WHITE;
        else
            primitive->luminance = voxel_s->color;
        
        for(int i = 4;i--;){
            primitive->position[i] = pos[i];
            if(texture)
                primitive->texture_crd[i] = texture_crd[i];
        }
    }
    else{
        drawSideRecursive(voxel,block_pos,side,(Vec2i){0},0,surfaceAngle(block_pos,g_normal_table[side]),size,(DrawSideFlags){0});
    }

    if(occlude)
        occlusionBufferFill(&g_surface,pos);
}

static void drawBox(Voxel* voxel,Vec3 block_pos,Vec3 size){
    real distance = vec3Distance(g_surface.position,vec3Add(block_pos,vec3Shr(size,1)));
    
    bool occlude = distance < depthToSize(voxel->depth) * 8;
	if(g_surface.position.x - block_pos.x < 0)
		drawSide(voxel,block_pos,SIDE_YZ_UP,(Vec2){size.y,size.z},occlude);
    if(g_surface.position.x - block_pos.x - size.x > 0)
		drawSide(voxel,vec3Add(block_pos,(Vec3){size.x,0,0}),SIDE_YZ_DOWN,(Vec2){size.y,size.z},occlude);
    if(g_surface.position.y - block_pos.y < 0)
		drawSide(voxel,block_pos,SIDE_XZ_UP,(Vec2){size.x,size.z},occlude);
    if(g_surface.position.y - block_pos.y - size.y > 0)
		drawSide(voxel,vec3Add(block_pos,(Vec3){0,size.y,0}),SIDE_XZ_DOWN,(Vec2){size.x,size.z},occlude);
    if(g_surface.position.z - block_pos.z < 0)
		drawSide(voxel,block_pos,SIDE_XY_UP,(Vec2){size.y,size.z},occlude);
    if(g_surface.position.z - block_pos.z - size.z > 0)
		drawSide(voxel,vec3Add(block_pos,(Vec3){0,0,size.z}),SIDE_XY_DOWN,(Vec2){size.y,size.z},occlude);
}

static void drawGui(Voxel* voxel,Vec3 block_pos,Vec3 size,VoxelGuiElement* gui,int n_gui){
    real distance = vec3Distance(g_surface.position,vec3Add(block_pos,vec3Shr(size,1)));
    
    bool occlude = distance < depthToSize(voxel->depth) * 8;
	if(g_surface.position.x - block_pos.x < 0)
        voxelGuiDraw(voxel,block_pos,SIDE_YZ_UP,gui,n_gui);
    if(g_surface.position.x - block_pos.x - size.x > 0)
        voxelGuiDraw(voxel,vec3Add(block_pos,(Vec3){size.x,0,0}),SIDE_YZ_DOWN,gui,n_gui);
    if(g_surface.position.y - block_pos.y < 0)
        voxelGuiDraw(voxel,block_pos,SIDE_XZ_UP,gui,n_gui);
    if(g_surface.position.y - block_pos.y - size.y > 0)
        voxelGuiDraw(voxel,vec3Add(block_pos,(Vec3){0,size.y,0}),SIDE_XZ_DOWN,gui,n_gui);
    if(g_surface.position.z - block_pos.z < 0)
	    voxelGuiDraw(voxel,block_pos,SIDE_XY_UP,gui,n_gui);
    if(g_surface.position.z - block_pos.z - size.z > 0)
        voxelGuiDraw(voxel,vec3Add(block_pos,(Vec3){0,0,size.z}),SIDE_XY_DOWN,gui,n_gui);
}

static void slopeDraw(Voxel* voxel,Vec3 block_pos,real block_size){
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
        drawSlopeRecursive(voxel,position,u,v,(Vec2i){0},0);

    real value = (flip_x ^ flip_y) ? g_surface.position.a[table[axis][0]] : g_surface.position.a[table[axis][0]] - block_size;
    if(value - block_pos.a[table[axis][0]] < 0 == (flip_x ^ flip_y)){
        Vec3 position = block_pos;
        position.a[table[axis][0]] += block_size  * !(flip_x ^ flip_y);
        drawSide(voxel,position,table[axis][0] << 1 | (flip_x ^ flip_y),(Vec2){block_size,block_size},false);
    }
    if(g_surface.position.a[axis] - block_pos.a[axis] - block_size > 0){
        Vec3 offset = {0};
        offset.a[axis] = block_size;
        drawSideRecursive(voxel,vec3Add(block_pos,offset),axis << 1 | 1,(Vec2i){0},0,FIXED_ONE,(Vec2){block_size,block_size},(DrawSideFlags){.triangle = true,.flip_x = flip_x ^ flip_y,.flip_y = flip_y});
    }
    if(g_surface.position.a[axis] - block_pos.a[axis] < 0){
        drawSideRecursive(voxel,block_pos,axis << 1,(Vec2i){0},0,FIXED_ONE,(Vec2){block_size,block_size},(DrawSideFlags){.triangle = true,.flip_x = flip_x ^ flip_y,.flip_y = flip_y});
    }

    value = flip_y ? g_surface.position.a[table[axis][1]] : g_surface.position.a[table[axis][1]] - block_size;
    if(value - block_pos.a[table[axis][1]] < 0 == !flip_y){
        Vec3 position = block_pos;
        position.a[table[axis][1]] += block_size * flip_y;
        drawSide(voxel,position,table[axis][1] << 1 | flip_y,(Vec2){block_size,block_size},true);
    }
}

static Vec3 polygonPositionNew(Vec3 inside,Vec3 outside,Plane plane){
    Vec3 direction = vec3Direction(outside,inside);
    real distance = rayPlaneIntersection(outside,direction,plane);
    return vec3Add(outside,vec3MulS(direction,tAbs(distance)));
}
#include "geometry.h"
void octreeDraw(Voxel* voxel){
	real block_size = depthToSize(voxel->depth);
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
        if(!cubeInScreenSpace(g_view_plane,point))
            return;

        if(sdVoxelSquare(vec3Shr(g_surface.position,4),vec3Shr(block_pos,4),realShr(block_size,4)) > RENDER_DISTANCE)
            return;
#if 0
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
            Vec2i axis = g_axis_table[i];
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
#endif
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
		Vec3i i_pos = {rel_pos.x * 2 / block_size,rel_pos.y * 2 / block_size,rel_pos.z * 2 / block_size};
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
#if 1
        for(Entity* entity = voxel->entity_list;entity;entity = entity->next_voxel)
            entityDraw(entity);
#endif
    }
    if(g_voxel_static[voxel->type].slope){
        slopeDraw(voxel,block_pos,block_size);
    }
    else{
        switch(voxel->type){
            case VOXEL_PLANE:{
                VoxelStatic* voxel_s = g_voxel_static + voxel->type;
                if(voxel == g_voxel_interact)
                    drawGui(voxel,block_pos,vec3Single(block_size),voxel_s->gui_interact,voxel_s->n_gui_interact);
                Vec3 relative = vec3AddS(block_pos,block_size / 2);
                Plane plane = {.normal = getLookDirection(voxel->angle),voxel->distance};
                
                int face[][4] = {
                    {0,1,3,2},
                    {4,5,7,6},
                    {0,4,5,1},
                    {2,3,7,6},
                    {0,2,6,4},
                    {1,5,7,3},
                };

                for(int i = countof(face);i--;){
                    if(i & 1){
                        if(g_surface.position.a[i / 2] - point[face[i][0]].a[i / 2] < 0)
                            continue;
                    }
                    else{
                        if(g_surface.position.a[i / 2] - point[face[i][0]].a[i / 2] > 0)
                            continue;
                    }
                    Vec3 d_point[] = {
                        vec3Sub(point[face[i][0]],relative),
                        vec3Sub(point[face[i][1]],relative),
                        vec3Sub(point[face[i][2]],relative),
                        vec3Sub(point[face[i][3]],relative),
                    };
                    real view_z[5] = {
                        -vec3Dot(d_point[0],plane.normal),
                        -vec3Dot(d_point[1],plane.normal),
                        -vec3Dot(d_point[2],plane.normal),
                        -vec3Dot(d_point[3],plane.normal),
                    };
                    Vec3 quad_new[5];
                    int quad_ptr = 0;
                    for(int j = 0;j < 4;j++){
                        int mix_value;
                        int i_next = (j + 1) % 4;
                        if(view_z[j] > 0){
                            quad_new[quad_ptr] = d_point[j];
                            quad_ptr += 1;
                            if(quad_ptr == countof(quad_new))
                                break;
                            if(view_z[(j + 1) % 4] <= 0){
                                quad_new[quad_ptr] = polygonPositionNew(d_point[j],d_point[i_next],plane);
                                quad_ptr += 1;
                                if(quad_ptr == countof(quad_new))
                                    break;
                            }
                        }
                        else{
                            if(view_z[(j + 1) % 4] > 0){
                                quad_new[quad_ptr] = polygonPositionNew(d_point[i_next],d_point[j],plane);
                                quad_ptr += 1;
                                if(quad_ptr == countof(quad_new))
                                    break;
                            }
                        }
                    }
                    int d_index[] = {0,1,3,2};
                    if(quad_ptr == 4){
                        DrawPrimitive* primitive = primitiveToDraw();

                        primitive->gpu_lightmap = true;
                        for(int j = 4;j--;)
                            primitive->position[j] = vec3Add(quad_new[d_index[j]],relative);
                        primitive->normal = g_normal_table[i];
                        primitive->luminance = voxel->color;
                        primitive->procedural_texture = voxel->procedural_texture;
                    }
                }
                Vec3 pos[4];

                int edge[][2] = {
                    {0,1},{1,3},{3,2},{2,0},
                    {4,5},{5,7},{7,6},{6,4},
                    {0,4},{1,5},{2,6},{3,7},
                };
                
                int pos_index = 0;
                
                for(int i = countof(edge);i--;){
                    for(int j = 2;j--;){
                        Vec3 pos_1 = vec3Sub(point[edge[i][!!j]],relative);
                        Vec3 pos_2 = vec3Sub(point[edge[i][!j]],relative);

                        Vec3 direction = vec3Direction(pos_1,pos_2);

                        real distance = rayPlaneIntersection(pos_1,direction,plane);

                        if(distance < 0)
                            continue;
 
                        if(distance < block_size){
                            pos[pos_index++] = vec3Add(vec3Add(pos_1,vec3MulS(direction,distance)),relative);
                            if(pos_index == 4)
                                goto quad;
                            j = 0;
                        }
                    }
                }
            quad:
                DrawPrimitive* primitive = primitiveToDraw();

                primitive->gpu_lightmap = true;
                for(int i = 4;i--;)
                    primitive->position[i] = pos[i];
                primitive->normal = plane.normal;
                primitive->luminance = voxel->color;
                primitive->procedural_texture = voxel->procedural_texture;
            } break;
            case VOXEL_AIR:
                return;
            case VOXEL_MOVABLE:{
                break;
                if(voxel->opened)
                    block_pos.z -= realMulR(block_size,FIXED_ONE - voxel->animation);
                else
                    block_pos.z -= realMulR(block_size,voxel->animation);
            } break;
            case VOXEL_DOOR:{
                break;
                real door_size = realMulR(block_size,FIXED_ONE - voxel->animation) / 2;
                if(voxel->opened)
                    door_size = block_size / 2 - door_size;
                real door_size_inv = block_size / 2 - door_size;
                real door_cove = realMulR(block_size,0x1000);
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
            case VOXEL_CYLINDER:{
                VoxelStatic* voxel_s = g_voxel_static + voxel->type;
                if(voxel == g_voxel_interact)
                    drawGui(voxel,block_pos,vec3Single(block_size),voxel_s->gui_interact,voxel_s->n_gui_interact);
                static ModelSprite model = {
                    .ellipsoid = {
                        [0] = {
                            .type = MODEL_CYLINDER,
                            .color = COLOR_WHITE,
                            .cylinder.radius = FIXED_ONE / 2,
                            .cylinder.axis = (Vec3){0,0,FIXED_ONE},
                        }
                    },
                    .n_ellipsoid = 1,
                };
                model.ellipsoid[0].cylinder.axis = getLookDirection(voxel->cylinder_angle);
                model.ellipsoid[0].cylinder.radius = depthToSize(voxel->depth) / 4;
                Vec3 v_position = voxelWorldPosCenter(voxel);
                if(!voxel->texture_dynamic){
                    voxel->cubemap = tMallocZero(sizeof *voxel->cubemap * 2);
                    
                    voxel->texture_dynamic = tMallocZero(sizeof *voxel->texture_dynamic);
                    *voxel->texture_dynamic = textureCreate(0x100);

                    Vec3 cylinder_direction = getLookDirection(voxel->cylinder_angle);
                    cylinder_direction = vec3MulS(cylinder_direction,depthToSize(voxel->depth) / 2 - REAL_UNIT * 0x10);

                    ellipsoidModelCubemapGenerate(voxel->cubemap,vec3Add(voxelWorldPosCenter(voxel),cylinder_direction),0);
                    ellipsoidModelCubemapGenerate(voxel->cubemap + 1,vec3Sub(voxelWorldPosCenter(voxel),cylinder_direction),0);
                }
                Vec3 cylinder_direction = getLookDirection(voxel->cylinder_angle);
                cylinder_direction = vec3MulS(cylinder_direction,depthToSize(voxel->depth) / 2);
                if(tRndChance(0x100)){
                    ellipsoidModelCubemapGenerate(voxel->cubemap,vec3Add(voxelWorldPosCenter(voxel),cylinder_direction),2);
                    ellipsoidModelCubemapGenerate(voxel->cubemap + 1,vec3Sub(voxelWorldPosCenter(voxel),cylinder_direction),2);
                }
                Vec3 point = pointToScreen(v_position);
                if(!point.z)
                    return;
                real size = spriteSize(v_position,depthToSize(voxel->depth));
                Vec2 points[] = {
                    {point.x - realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
                    {point.x - realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
                    {point.x + realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
                    {point.x + realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
                };
   
                DrawPrimitive* primitive = primitiveToDraw();
                primitive->is_sprite = true;
                for(int i = 4;i--;){
                    primitive->position_sprite[i] = points[i];
                }
                primitive->is_cylinder = true;
                primitive->voxel = voxel;
            } break;
            case VOXEL_SPHERE:{
                static ModelSprite model = {
                    .ellipsoid = {
                        [0] = {
                            .type = MODEL_ELLIPSOID,
                            .color = COLOR_WHITE,
                        }
                    },
                    .n_ellipsoid = 1,
                };
                model.ellipsoid[0].ellipsoid.radius = vec3Single(depthToSize(voxel->depth) / 2);
                Vec3 v_position = voxelWorldPosCenter(voxel);
                if(!voxel->texture_dynamic){
                    voxel->cubemap = tMallocZero(sizeof *voxel->cubemap);
                    
                    voxel->texture_dynamic = tMallocZero(sizeof *voxel->texture_dynamic);
                    *voxel->texture_dynamic = textureCreate(0x100);

                    ellipsoidModelCubemapGenerate(voxel->cubemap,voxelWorldPosCenter(voxel),4);
                }
                if(tRndChance(0x100))
                   ellipsoidModelCubemapGenerate(voxel->cubemap,voxelWorldPosCenter(voxel),5);

                Vec3 point = pointToScreen(v_position);
                if(!point.z)
                    return;
                real size = spriteSize(v_position,depthToSize(voxel->depth));
                Vec2 points[] = {
                    {point.x - realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
                    {point.x - realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
                    {point.x + realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
                    {point.x + realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
                };
   
                DrawPrimitive* primitive = primitiveToDraw();
                primitive->is_sprite = true;
                for(int i = 4;i--;){
                    primitive->position_sprite[i] = points[i];
                }
                primitive->is_sphere = true;
                primitive->voxel = voxel;
            } break;
            case VOXEL_TORUS:{
                VoxelStatic* voxel_s = g_voxel_static + voxel->type;
                if(voxel == g_voxel_interact)
                    drawGui(voxel,block_pos,vec3Single(block_size),voxel_s->gui_interact,voxel_s->n_gui_interact);
                static ModelSprite model = {
                    .ellipsoid = {
                        [0] = {
                            .type = MODEL_ELLIPSOID,
                            .color = COLOR_WHITE,
                        }
                    },
                    .n_ellipsoid = 1,
                };
                model.ellipsoid[0].ellipsoid.radius = vec3Single(depthToSize(voxel->depth) / 2);
                Vec3 v_position = voxelWorldPosCenter(voxel);
                if(!voxel->texture_dynamic){
                    voxel->cubemap = tMallocZero(sizeof *voxel->cubemap);
                    
                    voxel->texture_dynamic = tMallocZero(sizeof *voxel->texture_dynamic);
                    *voxel->texture_dynamic = textureCreate(0x100);

                    ellipsoidModelCubemapGenerate(voxel->cubemap,voxelWorldPosCenter(voxel),4);
                }
                if(tRndChance(0x100))
                   ellipsoidModelCubemapGenerate(voxel->cubemap,voxelWorldPosCenter(voxel),2);

                Vec3 point = pointToScreen(v_position);
                if(!point.z)
                    return;
                real size = spriteSize(v_position,depthToSize(voxel->depth));
                Vec2 points[] = {
                    {point.x - realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
                    {point.x - realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
                    {point.x + realMulR(size,g_surface.fov.x),point.y + realMulR(size,g_surface.fov.y)},
                    {point.x + realMulR(size,g_surface.fov.x),point.y - realMulR(size,g_surface.fov.y)},
                };
   
                DrawPrimitive* primitive = primitiveToDraw();
                primitive->is_sprite = true;
                for(int i = 4;i--;){
                    primitive->position_sprite[i] = points[i];
                }
                primitive->is_torus = true;
                primitive->voxel = voxel;
            } break;
            default:{
                drawBox(voxel,block_pos,vec3Single(block_size));
            } break;
        }
    }
}

#include "opengl.h"

void octreeDrawList(void){
    if(g_options.gl_qlightmap){
        lightmapUploadGL();
    }
    for(DrawPrimitive* primitive = draw_list;primitive;primitive = primitive->next){
        switch(primitive->type){
            case PRIMITIVE_CIRCLE:{
                if(primitive->is_sprite)
                    drawCircle(&g_surface,primitive->position_sprite[0].x,primitive->position_sprite[0].y,primitive->sprite_size.x,vec3MulS(primitive->luminance,g_exposure));
                else
                    drawCircle3d(&g_surface,primitive->position,vec3MulS(primitive->luminance,g_exposure));
            } break;
            case PRIMITIVE_LINE:{
                if(primitive->is_sprite)
                    drawLine(&g_surface,primitive->position[0].x,primitive->position[0].y,primitive->position[1].x,primitive->position[1].y,vec3MulS(primitive->luminance,g_exposure));
                else
                    drawLine3d(&g_surface,primitive->position[0],primitive->position[1],0x00FFFF);
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
                if(primitive->gpu_lightmap){
                    if(primitive->texture)
                        drawLightmapTexturePolygon3dGL(&g_surface,primitive->texture,primitive->texture_crd,primitive->position,primitive->lightmap_index,primitive->side,vec3MulS(primitive->luminance,g_exposure));
                    else
                        drawLightmapPolygon3dGL(&g_surface,primitive->position,primitive->lightmap_index,primitive->normal,primitive->side,vec3MulS(primitive->luminance,g_exposure),primitive->procedural_texture);
                }
                else if(primitive->texture){
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
                    if(primitive->is_sprite){
                        if(primitive->is_sphere)
                            drawSphereGL(&g_surface,primitive->position_sprite,primitive->voxel);
                        else if(primitive->is_cylinder)
                            drawCylinderGL(&g_surface,primitive->position_sprite,primitive->voxel);
                        else if(primitive->is_torus)
                            drawTorusGL(&g_surface,primitive->position_sprite,primitive->voxel);
                        else
                            drawPolygon(&g_surface,primitive->position_sprite,4,primitive->luminance);
                    }
                    else{
                        drawPolygon3d(&g_surface,primitive->position,primitive->luminance);
                    }
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
