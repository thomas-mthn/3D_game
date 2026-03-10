#include "voxel_gui.h"
#include "main.h"
#include "font.h"

static int tStrlen(char* str){
	char* str_b = str;
	while(*str++);
	return str - str_b;
}

static void drawString3D(DrawSurface surface,Voxel* voxel,Vec2 axis,Vec3 block_pos,Vec2 uv,char* str,int str_length,int scale,bool mirror){
    int down_offset = 0;
    int offset = 0;
	if(str_length == STRING_LENGTH_CSTRING)
		str_length = tStrlen(str);
    while(*str){
        if(*str == '\n'){
            down_offset += 1;
            offset = 0;
        }
        else{
            for(int i = 0;g_vector_font[*str][i][0];i++){
                uint8* coords = &g_vector_font[*str][i][0];
				int voxel_size = depthToSize(voxel->depth);
				Vec3 position = block_pos;
				int offset_f = mirror ? str_length - offset : offset;
				((int*)&position)[axis.x] += fixedMulR(voxel_size,uv.x + offset_f * scale);
				((int*)&position)[axis.y] += fixedMulR(voxel_size,uv.y);
				Vec3 points[] = {
					position,
					position,
				};
				((int*)&points[1])[axis.y] -= fixedMulR(coords[0] << 8,fixedMulR(scale,voxel_size));
				((int*)&points[1])[axis.x] += fixedMulR(coords[1] << 8,fixedMulR(scale,voxel_size)) * (!mirror * 2 - 1);
				((int*)&points[0])[axis.y] -= fixedMulR(coords[2] << 8,fixedMulR(scale,voxel_size));
				((int*)&points[0])[axis.x] += fixedMulR(coords[3] << 8,fixedMulR(scale,voxel_size)) * (!mirror * 2 - 1);

				((int*)&points[1])[axis.y] += fixedMulR(scale,voxel_size);
				((int*)&points[1])[axis.x] += fixedMulR(scale,voxel_size);
				((int*)&points[0])[axis.y] += fixedMulR(scale,voxel_size);
				((int*)&points[0])[axis.x] += fixedMulR(scale,voxel_size);

				points[0] = pointToScreen(points[0]);
				points[1] = pointToScreen(points[1]);

				Vec2 d_point[] = {
					{points[0].x,points[0].y},
					{points[1].x,points[1].y},
				};

				if(points[0].z <= 0 || points[1].z <= 0)
					continue;

                drawLine(surface,d_point[0].x,d_point[0].y,d_point[1].x,d_point[1].y,pixelColorToColor(0xFFFFFF));
            }
        }
        str += 1;
        offset += 1;
    }
}

static void drawNumber3D(DrawSurface surface,Voxel* voxel,Vec2 axis,Vec3 block_pos,Vec2 uv,int number,int scale,bool mirror){
	char buffer[0x10];
    intToString(buffer,number);
	drawString3D(g_surface,voxel,axis,block_pos,uv,buffer,-1,scale,mirror);
}

static void drawGuiRectangle(Voxel* voxel,Vec2 axis,Vec3 block_pos,Vec2 uv,Vec2 size,int color){
	int voxel_size = depthToSize(voxel->depth);
	Vec3 position = block_pos;
	((int*)&position)[axis.x] += fixedMulR(voxel_size,uv.x);
	((int*)&position)[axis.y] += fixedMulR(voxel_size,uv.y);
	Vec3 points[] = {
		position,
		position,
		position,
		position
	};
	((int*)&points[1])[axis.y] += fixedMulR(voxel_size,size.y);
	((int*)&points[2])[axis.x] += fixedMulR(voxel_size,size.x);
	((int*)&points[3])[axis.x] += fixedMulR(voxel_size,size.x);
	((int*)&points[3])[axis.y] += fixedMulR(voxel_size,size.y);

	points[0] = pointToScreen(points[0]);
	points[1] = pointToScreen(points[1]);
	points[2] = pointToScreen(points[2]);
	points[3] = pointToScreen(points[3]);

	Vec2 d_point[] = {
		{points[0].x,points[0].y},
		{points[1].x,points[1].y},
		{points[3].x,points[3].y},
		{points[2].x,points[2].y}
	};

	if(points[0].z <= 0 || points[1].z <= 0 || points[2].z <= 0 || points[3].z <= 0)
		return;

	drawPolygon(g_surface,d_point,4,pixelColorToColor(color));
}

