# EtherCAT Master Stack Makefile
# Version 1.0.0
# Target: C11 standard

# ============================================================================
# Configuration
# ============================================================================

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic
CFLAGS += -fno-strict-aliasing
CFLAGS += -D_POSIX_C_SOURCE=200809L

# Include directories
INCLUDES = -I./include

# Debug/Release flags
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O2 -DNDEBUG

# Default to debug build
CFLAGS += $(DEBUG_FLAGS)

# Linker flags
LDFLAGS =

# Libraries
LIBS = -lpthread

# ============================================================================
# Directories
# ============================================================================

SRC_DIR = src
INC_DIR = include
OBJ_DIR = build/obj
BIN_DIR = build/bin
LIB_DIR = build/lib
TEST_DIR = tests
EXAMPLE_DIR = examples

# ============================================================================
# Source Files
# ============================================================================

# DLL source files
DLL_SOURCES = $(wildcard $(SRC_DIR)/dll/*.c)

# HAL source files
HAL_SOURCES = $(wildcard $(SRC_DIR)/hal/*.c)

# AL source files
AL_SOURCES = $(wildcard $(SRC_DIR)/al/*.c)

# Master source files
MASTER_SOURCES = $(wildcard $(SRC_DIR)/master/*.c)

# All source files
SOURCES = $(DLL_SOURCES) $(HAL_SOURCES) $(AL_SOURCES) $(MASTER_SOURCES)

# Object files
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))

# Library name
LIB_NAME = libethercat.a
LIB_PATH = $(LIB_DIR)/$(LIB_NAME)

# ============================================================================
# Targets
# ============================================================================

.PHONY: all clean debug release lib test examples help

# Default target
all: lib

# Help target
help:
	@echo "EtherCAT Master Stack Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build library (default, debug mode)"
	@echo "  lib       - Build static library"
	@echo "  debug     - Build with debug flags"
	@echo "  release   - Build with release flags"
	@echo "  test      - Build and run tests"
	@echo "  examples  - Build example applications"
	@echo "  clean     - Remove all build artifacts"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  CC        - C compiler (default: gcc)"
	@echo "  CFLAGS    - Compiler flags"
	@echo "  LDFLAGS   - Linker flags"

# Build library
lib: $(LIB_PATH)

# Debug build
debug: CFLAGS := $(filter-out $(RELEASE_FLAGS),$(CFLAGS))
debug: CFLAGS += $(DEBUG_FLAGS)
debug: clean lib
	@echo "Debug build complete"

# Release build
release: CFLAGS := $(filter-out $(DEBUG_FLAGS),$(CFLAGS))
release: CFLAGS += $(RELEASE_FLAGS)
release: clean lib
	@echo "Release build complete"

# Create static library
$(LIB_PATH): $(OBJECTS)
	@echo "Creating static library: $@"
	@mkdir -p $(LIB_DIR)
	$(AR) rcs $@ $^
	@echo "Library created successfully"

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling: $<"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ============================================================================
# Test Targets
# ============================================================================

TEST_SOURCES = $(wildcard $(TEST_DIR)/dll/*.c)
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.c,$(OBJ_DIR)/test/%.o,$(TEST_SOURCES))
TEST_BINS = $(patsubst $(TEST_DIR)/dll/%.c,$(BIN_DIR)/%,$(TEST_SOURCES))

test: lib $(TEST_BINS)
	@echo "Running tests..."
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done
	@echo "All tests passed"

$(OBJ_DIR)/test/%.o: $(TEST_DIR)/%.c
	@echo "Compiling test: $<"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BIN_DIR)/%: $(OBJ_DIR)/test/dll/%.o $(LIB_PATH)
	@echo "Linking test: $@"
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $< -L$(LIB_DIR) -lethercat $(LIBS) -o $@

# ============================================================================
# Example Targets
# ============================================================================

EXAMPLE_SOURCES = $(wildcard $(EXAMPLE_DIR)/*.c)
EXAMPLE_OBJECTS = $(patsubst $(EXAMPLE_DIR)/%.c,$(OBJ_DIR)/example/%.o,$(EXAMPLE_SOURCES))
EXAMPLE_BINS = $(patsubst $(EXAMPLE_DIR)/%.c,$(BIN_DIR)/%,$(EXAMPLE_SOURCES))

examples: lib $(EXAMPLE_BINS)
	@echo "Examples built successfully"

$(OBJ_DIR)/example/%.o: $(EXAMPLE_DIR)/%.c
	@echo "Compiling example: $<"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BIN_DIR)/%: $(OBJ_DIR)/example/%.o $(LIB_PATH)
	@echo "Linking example: $@"
	@mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) $< -L$(LIB_DIR) -lethercat $(LIBS) -o $@

# ============================================================================
# Clean Target
# ============================================================================

clean:
	@echo "Cleaning build artifacts..."
	rm -rf build
	@echo "Clean complete"

# ============================================================================
# Dependency Generation
# ============================================================================

# Generate dependencies
-include $(OBJECTS:.o=.d)

$(OBJ_DIR)/%.d: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(INCLUDES) -MM -MT $(patsubst %.d,%.o,$@) $< > $@

# ============================================================================
# Info Target
# ============================================================================

info:
	@echo "Build Configuration:"
	@echo "  CC:       $(CC)"
	@echo "  CFLAGS:   $(CFLAGS)"
	@echo "  INCLUDES: $(INCLUDES)"
	@echo "  LDFLAGS:  $(LDFLAGS)"
	@echo "  LIBS:     $(LIBS)"
	@echo ""
	@echo "Source Files:"
	@echo "  DLL:      $(words $(DLL_SOURCES)) files"
	@echo "  HAL:      $(words $(HAL_SOURCES)) files"
	@echo "  AL:       $(words $(AL_SOURCES)) files"
	@echo "  Master:   $(words $(MASTER_SOURCES)) files"
	@echo "  Total:    $(words $(SOURCES)) files"
	@echo ""
	@echo "Output:"
	@echo "  Library:  $(LIB_PATH)"
