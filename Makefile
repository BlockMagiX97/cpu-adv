CC := cc
CFLAGS := -Wall -Wextra -Werror -std=c23 -Isource -include const.h -g -O0 -fsanitize=undefined -D_XOPEN_SOURCE=700 -D_GNU_SOURCE
CFLAGS += `sdl2-config --cflags`

CLANG_DETECTED := $(shell echo | $(CC) -dM -E -x c - | grep -q '__clang__' && echo 1 || echo 0)

ifeq ($(CLANG_DETECTED),1)
$(error Clang is not allowed. Please use GCC or another compiler.)
endif

BUILD_DIR := $(abspath ./build)
TARGET := cpu

CFILES := $(shell find -L . -type f -name '*.c' | sed 's|^\./||')
OBJS := $(addprefix $(BUILD_DIR)/, $(CFILES:.c=.c.o))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -lpthread -lncurses `sdl2-config --libs` -fsanitize=undefined $(OBJS) -o $@

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	@./$(TARGET)

clean:
	@clear
	rm -rf $(BUILD_DIR)

reset: clean all

.PHONY: all clean reset run