void drawGuiFrame(Voxel* voxel,Vec2 axis,Vec3 block_pos,Vec2 uv,Vec2 size,int color,int thickness){
	drawGuiRectangle(voxel,axis,block_pos,(Vec2){uv.x,uv.y},(Vec2){size.x,thickness},color);
	drawGuiRectangle(voxel,axis,block_pos,(Vec2){uv.x,uv.y},(Vec2){thickness,size.y},color);
	drawGuiRectangle(voxel,axis,block_pos,(Vec2){uv.x,uv.y + size.y - thickness},(Vec2){size.x,thickness},color);
	drawGuiRectangle(voxel,axis,block_pos,(Vec2){uv.x + size.x - thickness,uv.y},(Vec2){thickness,size.y},color);
}

static void drawGuiImage(Voxel* voxel,Texture* image,Vec2 axis,Vec3 block_pos,Vec2 uv,Vec2 size){
	int voxel_size = depthToSize(voxel->depth);
	Vec3 position = block_pos;
	((int*)&position)[axis.x] += fixedMulR(voxel_size,uv.x);
	((int*)&position)[axis.y] += fixedMulR(voxel_size,uv.y);
	Vec3 points[] = {
		position,
		position,
		position,
		position
	};
	((int*)&points[1])[axis.y] += fixedMulR(voxel_size,size.y);
	((int*)&points[2])[axis.x] += fixedMulR(voxel_size,size.x);
	((int*)&points[3])[axis.x] += fixedMulR(voxel_size,size.x);
	((int*)&points[3])[axis.y] += fixedMulR(voxel_size,size.y);

	points[0] = pointToScreen(points[0]);
	points[1] = pointToScreen(points[1]);
	points[2] = pointToScreen(points[2]);
	points[3] = pointToScreen(points[3]);

	Vec2 d_point[] = {
		{points[0].x,points[0].y},
		{points[1].x,points[1].y},
		{points[3].x,points[3].y},
		{points[2].x,points[2].y}
	};
	if(points[0].z <= 0 || points[1].z <= 0 || points[2].z <= 0 || points[3].z <= 0)
		return;
	drawTexturePolygon(g_surface,image,g_texture_coordinates_fill,d_point,vec3Single(FIXED_ONE << 4),4);
}

void drawGuiCircle(Voxel* voxel,Vec2 axis,Vec3 block_pos,Vec2 uv,int radius,int color){
	int voxel_size = depthToSize(voxel->depth);
	Vec3 position = block_pos;
	((int*)&position)[axis.x] += fixedMulR(voxel_size,uv.x);
	((int*)&position)[axis.y] += fixedMulR(voxel_size,uv.y);
	Vec3 points[] = {
		position,
		position,
		position,
		position
	};
	((int*)&points[0])[axis.x] += fixedMulR(voxel_size,-radius);
	((int*)&points[0])[axis.y] += fixedMulR(voxel_size,-radius);
	((int*)&points[1])[axis.x] += fixedMulR(voxel_size,-radius);
	((int*)&points[1])[axis.y] += fixedMulR(voxel_size,radius);
	((int*)&points[2])[axis.x] += fixedMulR(voxel_size,radius);
	((int*)&points[2])[axis.y] += fixedMulR(voxel_size,radius);
	((int*)&points[3])[axis.x] += fixedMulR(voxel_size,radius);
	((int*)&points[3])[axis.y] += fixedMulR(voxel_size,-radius);

	points[0] = pointToScreenRenderer(points[0],g_tri,g_position,g_fov);
	points[1] = pointToScreenRenderer(points[1],g_tri,g_position,g_fov);
	points[2] = pointToScreenRenderer(points[2],g_tri,g_position,g_fov);
	points[3] = pointToScreenRenderer(points[3],g_tri,g_position,g_fov);

	drawCircle3d(g_surface,points,pixelColorToColor(color));
}

