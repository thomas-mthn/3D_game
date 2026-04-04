#ifndef STRING_H
#define STRING_H

#include "langext.h"

structure(String){
    size_t size;
    char* data;
};

String stringMake(char* cstring);
String intToString(char* buffer,int number);
String stringInString(String string_1,String string_2);

#define STRING_LITERAL(STRING)  {.size = sizeof(STRING) - 1,.data = (char*)(STRING)}

#endif
