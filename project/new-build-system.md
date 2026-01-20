# New Build System Architecture

## Overview

The ikigai build system is designed for simplicity, modularity, and elegant output. It consists of a main orchestrating Makefile and modular check-* target files.

## File Structure

```
Makefile                    # Orchestration: variables, pattern rules, includes
.make/
  check-compile.mk         # Compilation checking
  check-link.mk            # Linking checking
  check-filesize.mk        # File size limits
  check-complexity.mk      # Cyclomatic complexity
  check-unit.mk            # Unit tests
  check-integration.mk     # Integration tests
  check-coverage.mk        # Code coverage
  check-sanitize.mk        # Address/UB sanitizers
  check-tsan.mk            # Thread sanitizer
  check-valgrind.mk        # Valgrind memcheck
  check-helgrind.mk        # Valgrind helgrind
.claude/scripts/
  check-compile            # Harness wrapper (calls make with --no-spinner)
  check-link
  ... (one per check-* target)
```

## Main Makefile Responsibilities

The top-level Makefile contains:

1. **Variables**: All compiler flags, build modes, source discovery
2. **Pattern Rules**: How to build .o files, binaries, etc.
3. **Core Targets**: `clean`, `help`
4. **Includes**: `include .make/*.mk` for check-* targets

**Critical Rule**: The Makefile NEVER lists individual files. All source discovery uses pattern-based `find` commands.

### Source Discovery Pattern

```makefile
# Discover all source files (never hardcode lists)
SRC_FILES = $(shell find src -name '*.c' -not -path '*/vendor/*' 2>/dev/null)
TEST_FILES = $(shell find tests -name '*.c' 2>/dev/null)
VENDOR_FILES = $(shell find src/vendor -name '*.c' 2>/dev/null)

# Convert to object files
SRC_OBJECTS = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SRC_FILES))
TEST_OBJECTS = $(patsubst tests/%.c,$(BUILDDIR)/tests/%.o,$(TEST_FILES))
VENDOR_OBJECTS = $(patsubst src/vendor/%.c,$(BUILDDIR)/vendor/%.o,$(VENDOR_FILES))

ALL_SOURCES = $(SRC_FILES) $(TEST_FILES) $(VENDOR_FILES)
ALL_OBJECTS = $(SRC_OBJECTS) $(TEST_OBJECTS) $(VENDOR_OBJECTS)
```

## .make/*.mk File Pattern

Each check-* target lives in its own `.make/*.mk` file. The file is self-contained and uses variables defined in the main Makefile.

### Standard Structure

```makefile
# .make/check-<name>.mk
.PHONY: check-<name>
check-<name>:
ifdef FILE
	# Single file mode logic
	@... handle $(FILE) ...
else
	# All files mode logic
	@... handle all files in parallel ...
endif
```

### File Responsibilities

Each `.make/*.mk` file:
- Defines ONE check-* target
- Handles both `FILE=` mode and bulk mode
- Produces standardized 🟢/🔴 output
- Exits with proper status codes
- Is 20-40 lines total

## The 10 Check Targets (Three-Tier Hierarchy)

The build system has exactly 10 quality check targets. Each target exists at three levels forming a complete check-fix workflow:

### The Three Tiers

For each of the 10 checks, there exists:

1. **`make check-<name>`** - The Make target (in `.make/check-<name>.mk`)
   - Core implementation
   - Produces structured 🟢/🔴 output
   - Used directly by developers

2. **`check-<name>`** - The harness wrapper script (in `.claude/scripts/`)
   - Thin wrapper: `exec make check-<name> "$@" --no-spinner`
   - Entry point for automated harness system
   - Passes through to make target

3. **`fix-<name>`** - The fix harness (in `.claude/harness/<name>/`)
   - Spawns sub-agents to fix failures
   - Reads 🟢/🔴 output from check-<name>
   - Escalates: sonnet:think → opus:think → opus:ultrathink
   - Maintains history.md for learning across attempts

### The 10 Complete Hierarchies

1. **compile**: `make check-compile` → `check-compile` → `fix-compile`
   - Compile all source files to .o files

2. **link**: `make check-link` → `check-link` → `fix-link`
   - Link all binaries (main, tests, tools)

3. **filesize**: `make check-filesize` → `check-filesize` → `fix-filesize`
   - Verify files are under size limits

4. **complexity**: `make check-complexity` → `check-complexity` → `fix-complexity`
   - Check cyclomatic complexity and nesting depth

5. **unit**: `make check-unit` → `check-unit` → `fix-unit`
   - Run unit tests

6. **integration**: `make check-integration` → `check-integration` → `fix-integration`
   - Run integration tests

7. **coverage**: `make check-coverage` → `check-coverage` → `fix-coverage`
   - Generate and enforce coverage thresholds

