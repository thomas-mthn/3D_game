#include "console.h"
#include "draw.h"
#include "memory.h"
#include "string.h"
#include "voxel_menu.h"
#include "font.h"
#include "opengl.h"
#include "libc.h"

#include "platform/thread.h"
#include "platform/storage.h"

#ifdef __linux__
#include "linux/l_main.h"
#elif defined(_MSC_VER)
#include "win32/w_main.h"
#endif

int g_debug_int1;
int g_debug_int2;

#define FONT_SIZE (REAL_UNIT * 0x08)

#define COMMAND_LIST                                                  \
    X(QUIT) X(MULTITHREAD) X(LIGHTING_ENGINE) X(RENDERBACKEND)\
        X(SMOOTH_LIGHTING) X(GL_WIREFRAME) X(SPELL) X(LOAD) X(SAVE) X(CREATE) \
        X(FAST_STARTUP) X(TEXTURES) X(DBG_INT1) X(DBG_INT2) X(OCTREE_WIREFRAME) \
        X(RD_OCCLUSION) X(MULTI_SAMPLE) X(RAY_TEST) X(ECHO) X(RD_ENTITY_HITBOX) \
        X(GL_QLIGHTMAP) X(RD_DSHADOW) X(OV_LUMINANCE)


typedef enum{
#define X(name) COMMAND_##name,
    COMMAND_LIST
#undef X
} CommandType;
static String commands[] = {
#define X(name) STRING_LITERAL(#name),
    COMMAND_LIST
#undef X
};

static struct{
    MemoryArena arena;
    ConsoleContent* content;
    ConsoleContent* pointed;
    char buffer[0x100];
    int index;
    int cursor;
    bool blink_prevention;
} console;

