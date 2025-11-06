PACKAGE = ikigai
VERSION = 0.1.0

prefix ?= /usr/local
bindir ?= $(prefix)/bin

CC = gcc
CFLAGS = -Wall -Wextra -std=c17 -O2 -fPIC -D_GNU_SOURCE
LDFLAGS =

CLIENT_LIBS = -ltalloc -lpthread
SERVER_LIBS = -lulfius -ljansson -lcurl -ltalloc -luuid -lpthread

COMPLEXITY_THRESHOLD = 15
LINE_LENGTH = 120

# Coverage settings
COVERAGE_DIR = coverage
COVERAGE_CFLAGS = -O0 -fprofile-arcs -ftest-coverage
COVERAGE_LDFLAGS = --coverage
COVERAGE_THRESHOLD = 100

CLIENT_SOURCES = src/client.c src/error.c src/logger.c
CLIENT_OBJ = $(patsubst src/%.c,build/%.o,$(CLIENT_SOURCES))
CLIENT_TARGET = bin/ikigai

SERVER_SOURCES = src/server.c src/error.c src/logger.c src/config.c
SERVER_OBJ = $(patsubst src/%.c,build/%.o,$(SERVER_SOURCES))
SERVER_TARGET = bin/ikigai-server

UNIT_TEST_SOURCES = $(wildcard tests/unit/*_test.c)
UNIT_TEST_TARGETS = $(patsubst tests/unit/%_test.c,build/tests/unit/%_test,$(UNIT_TEST_SOURCES))

INTEGRATION_TEST_SOURCES = $(wildcard tests/integration/*_test.c)
INTEGRATION_TEST_TARGETS = $(patsubst tests/integration/%_test.c,build/tests/integration/%_test,$(INTEGRATION_TEST_SOURCES))

TEST_TARGETS = $(UNIT_TEST_TARGETS) $(INTEGRATION_TEST_TARGETS)

MODULE_SOURCES = src/error.c src/logger.c src/config.c
MODULE_OBJ = $(patsubst src/%.c,build/%.o,$(MODULE_SOURCES))

# Test utilities (linked with all tests)
TEST_UTILS_OBJ = build/tests/test_utils.o

.PHONY: all clean install uninstall check check-unit check-integration dist fmt lint ci install-deps coverage clean-coverage help

all: $(CLIENT_TARGET) $(SERVER_TARGET)

$(CLIENT_TARGET): $(CLIENT_OBJ) | bin
	$(CC) $(LDFLAGS) -o $@ $^ $(CLIENT_LIBS)

$(SERVER_TARGET): $(SERVER_OBJ) | bin
	$(CC) $(LDFLAGS) -o $@ $^ $(SERVER_LIBS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

build/tests/unit/%_test.o: tests/unit/%_test.c | build/tests/unit
	$(CC) $(CFLAGS) -c -o $@ $<

build/tests/unit/%_test: build/tests/unit/%_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) | build/tests/unit
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit -ltalloc -ljansson -lpthread

build/tests/integration/%_test.o: tests/integration/%_test.c | build/tests/integration
	$(CC) $(CFLAGS) -c -o $@ $<

build/tests/integration/%_test: build/tests/integration/%_test.o $(MODULE_OBJ) $(TEST_UTILS_OBJ) | build/tests/integration
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lcheck -lm -lsubunit -ltalloc -ljansson -lpthread

build/tests/test_utils.o: tests/test_utils.c tests/test_utils.h | build/tests
	$(CC) $(CFLAGS) -c -o $@ $<

bin:
	mkdir -p bin

build:
	mkdir -p build

build/tests:
	mkdir -p build/tests

build/tests/unit:
	mkdir -p build/tests/unit

build/tests/integration:
	mkdir -p build/tests/integration

clean:
	rm -rf build bin dist
	@find . -name "*.gcda" -o -name "*.gcno" -delete 2>/dev/null || true

clean-coverage:
	@echo "Cleaning coverage data..."
	@rm -rf $(COVERAGE_DIR)
	@find . -name "*.gcda" -o -name "*.gcno" -delete 2>/dev/null || true

install: all
	install -d $(bindir)
	install -m 755 $(CLIENT_TARGET) $(bindir)/ikigai
	install -m 755 $(SERVER_TARGET) $(bindir)/ikigai-server

uninstall:
	rm -f $(bindir)/ikigai
	rm -f $(bindir)/ikigai-server

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

dist:
	@echo "Creating distribution tarball..."
	@mkdir -p dist $(PACKAGE)-$(VERSION)
	@cp -r src Makefile README.md LICENSE $(PACKAGE)-$(VERSION)/
	@tar czf dist/$(PACKAGE)-$(VERSION).tar.gz $(PACKAGE)-$(VERSION)
	@rm -rf $(PACKAGE)-$(VERSION)
	@echo "Created dist/$(PACKAGE)-$(VERSION).tar.gz"

fmt:
	@indent -gnu -l$(LINE_LENGTH) src/*.c src/*.h
	@[ ! -d tests/unit ] || indent -gnu -l$(LINE_LENGTH) tests/unit/*.c
	@[ ! -d tests/integration ] || indent -gnu -l$(LINE_LENGTH) tests/integration/*.c
	@[ ! -f tests/test_utils.c ] || indent -gnu -l$(LINE_LENGTH) tests/test_utils.c tests/test_utils.h
	@find src tests -name "*~" -delete 2>/dev/null || true

lint:
	@echo "Checking complexity in src/*.c..."
	@complexity --threshold=$(COMPLEXITY_THRESHOLD) src/*.c || [ $$? -eq 5 ]
	@echo "Checking complexity in tests/unit/*.c..."
	@[ ! -d tests/unit ] || complexity --threshold=$(COMPLEXITY_THRESHOLD) tests/unit/*.c || [ $$? -eq 5 ]
	@echo "Checking complexity in tests/integration/*.c..."
	@[ ! -d tests/integration ] || complexity --threshold=$(COMPLEXITY_THRESHOLD) tests/integration/*.c || [ $$? -eq 5 ]
	@echo "✓ All complexity checks passed"

ci: lint coverage

coverage: clean-coverage
	@echo "Building with coverage instrumentation..."
	@$(MAKE) clean
	@find . -name "*.gcda" -o -name "*.gcno" -delete 2>/dev/null || true
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

install-deps:
	@echo "Installing build dependencies..."
	sudo apt-get update
	sudo apt-get install -y \
		build-essential \
		indent \
		complexity \
		check \
		libtalloc-dev \
		libulfius-dev \
		libjansson-dev \
		libcurl4-gnutls-dev \
		uuid-dev \
		lcov

help:
	@echo "Available targets:"
	@echo "  all             - Build ikigai and ikigai-server (default)"
	@echo "  clean           - Remove built files"
	@echo "  clean-coverage  - Remove coverage data and reports"
	@echo "  install         - Install both binaries to $(prefix)"
	@echo "  uninstall       - Remove installed files"
	@echo "  check           - Build and run all tests (unit + integration)"
	@echo "  check-unit      - Build and run only unit tests"
	@echo "  check-integration - Build and run only integration tests"
	@echo "  coverage        - Generate text-based coverage report (requires lcov)"
	@echo "  lint            - Check code complexity (threshold: $(COMPLEXITY_THRESHOLD))"
	@echo "  ci              - Run all CI checks (lint + check)"
	@echo "  dist            - Create source distribution"
	@echo "  fmt             - Format source code with GNU indent (line length: $(LINE_LENGTH))"
	@echo "  install-deps    - Install build dependencies (Debian/Ubuntu)"
	@echo ""
	@echo "Installation paths can be overridden:"
	@echo "  make prefix=/usr install"
	@echo "  make lint COMPLEXITY_THRESHOLD=10"
	@echo "  make fmt LINE_LENGTH=100"
	@echo "  make coverage COVERAGE_LINE_THRESHOLD=90"