8. **sanitize**: `make check-sanitize` → `check-sanitize` → `fix-sanitize`
   - Run tests with AddressSanitizer and UndefinedBehaviorSanitizer

9. **tsan**: `make check-tsan` → `check-tsan` → `fix-tsan`
   - Run tests with ThreadSanitizer

10. **valgrind**: `make check-valgrind` → `check-valgrind` → `fix-valgrind`
    - Run tests under Valgrind Memcheck

11. **helgrind**: `make check-helgrind` → `check-helgrind` → `fix-helgrind`
    - Run tests under Valgrind Helgrind

### Workflow Example

```bash
# Developer runs check manually
$ make check-compile
🟢 src/main.c
🔴 src/agent.c
❌ 1 files failed to compile

# Automated harness invokes check and fix
$ .claude/scripts/check-compile
[JSON output for harness]

# Harness spawns fix agent
$ .claude/harness/compile/fix
[Sub-agent attempts to fix src/agent.c]
[Commits on success, reverts on failure]
```

## Output Format Requirements

All check-* targets follow the same output pattern for consistency and parseability.

### Example Outputs

#### Bulk Mode: Complete Success

```bash
$ make check-compile
🟢 src/main.c
🟢 src/config.c
🟢 src/agent.c
🟢 src/error.c
🟢 src/logger.c
🟢 src/repl.c
🟢 tests/unit/config_test.c
🟢 tests/unit/agent_test.c
🟢 src/vendor/yyjson/yyjson.c
🟢 src/vendor/fzy/match.c
✅ All files compiled
$ echo $?
0
```

#### Bulk Mode: Partial Failure

```bash
$ make check-compile
🟢 src/main.c
🟢 src/config.c
🔴 src/agent.c
🟢 src/error.c
🟢 src/logger.c
🔴 src/repl.c
🟢 tests/unit/config_test.c
🟢 tests/unit/agent_test.c
🔴 tests/unit/repl_test.c
🟢 src/vendor/yyjson/yyjson.c
🟢 src/vendor/fzy/match.c
❌ 3 files failed to compile
make: *** [.make/check-compile.mk:30: check-compile] Error 1
$ echo $?
2
```

**Bulk Mode Rules:**
- One line per file: `🟢 filename` or `🔴 filename`
- Files process in parallel, output may be interleaved but synchronized per line
- Final summary: `✅ All files <verb>` (success) or `❌ N files failed to <verb>` (failure)
- No other output (no "Building...", no progress bars, no verbose logs)
- Exit code 0 on success, non-zero on failure

#### Single File Mode: Success

```bash
$ make check-compile FILE=src/main.c
🟢 src/main.c
$ echo $?
0
```

#### Single File Mode: Failure

```bash
$ make check-compile FILE=src/agent.c
🔴 src/agent.c:42:5: error: 'unknown_var' undeclared (first use in this function)
$ echo $?
2
```

#### Single File Mode: Multiple Errors (One Line Per Error)

```bash
$ make check-compile FILE=tests/unit/web_search_brave_direct_test.c
🔴 tests/unit/web_search_brave_direct_test.c:1:10: fatal error: web_search_brave.h: No such file or directory
$ echo $?
2
```

**Single File Mode Rules:**
- Success: Exactly one line `🟢 filename`
- Failure: One `🔴` line per error/issue in that file
- Each error line starts with `🔴 `
- Format: `🔴 filename:line:col: error message` (follows compiler error format)
- With `-fmax-errors=1`, only first error shown per file
- No summary line in single-file mode
- Exit code 0 on success, non-zero on failure

### Compiler Flags for Clean Output

To ensure one-line-per-error output, use these flags:

```makefile
DIAG_FLAGS = -fmax-errors=1 -fno-diagnostics-show-caret
```

## Target Hierarchy (Harness Integration)

The build system integrates with the `.claude/harness/` system through a three-level hierarchy:

### Level 1: `make check-<name>`
- Direct make invocation
- Shows all output (🟢/🔴 lines, spinners, etc.)
- Used for manual developer workflow

### Level 2: `.claude/scripts/check-<name>`
- Thin wrapper script
- Calls `make check-<name> --no-spinner`
- Used by harness system

```bash
#!/usr/bin/env bash
exec make check-<name> "$@" --no-spinner
```

### Level 3: `.claude/harness/<name>/fix`
- Spawns sub-agents to fix failures
- Reads structured 🟢/🔴 output
- Escalates through sonnet:think → opus:think → opus:ultrathink
- Maintains history.md for cross-attempt learning

## Parallelization (CRITICAL REQUIREMENT)

**ALL check-* targets MUST honor MAKE_JOBS and run in parallel by default.**

