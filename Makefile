#compiler
CC = clang

# Default build type (can override from command line)
BUILD ?= release

BUILD_DIR = build/$(BUILD)

COMMON_FLAGS = -march=x86-64-v2 -w -fno-stack-protector

# Debug / Release flags
ifeq ($(BUILD),debug)
	LINKER_FLAGS =
    CFLAGS = -Og -g $(COMMON_FLAGS)
else ifeq ($(BUILD),debug_slow)
	LINKER_FLAGS =
	CFLAGS = -O0 -g $(COMMON_FLAGS)
else ifeq ($(BUILD),debug_asan)
	LINKER_FLAGS = -fsanitize=address -fsanitize=alignment
	CFLAGS = -Og -g -fsanitize=address -fsanitize=alignment $(COMMON_FLAGS)
else ifeq ($(BUILD),debug_asan_slow)
	LINKER_FLAGS = -fsanitize=address -fsanitize=signed-integer-overflow
	CFLAGS = -O0 -g -fsanitize=address -fsanitize=signed-integer-overflow $(COMMON_FLAGS)
else
	LINKER_FLAGS =
    CFLAGS = -O2 -g $(COMMON_FLAGS)
endif

LIBS = -lX11 -lXi

SRCS = audio.c console.c dda.c draw.c draw_soft.c entity.c equib_model.c font.c geometry.c libc.c library.c lighting.c linux/l_main.c linux/l_syscall.c main.c memory.c octree.c octree_render.c opengl.c pathfinding.c physics.c span.c staff.c string.c texture.c texture_markov.c tmath.c thread.c voxel_gui.c voxel_menu.c
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
	sudo gdb -ex run ./$(TARGET)

cln:
	rm -rf build $(TARGET)

bear:
	bear -- $(MAKE) cln all

dbg_slow:
	$(MAKE) BUILD=debug_slow
	sudo gdb -ex run ./$(TARGET)

dbg_asan:
	$(MAKE) BUILD=debug_asan
	sudo ASAN_OPTIONS=abort_on_error=1 gdb -ex run ./$(TARGET)

dbg_asan_slow:
	$(MAKE) BUILD=debug_asan_slow
	sudo ASAN_OPTIONS=abort_on_error=1 gdb -ex run ./$(TARGET)


