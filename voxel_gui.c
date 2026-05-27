#include "voxel_gui.h"
#include "draw.h"
#include "main.h"
#include "font.h"
#include "octree.h"
#include "span.h"
#include "octree_render.h"
#include "texture.h"

Vec2 uvMirror(Vec2 uv,int side){
    int mirror = (int[]){
        1,-1,
        -1,1,
        1,-1,
    }[side];
    if(mirror < 0)
        uv.x = FIXED_ONE - uv.x;
    return uv;
}

Vec2 voxelGuiPositionGet(Voxel* voxel,Vec3 position,Vec3 dir,int side){
    real block_size = depthToSize(voxel->depth);
	Vec3 pos = voxelWorldPos(voxel);
    Vec2i axis = g_axis_table[side << 1];
    Plane plane = getPlane(voxel,dir,side);
    real dst = rayPlaneIntersection(position,dir,plane);
    Vec3 hit_pos = vec3Add(position,vec3MulS(dir,dst));
	Vec2 uv = vec2DivS((Vec2){hit_pos.a[axis.x] - pos.a[axis.x],hit_pos.a[axis.y] - pos.a[axis.y]},block_size);
	return uvMirror(uv,side << 1 | dir.a[side] < 0);
}

void drawGuiChar(Voxel* voxel,int side,Vec2 uv,char string_char,real scale,real thickness,int color){
    Vec2i axis = g_axis_table[side];
    int mirror = (int[]){
        1,-1,
        -1,1,
        1,-1,
    }[side];
    real voxel_size = depthToSize(voxel->depth);
    Vec3 c_pos = voxelWorldPos(voxel);
    
    if(side & 1)
        c_pos.a[side >> 1] += voxel_size;
    
    uv = uvMirror(uv,side);
    
    for(int i = 0;g_vector_font[string_char].position[i][0];i++){
        uint8* coords = &g_vector_font[string_char].position[i][0];
        Vec3 position = c_pos;
        position.a[axis.x] += realMulR(voxel_size,uv.x);
        position.a[axis.y] += realMulR(voxel_size,uv.y);
        Vec3 points[] = {
            position,
            position,
        };        
        points[1].a[axis.y] -= realMulR(intToReal(coords[0]) / 0x100,realMulR(scale,voxel_size));
        points[1].a[axis.x] += realMulR(intToReal(coords[1]) / 0x100,realMulR(scale,voxel_size)) * mirror;
        points[0].a[axis.y] -= realMulR(intToReal(coords[2]) / 0x100,realMulR(scale,voxel_size));
        points[0].a[axis.x] += realMulR(intToReal(coords[3]) / 0x100,realMulR(scale,voxel_size)) * mirror;

        Vec2 point_1 = {points[0].a[axis.x],points[0].a[axis.y]};
        Vec2 point_2 = {points[1].a[axis.x],points[1].a[axis.y]};

        Vec2 direction = vec2MulS(vec2Direction(point_1,point_2),thickness);
    				                                
        Vec3 quad[] = {points[0],points[1],points[0],points[1]};
    				                
        Vec2 quad2d[] = {
            vec2Add(point_1,vec2Rotate(direction,FIXED_ONE / 8 * 3)),
            vec2Add(point_2,vec2Rotate(direction,FIXED_ONE / 8 * 1)),
            vec2Add(point_1,vec2Rotate(direction,FIXED_ONE / 8 * 5)),
            vec2Add(point_2,vec2Rotate(direction,FIXED_ONE / 8 * 7)),
        };

        if(points[0].z <= 0 || points[1].z <= 0)
            continue;
        for(int j = 0;j < 4;j++){
            quad[j].a[axis.x] = quad2d[j].x;
            quad[j].a[axis.y] = quad2d[j].y;
        }
        DrawPrimitive* polygon = primitiveToDraw();
        for(int i = countof(quad);i--;)
            polygon->position[i] = quad[i];
        polygon->luminance = pixelColorToColor(color);
    }
}

void drawGuiString(Voxel* voxel,int side,Vec2 uv,String string,real scale,real thickness,int color){
    Vec2i axis = g_axis_table[side];
    Vec2 c_pos = uv;
	for(int i = 0;i < string.size;i++){
		char string_char = string.data[i];
        if(string_char == '\n'){
            c_pos.x = uv.x;
            c_pos.y -= scale;
            continue;
        }
        drawGuiChar(voxel,side,c_pos,string_char,scale,thickness,color);
        c_pos.x += realMulR(g_vector_font[string_char].width,scale);
    }
}

