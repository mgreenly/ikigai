# Ikigai - Elegant Makefile
# Phase 1: Compilation only

.PHONY: help clean
.DEFAULT_GOAL := help

# Compiler
CC = gcc

# Build directory
BUILDDIR ?= build

# Warning flags
WARNING_FLAGS = -Wall -Wextra -Wshadow \
  -Wstrict-prototypes -Wmissing-prototypes -Wwrite-strings \
  -Wformat=2 -Wconversion -Wcast-qual -Wundef \
  -Wdate-time -Winit-self -Wstrict-overflow=2 \
  -Wimplicit-fallthrough -Walloca -Wvla \
  -Wnull-dereference -Wdouble-promotion -Werror

# Security hardening
SECURITY_FLAGS = -fstack-protector-strong

# Dependency generation
DEP_FLAGS = -MMD -MP

# Build mode flags
DEBUG_FLAGS = -O0 -g3 -fno-omit-frame-pointer -DDEBUG
RELEASE_FLAGS = -O2 -g -DNDEBUG -D_FORTIFY_SOURCE=2
SANITIZE_FLAGS = -fsanitize=address,undefined
TSAN_FLAGS = -fsanitize=thread
VALGRIND_FLAGS = -O0 -g3 -fno-omit-frame-pointer -DDEBUG

# Base flags
BASE_FLAGS = -std=c17 -fPIC -D_GNU_SOURCE -I. -Isrc -I/usr/include/postgresql -I/usr/include/libxml2

# Build type selection (debug, release, sanitize, tsan, or valgrind)
BUILD ?= debug

ifeq ($(BUILD),release)
  MODE_FLAGS = $(RELEASE_FLAGS)
else ifeq ($(BUILD),sanitize)
  MODE_FLAGS = $(DEBUG_FLAGS) $(SANITIZE_FLAGS)
else ifeq ($(BUILD),tsan)
  MODE_FLAGS = $(DEBUG_FLAGS) $(TSAN_FLAGS)
else ifeq ($(BUILD),valgrind)
  MODE_FLAGS = $(VALGRIND_FLAGS)
else
  MODE_FLAGS = $(DEBUG_FLAGS)
endif

# Diagnostic flags for cleaner error output
DIAG_FLAGS = -fmax-errors=1 -fno-diagnostics-show-caret

# Linker flags (varies by BUILD mode)
LDFLAGS ?=
ifeq ($(BUILD),sanitize)
  LDFLAGS = -fsanitize=address,undefined -Wl,--gc-sections
else ifeq ($(BUILD),tsan)
  LDFLAGS = -fsanitize=thread -Wl,--gc-sections
else
  LDFLAGS = -Wl,--gc-sections
endif

# Linker libraries
LDLIBS = -ltalloc -luuid -lb64 -lpthread -lutf8proc -lcurl -lpq -lxkbcommon -lxml2

# Function/data sections for gc-sections
SECTION_FLAGS = -ffunction-sections -fdata-sections

# Combined compiler flags
CFLAGS = $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(MODE_FLAGS) $(DEP_FLAGS) $(DIAG_FLAGS) $(SECTION_FLAGS)

# Relaxed flags for vendor files
VENDOR_CFLAGS = $(BASE_FLAGS) $(SECURITY_FLAGS) $(MODE_FLAGS) $(DEP_FLAGS) $(DIAG_FLAGS) -Wno-conversion

# Discover all source files
SRC_FILES = $(shell find src -name '*.c' -not -path '*/vendor/*' 2>/dev/null)
TOOL_FILES = $(shell find tools -name '*.c' 2>/dev/null)
TEST_FILES = $(shell find tests -name '*.c' 2>/dev/null)
VENDOR_FILES = $(shell find src/vendor -name '*.c' 2>/dev/null)

# Convert to object files
SRC_OBJECTS = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SRC_FILES))
TOOL_OBJECTS = $(patsubst tools/%.c,$(BUILDDIR)/tools/%.o,$(TOOL_FILES))
TEST_OBJECTS = $(patsubst tests/%.c,$(BUILDDIR)/tests/%.o,$(TEST_FILES))
VENDOR_OBJECTS = $(patsubst src/vendor/%.c,$(BUILDDIR)/vendor/%.o,$(VENDOR_FILES))

# All sources and objects
ALL_SOURCES = $(SRC_FILES) $(TOOL_FILES) $(TEST_FILES) $(VENDOR_FILES)
ALL_OBJECTS = $(SRC_OBJECTS) $(TOOL_OBJECTS) $(TEST_OBJECTS) $(VENDOR_OBJECTS)

# Binary-specific objects
VCR_STUBS = $(BUILDDIR)/tests/helpers/vcr_stubs_helper.o
IKIGAI_OBJECTS = $(SRC_OBJECTS) $(VENDOR_OBJECTS) $(VCR_STUBS)

