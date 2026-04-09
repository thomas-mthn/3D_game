#ifndef CONSOLE_H
#define CONSOLE_H

#include "string.h"
#include "vec3.h"

structure(ConsoleContent){
    ConsoleContent* next;
    size_t string_length;
    char string_data[];
};
extern ConsoleContent* g_console_content;
extern char g_console_buffer[0x100];
extern int g_console_buffer_index;

void consoleInput(char key);

void print(String string);
void printNumber(int number);
void printNumberNL(int number);
void printVec3(Vec3 v);
void printNL(String string);

void debugPrint(char* string);

#endif