```makefile
MAKE_JOBS ?= $(shell nproc=$(shell nproc); echo $$((nproc / 2)))
```

### Requirements

1. **Default parallel execution**: Every check-* target that processes multiple files MUST use `-j$(MAKE_JOBS)`
2. **MAKE_JOBS respected**: Users can override: `make check-compile MAKE_JOBS=8`
3. **No serial fallbacks**: Never run serially when parallel is possible
4. **Output synchronization**: Use `--output-sync=line` for clean parallel output
5. **Keep going**: Use `-k` flag to continue on errors and report all failures

### Standard Pattern

```makefile
check-<name>:
ifndef FILE
	@$(MAKE) -k -j$(MAKE_JOBS) $(TARGETS) 2>&1 | grep -E "^(🟢|🔴)" || true
	# ... count failures and report
endif
```

### Performance Expectation

- 48-core machine: MAKE_JOBS=24, expect 10-15x speedup
- 8-core machine: MAKE_JOBS=4, expect 3-4x speedup
- Single-core: MAKE_JOBS=1, degrades gracefully

## Build Modes

The system supports multiple build modes for different purposes:

```makefile
BUILD ?= debug

ifeq ($(BUILD),release)
  MODE_FLAGS = -O2 -g -DNDEBUG -D_FORTIFY_SOURCE=2
else ifeq ($(BUILD),sanitize)
  MODE_FLAGS = -O0 -g3 -fsanitize=address,undefined
else ifeq ($(BUILD),tsan)
  MODE_FLAGS = -O0 -g3 -fsanitize=thread
else ifeq ($(BUILD),valgrind)
  MODE_FLAGS = -O0 -g3 -fno-omit-frame-pointer
else
  MODE_FLAGS = -O0 -g3 -fno-omit-frame-pointer -DDEBUG
endif
```

Usage: `make check-compile BUILD=release`

## Pattern Rules

Pattern rules define how to build artifacts. They must:

1. Use pattern matching (never hardcode file paths)
2. Create output directories automatically (`@mkdir -p $(dir $@)`)
3. Emit 🟢 on success, 🔴 on failure
4. Exit with proper status codes

Example:

```makefile
$(BUILDDIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	@if $(CC) $(CFLAGS) -c $< -o $@ 2>&1; then \
		echo "🟢 $<"; \
	else \
		echo "🔴 $<"; \
		exit 1; \
	fi
```

## Design Principles

1. **Pattern-based discovery**: Never hardcode file lists, always use `find` and pattern substitution
2. **Modular organization**: One `.make/*.mk` file per check-* target
3. **Consistent output**: All targets use 🟢/🔴 format
4. **Parallel execution (MANDATORY)**: ALL targets honor MAKE_JOBS and run in parallel by default
5. **Self-documenting**: Clear variable names, commented sections
6. **Fail fast in FILE= mode**: Exit on first error in single-file mode, aggregate in bulk mode
7. **Testable**: Each check-* can be run independently with FILE=
8. **No magic**: Explicit is better than implicit
9. **Performance first**: Optimize for parallel execution, never accept serial bottlenecks

## Anti-Patterns (Don't Do This)

❌ **Hardcoding file lists**
```makefile
SOURCES = src/main.c src/config.c src/agent.c  # NO!
```

✅ **Pattern-based discovery**
```makefile
SRC_FILES = $(shell find src -name '*.c' -not -path '*/vendor/*')
```

❌ **Verbose output**
```makefile
check-compile:
	@echo "Building..."
	@echo "Processing src/main.c"
	gcc -c src/main.c
	@echo "Done!"
```

✅ **Clean output**
```makefile
check-compile:
	@$(MAKE) -j$(MAKE_JOBS) $(ALL_OBJECTS)
	@echo "✅ All files compiled"
```

❌ **Multiple .mk files for one target**
```makefile
include .make/check-compile-setup.mk
include .make/check-compile-logic.mk
include .make/check-compile-report.mk
```

✅ **One .mk file per target**
```makefile
include .make/check-compile.mk
```

## Future Additions

When adding new check-* targets:

1. Create `.make/check-<name>.mk` with the target logic
2. Add `include .make/check-<name>.mk` to main Makefile
3. Add `.PHONY: check-<name>` in the .mk file
4. Create `.claude/scripts/check-<name>` wrapper
5. Follow the 🟢/🔴 output format
6. Support both bulk and FILE= modes
7. Update `make help` to list the new target
8. Document in this file

## References

- Main Makefile: `/Makefile`
- Example check target: `/.make/check-compile.mk`
- Makefile skill: `/.claude/library/makefile/SKILL.md`
- Harness skill: `/.claude/library/harness/SKILL.md`