# Test helper objects
TEST_UTILS_OBJ = $(BUILDDIR)/tests/test_utils_helper.o
TEST_CONTEXTS_OBJ = $(BUILDDIR)/tests/helpers/test_contexts_helper.o
VCR_HELPER_OBJ = $(BUILDDIR)/tests/helpers/vcr_helper.o
TERMINAL_PTY_HELPERS_OBJ = $(BUILDDIR)/tests/unit/terminal/terminal_pty_helper.o
REQUEST_RESPONSES_TEST_HELPERS_OBJ = $(BUILDDIR)/tests/unit/providers/openai/request_responses_test_helper.o
REQUEST_CHAT_COVERAGE_HELPERS_OBJ = $(BUILDDIR)/tests/unit/providers/openai/request_chat_coverage_helper.o
OPENAI_SERIALIZE_HELPERS_OBJ = $(patsubst tests/%.c,$(BUILDDIR)/tests/%.o,$(shell find tests/unit/providers/openai/helpers -name '*.c' 2>/dev/null))
MESSAGE_TEST_HELPERS_OBJ = $(BUILDDIR)/tests/unit/message/message_tool_call_helper.o $(BUILDDIR)/tests/unit/message/message_tool_result_helper.o $(BUILDDIR)/tests/unit/message/message_thinking_helper.o
AGENT_RESTORE_TEST_HELPERS_OBJ = $(BUILDDIR)/tests/unit/repl/agent_restore_test_helper.o
ANTHROPIC_SERIALIZE_HELPERS_OBJ = $(BUILDDIR)/tests/unit/providers/anthropic/content_block_serialize_helper.o $(BUILDDIR)/tests/unit/providers/anthropic/message_serialize_helper.o

# Module objects for tests and tools (all src objects + vendor, EXCEPT main.o)
MODULE_OBJ = $(filter-out $(BUILDDIR)/main.o,$(SRC_OBJECTS)) $(VENDOR_OBJECTS)

# Tool objects for tests (all tool objects EXCEPT main.o files)
TOOL_MAIN_OBJECTS = $(patsubst tools/%.c,$(BUILDDIR)/tools/%.o,$(shell find tools -name 'main.c' 2>/dev/null))
TOOL_LIB_OBJECTS = $(filter-out $(TOOL_MAIN_OBJECTS),$(TOOL_OBJECTS))

# Discover all tool binaries
TOOL_NAMES = $(shell find tools -name 'main.c' 2>/dev/null | sed 's|tools/||; s|/main.c||')
TOOL_BINARIES = $(patsubst %,libexec/ikigai/%-tool,$(TOOL_NAMES))

# Discover all test binaries (exclude helpers/ - those are test suite helpers, not standalone tests)
UNIT_TEST_BINARIES = $(patsubst tests/%.c,$(BUILDDIR)/tests/%,$(shell find tests/unit -name '*_test.c' -not -path '*/helpers/*' 2>/dev/null))
INTEGRATION_TEST_BINARIES = $(patsubst tests/%.c,$(BUILDDIR)/tests/%,$(shell find tests/integration -name '*_test.c' -not -path '*/helpers/*' 2>/dev/null))
TEST_BINARIES = $(UNIT_TEST_BINARIES) $(INTEGRATION_TEST_BINARIES)

# All binaries
ALL_BINARIES = bin/ikigai $(TOOL_BINARIES) $(TEST_BINARIES)

# Parallel execution settings
MAKE_JOBS ?= $(shell nproc=$(shell nproc); echo $$((nproc / 2)))
MAKEFLAGS += --output-sync=line
MAKEFLAGS += --no-print-directory

# Pattern rule: compile source files from src/
$(BUILDDIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	@if $(CC) $(CFLAGS) -c $< -o $@ 2>&1; then \
		echo "🟢 $<"; \
	else \
		echo "🔴 $<"; \
		exit 1; \
	fi

# Pattern rule: compile test files from tests/
$(BUILDDIR)/tests/%.o: tests/%.c
	@mkdir -p $(dir $@)
	@if $(CC) $(CFLAGS) -c $< -o $@ 2>&1; then \
		echo "🟢 $<"; \
	else \
		echo "🔴 $<"; \
		exit 1; \
	fi

# Pattern rule: compile vendor files with relaxed warnings
$(BUILDDIR)/vendor/%.o: src/vendor/%.c
	@mkdir -p $(dir $@)
	@if $(CC) $(VENDOR_CFLAGS) -c $< -o $@ 2>&1; then \
		echo "🟢 $<"; \
	else \
		echo "🔴 $<"; \
		exit 1; \
	fi

# Pattern rule: compile tool files from tools/
$(BUILDDIR)/tools/%.o: tools/%.c
	@mkdir -p $(dir $@)
	@if $(CC) $(CFLAGS) -c $< -o $@ 2>&1; then \
		echo "🟢 $<"; \
	else \
		echo "🔴 $<"; \
		exit 1; \
	fi

# Include dependency files
-include $(ALL_OBJECTS:.o=.d)

# Include check targets
include .make/check-compile.mk
include .make/check-link.mk

# clean: Remove build artifacts
clean:
	@rm -rf $(BUILDDIR)
	@echo "✨ Cleaned"

# help: Show available targets
help:
	@echo "Available targets:"
	@echo "  check-compile  - Compile all source files to .o files"
	@echo "  check-link     - Link the main ikigai binary"
	@echo "  clean          - Remove build artifacts"
	@echo "  help           - Show this help"
	@echo ""
	@echo "Build modes (BUILD=<mode>):"
	@echo "  debug          - Debug build with symbols (default)"
	@echo "  release        - Optimized release build"
	@echo "  sanitize       - Debug build with address/undefined sanitizers"
	@echo "  tsan           - Debug build with thread sanitizer"
	@echo "  valgrind       - Debug build optimized for Valgrind"
	@echo ""
	@echo "Examples:"
	@echo "  make check-compile              - Compile all files"
	@echo "  make check-compile FILE=src/main.c  - Compile single file"
	@echo "  make check-link                 - Link bin/ikigai"
	@echo "  make check-compile BUILD=release    - Compile in release mode"
