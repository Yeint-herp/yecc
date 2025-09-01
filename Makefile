CC := cc
AR := ar

CPPFLAGS := -Isource
CFLAGS   := -Wall -Wextra -Werror -std=gnu23 -pipe -g3 -Wno-trigraphs 
SANFLAGS := -fsanitize=undefined

LDFLAGS  := $(SANFLAGS)
LDLIBS   := -lm

BUILD_DIR := $(abspath ./build)
TARGET := yecc

CFILES := $(shell find -L source -type f -name '*.c' | sed 's|^\./||')
OBJS   := $(addprefix $(BUILD_DIR)/, $(CFILES:.c=.c.o))

TEST_SRCS := $(wildcard tests/*.c)
TEST_BINS := $(patsubst tests/%.c, build/tests/%, $(TEST_SRCS))

LIB_SOURCES := $(filter-out source/main.c, $(CFILES))
LIB_OBJS 	:= $(addprefix $(BUILD_DIR)/, $(LIB_SOURCES:.c=.c.o))
LIB 		:= $(BUILD_DIR)/libyecc.a

.PHONY: all clean reset run test

all: $(TARGET) run

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LDLIBS)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANFLAGS) -c $< -o $@

run: $(TARGET)
	@./$(TARGET)

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "Running $$t..."; ./$$t || exit 1; done

build/tests/%: tests/%.c $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)
	@echo "Built test $@"

$(LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^

clean:
	@clear
	rm -rf $(BUILD_DIR)

reset: clean all
