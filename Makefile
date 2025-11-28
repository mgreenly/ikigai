PACKAGE = ikigai
VERSION = $(shell grep '\#define IK_VERSION "' src/version.h | cut -d'"' -f2)

PREFIX ?= /usr/local
bindir ?= $(PREFIX)/bin

CC = gcc

# Distribution list (default: all distros/*/ directories)
DISTROS ?= $(notdir $(patsubst %/,%,$(wildcard distros/*/)))

# Warning flags (always enabled)
WARNING_FLAGS = -Wall -Wextra -Wshadow \
  -Wstrict-prototypes -Wmissing-prototypes -Wwrite-strings \
  -Wformat=2 -Wconversion -Wcast-qual -Wundef \
  -Wdate-time -Winit-self -Wstrict-overflow=2 \
  -Wimplicit-fallthrough -Walloca -Wvla \
  -Wnull-dereference -Wdouble-promotion -Werror

# Security hardening flags (stack protector works at all optimization levels)
SECURITY_FLAGS = -fstack-protector-strong

# Dependency generation flags
DEP_FLAGS = -MMD -MP

# Debug build flags
DEBUG_FLAGS = -O0 -g3 -fno-omit-frame-pointer -DDEBUG

# Sanitizer flags (for sanitize build)
SANITIZE_FLAGS = -fsanitize=address,undefined

# Thread sanitizer flags (for tsan build)
TSAN_FLAGS = -fsanitize=thread

# Valgrind build flags (optimized for backtraces)
VALGRIND_FLAGS = -O0 -g3 -fno-omit-frame-pointer -DDEBUG

# Release build flags (_FORTIFY_SOURCE requires optimization)
RELEASE_FLAGS = -O2 -g -DNDEBUG -D_FORTIFY_SOURCE=2

# Base flags (always present)
BASE_FLAGS = -std=c17 -fPIC -D_GNU_SOURCE

# Build type selection (debug, release, sanitize, tsan, or valgrind)
BUILD ?= debug

# Parallelization settings
MAKE_JOBS ?= $(shell lscpu -b -p=Core,Socket | grep -v '^\#' | sort -u | wc -l)
PARALLEL ?= 0

# Build directory (can be overridden for parallel builds)
BUILDDIR ?= build

ifeq ($(BUILD),release)
  CFLAGS = $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(DEP_FLAGS) $(RELEASE_FLAGS)
else ifeq ($(BUILD),sanitize)
  CFLAGS = $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(DEP_FLAGS) $(DEBUG_FLAGS) $(SANITIZE_FLAGS)
  LDFLAGS = -fsanitize=address,undefined
else ifeq ($(BUILD),tsan)
  CFLAGS = $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(DEP_FLAGS) $(DEBUG_FLAGS) $(TSAN_FLAGS)
  LDFLAGS = -fsanitize=thread
else ifeq ($(BUILD),valgrind)
  CFLAGS = $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(DEP_FLAGS) $(VALGRIND_FLAGS)
else
  CFLAGS = $(BASE_FLAGS) $(WARNING_FLAGS) $(SECURITY_FLAGS) $(DEP_FLAGS) $(DEBUG_FLAGS)
endif

# Allow LDFLAGS override if not set by BUILD type
LDFLAGS ?=

CLIENT_LIBS ?= -ltalloc -luuid -lb64 -lpthread -lutf8proc
CLIENT_STATIC_LIBS ?=

COMPLEXITY_THRESHOLD = 15
NESTING_DEPTH_THRESHOLD = 5
LINE_LENGTH = 120
MAX_FILE_LINES = 500

# Coverage settings
COVERAGE_DIR = coverage
COVERAGE_CFLAGS = -O0 -fprofile-arcs -ftest-coverage
COVERAGE_LDFLAGS = --coverage
COVERAGE_THRESHOLD = 100
LCOV_EXCL_COVERAGE = 308

CLIENT_SOURCES = src/client.c src/error.c src/logger.c src/wrapper.c src/array.c src/byte_array.c src/line_array.c src/terminal.c src/input.c src/input_buffer.c src/input_buffer_multiline.c src/input_buffer_cursor.c src/input_buffer_layout.c src/render.c src/render_cursor.c src/repl.c src/repl_actions.c src/signal_handler.c src/format.c src/pp_helpers.c src/input_buffer_pp.c src/input_buffer_cursor_pp.c src/scrollback.c src/panic.c src/json_allocator.c src/vendor/yyjson/yyjson.c
CLIENT_OBJ = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(CLIENT_SOURCES))
CLIENT_TARGET = bin/ikigai

UNIT_TEST_SOURCES = $(wildcard tests/unit/*/*_test.c)
UNIT_TEST_TARGETS = $(patsubst tests/unit/%_test.c,$(BUILDDIR)/tests/unit/%_test,$(UNIT_TEST_SOURCES))

INTEGRATION_TEST_SOURCES = $(wildcard tests/integration/*_test.c)
INTEGRATION_TEST_TARGETS = $(patsubst tests/integration/%_test.c,$(BUILDDIR)/tests/integration/%_test,$(INTEGRATION_TEST_SOURCES))

PERFORMANCE_TEST_SOURCES = $(wildcard tests/performance/*_perf.c)
PERFORMANCE_TEST_TARGETS = $(patsubst tests/performance/%_perf.c,$(BUILDDIR)/tests/performance/%_perf,$(PERFORMANCE_TEST_SOURCES))

TEST_TARGETS = $(UNIT_TEST_TARGETS) $(INTEGRATION_TEST_TARGETS) $(PERFORMANCE_TEST_TARGETS)

MODULE_SOURCES = src/error.c src/logger.c src/config.c src/wrapper.c src/array.c src/byte_array.c src/line_array.c src/terminal.c src/input.c src/input_buffer.c src/input_buffer_multiline.c src/input_buffer_cursor.c src/input_buffer_layout.c src/repl.c src/repl_actions.c src/signal_handler.c src/render.c src/render_cursor.c src/format.c src/pp_helpers.c src/input_buffer_pp.c src/input_buffer_cursor_pp.c src/scrollback.c src/panic.c src/json_allocator.c src/vendor/yyjson/yyjson.c
MODULE_OBJ = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(MODULE_SOURCES))

# Test utilities (linked with all tests)
TEST_UTILS_OBJ = $(BUILDDIR)/tests/test_utils.o
REPL_RUN_COMMON_OBJ = $(BUILDDIR)/tests/unit/repl/repl_run_test_common.o

.PHONY: all release clean install uninstall check check-unit check-integration check-performance check-sanitize check-valgrind check-helgrind check-tsan check-dynamic dist fmt lint cloc ci install-deps coverage help distro-check distro-images distro-images-clean distro-clean distro-package clean-test-runs $(UNIT_TEST_RUNS) $(INTEGRATION_TEST_RUNS) $(PERFORMANCE_TEST_RUNS)

# Prevent Make from deleting intermediate files (needed for coverage .gcno files)
.SECONDARY:

all: $(CLIENT_TARGET)

release:
	@$(MAKE) clean
	@$(MAKE) all BUILD=release

$(CLIENT_TARGET): $(CLIENT_OBJ) | bin
	$(CC) $(LDFLAGS) -o $@ $^ -Wl,-Bstatic $(CLIENT_STATIC_LIBS) -Wl,-Bdynamic $(CLIENT_LIBS)

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/tests/unit/%_test.o: tests/unit/%_test.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/tests/unit/%_test: $(BUILDDIR)/tests/unit/%_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit -ltalloc -luuid -lb64 -lpthread -lutf8proc

$(BUILDDIR)/tests/integration/%_test.o: tests/integration/%_test.c | $(BUILDDIR)/tests/integration
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/tests/integration/%_test: $(BUILDDIR)/tests/integration/%_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) | $(BUILDDIR)/tests/integration
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit -ltalloc -luuid -lb64 -lpthread -lutf8proc

$(BUILDDIR)/tests/performance/%_perf.o: tests/performance/%_perf.c | $(BUILDDIR)/tests/performance
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/tests/performance/%_perf: $(BUILDDIR)/tests/performance/%_perf.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) | $(BUILDDIR)/tests/performance
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit -ltalloc -luuid -lb64 -lpthread -lutf8proc

$(BUILDDIR)/tests/test_utils.o: tests/test_utils.c tests/test_utils.h | $(BUILDDIR)/tests
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/tests/unit/repl/repl_run_test_common.o: tests/unit/repl/repl_run_test_common.c tests/unit/repl/repl_run_test_common.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# Special rule for repl_run tests that need the common object
$(BUILDDIR)/tests/unit/repl/repl_run_basic_test: $(BUILDDIR)/tests/unit/repl/repl_run_basic_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(REPL_RUN_COMMON_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit -ltalloc -luuid -lb64 -lpthread -lutf8proc

$(BUILDDIR)/tests/unit/repl/repl_run_error_test: $(BUILDDIR)/tests/unit/repl/repl_run_error_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) $(REPL_RUN_COMMON_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit -ltalloc -luuid -lb64 -lpthread -lutf8proc

bin:
	mkdir -p bin

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/tests: | $(BUILDDIR)
	mkdir -p $(BUILDDIR)/tests

$(BUILDDIR)/tests/unit: | $(BUILDDIR)/tests
	mkdir -p $(BUILDDIR)/tests/unit

$(BUILDDIR)/tests/integration: | $(BUILDDIR)/tests
	mkdir -p $(BUILDDIR)/tests/integration

$(BUILDDIR)/tests/performance: | $(BUILDDIR)/tests
	mkdir -p $(BUILDDIR)/tests/performance

# Include dependency files (auto-generated by -MMD -MP)
# The '-' prefix means don't error if files don't exist yet
-include $(wildcard $(BUILDDIR)/*.d)
-include $(wildcard $(BUILDDIR)/tests/*.d)
-include $(wildcard $(BUILDDIR)/tests/unit/*/*.d)
-include $(wildcard $(BUILDDIR)/tests/integration/*.d)

clean:
	rm -rf build build-* bin $(COVERAGE_DIR)
	@rm -rf distros/dist distros/*/build 2>/dev/null || true
	@find . -name "*.gcda" -o -name "*.gcno" -delete 2>/dev/null || true
	@rm -f core.* vgcore.* 2>/dev/null || true

install: all
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(CLIENT_TARGET) $(DESTDIR)$(bindir)/ikigai

uninstall:
	rm -f $(DESTDIR)$(bindir)/ikigai

# Individual test run targets (enables parallel execution)
# Usage: make -j8 check (runs tests in parallel)
# Speedup: ~7.75x faster on typical systems with clean build
UNIT_TEST_RUNS = $(UNIT_TEST_TARGETS:%=%.run)
INTEGRATION_TEST_RUNS = $(INTEGRATION_TEST_TARGETS:%=%.run)
PERFORMANCE_TEST_RUNS = $(PERFORMANCE_TEST_TARGETS:%=%.run)

# Pattern rule to run a test
%.run: %
	@echo "Running $<..."
	@$< || (echo "✗ Test failed: $<" && exit 1)

check: check-unit check-integration
	@echo "All tests passed!"

# Parallel-safe test execution using Make's -j flag
# Each test creates a .run target that depends on the test binary
# This allows Make to build and run tests in parallel when -j is used
check-unit: $(UNIT_TEST_RUNS)
	@echo "Unit tests passed!"

check-integration: $(INTEGRATION_TEST_RUNS)
	@echo "Integration tests passed!"

check-performance: $(PERFORMANCE_TEST_RUNS)
	@echo "Performance tests passed!"

# Clean up .run sentinel files
clean: clean-test-runs

.PHONY: clean-test-runs
clean-test-runs:
	@rm -f $(UNIT_TEST_RUNS) $(INTEGRATION_TEST_RUNS) $(PERFORMANCE_TEST_RUNS)

check-sanitize:
	@echo "Building with AddressSanitizer + UndefinedBehaviorSanitizer..."
	@rm -rf build-sanitize
	@mkdir -p build-sanitize/tests/unit build-sanitize/tests/integration
	@find tests/unit -type d | sed 's|tests/unit|build-sanitize/tests/unit|' | xargs mkdir -p
	@$(MAKE) -j$(MAKE_JOBS) check BUILD=sanitize BUILDDIR=build-sanitize
	@echo "✓ Sanitizer checks passed!"

check-valgrind:
	@echo "Building for Valgrind with enhanced debug info..."
	@rm -rf build-valgrind
	@mkdir -p build-valgrind/tests/unit build-valgrind/tests/integration
	@find tests/unit -type d | sed 's|tests/unit|build-valgrind/tests/unit|' | xargs mkdir -p
	@$(MAKE) -j$(MAKE_JOBS) check BUILD=valgrind BUILDDIR=build-valgrind TEST_TARGETS_VAR=1
	@echo "Running tests under Valgrind Memcheck..."
	@ulimit -n 1024; \
	if ! find build-valgrind/tests -type f -executable | sort | xargs -I {} -P $(MAKE_JOBS) sh -c \
		'echo -n "Valgrind: {}... "; \
		if valgrind --leak-check=full --show-leak-kinds=all \
		            --track-origins=yes --error-exitcode=1 \
		            --quiet --gen-suppressions=no \
		            ./{} > /tmp/valgrind-$$$$.log 2>&1; then \
			echo "✓"; \
		else \
			echo "✗ FAILED"; \
			cat /tmp/valgrind-$$$$.log; \
			rm -f /tmp/valgrind-$$$$.log; \
			exit 1; \
		fi; \
		rm -f /tmp/valgrind-$$$$.log'; then \
		echo "✗ Valgrind checks failed"; \
		exit 1; \
	fi
	@total=$$(find build-valgrind/tests -type f -executable | wc -l); \
	echo "Valgrind: $$total passed, 0 failed"

check-helgrind:
	@echo "Building for Helgrind with enhanced debug info..."
	@rm -rf build-helgrind
	@mkdir -p build-helgrind/tests/unit build-helgrind/tests/integration
	@find tests/unit -type d | sed 's|tests/unit|build-helgrind/tests/unit|' | xargs mkdir -p
	@$(MAKE) -j$(MAKE_JOBS) check BUILD=valgrind BUILDDIR=build-helgrind
	@echo "Running tests under Valgrind Helgrind..."
	@ulimit -n 1024; \
	if ! find build-helgrind/tests -type f -executable | sort | xargs -I {} -P $(MAKE_JOBS) sh -c \
		'echo -n "Helgrind: {}... "; \
		if valgrind --tool=helgrind --error-exitcode=1 \
		            --history-level=approx --quiet \
		            ./{} > /tmp/helgrind-$$$$.log 2>&1; then \
			echo "✓"; \
		else \
			echo "✗ FAILED"; \
			cat /tmp/helgrind-$$$$.log; \
			rm -f /tmp/helgrind-$$$$.log; \
			exit 1; \
		fi; \
		rm -f /tmp/helgrind-$$$$.log'; then \
		echo "✗ Helgrind checks failed"; \
		exit 1; \
	fi
	@total=$$(find build-helgrind/tests -type f -executable | wc -l); \
	echo "Helgrind: $$total passed, 0 failed"

check-tsan:
	@echo "Building with ThreadSanitizer..."
	@rm -rf build-tsan
	@mkdir -p build-tsan/tests/unit build-tsan/tests/integration
	@find tests/unit -type d | sed 's|tests/unit|build-tsan/tests/unit|' | xargs mkdir -p
	@$(MAKE) -j$(MAKE_JOBS) check BUILD=tsan BUILDDIR=build-tsan
	@echo "✓ ThreadSanitizer checks passed!"

check-dynamic:
ifeq ($(PARALLEL),1)
	@echo "Running dynamic analysis checks in parallel..."
	@$(MAKE) -j4 check-sanitize check-valgrind check-helgrind check-tsan
else
	@echo "Running dynamic analysis checks sequentially..."
	@$(MAKE) check-sanitize
	@$(MAKE) check-valgrind
	@$(MAKE) check-helgrind
	@$(MAKE) check-tsan
endif
	@echo "✓ All dynamic analysis checks passed!"

distro-images:
	@echo "Building Docker images for distributions: $(DISTROS)"
	@for distro in $(DISTROS); do \
		echo "Building ikigai-ci-$$distro..."; \
		docker build -f distros/$$distro/Dockerfile -t ikigai-ci-$$distro . || exit 1; \
	done
	@echo "✓ All images built successfully!"

distro-images-clean:
	@echo "Removing Docker images for distributions: $(DISTROS)"
	@for distro in $(DISTROS); do \
		echo "Removing ikigai-ci-$$distro..."; \
		docker rmi ikigai-ci-$$distro 2>/dev/null || true; \
	done
	@echo "✓ All images removed!"

distro-clean:
	@echo "Cleaning build artifacts using $(word 1,$(DISTROS)) Docker image..."
	@docker run --rm --user $$(id -u):$$(id -g) -v "$$(pwd)":/workspace ikigai-ci-$(word 1,$(DISTROS)) bash -c "make clean"
	@echo "✓ Clean complete!"

distro-check:
	@echo "Testing on distributions: $(DISTROS)"
	@for distro in $(DISTROS); do \
		echo ""; \
		echo "=== Testing on $$distro ==="; \
		docker build -f distros/$$distro/Dockerfile -t ikigai-ci-$$distro . || exit 1; \
		docker run --rm --user $$(id -u):$$(id -g) -v "$$(pwd)":/workspace ikigai-ci-$$distro bash -c "make ci" || exit 1; \
		echo "✓ $$distro passed!"; \
	done
	@echo ""
	@echo "✓ All distributions passed!"

distro-package:
	@echo "Building packages for distributions: $(DISTROS)"
	@$(MAKE) dist
	@for distro in $(DISTROS); do \
		echo ""; \
		echo "=== Building package for $$distro ==="; \
		docker build -f distros/$$distro/Dockerfile -t ikigai-ci-$$distro . || exit 1; \
		docker run --rm --user $$(id -u):$$(id -g) -v "$$(pwd)":/workspace ikigai-ci-$$distro bash -c "distros/$$distro/package.sh" || exit 1; \
		echo "✓ $$distro package built!"; \
	done
	@echo ""
	@echo "✓ All packages built!"
	@echo ""
	@ls -lh distros/dist/*.deb distros/dist/*.rpm 2>/dev/null || true

dist:
	@echo "Creating distribution tarball..."
	@mkdir -p distros/dist $(PACKAGE)-$(VERSION)
	@cp -r src tests Makefile README.md LICENSE $(PACKAGE)-$(VERSION)/
	@tar czf distros/dist/$(PACKAGE)-$(VERSION).tar.gz $(PACKAGE)-$(VERSION)
	@rm -rf $(PACKAGE)-$(VERSION)
	@echo "Created distros/dist/$(PACKAGE)-$(VERSION).tar.gz"

fmt:
	@uncrustify -c .uncrustify.cfg --replace --no-backup src/*.c src/*.h
	@find tests/unit -name "*.c" -o -name "*.h" | xargs uncrustify -c .uncrustify.cfg --replace --no-backup
	@[ ! -d tests/integration ] || uncrustify -c .uncrustify.cfg --replace --no-backup tests/integration/*.c
	@[ ! -f tests/test_utils.c ] || uncrustify -c .uncrustify.cfg --replace --no-backup tests/test_utils.c tests/test_utils.h

lint:
	@echo "Checking complexity in src/*.c..."
	@output=$$(complexity --threshold=$(COMPLEXITY_THRESHOLD) src/*.c 2>&1); \
	echo "$$output" | grep -v "^No procedures were scored$$" || [ $$? -eq 1 ]; \
	if echo "$$output" | grep -q "nesting depth reached level [6-9]"; then \
		echo "✗ Nesting depth exceeds threshold ($(NESTING_DEPTH_THRESHOLD))"; \
		echo "$$output" | grep "nesting depth"; \
		exit 1; \
	fi
	@echo "Checking complexity in tests/unit/*/*.c..."
	@output=$$(find tests/unit -name "*.c" -exec complexity --threshold=$(COMPLEXITY_THRESHOLD) {} \; 2>&1); \
	echo "$$output" | grep -v "^No procedures were scored$$" || [ $$? -eq 1 ]; \
	if echo "$$output" | grep -q "nesting depth reached level [6-9]"; then \
		echo "✗ Nesting depth exceeds threshold ($(NESTING_DEPTH_THRESHOLD))"; \
		echo "$$output" | grep "nesting depth"; \
		exit 1; \
	fi
	@echo "Checking complexity in tests/integration/*.c..."
	@if [ -d tests/integration ]; then \
		output=$$(complexity --threshold=$(COMPLEXITY_THRESHOLD) tests/integration/*.c 2>&1); \
		echo "$$output" | grep -v "^No procedures were scored$$" || [ $$? -eq 1 ]; \
		if echo "$$output" | grep -q "nesting depth reached level [6-9]"; then \
			echo "✗ Nesting depth exceeds threshold ($(NESTING_DEPTH_THRESHOLD))"; \
			echo "$$output" | grep "nesting depth"; \
			exit 1; \
		fi; \
	fi
	@echo "✓ All complexity checks passed"
	@echo "Checking file line counts (max: $(MAX_FILE_LINES))..."
	@failed=0; \
	for file in src/*.c src/*.h; do \
		[ -f "$$file" ] || continue; \
		lines=$$(wc -l < "$$file"); \
		if [ $$lines -gt $(MAX_FILE_LINES) ]; then \
			echo "✗ $$file: $$lines lines (exceeds $(MAX_FILE_LINES))"; \
			failed=1; \
		fi; \
	done; \
	for file in $$(find tests/unit -name "*.c"); do \
		lines=$$(wc -l < "$$file"); \
		if [ $$lines -gt $(MAX_FILE_LINES) ]; then \
			echo "✗ $$file: $$lines lines (exceeds $(MAX_FILE_LINES))"; \
			failed=1; \
		fi; \
	done; \
	for file in tests/integration/*.c; do \
		[ -f "$$file" ] || continue; \
		lines=$$(wc -l < "$$file"); \
		if [ $$lines -gt $(MAX_FILE_LINES) ]; then \
			echo "✗ $$file: $$lines lines (exceeds $(MAX_FILE_LINES))"; \
			failed=1; \
		fi; \
	done; \
	for file in docs/*.md docs/*/*.md; do \
		[ -f "$$file" ] || continue; \
		lines=$$(wc -l < "$$file"); \
		if [ $$lines -gt $(MAX_FILE_LINES) ]; then \
			echo "✗ $$file: $$lines lines (exceeds $(MAX_FILE_LINES))"; \
			failed=1; \
		fi; \
	done; \
	if [ $$failed -eq 1 ]; then \
		echo "✗ Some files exceed $(MAX_FILE_LINES) line limit"; \
		exit 1; \
	fi
	@echo "✓ All file line count checks passed"

cloc:
	@cloc src/ tests/ Makefile

ci: lint coverage check-dynamic
	@echo "Building release build to enforce -Werror..."
	@$(MAKE) clean
	@$(MAKE) all BUILD=release
	@echo "✓ All CI checks passed (lint, coverage, dynamic analysis, release build)"
	@$(MAKE) clean

coverage:
	@echo "Building with coverage instrumentation..."
	@$(MAKE) clean
	@find . -name "*.gcda" -o -name "*.gcno" -delete 2>/dev/null || true
	@mkdir -p build/tests/unit build/tests/integration
	@find tests/unit -type d | sed 's|tests/unit|build/tests/unit|' | xargs mkdir -p
	@$(MAKE) -j$(MAKE_JOBS) check CFLAGS="$(CFLAGS) $(COVERAGE_CFLAGS)" LDFLAGS="$(LDFLAGS) $(COVERAGE_LDFLAGS)"
	@echo "Generating coverage report..."
	@mkdir -p $(COVERAGE_DIR)
	@lcov --capture --directory . --output-file $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 --ignore-errors inconsistent,deprecated --quiet
	@lcov --extract $(COVERAGE_DIR)/coverage.info '*/src/*' --output-file $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 --quiet
	@lcov --remove $(COVERAGE_DIR)/coverage.info '*/src/vendor/*' --output-file $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 --quiet
	@echo ""
	@echo "=== Coverage by File ===" > $(COVERAGE_DIR)/summary.txt
	@lcov --list $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 2>&1 >> $(COVERAGE_DIR)/summary.txt
	@echo "" >> $(COVERAGE_DIR)/summary.txt
	@echo "=== Coverage Summary ===" >> $(COVERAGE_DIR)/summary.txt
	@lcov --summary $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 --ignore-errors deprecated 2>&1 >> $(COVERAGE_DIR)/summary.txt
	@echo "" >> $(COVERAGE_DIR)/summary.txt
	@cat $(COVERAGE_DIR)/summary.txt
	@echo "Coverage report saved to $(COVERAGE_DIR)/summary.txt"
	@echo ""
	@echo "Checking coverage thresholds (lines, functions, branches: $(COVERAGE_THRESHOLD)%)..."
	@lcov --summary $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 --ignore-errors deprecated \
		--fail-under-lines $(COVERAGE_THRESHOLD) \
		--fail-under-branches $(COVERAGE_THRESHOLD) 2>&1 | \
		grep -E "(Failed|Coverage report saved)" || true
	@if lcov --summary $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 --ignore-errors deprecated \
		--fail-under-lines $(COVERAGE_THRESHOLD) \
		--fail-under-branches $(COVERAGE_THRESHOLD) >/dev/null 2>&1; then \
		echo "✓ All coverage thresholds met ($(COVERAGE_THRESHOLD)%)"; \
	else \
		echo "✗ Coverage below $(COVERAGE_THRESHOLD)% threshold"; \
		exit 1; \
	fi
	@echo ""
	@echo "Checking LCOV exclusion count..."
	@EXCL_COUNT=$$(grep -r "LCOV_EXCL_" src/ | wc -l); \
	echo "Found $$EXCL_COUNT LCOV_EXCL_* markers (limit: $(LCOV_EXCL_COVERAGE))"; \
	if [ $$EXCL_COUNT -gt $(LCOV_EXCL_COVERAGE) ]; then \
		echo "✗ LCOV exclusions exceed limit ($$EXCL_COUNT > $(LCOV_EXCL_COVERAGE))"; \
		echo "   This indicates new code is using coverage exclusions instead of proper testing."; \
		exit 1; \
	else \
		echo "✓ LCOV exclusion count within limit"; \
	fi

# Default package manager and package list (Debian)
PKG_UPDATE ?= apt-get update
PKG_INSTALL ?= apt-get install -y
PACKAGES ?= \
	build-essential \
	uncrustify \
	complexity \
	check \
	libtalloc-dev \
	libulfius-dev \
	libcurl4-gnutls-dev \
	uuid-dev \
	libb64-dev \
	libutf8proc-dev \
	lcov \
	valgrind

# Include distro-specific overrides if specified
ifdef DISTRO
-include mk/$(DISTRO).mk
endif

install-deps:
	@echo "Installing build dependencies..."
	sudo $(PKG_UPDATE)
	sudo $(PKG_INSTALL) $(PACKAGES)

help:
	@echo "Available targets:"
	@echo "  all             - Build ikigai (default: BUILD=debug)"
	@echo "  release         - Build in release mode (shortcut for BUILD=release)"
	@echo "  clean           - Remove all built files, coverage data, and reports"
	@echo "  install         - Install binary to $(prefix)"
	@echo "  uninstall       - Remove installed files"
	@echo ""
	@echo "Testing targets:"
	@echo "  check           - Build and run all tests (unit + integration)"
	@echo "  check-unit      - Build and run only unit tests"
	@echo "  check-integration - Build and run only integration tests"
	@echo "  check-sanitize  - Run all tests with AddressSanitizer + UBSanitizer"
	@echo "  check-valgrind  - Run all tests under Valgrind Memcheck"
	@echo "  check-helgrind  - Run all tests under Valgrind Helgrind (thread errors)"
	@echo "  check-tsan      - Run all tests with ThreadSanitizer"
	@echo "  check-dynamic   - Run all dynamic analysis (all of the above)"
	@echo ""
	@echo "Quality assurance:"
	@echo "  coverage        - Generate text-based coverage report (requires lcov)"
	@echo "  lint            - Check code complexity (threshold: $(COMPLEXITY_THRESHOLD))"
	@echo "  ci              - Run all CI checks (lint + coverage + dynamic + release)"
	@echo ""
	@echo "Distribution targets:"
	@echo "  distro-images       - Build all Docker images (distros/*/Dockerfile)"
	@echo "  distro-images-clean - Remove all Docker images"
	@echo "  distro-clean        - Clean build artifacts using first Docker image"
	@echo "  distro-check        - Run CI on all supported distributions (via Docker)"
	@echo "  distro-package      - Build distribution packages (.deb, .rpm) in Docker"
	@echo ""
	@echo "Other targets:"
	@echo "  dist            - Create source distribution"
	@echo "  fmt             - Format source code with uncrustify (K&R style, line length: $(LINE_LENGTH))"
	@echo "  cloc            - Count lines of code in src/, tests/, and Makefile"
	@echo "  install-deps    - Install build dependencies (Debian/Ubuntu)"
	@echo ""
	@echo "Build modes (use with any target):"
	@echo "  BUILD=debug    - Debug: -O0 -g3 -DDEBUG (default)"
	@echo "  BUILD=release  - Release: -O2 -g -DNDEBUG -Werror"
	@echo "  BUILD=sanitize - Sanitize: debug + ASan + UBSan"
	@echo "  BUILD=tsan     - TSan: debug + ThreadSanitizer"
	@echo "  BUILD=valgrind - Valgrind: enhanced backtraces"
	@echo ""
	@echo "Examples:"
	@echo "  make all BUILD=release"
	@echo "  make check BUILD=sanitize"
	@echo "  make prefix=/usr install"
	@echo "  make lint COMPLEXITY_THRESHOLD=10"
	@echo "  make fmt LINE_LENGTH=100"
	@echo "  make coverage COVERAGE_LINE_THRESHOLD=90"
	@echo "  make distro-images DISTROS=\"debian fedora\""
	@echo "  make distro-check DISTROS=\"debian\""
