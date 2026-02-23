# Ikigai - Elegant Makefile
# Build system for ikigai terminal agent

.PHONY: all help clean install uninstall
.DEFAULT_GOAL := all

# Compiler
CC = gcc

# Build directory
BUILDDIR ?= build

# Installation paths (always ~/.local, XDG-aware)
HOME_DIR := $(shell echo $$HOME)
XDG_DATA_HOME   ?= $(shell echo $${XDG_DATA_HOME:-$(HOME_DIR)/.local/share})
XDG_CONFIG_HOME ?= $(shell echo $${XDG_CONFIG_HOME:-$(HOME_DIR)/.config})

bindir    = $(HOME_DIR)/.local/bin
libexecdir = $(HOME_DIR)/.local/libexec
datadir   = $(XDG_DATA_HOME)
configdir = $(XDG_CONFIG_HOME)/ikigai

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
COVERAGE_FLAGS = -O0 -g3 -fprofile-arcs -ftest-coverage

# Coverage settings
COVERAGE_DIR = reports/coverage
COVERAGE_THRESHOLD ?= 90
COVERAGE_LDFLAGS = --coverage

# Base flags
BASE_FLAGS = -std=c17 -fPIC -D_GNU_SOURCE -I. -I/usr/include/postgresql -I/usr/include/libxml2

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

# Dev mode flags (enabled via IKIGAI_DEV environment variable)
ifdef IKIGAI_DEV
  MODE_FLAGS += -DIKIGAI_DEV
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
APP_FILES = $(shell find apps/ikigai -name '*.c' 2>/dev/null)
MOCK_FILES = $(shell find apps/mock-provider -name '*.c' 2>/dev/null)
SHARED_FILES = $(shell find shared -name '*.c' 2>/dev/null)
SRC_FILES = $(APP_FILES) $(SHARED_FILES)
TOOL_FILES = $(shell find tools -name '*.c' 2>/dev/null)
TEST_FILES = $(shell find tests -name '*.c' 2>/dev/null)
VENDOR_FILES = $(shell find vendor -name '*.c' 2>/dev/null)

# Convert to object files
APP_OBJECTS = $(patsubst apps/ikigai/%.c,$(BUILDDIR)/apps/ikigai/%.o,$(APP_FILES))
MOCK_OBJECTS = $(patsubst apps/mock-provider/%.c,$(BUILDDIR)/apps/mock-provider/%.o,$(MOCK_FILES))
SHARED_OBJECTS = $(patsubst shared/%.c,$(BUILDDIR)/shared/%.o,$(SHARED_FILES))
TOOL_OBJECTS = $(patsubst tools/%.c,$(BUILDDIR)/tools/%.o,$(TOOL_FILES))
TEST_OBJECTS = $(patsubst tests/%.c,$(BUILDDIR)/tests/%.o,$(TEST_FILES))
VENDOR_OBJECTS = $(patsubst vendor/%.c,$(BUILDDIR)/vendor/%.o,$(VENDOR_FILES))

# Combined source objects (app + shared)
SRC_OBJECTS = $(APP_OBJECTS) $(SHARED_OBJECTS)

# All objects
ALL_OBJECTS = $(SRC_OBJECTS) $(MOCK_OBJECTS) $(TOOL_OBJECTS) $(TEST_OBJECTS) $(VENDOR_OBJECTS)

# Binary-specific objects
VCR_STUBS = $(BUILDDIR)/tests/helpers/vcr_stubs_helper.o
IKIGAI_OBJECTS = $(SRC_OBJECTS) $(VENDOR_OBJECTS) $(VCR_STUBS)

# Module objects for tests and tools (all src objects + vendor, EXCEPT main.o)
MODULE_OBJ = $(filter-out $(BUILDDIR)/apps/ikigai/main.o,$(SRC_OBJECTS)) $(VENDOR_OBJECTS)

# Tool objects for tests (all tool objects EXCEPT main.o files)
TOOL_MAIN_OBJECTS = $(patsubst tools/%.c,$(BUILDDIR)/tools/%.o,$(shell find tools -name 'main.c' 2>/dev/null))
TOOL_LIB_OBJECTS = $(filter-out $(TOOL_MAIN_OBJECTS),$(TOOL_OBJECTS))

# Discover all tool binaries (convert underscores to hyphens for Unix convention)
TOOL_NAMES = $(shell find tools -name 'main.c' 2>/dev/null | sed 's|tools/||; s|/main.c||')
TOOL_BINARIES = $(patsubst %,libexec/%-tool,$(subst _,-,$(TOOL_NAMES)))

