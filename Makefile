CC := cc
CFLAGS := -Wall -Wextra -Werror -std=gnu23 -pipe -Isource -fsanitize=undefined -g3

BUILD_DIR := $(abspath ./build)
TARGET := yecc

CFILES := $(shell find -L source -type f -name '*.c' | sed 's|^\./||')
OBJS := $(addprefix $(BUILD_DIR)/, $(CFILES:.c=.c.o))
TEST_SRCS := $(wildcard tests/*.c)
TEST_BINS := $(patsubst tests/%.c, build/tests/%, $(TEST_SRCS))

LIB_SOURCES := $(filter-out source/main.c, $(CFILES))
LIB_OBJS := $(addprefix $(BUILD_DIR)/, $(LIB_SOURCES:.c=.c.o))

.PHONY: all clean reset run test

all: $(TARGET) run

$(TARGET): $(OBJS)
	$(CC) -fsanitize=undefined $(OBJS) -o $@

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	@./$(TARGET)

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "Running $$t..."; ./$$t || exit 1; done

build/tests/%: tests/%.c $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@
	@echo "Built test $@"

clean:
	@clear
	rm -rf $(BUILD_DIR)

reset: clean all
