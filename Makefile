# Compiler
CC = gcc

# Default build type (can override from command line)
BUILD ?= release

# Build directory
BUILD_DIR = build/$(BUILD)

# Common flags
COMMON_FLAGS = -march=x86-64-v2 -w -fno-stack-protector

# Debug / Release flags
ifeq ($(BUILD),debug)
	LINKER_FLAGS = -fsanitize=address,undefined
    CFLAGS = -O0 -g $(COMMON_FLAGS)
else
	LINKER_FLAGS =
    CFLAGS = -O2 -g $(COMMON_FLAGS)
endif

LIBS = -lX11 -lXi

SRCS = audio.c console.c dda.c draw.c draw_soft.c entity.c equib_model.c font.c geometry.c libc.c library.c lighting.c linux/l_main.c linux/l_syscall.c main.c memory.c octree.c octree_render.c opengl.c pathfinding.c physics.c span.c staff.c string.c texture.c texture_markov.c tmath.c thread.c voxel_gui.c voxel_menu.c
#SRCS = linux/l_main.c
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)

TARGET = program

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS) $(LINKER_FLAGS)

# Compile rule with directory creation
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

rls: $(TARGET)
	sudo ./$(TARGET)

dbg:
	$(MAKE) BUILD=debug
	sudo ./$(TARGET)

cln:
	rm -rf build $(TARGET)

bear:
	bear -- $(MAKE) cln all