# Discover all test binaries (exclude helpers/ - those are test suite helpers, not standalone tests)
UNIT_TEST_BINARIES = $(patsubst tests/%.c,$(BUILDDIR)/tests/%,$(shell find tests/unit -name '*_test.c' -not -path '*/helpers/*' 2>/dev/null))
INTEGRATION_TEST_BINARIES = $(patsubst tests/%.c,$(BUILDDIR)/tests/%,$(shell find tests/integration -name '*_test.c' -not -path '*/helpers/*' 2>/dev/null))
FUNCTIONAL_TEST_BINARIES = $(patsubst tests/%.c,$(BUILDDIR)/tests/%,$(shell find tests/functional -name '*_test.c' -not -path '*/helpers/*' 2>/dev/null))
TEST_BINARIES = $(UNIT_TEST_BINARIES) $(INTEGRATION_TEST_BINARIES) $(FUNCTIONAL_TEST_BINARIES)

# All binaries
ALL_BINARIES = bin/ikigai bin/mock-provider $(TOOL_BINARIES) $(TEST_BINARIES)

# Parallel execution settings
MAKE_JOBS ?= $(shell nproc=$(shell nproc); echo $$((nproc / 2)))
MAKEFLAGS += --output-sync=line
MAKEFLAGS += --no-print-directory

# Pattern rule: compile source files from apps/ikigai/
$(BUILDDIR)/apps/ikigai/%.o: apps/ikigai/%.c
	@mkdir -p $(dir $@)
	@if $(CC) $(CFLAGS) -c $< -o $@ 2>&1; then \
		echo "🟢 $<"; \
	else \
		echo "🔴 $<"; \
		exit 1; \
	fi

# Pattern rule: compile source files from apps/mock-provider/
$(BUILDDIR)/apps/mock-provider/%.o: apps/mock-provider/%.c
	@mkdir -p $(dir $@)
	@if $(CC) $(CFLAGS) -c $< -o $@ 2>&1; then \
		echo "🟢 $<"; \
	else \
		echo "🔴 $<"; \
		exit 1; \
	fi

# Pattern rule: compile source files from shared/
$(BUILDDIR)/shared/%.o: shared/%.c
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
$(BUILDDIR)/vendor/%.o: vendor/%.c
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
include .make/check-unit.mk
include .make/check-integration.mk
include .make/check-functional.mk
include .make/check-filesize.mk
include .make/check-complexity.mk
include .make/check-sanitize.mk
include .make/check-tsan.mk
include .make/check-valgrind.mk
include .make/check-helgrind.mk
include .make/check-coverage.mk

# all: Build main binary and tools
all:
	@# Phase 1: Compile all required objects in parallel
	@$(MAKE) -k -j$(MAKE_JOBS) $(SRC_OBJECTS) $(MOCK_OBJECTS) $(VENDOR_OBJECTS) $(TOOL_OBJECTS) $(VCR_STUBS) 2>&1 | grep -E "^(🟢|🔴)" || true
	@# Phase 2: Link binaries in parallel
	@$(MAKE) -k -j$(MAKE_JOBS) bin/ikigai bin/mock-provider $(TOOL_BINARIES) 2>&1 | grep -E "^(🟢|🔴)" || true
	@# Check for failures
	@failed=0; \
	for bin in bin/ikigai bin/mock-provider $(TOOL_BINARIES); do \
		[ ! -f "$$bin" ] && failed=$$((failed + 1)); \
	done; \
	if [ $$failed -eq 0 ]; then \
		echo "✅ Build complete"; \
	else \
		echo "❌ $$failed binaries failed to build"; \
		exit 1; \
	fi

# clean: Remove build artifacts
clean:
	@rm -rf $(BUILDDIR) build-sanitize build-tsan build-valgrind build-helgrind build-coverage $(COVERAGE_DIR) IKIGAI_DEBUG.LOG reports/ bin/ libexec/
	@find apps/ tests/ shared/ tools/ vendor/ -name '*.d' -delete 2>/dev/null || true
	@echo "✨ Cleaned"

