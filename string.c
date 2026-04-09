#include "string.h"

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