static bool rectInRect(Vec2 position_1,Vec2 size_1,Vec2 position_2,Vec2 size_2){
	bool bound_x = position_1.x < position_2.x + size_2.x && position_1.x + size_1.x > position_2.x;
	bool bound_y = position_1.y < position_2.y + size_2.y && position_1.y + size_1.y > position_2.y;

    return bound_x && bound_y;
}

void voxelGuiDraw(Voxel* voxel,Vec3 block_pos,int side){
	VoxelStatic* voxel_s = g_voxel_static + voxel->type;
	int voxel_size = depthToSize(voxel->depth);
	Vec2 axis = g_axis_table[side];

	int n_gui = voxel_s->side[side].custom ? voxel_s->side[side].n_gui : voxel_s->n_gui;
	VoxelGuiElement* gui = voxel_s->side[side].custom ? voxel_s->side[side].gui : voxel_s->gui;

	if(!n_gui)
		return;

	for(int i = 0;i < n_gui;i++){
		VoxelGuiElement* element = gui + i;
		switch(element->type){
			case VOXEL_GUI_INVENTORY_SLOT:{
				int color = 0x808080;
				if(
					g_voxel_pointed.voxel == voxel && 
					rectInRect(element->position,(Vec2){0x2000,0x2000},g_voxel_pointed.uv,vec2Single(0x200))
				){
					color = 0x20A020;
				}
				else if(element->inventory_slot->type == INVENTORY_SPELL){
					if(g_spell_static[element->inventory_slot->spell_type].adjective)
						color = 0xA02020;
					else
						color = 0x20A0A0;
				}
				drawGuiFrame(voxel,g_axis_table[side],block_pos,element->position,(Vec2){0x2000,0x2000},color,0x200);
				Vec2 spell_uv = vec2AddR(element->position,vec2Single(0x1000));
				if(!element->inventory_slot->type)
					continue;
				switch(element->inventory_slot->spell_type){
					case SPELL_BOLT:{
						drawGuiCircle(voxel,g_axis_table[side],block_pos,spell_uv,0xA00,0xFF0000);
					} break;
					case SPELL_BOMB:{
						drawGuiCircle(voxel,g_axis_table[side],block_pos,spell_uv,0xA00,0x0000FF);
					} break;
					case SPELL_ORB:{
						drawGuiCircle(voxel,g_axis_table[side],block_pos,spell_uv,0xA00,0x00FF00);
					} break;
					case SPELL_ADJ_SPEED:{
						Vec2 string_uv = vec2AddR(spell_uv,(Vec2){-0x1000,-0x400});
						drawString3D(g_surface,voxel,g_axis_table[side],block_pos,string_uv,">>",-1,0x800,false);
					} break;
					case SPELL_ADJ_DAMAGE:{
						Vec2 string_uv = vec2AddR(spell_uv,(Vec2){-0x1000,-0x400});
						drawString3D(g_surface,voxel,g_axis_table[side],block_pos,string_uv,"#+",-1,0x800,false);
					} break;
					case SPELL_ADJ_DOUBLER:{
						Vec2 string_uv = vec2AddR(spell_uv,(Vec2){-0x1000,-0x400});
						drawString3D(g_surface,voxel,g_axis_table[side],block_pos,string_uv,"x2",-1,0x800,false);
					} break;
					default:{
						drawGuiCircle(voxel,g_axis_table[side],block_pos,spell_uv,0xA00,0xFF00FF);
					} break;
				}
			} break;
			case VOXEL_GUI_IMAGE:{
				drawGuiImage(voxel,element->image,axis,block_pos,element->position,vec2Single(0x6000));
			} break;
			case VOXEL_GUI_CHECKBOX:{
				int color;
				bool is_pointed = rectInRect(element->position,vec2Single(0x1000),g_voxel_pointed.uv,vec2Single(0x200));
				if(element->checkbox_state){
					if(*element->checkbox_state)
						color = is_pointed ? 0x90C090 : 0x809080;
					else
						color = is_pointed ? 0x9090C0 : 0x808090;
				}
				else{
					color = is_pointed ?  0xA0A0A0 : 0x808080;
				}
				drawGuiRectangle(voxel,axis,block_pos,element->position,vec2Single(0x1000),color);
			} break;
			case VOXEL_GUI_BUTTON:{
				int color;
				Vec2 button_size = element->size ? vec2Single(element->size) : vec2Single(0x1000);
				if(g_voxel_pointed.voxel == voxel && rectInRect(element->position,button_size,g_voxel_pointed.uv,vec2Single(0x200)))
					color = 0x90C090;
				else
					color = 0x809080;
				drawGuiRectangle(voxel,axis,block_pos,element->position,button_size,color);
			} break;
			case VOXEL_GUI_STRING:{
				int size = !element->size ? 0x800 : element->size;
				bool mirror[] = {
					false,
					false,
					true,
					true,
					false,
					false,
				};
				drawString3D(g_surface,voxel,axis,block_pos,element->position,element->string,-1,size,mirror[side]);
			} break;
			case VOXEL_GUI_NUMBER:{
				int size = !element->size ? 0x800 : element->size;
				drawNumber3D(g_surface,voxel,axis,block_pos,element->position,*element->number,size,side & 1);	
			} break;
		}
	}
	if(g_voxel_pointed.voxel == voxel && g_voxel_pointed.side == side){
		drawGuiRectangle(voxel,axis,block_pos,vec2SubR(g_voxel_pointed.uv,vec2Single(0x150)),vec2Single(0x300),0x000000);
		drawGuiRectangle(voxel,axis,block_pos,vec2SubR(g_voxel_pointed.uv,vec2Single(0x100)),vec2Single(0x200),0xFFFFFF);
	}
}