static void drawNumber3D(Voxel* voxel,int side,Vec2 uv,int number,real scale){
	char buffer[0x10];
    String string = numberToString(buffer,number);
	drawGuiString(voxel,side,uv,string,scale,0x300,0xFFFFFF);
}

static void drawReal3D(Voxel* voxel,int side,Vec2 uv,real number,real scale){
	char buffer[0x10];
    String string = realToString(buffer,number);
	drawGuiString(voxel,side,uv,string,scale,0x300,0xFFFFFF);
}

void drawGuiRectangle(Voxel* voxel,Vec2i axis,Vec3 block_pos,Vec2 uv,Vec2 size,int color,int side){
    int mirror = (int[]){
        1,-1,
        -1,1,
        1,-1,
    }[side];

    if(mirror < 0)
        uv.x = FIXED_ONE - uv.x;
    
	real voxel_size = depthToSize(voxel->depth);
	Vec3 position = block_pos;
    size = vec2MulS(size,voxel_size);
	position.a[axis.x] += realMulR(uv.x,voxel_size);
	position.a[axis.y] += realMulR(uv.y,voxel_size);
	Vec3 points[] = {
		position,
		position,
		position,
		position
	};
	points[1].a[axis.y] += size.y;
	points[2].a[axis.x] += size.x * mirror;
	points[3].a[axis.x] += size.x * mirror;
	points[3].a[axis.y] += size.y;

    DrawPrimitive* polygon = primitiveToDraw();
    for(int i = countof(points);i--;)
        polygon->position[i] = points[i];
    polygon->luminance = pixelColorToColor(color);
}

void drawGuiFrame(Voxel* voxel,Vec2i axis,Vec3 block_pos,Vec2 uv,Vec2 size,int color,real thickness,int side){
	drawGuiRectangle(voxel,axis,block_pos,(Vec2){uv.x,uv.y},(Vec2){size.x,thickness},color,side);
	drawGuiRectangle(voxel,axis,block_pos,(Vec2){uv.x,uv.y},(Vec2){thickness,size.y},color,side);
	drawGuiRectangle(voxel,axis,block_pos,(Vec2){uv.x,uv.y + size.y - thickness},(Vec2){size.x,thickness},color,side);
	drawGuiRectangle(voxel,axis,block_pos,(Vec2){uv.x + size.x - thickness,uv.y},(Vec2){thickness,size.y},color,side);
}

static void drawGuiImage(Voxel* voxel,Texture* image,Vec2i axis,Vec3 block_pos,Vec2 uv,Vec2 size,Side side){
    int mirror = (int[]){
        1,-1,
        -1,1,
        1,-1,
    }[side];

    if(mirror < 0)
        uv.x = FIXED_ONE - uv.x;
    
	real voxel_size = depthToSize(voxel->depth);
	Vec3 position = block_pos;
    position.a[axis.x] += realMulR(voxel_size,uv.x);
	position.a[axis.y] += realMulR(voxel_size,uv.y);
	Vec3 points[] = {
		position,
		position,
		position,
		position
	};
    
	points[1].a[axis.y] += realMulR(voxel_size,size.y);
	points[2].a[axis.x] += realMulR(voxel_size,size.x) * mirror;
	points[3].a[axis.x] += realMulR(voxel_size,size.x) * mirror;
	points[3].a[axis.y] += realMulR(voxel_size,size.y);



	Vec2 d_point[] = {
		{points[0].x,points[0].y},
		{points[1].x,points[1].y},
		{points[3].x,points[3].y},
		{points[2].x,points[2].y}
	};

    DrawPrimitive* primitive = primitiveToDraw();
    primitive->texture = image;
    for(int i = 4;i--;){
        primitive->position[i] = points[i];
        primitive->texture_crd[i] = g_texture_coordinates_fill[i];
    }
    primitive->luminance = COLOR_WHITE;
}

