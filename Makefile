# Ikigai - Elegant Makefile
# Phase 1: Compilation only

.PHONY: all help clean install uninstall
.DEFAULT_GOAL := all

# Compiler
CC = gcc

# Build directory
BUILDDIR ?= build

# Installation paths
PREFIX ?= /usr/local
bindir ?= $(PREFIX)/bin
libexecdir ?= $(PREFIX)/libexec
datadir ?= $(PREFIX)/share

# Special handling for sysconfdir and configdir based on PREFIX
HOME_DIR := $(shell echo $$HOME)

# Detect install type
IS_OPT_INSTALL := $(if $(filter /opt%,$(PREFIX)),yes,no)
IS_USER_INSTALL := $(if $(findstring /home/,$(PREFIX)),yes,no)

ifeq ($(PREFIX),/usr)
    # /usr uses /etc not /usr/etc
    sysconfdir ?= /etc
    configdir = /etc/ikigai
else ifeq ($(IS_OPT_INSTALL),yes)
    # PREFIX is /opt/* - use PREFIX/etc not PREFIX/etc/ikigai
    sysconfdir ?= $(PREFIX)/etc
    configdir = $(PREFIX)/etc
else ifeq ($(IS_USER_INSTALL),yes)
    # PREFIX is under /home/* - use XDG variables or defaults
    XDG_CONFIG_HOME ?= $(shell echo $${XDG_CONFIG_HOME:-$(HOME_DIR)/.config})
    XDG_DATA_HOME ?= $(shell echo $${XDG_DATA_HOME:-$(HOME_DIR)/.local/share})
    sysconfdir ?= $(XDG_CONFIG_HOME)
    configdir = $(XDG_CONFIG_HOME)/ikigai
    user_datadir = $(XDG_DATA_HOME)/ikigai
else
    # Default: /usr/local and others use PREFIX/etc/ikigai
    sysconfdir ?= $(PREFIX)/etc
    configdir = $(sysconfdir)/ikigai
endif

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

# All objects
ALL_OBJECTS = $(SRC_OBJECTS) $(TOOL_OBJECTS) $(TEST_OBJECTS) $(VENDOR_OBJECTS)

# Binary-specific objects
VCR_STUBS = $(BUILDDIR)/tests/helpers/vcr_stubs_helper.o
IKIGAI_OBJECTS = $(SRC_OBJECTS) $(VENDOR_OBJECTS) $(VCR_STUBS)

# Module objects for tests and tools (all src objects + vendor, EXCEPT main.o)
MODULE_OBJ = $(filter-out $(BUILDDIR)/main.o,$(SRC_OBJECTS)) $(VENDOR_OBJECTS)

# Tool objects for tests (all tool objects EXCEPT main.o files)
TOOL_MAIN_OBJECTS = $(patsubst tools/%.c,$(BUILDDIR)/tools/%.o,$(shell find tools -name 'main.c' 2>/dev/null))
TOOL_LIB_OBJECTS = $(filter-out $(TOOL_MAIN_OBJECTS),$(TOOL_OBJECTS))

# Discover all tool binaries (convert underscores to hyphens for Unix convention)
TOOL_NAMES = $(shell find tools -name 'main.c' 2>/dev/null | sed 's|tools/||; s|/main.c||')
TOOL_BINARIES = $(patsubst %,libexec/ikigai/%-tool,$(subst _,-,$(TOOL_NAMES)))

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
include .make/check-unit.mk
include .make/check-integration.mk
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
	@$(MAKE) -k -j$(MAKE_JOBS) $(SRC_OBJECTS) $(VENDOR_OBJECTS) $(TOOL_OBJECTS) $(VCR_STUBS) 2>&1 | grep -E "^(🟢|🔴)" || true
	@# Phase 2: Link binaries in parallel
	@$(MAKE) -k -j$(MAKE_JOBS) bin/ikigai $(TOOL_BINARIES) 2>&1 | grep -E "^(🟢|🔴)" || true
	@# Check for failures
	@failed=0; \
	for bin in bin/ikigai $(TOOL_BINARIES); do \
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
	@rm -rf $(BUILDDIR) build-sanitize build-tsan build-valgrind build-helgrind build-coverage $(COVERAGE_DIR) IKIGAI_DEBUG.LOG reports/
	@echo "✨ Cleaned"

# install: Install binaries to PREFIX
install: all
	# Create directories
	install -d $(DESTDIR)$(bindir)
	install -d $(DESTDIR)$(libexecdir)/ikigai
	install -d $(DESTDIR)$(configdir)
ifeq ($(IS_USER_INSTALL),yes)
	install -d $(DESTDIR)$(user_datadir)
else
	install -d $(DESTDIR)$(datadir)/ikigai
endif
	# Install actual binary to libexec
	install -m 755 bin/ikigai $(DESTDIR)$(libexecdir)/ikigai/ikigai
	# Install tool binaries to libexec
	@for tool in $(TOOL_BINARIES); do \
		install -m 755 $$tool $(DESTDIR)$(libexecdir)/ikigai/; \
	done
	# Generate and install wrapper script to bin
	@printf '#!/bin/bash\n' > $(DESTDIR)$(bindir)/ikigai
	@printf 'IKIGAI_BIN_DIR=%s\n' "$(bindir)" >> $(DESTDIR)$(bindir)/ikigai
