#ifndef GUI2D_H
#define GUI2D_H

#include "langext.h"
#include "string.h"
#include "vec2.h"

structure(Gui2dFlags){
    bool invert_x : 1;
    bool invert_y : 1;
    bool middle_x : 1;
    bool middle_y : 1;
};

void gui2dRectangleDraw(Vec2 position,Vec2 size,int color,Gui2dFlags flags);
void gui2dFrameDraw(real x,real y,real size_x,real size_y,int color,real thickness,Gui2dFlags flags);
void gui2dStringDraw(real x,real y,String string,real scale,int color,real thickness,Gui2dFlags flags);
void gui2dNumberDraw(real x,real y,int number,real scale,Gui2dFlags flags);
void gui2dEllipsesDraw(Vec2 position,Vec2 size,int color,Gui2dFlags flags);

#endif
