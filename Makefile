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
  -Wnull-dereference -Wdouble-promotion

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
RELEASE_FLAGS = -O2 -g -DNDEBUG -Werror -D_FORTIFY_SOURCE=2

# Base flags (always present)
BASE_FLAGS = -std=c17 -fPIC -D_GNU_SOURCE

# Build type selection (debug, release, sanitize, tsan, or valgrind)
BUILD ?= debug

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

CLIENT_LIBS ?= -ltalloc -ljansson -luuid -lb64 -lpthread
CLIENT_STATIC_LIBS ?=
SERVER_LIBS ?= -lulfius -ljansson -lcurl -ltalloc -luuid -lb64 -lpthread
SERVER_STATIC_LIBS ?=

COMPLEXITY_THRESHOLD = 15
LINE_LENGTH = 120

# Coverage settings
COVERAGE_DIR = coverage
COVERAGE_CFLAGS = -O0 -fprofile-arcs -ftest-coverage
COVERAGE_LDFLAGS = --coverage
COVERAGE_THRESHOLD = 100

CLIENT_SOURCES = src/client.c src/error.c src/logger.c src/wrapper.c src/array.c
CLIENT_OBJ = $(patsubst src/%.c,build/%.o,$(CLIENT_SOURCES))
CLIENT_TARGET = bin/ikigai

SERVER_SOURCES = src/server.c src/error.c src/logger.c src/config.c src/wrapper.c
SERVER_OBJ = $(patsubst src/%.c,build/%.o,$(SERVER_SOURCES))
SERVER_TARGET = bin/ikigai-server