void print(String string){
    ConsoleContent* content = memoryArenaAllocate(&console.arena,string.size + sizeof(ConsoleContent));
    content->string_length = string.size;
    tMemcpy(content->string_data,string.data,string.size);
    content->next = console.content;
    content->previous = 0;
    if(console.content)
        console.content->previous = content;
    console.content = content;
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

static void changeBooleanSetting(bool* setting){
    *setting ^= true;
    printNL(*setting ? (String)STRING_LITERAL("true") : (String)STRING_LITERAL("false"));
    configSave();
}

static void drawConsoleLine(Voxel* voxel,int side,char* buffer,int buffer_index,int buffer_size,int offset_y){
    real draw_length = 0;
    int buffer_length = buffer_size - buffer_index;
    for(int i = 0;i < buffer_length;i++){
        if(draw_length > REAL_UNIT * 0x1E00){
            buffer_length = i;
            break;
        }
        draw_length += g_vector_font[buffer[buffer_index + i]].width;
    }
    Vec2 uv = {FONT_SIZE,offset_y * (FONT_SIZE + FONT_SIZE / 4)};
    String draw_string = {.data = buffer + buffer_index,.size = buffer_length};
    drawGuiString(voxel,side,uv,draw_string,FONT_SIZE,REAL_UNIT * 0x02,0xFFFFFF);
}

String autoFillGet(void){
    String command = {.data = console.buffer,.size = console.index};
    for(int i = countof(commands);i--;){
        if(!stringCompareSizeInsensitive(commands[i],command))
            continue;
        return commands[i];
    }
    return (String){0};
}

void consoleVoxelDraw(Voxel* voxel,int side){
    if(!console.index){
        int offset_y = 3;
        char buffer[0x100];
        int buffer_index = countof(buffer) - 1;
        for(ConsoleContent* content = console.content;content;content = content->next){
            String string = {.data = content->string_data,.size = content->string_length};
            for(int i = content->string_length;i--;){
                if(content->string_data[i] != '\n'){
                    buffer[buffer_index--] = content->string_data[i];
                    continue;
                }
                drawConsoleLine(voxel,side,buffer,buffer_index + 1,countof(buffer),offset_y);
                offset_y += 1;
                buffer_index = countof(buffer) - 1;
                if(offset_y == 26)
                    goto end_console;
            }
        }
        drawConsoleLine(voxel,side,buffer,buffer_index + 1,countof(buffer),offset_y);
    }
 end_console:
    String input = {.data = console.buffer,.size = console.index};
    if(g_voxel_interact == voxel && (g_time.tick >> 6 & 1) || console.blink_prevention){
        if(console.cursor == console.index){
            input = stringConcatChar(&g_arena_frame,input,'_');
        }
        else{
            input = stringCopy(&g_arena_frame,input);
            input.data[console.cursor] = '_';
        }
    }
    drawGuiString(voxel,side,(Vec2){REAL_UNIT * 0x10,REAL_UNIT * 0x0D},input,FONT_SIZE,REAL_UNIT * 0x02,0xFFFFFF);
    drawGuiChar(voxel,side,(Vec2){REAL_UNIT * 0x08,REAL_UNIT * 0x0D},'>',FONT_SIZE,REAL_UNIT * 0x02,0xFFFFFF);

    if(g_time.tick >> 6 & 1)
        console.blink_prevention = false;
    
    if(console.index){
        String fill = autoFillGet();
        drawConsoleLine(voxel,side,fill.data,0,fill.size,3);
    }
}

static void commandExecute(CommandType command_type){
    String command = {.data = console.buffer,.size = console.index};
    switch(command_type){
        case COMMAND_OV_LUMINANCE:{
            changeBooleanSetting(&g_options.ov_luminance);
        } break;
        case COMMAND_RD_DSHADOW:{
            changeBooleanSetting(&g_options.rd_dshadow);
        } break;
        case COMMAND_GL_QLIGHTMAP:{
            changeBooleanSetting(&g_options.gl_qlightmap);
        } break;
        case COMMAND_ECHO:{
            String echo = stringForwardSlice(command,commands[command_type].size + 1);
            print(echo);
        } break;
        case COMMAND_RAY_TEST:{
            changeBooleanSetting(&g_options.ray_test);
        } break;
        case COMMAND_MULTI_SAMPLE:{
            String number = stringForwardSlice(command,commands[command_type].size + 1);
            g_options.multi_sample = stringToNumber(number);
            configSave();
        } break;
        case COMMAND_RD_ENTITY_HITBOX:{
            changeBooleanSetting(&g_options.rd_entity_hitbox);
        } break;
        case COMMAND_RD_OCCLUSION:{
            changeBooleanSetting(&g_options.rd_occlusion);
        } break;
        case COMMAND_DBG_INT1:{
            String number = stringForwardSlice(command,commands[command_type].size + 1);
            g_debug_int1 = stringToNumber(number);
        } break;
        case COMMAND_DBG_INT2:{
            String number = stringForwardSlice(command,commands[command_type].size + 1);
            g_debug_int2 = stringToNumber(number);
        } break;
        case COMMAND_OCTREE_WIREFRAME:{
            changeBooleanSetting(&g_options.rd_octree_wireframe);
        } break;
        case COMMAND_TEXTURES:{
            changeBooleanSetting(&g_options.textures);
        } break;
        case COMMAND_FAST_STARTUP:{
            changeBooleanSetting(&g_options.fast_startup);
        } break;
        case COMMAND_CREATE:{
            worldDestroy();
            worldDefaultGenerate();
            g_voxel_interact = 0;
        } break;
        case COMMAND_SAVE:{
            String world_name = stringForwardSlice(command,commands[command_type].size + 1);
            world_name = stringWordSlice(stringConcat(&g_arena_frame,world_name,(String)STRING_LITERAL("\n")));
            if(!world_name.size)
                world_name = (String)STRING_LITERAL("world_1");
            else
                stringToLower(world_name);
            printNL(world_name);
            worldSave(world_name);
        } break;
        case COMMAND_LOAD:{
            String world_name = stringWordSlice(stringForwardSlice(command,commands[command_type].size + 1));
            world_name = stringWordSlice(stringConcat(&g_arena_frame,world_name,(String)STRING_LITERAL("\n")));
            stringToLower(world_name);
            if(!worldExist(world_name)){
                print((String)STRING_LITERAL("world does not exist\n"));
                break;
            }
            worldDestroy();
            worldLoad(world_name);
            octreeRefresh();
            g_voxel_interact = 0;
        } break;
        case COMMAND_SPELL:{
            if(!g_equipped_staff)
                break;
            String spell = stringForwardSlice(command,commands[command_type].size + 1);
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
            String backend = stringForwardSlice(command,commands[command_type].size + 1);
            String backends[] = {
                [RENDER_BACKEND_GL] = STRING_LITERAL("gl"),
                [RENDER_BACKEND_SOFTWARE] = STRING_LITERAL("software"),
            };
            for(int j = countof(backends);j--;){
                if(!backends[j].size)
                    continue;
                if(!stringCompareSizeCaseInsensitive(backend,backends[j]))
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
            applicationExit();
        } break;
    }
}

void consoleInput(KeyTranslate key){
    if(!key)
        return;
    console.blink_prevention = true;
    String command = {.data = console.buffer,.size = console.index};
    switch(key){
        case '\t':{
            String fill = autoFillGet();
            tMemcpy(console.buffer,fill.data,fill.size);
            console.index = fill.size;
            console.cursor = console.index;
        } return;
        case KEY_TRANSLATE_BACK:{
            if(!console.cursor)
                break;
            for(int i = console.cursor - 1;i < console.index;i++)
                console.buffer[i] = console.buffer[i + 1];
            console.index -= 1;
            console.cursor -= 1;
        } return;
        case KEY_TRANSLATE_UP:{
            ConsoleContent* up = console.pointed ? console.pointed->next : console.content;
            if(!up)
                break;
            tMemcpy(console.buffer,up->string_data,up->string_length);
            console.index = up->string_length;
            console.pointed = up;
        } return;
        case KEY_TRANSLATE_DOWN:{
            ConsoleContent* down = console.pointed ? console.pointed->previous : 0;
            if(!down){
                console.index = 0;
                break;
            }
            tMemcpy(console.buffer,down->string_data,down->string_length);
            console.index = down->string_length;
            console.pointed = down;
        } return;
        case KEY_TRANSLATE_LEFT:{
            console.cursor = tMax(0,console.cursor - 1);
        } return;
        case KEY_TRANSLATE_RIGHT:{
            console.cursor = tMin(console.index,console.cursor + 1);
        } return;
        case '\n':{
            for(CommandType i = countof(commands);i--;){
                if(!stringCompareSizeInsensitive(command,commands[i]))
                    continue;
                commandExecute(i);
                console.index = 0;
                console.cursor = 0;
                console.pointed = 0;
                goto executed;
            }
            print((String)STRING_LITERAL("command not found\n"));
        executed:
        } break;
        default:{
            for(int i = console.index;i >= console.cursor;i--)
                console.buffer[i + 1] = console.buffer[i];
            console.buffer[console.cursor++] = key;
            console.index += 1;
            console.pointed = 0;
        } return;
    }
}
