CC := cc
AR := ar

CPPFLAGS := -Isource
CFLAGS   := -Wall -Wextra -Werror -std=gnu23 -pipe -g3 -Wno-trigraphs -fPIC
SANFLAGS := -fsanitize=undefined

LDFLAGS  := $(SANFLAGS) -Wl,-rpath,'$$ORIGIN'
LDLIBS   := -lm

SRC_DIR       := source
BUILD_DIR     := $(abspath ./build)
OBJ_DIR       := $(BUILD_DIR)/obj
LIB_DIR       := $(BUILD_DIR)
BIN_DIR       := $(BUILD_DIR)
TEST_BIN_DIR  := $(BUILD_DIR)

MODULES := base context diag lex
TOOLS   := yecc ycc1 yop ybe

MODULE_LIBS :=

define FIND_MODULE
SRCS_$(1) := $(shell find -L $(SRC_DIR)/$(1) -type f -name '*.c' 2>/dev/null)
OBJS_$(1) := $$(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$$(SRCS_$(1)))
LIB_$(1)  := $(LIB_DIR)/libyecc_$(1).so
MODULE_LIBS += $$(LIB_$(1))
endef
$(foreach m,$(MODULES),$(eval $(call FIND_MODULE,$(m))))

TOOL_BINS :=

define FIND_TOOL
TSRCS_$(1) := $(shell find -L $(SRC_DIR)/$(1) -type f -name '*.c' 2>/dev/null)
TOBJS_$(1) := $$(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$$(TSRCS_$(1)))
BIN_$(1)   := $(BIN_DIR)/$(1)
TOOL_BINS  += $$(BIN_$(1))
endef
$(foreach t,$(TOOLS),$(eval $(call FIND_TOOL,$(t))))

TEST_SRCS := $(shell find -L tests -type f -name '*.c' 2>/dev/null)
TEST_BINS := $(patsubst tests/%.c,$(TEST_BIN_DIR)/%,$(TEST_SRCS))

.PHONY: all libs tools test run clean reset print

all: libs tools

libs: $(MODULE_LIBS)

tools: $(TOOL_BINS)

run: $(BIN_DIR)/yecc
	@$(BIN_DIR)/yecc

test: libs $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "Running $$t..."; "$$t" || exit 1; done

clean:
	@clear
	rm -rf $(BUILD_DIR)

reset: clean all

define RULE_MODULE
$$(LIB_$(1)): $$(OBJS_$(1))
	@mkdir -p $(BUILD_DIR)
	$$(CC) -shared $(SANFLAGS) $$(OBJS_$(1)) -o $$@ $(LDLIBS)
	@echo "Linked $$@"
endef
$(foreach m,$(MODULES),$(eval $(call RULE_MODULE,$(m))))

define RULE_TOOL
$$(BIN_$(1)): $$(TOBJS_$(1)) $(MODULE_LIBS)
	@mkdir -p $(BUILD_DIR)
	$$(CC) $(LDFLAGS) $$(TOBJS_$(1)) -o $$@ \
	    -L$(BUILD_DIR) $$(addprefix -lyecc_, $(MODULES)) $(LDLIBS)
	@echo "Linked $$@"
endef
$(foreach t,$(TOOLS),$(eval $(call RULE_TOOL,$(t))))

$(TEST_BIN_DIR)/%: tests/%.c $(MODULE_LIBS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANFLAGS) $< -o $@ \
	    $(LDFLAGS) -L$(BUILD_DIR) $(addprefix -lyecc_, $(MODULES)) $(LDLIBS)
	@echo "Built test $@"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANFLAGS) -c $< -o $@