SpellType g_spell_hold;

bool voxelGuiOnClick(Voxel* voxel,int side){
	if(!voxel)
		return false;
	VoxelStatic* voxel_s = g_voxel_static + g_voxel_pointed.voxel->type;

	int n_gui = voxel_s->side[side].custom ? voxel_s->side[side].n_gui : voxel_s->n_gui;
	VoxelGuiElement* gui = voxel_s->side[side].custom ? voxel_s->side[side].gui : voxel_s->gui;

	if(!n_gui)
		return false;

	for(int i = 0;i < n_gui;i++){
		VoxelGuiElement* element = gui + i;
		switch(element->type){
			case VOXEL_GUI_INVENTORY_SLOT:{
				if(rectInRect(element->position,(Vec2){0x2000,0x2000},g_voxel_pointed.uv,vec2Single(0x200))){
					g_spell_hold = element->inventory_slot->spell_type;
					element->inventory_slot->type = 0;
					return true;
				}
			} break;
			case VOXEL_GUI_CHECKBOX:{
				if(element->checkbox_state)
					*element->checkbox_state ^= true;
			} //falltrough
			case VOXEL_GUI_BUTTON:{
				Vec2 button_size = element->size ? vec2Single(element->size) : vec2Single(0x1000);
				if(rectInRect(element->position,button_size,g_voxel_pointed.uv,vec2Single(0x200)) && element->on_click){
					element->voxel = voxel;
					element->on_click(element);
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
				if(element->inventory_slot->type)
					continue;
				if(rectInRect(element->position,(Vec2){0x2000,0x2000},g_voxel_pointed.uv,vec2Single(0x200))){
					element->inventory_slot->type = INVENTORY_SPELL;
					element->inventory_slot->spell_type = g_spell_hold;
					g_spell_hold = 0;
					return;
				}
			} break;
		}
	}
}