void drawGuiCircle(Voxel* voxel,Vec2i axis,Vec3 block_pos,Vec2 uv,real radius,int color,int side){
    uv = uvMirror(uv,side);
	int voxel_size = depthToSize(voxel->depth);
	Vec3 position = block_pos;
	position.a[axis.x] += realMulR(voxel_size,uv.x);
	position.a[axis.y] += realMulR(voxel_size,uv.y);
	Vec3 points[] = {
		position,
		position,
		position,
		position
	};
	points[0].a[axis.x] += realMulR(voxel_size,-radius);
	points[0].a[axis.y] += realMulR(voxel_size,-radius);
	points[1].a[axis.x] += realMulR(voxel_size,-radius);
    points[1].a[axis.y] += realMulR(voxel_size,radius);
	points[2].a[axis.x] += realMulR(voxel_size,radius);
	points[2].a[axis.y] += realMulR(voxel_size,radius);
	points[3].a[axis.x] += realMulR(voxel_size,radius);
	points[3].a[axis.y] += realMulR(voxel_size,-radius);

	points[0] = pointToScreenRenderer(points[0],g_surface.rotation_matrix,g_surface.position,g_surface.fov);
	points[1] = pointToScreenRenderer(points[1],g_surface.rotation_matrix,g_surface.position,g_surface.fov);
	points[2] = pointToScreenRenderer(points[2],g_surface.rotation_matrix,g_surface.position,g_surface.fov);
	points[3] = pointToScreenRenderer(points[3],g_surface.rotation_matrix,g_surface.position,g_surface.fov);

    DrawPrimitive* polygon = primitiveToDraw();
    polygon->luminance = pixelColorToColor(color);
    polygon->type = PRIMITIVE_CIRCLE;
    for(int i = 4;i--;)
        polygon->position[i] = points[i];
}

static bool rectInRect(Vec2 position_1,Vec2 size_1,Vec2 position_2,Vec2 size_2){
	bool bound_x = position_1.x < position_2.x + size_2.x && position_1.x + size_1.x > position_2.x;
	bool bound_y = position_1.y < position_2.y + size_2.y && position_1.y + size_1.y > position_2.y;

    return bound_x && bound_y;
}

structure(HSV){
    real h;
    real s;
    real v;
};

static HSV rgb2hsv(Vec3 in){
    HSV  out;
    real min, max, delta;

    min = in.x < in.y ? in.x : in.y;
    min = min  < in.z ? min  : in.z;

    max = in.x > in.y ? in.x : in.y;
    max = max  > in.z ? max  : in.z;

    out.v = max;            
    delta = max - min;
    if(delta < REAL_EPSILON){
        out.s = 0;
        out.h = 0;
        return out;
    }
    if(max > 0){
        out.s = realDivR(delta,max);        
    }
    else{
        out.s = 0;
        out.h = NAN;     
        return out;
    }
    if(in.x >= max)                    
        out.h = realDivR((in.y - in.z),delta);    
    else
    if(in.y >= max)
        out.h = FIXED_ONE * 2 + realDivR((in.z - in.x),delta);
    else
        out.h = FIXED_ONE * 4 + realDivR((in.x - in.y),delta);  

    out.h = realMulR(out.h,FIXED_ONE * 60);                        

    if(out.h < 0)
        out.h += FIXED_ONE * 360;

    return out;
}


static Vec3 hsv2rgb(HSV in){
    real hh, p, q, t, ff;
    long i;
    Vec3 out;

    if(in.s <= 0){  
        out.x = in.v;
        out.y = in.v;
        out.z = in.v;
        return out;
    }
    hh = in.h;
    if(hh >= FIXED_ONE * 360)
        hh = 0;
    hh = realDivR(hh,FIXED_ONE * 60.0);
    i = realToInt(hh);
    ff = hh - i;
    p = realMulR(in.v,(FIXED_ONE - in.s));
    q = realMulR(in.v,(FIXED_ONE - realMulR(in.s,ff)));
    t = realMulR(in.v,(FIXED_ONE - realMulR(in.s,(FIXED_ONE - ff))));

    switch(i) {
    case 0:
        out.x = in.v;
        out.y = t;
        out.z = p;
        break;
    case 1:
        out.x = q;
        out.y = in.v;
        out.z = p;
        break;
    case 2:
        out.x = p;
        out.y = in.v;
        out.z = t;
        break;

    case 3:
        out.x = p;
        out.y = q;
        out.z = in.v;
        break;
    case 4:
        out.x = t;
        out.y = p;
        out.z = in.v;
        break;
    case 5:
    default:
        out.x = in.v;
        out.y = p;
        out.z = q;
        break;
    }
    return out;     
}

#include "opengl.h"

