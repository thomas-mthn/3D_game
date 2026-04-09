#include "console.h"
#include "main.h"
#include "memory.h"
#include "libc.h"
#include "string.h"
#include "thread.h"

#ifdef __linux__
#include "linux/l_main.h"
#endif

static MemoryArena content_arena;

ConsoleContent* g_console_content;

void print(String string){
    ConsoleContent* content = memoryArenaAllocate(&content_arena,string.size + sizeof(ConsoleContent));
    content->string_length = string.size;
    tMemcpy(content->string_data,string.data,string.size);
    content->next = g_console_content;
    g_console_content = content;
#ifdef __linux__
    linuxPrint(string);
#elif !defined(__wasm__)
    win32Print(string);
#endif
}

void debugPrint(char* string){
	print(stringMake(string));
}

void printNumber(int number){
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
    buffer[length_copy] = 0;
    print((String){.data = buffer,.size = length_copy});
}

void printNumberNL(int number){
    printNumber(number);
    print((String)STRING_LITERAL("\n"));
}

void printVec3(Vec3 v){
    printNumber(v.x);
    print((String)STRING_LITERAL(","));
    printNumber(v.y);
    print((String)STRING_LITERAL(","));
    printNumberNL(v.z);
}

void printNL(String string){
    print(string);
    print((String)STRING_LITERAL("\n"));
}

char g_console_buffer[0x100];
int g_console_buffer_index;

void consoleInput(char key){
    if(!key)
        return;
    if(key == KEYTRANSLATE_BACK && g_console_buffer_index){
        g_console_buffer_index -= 1;
        return;
    }
    g_console_buffer[g_console_buffer_index++] = key;
    if(key == '\n'){
        enum{
            COMMAND_QUIT,
            COMMAND_MULTITHREAD,
        };
        String commands[] = {
            [COMMAND_QUIT] = STRING_LITERAL("quit"),
            [COMMAND_MULTITHREAD] = STRING_LITERAL("multi_thread"),
        };
        String command = {.data = g_console_buffer,.size = g_console_buffer_index};
        print(command);
        for(int i = countof(commands);i--;){
            if(!stringCompareSizeInsensitive(command,commands[i]))
                continue;
            switch(i){
                case COMMAND_MULTITHREAD:{
                    g_options.multi_thread ^= true;
                    g_n_threads = g_options.multi_thread ? g_n_thread_available : 1;
                    printNL(g_options.multi_thread ? (String)STRING_LITERAL("true") : (String)STRING_LITERAL("false"));
                    configSave();
                } break;
                case COMMAND_QUIT:{
                    applicationQuit();
                } break;
            }
        }
        g_console_buffer_index = 0;
        return;
    }
}
