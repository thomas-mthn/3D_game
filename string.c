#include "string.h"
#include "memory.h"
#include "libc.h"

static int numberStringLength(int number){
    int length = 0;
    if(!number)
        return 1;
    if(number < 0)
        length += 1;
    while(number){
        number /= 10;
        length += 1;
    }
    return length;
}

bool stringCompareSizeInsensitive(String string,String compare){
    if(string.size < compare.size)
        return false;
    for(int i = compare.size;i--;){
        if(*string.data++ != *compare.data++)
            return false;
    }
    return true;
}

String stringWordSlice(String string){
    String word = {.data = string.data};
    while(*string.data > ' ')
        string.data += 1;
    word.size = string.data - word.data;
    return word;
}

void stringToUpper(String string){
    for(int i = string.size;i--;)
        string.data[i] = string.data[i] >= 'a' && string.data[i] <= 'z' ? string.data[i] & ~0x20 : string.data[i];
}

void stringToLower(String string){
    for(int i = string.size;i--;)
        string.data[i] = string.data[i] >= 'A' && string.data[i] <= 'Z' ? string.data[i] | 0x20 : string.data[i];
}

bool stringCompareSizeCaseInsensitive(String string,String compare){
    if(string.size < compare.size)
        return false;
    for(int i = compare.size;i--;){
        char c1 = string.data[i] >= 'a' && string.data[i] <= 'z' ? string.data[i] & ~0x20 : string.data[i];
        char c2 = compare.data[i] >= 'a' && compare.data[i] <= 'z' ? compare.data[i] & ~0x20 : compare.data[i];
        if(c1 != c2)
            return false;
    }
    return true;
}

String stringConcat(String string,String append){
    String string_new = {
        .data = tMalloc(string.size + append.size),
        .size = string.size + append.size,
        .flags = STRING_MALLOC
    };
    tMemcpy(string_new.data,string.data,string.size);
    tMemcpy(string_new.data + string.size,append.data,append.size);
    return string_new;
}

String stringInString(String string_1,String string_2){
    if(string_1.size < string_2.size) 
        return (String){.data = nullptr,.size = 0};
    for(int i = 0;i <= string_1.size - string_2.size;i++){
        if(stringCompareSizeInsensitive((String){.data = string_1.data + i,.size = string_1.size - i},string_2))
            return (String){.data = string_1.data + i,.size = string_1.size - i};
    }
    return (String){.data = nullptr,.size = 0};
}

String stringMake(char* cstring){
    char* copy = cstring;
    while(*cstring++);
    size_t len = cstring - copy - 1;
    return (String){.data = copy,.size = len};
}

String intToString(char* buffer,int number){
	int length = numberStringLength(number);
	int length_copy;
	bool negative = false;
	int number_copy;

    if(number < 0){
        negative = true;
        number = -number;
    }
	number_copy = number;
	length_copy = length;

    while(length--){
        buffer[length] = number % 10 + '0';
        number /= 10;
    }
    if(negative)
        buffer[0] = '-';
    buffer[length_copy] = 0;
    return (String){.data = buffer,.size = length_copy};
}
