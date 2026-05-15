#ifndef FONT_H
#define FONT_H

#include "langext.h"
#include "real.h"

structure(FontChar){
	uint8 position[0x10 - 1][4];
	real  width;
};

extern FontChar g_vector_font[];

#endif