void voxelGuiDraw(Voxel* voxel,Vec3 block_pos,int side,VoxelGuiElement* gui,int n_gui){
	VoxelStatic* voxel_s = g_voxel_static + voxel->type;
	real voxel_size = depthToSize(voxel->depth);
	Vec2i axis = g_axis_table[side];

	for(int i = 0;i < n_gui;i++){
		VoxelGuiElement* element = gui + i;
		switch(element->type){
            case VOXEL_GUI_COLORPICKER:{
                Vec2 uv = element->position;
                Vec2 size = {REAL_UNIT * 0x40,REAL_UNIT * 0x10};

                static Texture hue_texture;
                static Texture sat_texture;
                static Texture val_texture;
                
                if(!hue_texture.pixel_data){
                    hue_texture = textureCreate(0x40);
                    sat_texture = textureCreate(0x40);
                    val_texture = textureCreate(0x40);
                }
                HSV hsv = rgb2hsv(*element->colorpicker.color);
                for(int j = hue_texture.size * hue_texture.size;j--;){
                    int y = j / hue_texture.size;
                    real y_r = intToReal(hue_texture.size - y - 1) / hue_texture.size * 360;
                    if(tAbs(y_r - hsv.h) < FIXED_ONE * 4){
                        hue_texture.pixel_data[j] = 0;
                        continue;
                    }
                    Vec3 color = hsv2rgb((HSV){
                            .v = hsv.v,
                            .h = y_r,
                            .s = hsv.s,
                    });
                    hue_texture.pixel_data[j] = colorToPixelColor(color);
                }
                for(int j = hue_texture.size * hue_texture.size;j--;){
                    int y = j / hue_texture.size;
                    real y_r = intToReal(hue_texture.size - y - 1) / hue_texture.size;
                    if(tAbs(y_r - hsv.s) < REAL_UNIT * 4){
                        sat_texture.pixel_data[j] = 0;
                        continue;
                    }
                    Vec3 color = hsv2rgb((HSV){
                            .v = hsv.v,
                            .h = hsv.s,
                            .s = y_r,
                    });
                    sat_texture.pixel_data[j] = colorToPixelColor(color);
                }
                for(int j = hue_texture.size * hue_texture.size;j--;){
                    int y = j / hue_texture.size;
                    real y_r = intToReal(hue_texture.size - y - 1) / hue_texture.size;
                    if(tAbs(y_r - hsv.v) < REAL_UNIT * 4){
                        val_texture.pixel_data[j] = 0;
                        continue;
                    }
                    Vec3 color = hsv2rgb((HSV){
                            .v = y_r,
                            .h = hsv.h,
                            .s = hsv.s,
                    });
                    val_texture.pixel_data[j] = colorToPixelColor(color);
                }
                generateMipmaps(&hue_texture);
                generateMipmaps(&val_texture);
                generateMipmaps(&sat_texture);

                textureUpdateGL(&hue_texture);
                textureUpdateGL(&val_texture);
                textureUpdateGL(&sat_texture);

                drawGuiImage(voxel,&hue_texture,axis,block_pos,uv,size,side);
                drawGuiImage(voxel,&val_texture,axis,block_pos,vec2Add(uv,(Vec2){0,REAL_UNIT * 0x10}),size,side);
                drawGuiImage(voxel,&sat_texture,axis,block_pos,vec2Add(uv,(Vec2){0,REAL_UNIT * 0x20}),size,side);
            } break;
			case VOXEL_GUI_INVENTORY_SLOT:{
				int color = 0x808080;
				if(
					g_voxel_pointed.voxel == voxel && 
					rectInRect(element->position,(Vec2){REAL_UNIT * 0x20,REAL_UNIT * 0x20},g_voxel_pointed.uv,vec2Single(REAL_UNIT * 0x02))
				){
					color = 0x20A020;
				}
				else if(element->inventory_slot.slot->type == INVENTORY_SPELL){
					if(g_spell_static[element->inventory_slot.slot->spell_type].adjective)
						color = 0xA02020;
					else
						color = 0x20A0A0;
				}
				drawGuiFrame(voxel,g_axis_table[side],block_pos,element->position,(Vec2){REAL_UNIT * 0x20,REAL_UNIT * 0x20},color,REAL_UNIT * 0x02,side);
				Vec2 spell_uv = vec2Add(element->position,vec2Single(0x1000));
				if(!element->inventory_slot.slot->type)
					continue;
				switch(element->inventory_slot.slot->spell_type){
					case SPELL_BOLT:{
						drawGuiCircle(voxel,g_axis_table[side],block_pos,spell_uv,0xA00,0xFF0000,side);
					} break;
					case SPELL_BOMB:{
						drawGuiCircle(voxel,g_axis_table[side],block_pos,spell_uv,0xA00,0x0000FF,side);
					} break;
					case SPELL_ORB:{
						drawGuiCircle(voxel,g_axis_table[side],block_pos,spell_uv,0xA00,0x00FF00,side);
					} break;
					case SPELL_ADJ_SPEED:{
						Vec2 string_uv = vec2Add(spell_uv,(Vec2){-0x600,0x400});
                        drawGuiString(voxel,side,string_uv,(String)STRING_LITERAL(">>"),0x800,0x300,0xFFFFFF);
					} break;
					case SPELL_ADJ_DAMAGE:{
						Vec2 string_uv = vec2Add(spell_uv,(Vec2){-0x600,0x400});
						drawGuiString(voxel,side,string_uv,(String)STRING_LITERAL("#+"),0x800,0x300,0xFFFFFF);
					} break;
					case SPELL_ADJ_DOUBLER:{
						Vec2 string_uv = vec2Add(spell_uv,(Vec2){-0x600,0x400});
						drawGuiString(voxel,side,string_uv,(String)STRING_LITERAL("x2"),0x800,0x300,0xFFFFFF);
					} break;
					default:{
						drawGuiCircle(voxel,g_axis_table[side],block_pos,spell_uv,0xA00,0xFF00FF,side);
					} break;
				}
			} break;
			case VOXEL_GUI_IMAGE:{
				drawGuiImage(voxel,element->image.image,axis,block_pos,element->position,vec2Single(0x6000),side);
			} break;
			case VOXEL_GUI_CHECKBOX:{
				int color;
				bool is_pointed = rectInRect(element->position,vec2Single(0x1000),g_voxel_pointed.uv,vec2Single(0x200));
				if(element->checkbox.state){
					if(*element->checkbox.state)
						color = is_pointed ? 0x90C090 : 0x809080;
					else
						color = is_pointed ? 0xC09090 : 0x908080;
				}
				else{
					color = is_pointed ?  0xA0A0A0 : 0x808080;
				}
				drawGuiRectangle(voxel,axis,block_pos,element->position,vec2Single(0x1000),color,side);
			} break;
            case VOXEL_GUI_RECTANGLE:{
                Vec2 size = element->rectangle.size;
                drawGuiRectangle(voxel,axis,block_pos,element->position,size,element->rectangle.color,side);
            } break;
			case VOXEL_GUI_BUTTON:{
				int color;
				Vec2 button_size = element->button.size ? vec2Single(element->button.size) : vec2Single(REAL_UNIT * 0x10);
				if(g_voxel_pointed.voxel == voxel && rectInRect(element->position,button_size,g_voxel_pointed.uv,vec2Single(REAL_UNIT * 2)))
					color = 0x90C090;
				else
					color = 0x809080;
                drawGuiFrame(voxel,axis,block_pos,element->position,button_size,0x000000,REAL_UNIT * 2,side);
                drawGuiRectangle(voxel,axis,block_pos,element->position,button_size,color,side);
			} break;
			case VOXEL_GUI_STRING:{
                
				real size = !element->string.size ? REAL_UNIT * 0x08 : element->string.size;
				drawGuiString(voxel,side,element->position,element->string.string,size,REAL_UNIT * 0x03,0xFFFFFF);
			} break;
			case VOXEL_GUI_NUMBER:{
				real size = !element->number.size ? 0x800 : element->number.size;
				drawNumber3D(voxel,side,element->position,*element->number.number,size);	
			} break;
            case VOXEL_GUI_REAL:{
                real size = !element->number.size ? 0x800 : element->number.size;
				drawReal3D(voxel,side,element->position,*element->number.number,size);	
            } break;
		}
	}
    if(!n_gui)
        return;
	if(g_voxel_pointed.voxel == voxel && g_voxel_pointed.side == side){
		drawGuiRectangle(voxel,axis,block_pos,vec2Sub(g_voxel_pointed.uv,vec2Single(REAL_UNIT * 0x01)),vec2Single(REAL_UNIT * 0x03),0x000000,side);
		drawGuiRectangle(voxel,axis,block_pos,vec2Sub(g_voxel_pointed.uv,vec2Single(REAL_UNIT * 0x01)),vec2Single(REAL_UNIT * 0x02),0xFFFFFF,side);
	}
}