# install: Install binaries to ~/.local
install: all
	# Create directories
	install -d $(bindir)
	install -d $(libexecdir)/ikigai
	install -d $(configdir)
	install -d $(datadir)/ikigai/migrations
	install -d $(datadir)/ikigai/system
	# Install actual binary to libexec
	install -m 755 bin/ikigai $(libexecdir)/ikigai/ikigai
	# Install tool binaries to libexec
	@for tool in $(TOOL_BINARIES); do \
		install -m 755 $$tool $(libexecdir)/ikigai/; \
	done
	# Generate and install wrapper script to bin
	@printf '#!/bin/bash\n' > $(bindir)/ikigai
	@printf 'IKIGAI_BIN_DIR=%s\n' "$(bindir)" >> $(bindir)/ikigai
	@printf 'IKIGAI_DATA_DIR=$${XDG_DATA_HOME:-$$HOME/.local/share}/ikigai\n' >> $(bindir)/ikigai
	@printf 'IKIGAI_LIBEXEC_DIR=%s/ikigai\n' "$(libexecdir)" >> $(bindir)/ikigai
	@printf 'IKIGAI_CONFIG_DIR=$${XDG_CONFIG_HOME:-$$HOME/.config}/ikigai\n' >> $(bindir)/ikigai
	@printf 'IKIGAI_CACHE_DIR=$${XDG_CACHE_HOME:-$$HOME/.cache}/ikigai\n' >> $(bindir)/ikigai
	@printf 'IKIGAI_STATE_DIR=$${XDG_STATE_HOME:-$$HOME/.local/state}/ikigai\n' >> $(bindir)/ikigai
	@printf 'IKIGAI_RUNTIME_DIR=$${XDG_RUNTIME_DIR:-/run/user/$$(id -u)}/ikigai\n' >> $(bindir)/ikigai
	@printf 'export IKIGAI_BIN_DIR IKIGAI_DATA_DIR IKIGAI_LIBEXEC_DIR IKIGAI_CONFIG_DIR IKIGAI_CACHE_DIR IKIGAI_STATE_DIR IKIGAI_RUNTIME_DIR\n' >> $(bindir)/ikigai
	@printf 'exec %s/ikigai/ikigai "$$@"\n' "$(libexecdir)" >> $(bindir)/ikigai
	@chmod 755 $(bindir)/ikigai
	# Install example credentials (no-clobber)
	@test -f $(configdir)/credentials.example.json || \
		install -m 644 etc/credentials.example.json $(configdir)/credentials.example.json
	# Install data files
	install -m 644 share/migrations/*.sql $(datadir)/ikigai/migrations/
	install -m 644 share/system/* $(datadir)/ikigai/system/
	@echo "Installed to ~/.local"

# uninstall: Remove installed files
uninstall:
	rm -f $(bindir)/ikigai
	rm -f $(libexecdir)/ikigai/ikigai
	@for tool in $(TOOL_BINARIES); do \
		rm -f $(libexecdir)/ikigai/$$(basename $$tool); \
	done
	rmdir $(libexecdir)/ikigai 2>/dev/null || true
ifeq ($(PURGE),1)
	rm -f $(configdir)/credentials.json
	rm -f $(configdir)/credentials.example.json
	rmdir $(configdir) 2>/dev/null || true
	rm -rf $(datadir)/ikigai
endif
	@echo "Uninstalled from ~/.local"

# help: Show available targets
help:
	@echo "Available targets:"
	@echo "  all            - Build main binary and tools (default)"
	@echo "  install        - Install to ~/.local (XDG)"
	@echo "  uninstall      - Remove installed files"
	@echo "  clean          - Remove build artifacts"
	@echo "  help           - Show this help"
	@echo ""
	@echo "Quality check targets:"
	@echo "  check-compile     - Compile all source files to .o files"
	@echo "  check-link        - Link all binaries (main, tools, tests)"
	@echo "  check-unit        - Run unit tests with XML output"
	@echo "  check-integration - Run integration tests with XML output"
	@echo "  check-filesize    - Verify source files under 16KB"
	@echo "  check-complexity  - Verify cyclomatic complexity under threshold (default: 15)"
	@echo "  check-sanitize    - Run tests with AddressSanitizer/UBSan (uses build-sanitize/)"
	@echo "  check-tsan        - Run tests with ThreadSanitizer (uses build-tsan/)"
	@echo "  check-valgrind    - Run tests under Valgrind Memcheck (uses build-valgrind/)"
	@echo "  check-helgrind    - Run tests under Valgrind Helgrind (uses build-helgrind/)"
	@echo "  check-coverage    - Check code coverage meets $(COVERAGE_THRESHOLD)% threshold"
	@echo ""
	@echo "Build modes (BUILD=<mode>):"
	@echo "  debug          - Debug build with symbols (default)"
	@echo "  release        - Optimized release build"
	@echo "  sanitize       - Debug build with address/undefined sanitizers"
	@echo "  tsan           - Debug build with thread sanitizer"
	@echo "  valgrind       - Debug build optimized for Valgrind"
	@echo ""
	@echo "Installation variables:"
	@echo "  PURGE=1        - Remove config files on uninstall"
	@echo ""
	@echo "Quality check variables:"
	@echo "  RAW=1          - Show full unfiltered output (for CI debugging)"
	@echo ""
	@echo "Examples:"
	@echo "  make                                - Build main binary and tools"
	@echo "  make install                        - Install to ~/.local"
	@echo "  make PURGE=1 uninstall              - Uninstall and remove config files"
	@echo "  make check-compile FILE=src/main.c  - Compile single file"
	@echo "  make BUILD=release                  - Build in release mode"
