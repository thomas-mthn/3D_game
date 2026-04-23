#ifndef STRING_H
#define STRING_H

#include "langext.h"

structure(String){
    enum{
        STRING_MALLOC = 1 << 0,
    } flags;
    int size;
    char* data;
};

bool stringCompareSizeInsensitive(String string,String compare);
bool stringCompareSizeCaseInsensitive(String string,String compare);
String stringMake(char* cstring);
String intToString(char* buffer,int number);
String stringInString(String string_1,String string_2);
String stringConcat(String string,String append);
String stringWordSlice(String string);
void stringToUpper(String string);
void stringToLower(String string);

#define STRING_LITERAL(STRING) {.size = sizeof(STRING) - 1,.data = (char*)(STRING)}

static String stringForwardSlice(String string,int amount){
    if(string.size <= amount)
        return (String){0};
    return (String){.data = string.data + amount,.size = string.size - amount};
}

#endif
