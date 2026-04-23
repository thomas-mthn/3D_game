#ifndef GUI2D_H
#define GUI2D_H

#include "langext.h"
#include "string.h"

structure(Gui2dFlags){
    bool invert_x : 1;
    bool invert_y : 1;
    bool middle_x : 1;
    bool middle_y : 1;
};

void gui2dRectangleDraw(int x,int y,int size_x,int size_y,int color,Gui2dFlags flags);
void gui2dFrameDraw(int x,int y,int size_x,int size_y,int color,int thickness,Gui2dFlags flags);
void gui2dStringDraw(int x,int y,String string,int scale,int color,int thickness,Gui2dFlags flags);
void gui2dNumberDraw(int x,int y,int number,int scale,Gui2dFlags flags);
void gui2dEllipsesDraw(int x,int y,int size_x,int size_y,int color,Gui2dFlags flags);

#endif
