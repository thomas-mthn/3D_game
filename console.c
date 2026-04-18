#include "console.h"
#include "draw.h"
#include "main.h"
#include "memory.h"
#include "string.h"
#include "thread.h"
#include "voxel_menu.h"
#include "font.h"
#include "opengl.h"


#ifdef __linux__
#include "linux/l_main.h"
#endif

static MemoryArena console_content_arena;

static ConsoleContent* console_content;

void print(String string){
    /*
    ConsoleContent* content = memoryArenaAllocate(&console_content_arena,string.size + sizeof(ConsoleContent));
    content->string_length = string.size;
    tMemcpy(content->string_data,string.data,string.size);
    content->next = console_content;
    console_content = content;
    */
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

void printVec2(Vec2 v){
    printNumber(v.x);
    print((String)STRING_LITERAL(","));
    printNumberNL(v.y);
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

static char console_buffer[0x100];
static int console_buffer_index;

static void changeBooleanSetting(bool* setting){
    *setting ^= true;
    printNL(*setting ? (String)STRING_LITERAL("true") : (String)STRING_LITERAL("false"));
    configSave();
}

static void drawConsoleLine(Voxel* voxel,int side,char* buffer,int buffer_index,int buffer_size,int offset_y){
    int draw_length = 0;
    int buffer_length = buffer_size - buffer_index;
    for(int i = 0;i < buffer_length;i++){
        if(draw_length > 0xE0000){
            buffer_length = i;
            break;
        }
        draw_length += g_vector_font[buffer[buffer_index + i]].width;
    }
    Vec2 uv = {0x1000,offset_y * 0x1000};
    String draw_string = {.data = buffer + buffer_index,.size = buffer_length};
    drawGuiString(voxel,side,uv,draw_string,0x1000,0x400);
}

static bool blink_prevention;

void consoleVoxelDraw(Voxel* voxel,int side){
    int offset_y = 2;
    char buffer[0x100];
    int buffer_index = countof(buffer) - 1;
    for(ConsoleContent* content = console_content;content;content = content->next){
        String string = {.data = content->string_data,.size = content->string_length};
        for(int i = content->string_length;i--;){
            if(content->string_data[i] == '\n'){
                drawConsoleLine(voxel,side,buffer,buffer_index + 1,countof(buffer),offset_y);
                offset_y += 1;
                buffer_index = countof(buffer) - 1;
                if(offset_y == 17)
                    goto end_console;
                continue;
            }
            buffer[buffer_index--] = content->string_data[i];
        }
    }
    drawConsoleLine(voxel,side,buffer,buffer_index + 1,countof(buffer),offset_y);
 end_console:
    String input = {.data = console_buffer,.size = console_buffer_index};
    if(g_voxel_interact == voxel && (g_tick >> 6 & 1) || blink_prevention > 0)
        input = stringConcat(input,(String)STRING_LITERAL("_"));
    drawGuiString(voxel,side,(Vec2){0x1800,0x1600},input,0x1000,0x400);
    drawGuiChar(voxel,side,(Vec2){0xC00,0x1600},'>',0x1000,0x400);
    if(g_voxel_interact == voxel && (g_tick >> 6 & 1) || blink_prevention > 0)
        tFree(input.data);
    if(g_tick >> 6 & 1)
        blink_prevention = false;
}

#define COMMAND_LIST                                                  \
    X(QUIT) X(MULTITHREAD) X(LIGHTING_ENGINE) X(RENDERBACKEND)\
        X(SMOOTH_LIGHTING) X(GL_WIREFRAME) X(SPELL) X(LOAD) X(SAVE) X(CREATE) \

void consoleInput(char key){
    if(!key)
        return;
    blink_prevention = true;
    if(key == KEYTRANSLATE_BACK && console_buffer_index){
        console_buffer_index -= 1;
        return;
    }
    console_buffer[console_buffer_index++] = key;
    if(key != '\n')
        return;
    enum{
#define X(name) COMMAND_##name,
        COMMAND_LIST
#undef X
    };
    String commands[] = {
#define X(name) STRING_LITERAL(#name),
        COMMAND_LIST
#undef X
    };
    String command = {.data = console_buffer,.size = console_buffer_index};
    print(command);
    for(int i = countof(commands);i--;){
        if(!stringCompareSizeInsensitive(command,commands[i]))
            continue;
        switch(i){
            case COMMAND_CREATE:{
                worldDestroy();
                worldDefaultGenerate();
                g_voxel_interact = 0;
            } break;
            case COMMAND_SAVE:{
                worldSave(stringWordSlice(stringForwardSlice(command,commands[i].size + 1)));
            } break;
            case COMMAND_LOAD:{
                String world_name = stringWordSlice(stringForwardSlice(command,commands[i].size + 1));
                stringToLower(world_name);
                if(!worldExist(world_name)){
                    print((String)STRING_LITERAL("world does not exist\n"));
                    break;
                }
                worldDestroy();
                worldLoad(world_name);
                g_voxel_interact = 0;
            } break;
            case COMMAND_SPELL:{
                if(!g_equipped_staff)
                    break;
                String spell = stringForwardSlice(command,commands[i].size + 1);
                for(int i = SPELL_ECOUNT;i--;){
                    if(!stringCompareSizeCaseInsensitive(spell,g_spell_names[i]))
                        continue;
                    g_equipped.spell_array[0] = (InventorySlot){.type = INVENTORY_SPELL,.spell_type = i};
                    break;
                }
            } break;
            case COMMAND_GL_WIREFRAME:{
                changeBooleanSetting(&g_options.gl_wireframe);
                openglPolygonFill(!g_options.gl_wireframe);
            } break;
            case COMMAND_RENDERBACKEND:{
                String backend = stringForwardSlice(command,commands[i].size + 1);
                String backends[] = {
                    [RENDER_BACKEND_GL] = STRING_LITERAL("gl"),
                    [RENDER_BACKEND_SOFTWARE] = STRING_LITERAL("software"),
                };
                for(int j = countof(backends);j--;){
                    if(!backends[j].size)
                        continue;
                    if(!stringCompareSizeInsensitive(backend,backends[j]))
                        continue;
                    switch(j){
                        case RENDER_BACKEND_GL:{
                            renderBackendChangeGl();
                        } break;
                        case RENDER_BACKEND_SOFTWARE:{
                            renderBackendChangeSoftware();
                        } break;
                    };
                }
            } break;
            case COMMAND_LIGHTING_ENGINE:{
                changeBooleanSetting(&g_options.lighting_engine);
                if(!g_options.lighting_engine)
                    g_exposure = FIXED_ONE;
            } break;
            case COMMAND_MULTITHREAD:{
                changeBooleanSetting(&g_options.multi_thread);
                g_n_threads = g_options.multi_thread ? g_n_thread_available : 1;
            } break;
            case COMMAND_SMOOTH_LIGHTING:{
                changeBooleanSetting(&g_options.smooth_lighting);
            } break;
            case COMMAND_QUIT:{
                applicationQuit();
            } break;
        }
    }
    console_buffer_index = 0;
    return;
}
