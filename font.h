#ifndef FONT_H
#define FONT_H

#include "langext.h"

structure(FontChar){
	uint8  position[0x10 - 1][4];
	uint32 width;
};

extern FontChar g_vector_font[0x100];

#endif