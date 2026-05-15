#ifndef CONSOLE_H
#define CONSOLE_H

#include "string.h"
#include "vec3.h"
#include "vec2.h"
#include "main.h"

structure(Voxel);

structure(ConsoleContent){
    ConsoleContent* next;
    ConsoleContent* previous;
    size_t string_length;
    char string_data[];
};
void consoleInput(KeyTranslate key);

void consoleVoxelDraw(Voxel* voxel,int side);

void print(String string);
void printNumber(int number);
void printNumberNL(int number);
void printVec2(Vec2 v);
void printVec3(Vec3 v);
void printNL(String string);

void debugPrint(char* string);

#define PRINT_VAR(VAR) debugPrint(#VAR);debugPrint(": ");printNumberNL(VAR)
#define PRINT_VEC2(VAR) debugPrint(#VAR);debugPrint(": ");printVec2(VAR)
#define PRINT_VEC3(VAR) debugPrint(#VAR);debugPrint(": ");printVec3(VAR)

extern int g_debug_int1;
extern int g_debug_int2;

#endif