ifeq ($(IS_USER_INSTALL),yes)
	@printf 'IKIGAI_DATA_DIR=%s\n' "$(user_datadir)" >> $(DESTDIR)$(bindir)/ikigai
else
	@printf 'IKIGAI_DATA_DIR=%s\n' "$(datadir)/ikigai" >> $(DESTDIR)$(bindir)/ikigai
endif
	@printf 'IKIGAI_LIBEXEC_DIR=%s\n' "$(libexecdir)/ikigai" >> $(DESTDIR)$(bindir)/ikigai
	@printf 'IKIGAI_CONFIG_DIR=$${XDG_CONFIG_HOME:-$$HOME/.config}/ikigai\n' >> $(DESTDIR)$(bindir)/ikigai
	@printf 'IKIGAI_CACHE_DIR=$${XDG_CACHE_HOME:-$$HOME/.cache}/ikigai\n' >> $(DESTDIR)$(bindir)/ikigai
	@printf 'IKIGAI_STATE_DIR=$${XDG_STATE_HOME:-$$HOME/.local/state}/ikigai\n' >> $(DESTDIR)$(bindir)/ikigai
	@printf 'export IKIGAI_BIN_DIR IKIGAI_DATA_DIR IKIGAI_LIBEXEC_DIR IKIGAI_CONFIG_DIR IKIGAI_CACHE_DIR IKIGAI_STATE_DIR\n' >> $(DESTDIR)$(bindir)/ikigai
	@printf 'exec %s/ikigai/ikigai "$$@"\n' "$(libexecdir)" >> $(DESTDIR)$(bindir)/ikigai
	@chmod 755 $(DESTDIR)$(bindir)/ikigai
	# Install config files
ifeq ($(FORCE),1)
	install -m 644 etc/ikigai/config.json $(DESTDIR)$(configdir)/config.json
	install -m 644 etc/ikigai/credentials.example.json $(DESTDIR)$(configdir)/credentials.example.json
else
	@test -f $(DESTDIR)$(configdir)/config.json || install -m 644 etc/ikigai/config.json $(DESTDIR)$(configdir)/config.json
	@test -f $(DESTDIR)$(configdir)/credentials.example.json || install -m 644 etc/ikigai/credentials.example.json $(DESTDIR)$(configdir)/credentials.example.json
endif
	# Install database migrations
ifeq ($(IS_USER_INSTALL),yes)
	install -d $(DESTDIR)$(user_datadir)/migrations
	install -m 644 share/ikigai/migrations/*.sql $(DESTDIR)$(user_datadir)/migrations/
else
	install -d $(DESTDIR)$(datadir)/ikigai/migrations
	install -m 644 share/ikigai/migrations/*.sql $(DESTDIR)$(datadir)/ikigai/migrations/
endif
	@echo "✅ Installed to $(PREFIX)"

# uninstall: Remove installed files
uninstall:
	rm -f $(DESTDIR)$(bindir)/ikigai
	rm -f $(DESTDIR)$(libexecdir)/ikigai/ikigai
	@for tool in $(TOOL_BINARIES); do \
		rm -f $(DESTDIR)$(libexecdir)/ikigai/$$(basename $$tool); \
	done
	rmdir $(DESTDIR)$(libexecdir)/ikigai 2>/dev/null || true
	rmdir $(DESTDIR)$(libexecdir) 2>/dev/null || true
ifeq ($(PURGE),1)
	rm -f $(DESTDIR)$(configdir)/config.json
	rm -f $(DESTDIR)$(configdir)/credentials.json
	rm -f $(DESTDIR)$(configdir)/credentials.example.json
endif
	rmdir $(DESTDIR)$(configdir) 2>/dev/null || true
ifeq ($(IS_USER_INSTALL),yes)
	rm -rf $(DESTDIR)$(user_datadir)/migrations
	rmdir $(DESTDIR)$(user_datadir) 2>/dev/null || true
else
	rm -rf $(DESTDIR)$(datadir)/ikigai/migrations
	rmdir $(DESTDIR)$(datadir)/ikigai 2>/dev/null || true
endif
	@echo "✅ Uninstalled from $(PREFIX)"

# help: Show available targets
help:
	@echo "Available targets:"
	@echo "  all            - Build main binary and tools (default)"
	@echo "  install        - Install to PREFIX (default: /usr/local)"
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
	@echo "  PREFIX         - Installation prefix (default: /usr/local)"
	@echo "  DESTDIR        - Staging directory for packagers"
	@echo "  FORCE=1        - Overwrite existing config files"
	@echo "  PURGE=1        - Remove config files on uninstall"
	@echo ""
	@echo "Quality check variables:"
	@echo "  RAW=1          - Show full unfiltered output (for CI debugging)"
	@echo ""
	@echo "Examples:"
	@echo "  make                                - Build main binary and tools"
	@echo "  make install                        - Install to /usr/local"
	@echo "  make PREFIX=/opt/ikigai install     - Install to /opt/ikigai"
	@echo "  make PREFIX=~/.local install        - Install to ~/.local (uses XDG paths)"
	@echo "  make check-compile FILE=src/main.c  - Compile single file"
	@echo "  make BUILD=release                  - Build in release mode"
