#include "gui2d.h"
#include "draw.h"
#include "main.h"
#include "font.h"

void gui2dRectangleDraw(Vec2 position,Vec2 size,int color,Gui2dFlags flags){
    bool height_bigger = g_surface.window_width < g_surface.window_height;

    position = aspectRatioTransform(position);
    size = aspectRatioTransform(size);

    if(!flags.middle_x){
        if(flags.invert_x)
            position.x = FIXED_ONE - position.x - size.x;
        else
            position.x -= FIXED_ONE;
    }

    if(!flags.middle_y){
        if(flags.invert_y)
            position.y = FIXED_ONE - position.y - size.y;
        else
            position.y -= FIXED_ONE;
    }
    
    drawRectangle(&g_surface,position.x,position.y,size.x,size.y,pixelColorToColor(color));
}

static void gui2dSegmentDraw(Vec2 pos_1,Vec2 pos_2,real thickness,int color,Gui2dFlags flags){
    return;
    
    pos_1 = aspectRatioTransform(pos_1);
    pos_2 = aspectRatioTransform(pos_2);

    if(flags.middle_x){
        pos_1.x  = -pos_1.x ;
        pos_2.x = -pos_2.x;
    }
    else{
        if(flags.invert_x){
            pos_1.x  = FIXED_ONE - pos_1.x ;
            pos_2.x = FIXED_ONE - pos_2.x;
        }
        else{
            pos_1.x  -= FIXED_ONE;
            pos_2.x -= FIXED_ONE;
        }
    }

    if(flags.middle_y){
        pos_1.y  = -pos_1.y ;
        pos_2.y = -pos_2.y;
    }
    else{
        if(flags.invert_y){
            pos_1.y  = FIXED_ONE - pos_1.y ;
            pos_2.y = FIXED_ONE - pos_2.y;
        }
        else{
            pos_1.y  -= FIXED_ONE;
            pos_2.y -= FIXED_ONE;
        }
    }
    
    drawSegment(&g_surface,pos_1.x,pos_1.y,pos_2.x,pos_2.y,thickness,pixelColorToColor(color));
}

void gui2dFrameDraw(real x,real y,real size_x,real size_y,int color,int thickness,Gui2dFlags flags){
    gui2dRectangleDraw((Vec2){x,y},(Vec2){size_x,thickness},color,flags);
    gui2dRectangleDraw((Vec2){x,y},(Vec2){thickness,size_y},color,flags);
    gui2dRectangleDraw((Vec2){x,y + size_y - thickness},(Vec2){size_x,thickness},color,flags);
    gui2dRectangleDraw((Vec2){x + size_x - thickness,y},(Vec2){thickness,size_y},color,flags);
}

void gui2dNumberDraw(real x,real y,int number,real scale,Gui2dFlags flags){
    return;
    char buffer[0x10];
    String string = numberToString(buffer,number);
    gui2dStringDraw(x,y,string,scale,0xFFFFFF,0x1800,flags);
}

void gui2dStringDraw(real x,real y,String string,real scale,int color,real thickness,Gui2dFlags flags){
    real down_offset = 0;
    real offset = 0;
    int mirror_x = flags.invert_x ? -1 : 1;
    int mirror_y = flags.invert_y ? 1 : -1;
    for(int j = 0;j < string.size;j++){
        char string_char = string.data[j];
        if(string_char == '\n'){
            down_offset += FIXED_ONE;
            offset = 0;
        }
        else{
            for(int i = 0;g_vector_font[string_char].position[i][0];i++){
                uint8* coords = &g_vector_font[string_char].position[i][0];
                real offset_transform = realMulR(offset,scale);
                real offset_transform_x = realMulR(down_offset,scale);
                Vec2 coord_transform[] = {
                    {
                        realMulR(intToReal(coords[0]),scale) * mirror_x + x + offset_transform_x * mirror_x,
                        realMulR(intToReal(coords[1]),scale) * mirror_y + y + offset_transform * mirror_y,
                    },
                    {
                        realMulR(intToReal(coords[2]),scale) * mirror_x + x + offset_transform_x * mirror_x,
                        realMulR(intToReal(coords[3]),scale) * mirror_y + y + offset_transform * mirror_y
                    }
                };
                
                gui2dSegmentDraw(coord_transform[0],coord_transform[1],realMulR(thickness,scale),color,flags);
            }
        }
        offset += g_vector_font[string_char].width;
    }
}

void gui2dEllipsesDraw(Vec2 position,Vec2 size,int color,Gui2dFlags flags){
    return;
    position = aspectRatioTransform(position);
    
    size = aspectRatioTransform(size);
        
    if(flags.invert_x)
        position.x = FIXED_ONE - position.x - size.x;
    else
        position.x -= FIXED_ONE;
    
    if(flags.invert_y)
        position.y = FIXED_ONE - position.y - size.y;
    else
        position.y -= FIXED_ONE;

    drawEllipses(&g_surface,position.x,position.y,size.x,size.y,pixelColorToColor(color));
}
