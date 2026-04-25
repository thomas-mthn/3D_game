#include "gui2d.h"
#include "draw.h"
#include "main.h"
#include "font.h"

static int aspectScaleGet(void){
    return fixedDivR(tMax(g_surface.window_width,g_surface.window_height),tMin(g_surface.window_width,g_surface.window_height));
}

void gui2dRectangleDraw(int x,int y,int size_x,int size_y,int color,Gui2dFlags flags){
    bool height_bigger = g_surface.window_width < g_surface.window_height;
    int aspect_scale = aspectScaleGet();
    
    if(height_bigger)
        x = fixedDivR(x,aspect_scale);
    else
        y = fixedDivR(y,aspect_scale);
    
    if(height_bigger)
        size_x = fixedDivR(size_x,aspect_scale);
    else
        size_y = fixedDivR(size_y,aspect_scale);

    if(!flags.middle_x){
        if(flags.invert_x)
            x = FIXED_ONE - x - size_x;
        else
            x -= FIXED_ONE;
    }

    if(!flags.middle_y){
        if(flags.invert_y)
            y = FIXED_ONE - y - size_y;
        else
            y -= FIXED_ONE;
    }
    
    drawRectangle(&g_surface,x,y,size_x,size_y,pixelColorToColor(color));
}

static void gui2dSegmentDraw(int x1,int y1,int x2,int y2,int thickness,int color,Gui2dFlags flags){
    bool height_bigger = g_surface.window_width < g_surface.window_height;
    int aspect_scale = aspectScaleGet();
    
    if(height_bigger){
        x1 = fixedDivR(x1,aspect_scale);
        x2 = fixedDivR(x2,aspect_scale);
    }
    else{
        y1 = fixedDivR(y1,aspect_scale);
        y2 = fixedDivR(y2,aspect_scale);
    }

    if(flags.middle_x){
        x1 = -x1;
        x2 = -x2;
    }
    else{
        if(flags.invert_x){
            x1 = FIXED_ONE - x1;
            x2 = FIXED_ONE - x2;
        }
        else{
            x1 -= FIXED_ONE;
            x2 -= FIXED_ONE;
        }
    }

    if(flags.middle_y){
        y1 = -y1;
        y2 = -y2;
    }
    else{
        if(flags.invert_y){
            y1 = FIXED_ONE - y1;
            y2 = FIXED_ONE - y2;
        }
        else{
            y1 -= FIXED_ONE;
            y2 -= FIXED_ONE;
        }
    }
    
    drawSegment(&g_surface,x1,y1,x2,y2,thickness,pixelColorToColor(color));
}

void gui2dFrameDraw(int x,int y,int size_x,int size_y,int color,int thickness,Gui2dFlags flags){
    gui2dRectangleDraw(x,y,size_x,thickness,color,flags);
    gui2dRectangleDraw(x,y,thickness,size_y,color,flags);
    gui2dRectangleDraw(x,y + size_y - thickness,size_x,thickness,color,flags);
    gui2dRectangleDraw(x + size_x - thickness,y,thickness,size_y,color,flags);
}

void gui2dNumberDraw(int x,int y,int number,int scale,Gui2dFlags flags){
    char buffer[0x10];
    String string = numberToString(buffer,number);
    gui2dStringDraw(x,y,string,scale,0xFFFFFF,0x1800,flags);
}

void gui2dStringDraw(int x,int y,String string,int scale,int color,int thickness,Gui2dFlags flags){
    int down_offset = 0;
    int offset = 0;
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
                int offset_transform = fixedMulR(offset,scale);
                int offset_transform_x = fixedMulR(down_offset,scale);
                int coord_transform[] = {
                    (coords[0] * scale * mirror_x >> 8) + x + offset_transform_x * mirror_x,(coords[1] * scale * mirror_y >> 8) + y + offset_transform * mirror_y,
                    (coords[2] * scale * mirror_x >> 8) + x + offset_transform_x * mirror_x,(coords[3] * scale * mirror_y >> 8) + y + offset_transform * mirror_y
                };
                
                gui2dSegmentDraw(coord_transform[0],coord_transform[1],coord_transform[2],coord_transform[3],fixedMulR(thickness,scale),color,flags);
            }
        }
        offset += g_vector_font[string_char].width;
    }
}

void gui2dEllipsesDraw(int x,int y,int size_x,int size_y,int color,Gui2dFlags flags){
    bool height_bigger = g_surface.window_width < g_surface.window_height;
    int aspect_scale = aspectScaleGet();
    
    if(height_bigger)
        x = fixedDivR(x,aspect_scale);
    else
        y = fixedDivR(y,aspect_scale);
    
    if(height_bigger)
        size_x = fixedDivR(size_x,aspect_scale);
    else
        size_y = fixedDivR(size_y,aspect_scale);
        
    if(flags.invert_x)
        x = FIXED_ONE - x - size_x;
    else
        x -= FIXED_ONE;
    
    if(flags.invert_y)
        y = FIXED_ONE - y - size_y;
    else
        y -= FIXED_ONE;

    drawEllipses(&g_surface,x,y,size_x,size_y,pixelColorToColor(color));
}
