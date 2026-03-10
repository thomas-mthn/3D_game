#include "w_console.h"
#include "w_kernel.h"

void printNumber(int number){
    if(!GetConsoleWindow())
        AllocConsole();
	int length = 0;
	int length_copy;
	char buffer[0x10];
    bool negative = false;
	int number_copy;

    if(number < 0){
        length++;
        negative = true;
        number = -number;
    }
	number_copy = number;

    if(!number)
        length = 1;
    else{
        while(number_copy){
            number_copy /= 10;
            length++;
        }
    }
	length_copy = length;

    while(length--){
        buffer[length] = number % 10 + '0';
        number /= 10;
    }
    if(negative)
        buffer[0] = '-';
	WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE),buffer,length_copy,0,0);
}

void print(const char* str){
    if(!GetConsoleWindow())
    	AllocConsole();
    const char* end = str;
    while(*end++);
    int l = end - str;
    WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE),str,l,0,0);
}

void printNumberNL(int number){
    printNumber(number);
    print("\n");
}

void printNL(char* str){
    print(str);
    print("\n");
}