UNIT_TEST_SOURCES = $(wildcard tests/unit/*_test.c)
UNIT_TEST_TARGETS = $(patsubst tests/unit/%_test.c,build/tests/unit/%_test,$(UNIT_TEST_SOURCES))

INTEGRATION_TEST_SOURCES = $(wildcard tests/integration/*_test.c)
INTEGRATION_TEST_TARGETS = $(patsubst tests/integration/%_test.c,build/tests/integration/%_test,$(INTEGRATION_TEST_SOURCES))

TEST_TARGETS = $(UNIT_TEST_TARGETS) $(INTEGRATION_TEST_TARGETS)

MODULE_SOURCES = src/error.c src/logger.c src/config.c src/wrapper.c src/protocol.c src/array.c
MODULE_OBJ = $(patsubst src/%.c,build/%.o,$(MODULE_SOURCES))

# Test utilities (linked with all tests)
TEST_UTILS_OBJ = build/tests/test_utils.o

.PHONY: all release clean install uninstall check check-unit check-integration check-sanitize check-valgrind check-helgrind check-tsan check-dynamic dist fmt lint cloc ci install-deps coverage help distro-check distro-images distro-images-clean distro-clean distro-package

# Prevent Make from deleting intermediate files (needed for coverage .gcno files)
.SECONDARY:

all: $(CLIENT_TARGET) $(SERVER_TARGET)

release:
	@$(MAKE) all BUILD=release

$(CLIENT_TARGET): $(CLIENT_OBJ) | bin
	$(CC) $(LDFLAGS) -o $@ $^ -Wl,-Bstatic $(CLIENT_STATIC_LIBS) -Wl,-Bdynamic $(CLIENT_LIBS)

$(SERVER_TARGET): $(SERVER_OBJ) | bin
	$(CC) $(LDFLAGS) -o $@ $^ -Wl,-Bstatic $(SERVER_STATIC_LIBS) -Wl,-Bdynamic $(SERVER_LIBS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

build/tests/unit/%_test.o: tests/unit/%_test.c | build/tests/unit
	$(CC) $(CFLAGS) -c -o $@ $<

build/tests/unit/%_test: build/tests/unit/%_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) | build/tests/unit
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit -ltalloc -ljansson -luuid -lb64 -lpthread

build/tests/integration/%_test.o: tests/integration/%_test.c | build/tests/integration
	$(CC) $(CFLAGS) -c -o $@ $<

build/tests/integration/%_test: build/tests/integration/%_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) | build/tests/integration
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit -ltalloc -ljansson -luuid -lb64 -lpthread

build/tests/test_utils.o: tests/test_utils.c tests/test_utils.h | build/tests
	$(CC) $(CFLAGS) -c -o $@ $<

bin:
	mkdir -p bin

build:
	mkdir -p build

build/tests: | build
	mkdir -p build/tests

build/tests/unit: | build/tests
	mkdir -p build/tests/unit

build/tests/integration: | build/tests
	mkdir -p build/tests/integration

# Include dependency files (auto-generated by -MMD -MP)
# The '-' prefix means don't error if files don't exist yet
-include $(wildcard build/*.d)
-include $(wildcard build/tests/*.d)
-include $(wildcard build/tests/unit/*.d)
-include $(wildcard build/tests/integration/*.d)

clean:
	rm -rf build bin $(COVERAGE_DIR)
	@rm -rf distros/dist distros/*/build 2>/dev/null || true
	@find . -name "*.gcda" -o -name "*.gcno" -delete 2>/dev/null || true

install: all
	install -d $(DESTDIR)$(bindir)
	install -m 755 $(CLIENT_TARGET) $(DESTDIR)$(bindir)/ikigai
	install -m 755 $(SERVER_TARGET) $(DESTDIR)$(bindir)/ikigai-server

uninstall:
	rm -f $(DESTDIR)$(bindir)/ikigai
	rm -f $(DESTDIR)$(bindir)/ikigai-server

check: check-unit check-integration
	@echo "All tests passed!"

check-unit: $(UNIT_TEST_TARGETS)
	@echo "Running unit tests..."
	@for test in $(UNIT_TEST_TARGETS); do \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done
	@echo "Unit tests passed!"

check-integration: $(INTEGRATION_TEST_TARGETS)
	@echo "Running integration tests..."
	@for test in $(INTEGRATION_TEST_TARGETS); do \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done
	@echo "Integration tests passed!"

check-sanitize:
	@echo "Building with AddressSanitizer + UndefinedBehaviorSanitizer..."
	@$(MAKE) clean
	@mkdir -p build/tests/unit build/tests/integration
	@$(MAKE) check BUILD=sanitize
	@echo "✓ Sanitizer checks passed!"

check-valgrind:
	@echo "Building for Valgrind with enhanced debug info..."
	@$(MAKE) clean
	@mkdir -p build/tests/unit build/tests/integration
	@$(MAKE) $(TEST_TARGETS) BUILD=valgrind
	@echo "Running tests under Valgrind Memcheck..."
	@ulimit -n 1024; \
	passed=0; failed=0; \
	for test in $(TEST_TARGETS); do \
		echo -n "Valgrind: $$test... "; \
		if valgrind --leak-check=full --show-leak-kinds=all \
		            --track-origins=yes --error-exitcode=1 \
		            --quiet --gen-suppressions=no \
		            ./$$test > /tmp/valgrind-$$$$.log 2>&1; then \
			echo "✓"; \
			passed=$$((passed + 1)); \
		else \
			echo "✗ FAILED"; \
			cat /tmp/valgrind-$$$$.log; \
			failed=$$((failed + 1)); \
		fi; \
		rm -f /tmp/valgrind-$$$$.log; \
	done; \
	echo "Valgrind: $$passed passed, $$failed failed"; \
	[ $$failed -eq 0 ]

check-helgrind:
	@echo "Building for Helgrind with enhanced debug info..."
	@$(MAKE) clean
	@mkdir -p build/tests/unit build/tests/integration
	@$(MAKE) $(TEST_TARGETS) BUILD=valgrind
	@echo "Running tests under Valgrind Helgrind..."
	@ulimit -n 1024; \
	passed=0; failed=0; \
	for test in $(TEST_TARGETS); do \
		echo -n "Helgrind: $$test... "; \
		if valgrind --tool=helgrind --error-exitcode=1 \
		            --history-level=approx --quiet \
		            ./$$test > /tmp/helgrind-$$$$.log 2>&1; then \
			echo "✓"; \
			passed=$$((passed + 1)); \
		else \
			echo "✗ FAILED"; \
			cat /tmp/helgrind-$$$$.log; \
			failed=$$((failed + 1)); \
		fi; \
		rm -f /tmp/helgrind-$$$$.log; \
	done; \
	echo "Helgrind: $$passed passed, $$failed failed"; \
	[ $$failed -eq 0 ]

check-tsan:
	@echo "Building with ThreadSanitizer..."
	@$(MAKE) clean
	@mkdir -p build/tests/unit build/tests/integration
	@$(MAKE) check BUILD=tsan
	@echo "✓ ThreadSanitizer checks passed!"

check-dynamic: check-sanitize check-valgrind check-helgrind check-tsan
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
	@[ ! -d tests/unit ] || uncrustify -c .uncrustify.cfg --replace --no-backup tests/unit/*.c
	@[ ! -d tests/integration ] || uncrustify -c .uncrustify.cfg --replace --no-backup tests/integration/*.c
	@[ ! -f tests/test_utils.c ] || uncrustify -c .uncrustify.cfg --replace --no-backup tests/test_utils.c tests/test_utils.h

lint:
	@echo "Checking complexity in src/*.c..."
	@complexity --threshold=$(COMPLEXITY_THRESHOLD) src/*.c || [ $$? -eq 5 ]
	@echo "Checking complexity in tests/unit/*.c..."
	@[ ! -d tests/unit ] || complexity --threshold=$(COMPLEXITY_THRESHOLD) tests/unit/*.c || [ $$? -eq 5 ]
	@echo "Checking complexity in tests/integration/*.c..."
	@[ ! -d tests/integration ] || complexity --threshold=$(COMPLEXITY_THRESHOLD) tests/integration/*.c || [ $$? -eq 5 ]
	@echo "✓ All complexity checks passed"

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
	@$(MAKE) check CFLAGS="$(CFLAGS) $(COVERAGE_CFLAGS)" LDFLAGS="$(LDFLAGS) $(COVERAGE_LDFLAGS)"
	@echo "Generating coverage report..."
	@mkdir -p $(COVERAGE_DIR)
	@lcov --capture --directory . --output-file $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 --ignore-errors inconsistent,deprecated --quiet
	@lcov --extract $(COVERAGE_DIR)/coverage.info '*/src/*' --output-file $(COVERAGE_DIR)/coverage.info --rc branch_coverage=1 --quiet
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
	libjansson-dev \
	libcurl4-gnutls-dev \
	uuid-dev \
	libb64-dev \
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
	@echo "  all             - Build ikigai and ikigai-server (default: BUILD=debug)"
	@echo "  release         - Build in release mode (shortcut for BUILD=release)"
	@echo "  clean           - Remove all built files, coverage data, and reports"
	@echo "  install         - Install both binaries to $(prefix)"
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