SpellType g_spell_hold;

bool voxelGuiOnClick(Voxel* voxel,int side,VoxelGuiElement* gui,int n_gui){
	if(!voxel)
		return false;
	VoxelStatic* voxel_s = g_voxel_static + g_voxel_pointed.voxel->type;

	if(!n_gui)
		return false;

	for(int i = 0;i < n_gui;i++){
		VoxelGuiElement* element = gui + i;
		switch(element->type){
            case VOXEL_GUI_COLORPICKER:{
                Vec2 size = {REAL_UNIT * 0x40,REAL_UNIT * 0x10};
                if(!element->colorpicker.color)
                    break;
				if(rectInRect(element->position,size,g_voxel_pointed.uv,vec2Single(REAL_UNIT * 2))){
				    real relative = g_voxel_pointed.uv.x - element->position.x;
                    relative = realDivR(relative,size.x);
                    Vec3 dummy = *element->colorpicker.color;
                    HSV hsl = rgb2hsv(*element->colorpicker.color);
                    hsl.h = relative * 360;
                    *element->colorpicker.color = hsv2rgb(hsl);
                    octreeRefresh();
                    return true;
				}
                if(rectInRect(element->position,size,vec2Add(g_voxel_pointed.uv,(Vec2){0,-REAL_UNIT * 0x10}),vec2Single(REAL_UNIT * 2))){
				    real relative = g_voxel_pointed.uv.x - element->position.x;
                    relative = realDivR(relative,size.x);
                    Vec3 dummy = *element->colorpicker.color;
                    HSV hsl = rgb2hsv(*element->colorpicker.color);
                    hsl.v = relative;
                    *element->colorpicker.color = hsv2rgb(hsl);
                    octreeRefresh();
                    return true;
				}
                if(rectInRect(element->position,size,vec2Add(g_voxel_pointed.uv,(Vec2){0,-REAL_UNIT * 0x20}),vec2Single(REAL_UNIT * 2))){
				    real relative = g_voxel_pointed.uv.x - element->position.x;
                    relative = realDivR(relative,size.x);
                    Vec3 dummy = *element->colorpicker.color;
                    HSV hsl = rgb2hsv(*element->colorpicker.color);
                    hsl.s = relative;
                    *element->colorpicker.color = hsv2rgb(hsl);
                    octreeRefresh();
                    return true;
				}
            } break;
			case VOXEL_GUI_INVENTORY_SLOT:{
				if(rectInRect(element->position,(Vec2){0x2000,0x2000},g_voxel_pointed.uv,vec2Single(0x200))){
					g_spell_hold = element->inventory_slot.slot->spell_type;
					element->inventory_slot.slot->type = 0;
					return true;
				}
			} break;
			case VOXEL_GUI_CHECKBOX:{
                Vec2 button_size = element->button.size ? vec2Single(element->button.size) : vec2Single(0x1000);
				if(rectInRect(element->position,button_size,g_voxel_pointed.uv,vec2Single(0x200)) && element->checkbox.state){
					*element->checkbox.state ^= true;
                    return true;
                }
			} break;
			case VOXEL_GUI_BUTTON:{
				Vec2 button_size = element->button.size ? vec2Single(element->button.size) : vec2Single(REAL_UNIT * 0x10);
				if(rectInRect(element->position,button_size,g_voxel_pointed.uv,vec2Single(REAL_UNIT * 2)) && element->button.on_click){
					element->button.voxel = voxel;
					element->button.on_click(element);
					return true;
				}
			} break;
		}
	}
	return voxel_s->n_gui;
}

void voxelGuiOnRelease(Voxel* voxel,int side){
	if(!voxel)
		return;
	Vec3 block_pos = voxelWorldPos(voxel);
	VoxelStatic* voxel_s = g_voxel_static + g_voxel_pointed.voxel->type;

	int n_gui = voxel_s->side[side].custom ? voxel_s->side[side].n_gui : voxel_s->n_gui;
	VoxelGuiElement* gui = voxel_s->side[side].custom ? voxel_s->side[side].gui : voxel_s->gui;

	if(!n_gui)
		return;

	for(int i = 0;i < n_gui;i++){
		VoxelGuiElement* element = gui + i;
		switch(element->type){
			case VOXEL_GUI_INVENTORY_SLOT:{
				if(element->inventory_slot.slot->type)
					continue;
				if(rectInRect(element->position,(Vec2){0x2000,0x2000},g_voxel_pointed.uv,vec2Single(0x200))){
					element->inventory_slot.slot->type = INVENTORY_SPELL;
					element->inventory_slot.slot->spell_type = g_spell_hold;
					g_spell_hold = 0;
					return;
				}
			} break;
		}
	}
}
