# Ikigai - Elegant Makefile
# Phase 1: Compilation only

.PHONY: help clean check-compile
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
BASE_FLAGS = -std=c17 -fPIC -D_GNU_SOURCE -Isrc -I/usr/include/postgresql

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

# Combined compiler flags
CFLAGS = $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(MODE_FLAGS) $(DEP_FLAGS)

# Relaxed flags for vendor files
VENDOR_CFLAGS = $(BASE_FLAGS) $(SECURITY_FLAGS) $(MODE_FLAGS) $(DEP_FLAGS) -Wno-conversion

# Discover all source files
SOURCES = $(shell find src tests -name '*.c' -not -path '*/vendor/*' 2>/dev/null)
VENDOR_SOURCES = $(shell find src/vendor -name '*.c' 2>/dev/null)
ALL_SOURCES = $(SOURCES) $(VENDOR_SOURCES)

# Convert to object files
OBJECTS = $(patsubst %.c,$(BUILDDIR)/%.o,$(SOURCES))
VENDOR_OBJECTS = $(patsubst %.c,$(BUILDDIR)/%.o,$(VENDOR_SOURCES))
ALL_OBJECTS = $(OBJECTS) $(VENDOR_OBJECTS)

# Parallel execution
MAKEFLAGS += --output-sync=line
MAKEFLAGS += --no-print-directory
MAKE_JOBS ?= $(shell expr $$(nproc) / 2)
MAKEFLAGS += -j$(MAKE_JOBS)

# Pattern rule: compile source files from src/
$(BUILDDIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# Pattern rule: compile test files from tests/
$(BUILDDIR)/tests/%.o: tests/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# Pattern rule: compile vendor files with relaxed warnings
$(BUILDDIR)/vendor/%.o: src/vendor/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(VENDOR_CFLAGS) -c $< -o $@

# Include dependency files
-include $(ALL_OBJECTS:.o=.d)

# check-compile: Compile all source files
check-compile:
ifdef FILE
	@obj=$$(echo $(FILE) | sed 's|^src/|$(BUILDDIR)/|; s|^tests/|$(BUILDDIR)/tests/|; s|\.c$$|.o|'); \
	if output=$$($(MAKE) -s $$obj 2>&1); then \
		echo "🟢 $(FILE)"; \
	else \
		echo "🔴 $(FILE)"; \
		echo "$$output" | grep -v '^make\[' | grep -v '^cc1:'; \
		exit 1; \
	fi
else
	@failed=0; \
	for src in $(ALL_SOURCES); do \
		obj=$$(echo $$src | sed 's|^src/|$(BUILDDIR)/|; s|^tests/|$(BUILDDIR)/tests/|; s|\.c$$|.o|'); \
		if output=$$($(MAKE) -s $$obj 2>&1); then \
			echo "🟢 $$src"; \
		else \
			echo "🔴 $$src"; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	if [ $$failed -eq 0 ]; then \
		echo "✅ All files compiled"; \
	else \
		echo "❌ $$failed files failed to compile"; \
		exit 1; \
	fi
endif

# clean: Remove build artifacts
clean:
	@rm -rf $(BUILDDIR)
	@echo "✨ Cleaned"

# help: Show available targets
help:
	@echo "Available targets:"
	@echo "  check-compile  - Compile all source files to .o files"
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
	@echo "  make check-compile BUILD=release    - Compile in release mode"
