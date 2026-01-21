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

# Automated harness invokes check (outputs JSON)
$ .claude/scripts/check-compile
{"ok": false, "items": ["src/agent.c:42:5: error: undeclared"]}

# Harness spawns fix agent
$ .claude/harness/compile/fix
[Sub-agent attempts to fix src/agent.c]
[Commits on success, reverts on failure]
```

## Output Format Requirements (CRITICAL)

**These two rules are non-negotiable. Every check-* target MUST follow them exactly.**

### Rule 1: Bulk Mode (no FILE=)

**One line per file/target. Green circle or red circle. Nothing else.**

```
🟢 src/main.c
🟢 src/config.c
🔴 src/agent.c
🟢 src/error.c
✅ All files compiled
```

Or on failure:
```
🟢 src/main.c
🔴 src/agent.c
❌ 1 files failed to compile
```

- Each file gets exactly ONE line: `🟢 path` or `🔴 path`
- Final summary line: `✅ All ...` or `❌ N failed ...`
- **Nothing else.** No "Building...", no progress, no verbose output.
- Exit code: 0 on success, non-zero on failure

### Rule 2: Single File Mode (FILE=path)

**One line per error. Red circle followed by error description. Nothing else.**

Success:
```
🟢 src/main.c
```

Failure:
```
🔴 src/agent.c:42:5: error: 'unknown_var' undeclared
🔴 src/agent.c:50:10: error: expected ';' before '}'
```

- Success: exactly ONE line `🟢 path`
- Failure: ONE line per error, each starting with `🔴 `
- Error format: `🔴 path:line:col: message`
- **No summary line** in single-file mode
- Exit code: 0 on success, non-zero on failure

### The Unavoidable `make: ***` Line

When a recipe exits non-zero, make prints `make: *** [target] Error N` to stderr. **This cannot be suppressed from within the Makefile.** It's printed by make itself after the recipe completes.

For direct `make` invocation, users will see this trailing line:
```
🔴 src/agent.c:42:5: error: 'unknown_var' undeclared
make: *** [.make/check-compile.mk:30: check-compile] Error 1
```

The harness scripts (`.claude/scripts/check-*`) parse only 🟢/🔴 lines when producing JSON output, so the `make: ***` line is ignored.

**Accept this limitation.** Don't waste time trying to suppress it from the Makefile.

### Compiler Flags for Clean Output

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
- Symlink to `.claude/harness/<name>/run`
- Parses 🟢/🔴 output from make and translates to JSON
- Used by harness system and sub-agents

**JSON Output Format:**
```json
// Success
{"ok": true}

// Failure
{"ok": false, "items": ["file:line: error message", ...]}
```

**Supports FILE= parameter** for single-file mode:
- `check-unit --file build/tests/unit/foo_test`
- `check-integration --file build/tests/integration/bar_test`

### Level 3: `.claude/harness/<name>/fix`
- Spawns sub-agents to fix failures
- Reads JSON output from Level 2 scripts
- Escalates through sonnet:think → opus:think → opus:ultrathink
- Maintains history.md for cross-attempt learning

## Parallelization (CRITICAL REQUIREMENT)

**Parallel execution is non-negotiable. Any approach that runs serially will be rejected.**

This is a hard constraint, not a preference. If an implementation choice cannot support parallel execution, find a different approach. Serial builds are unacceptable on modern multi-core machines.

```makefile
MAKE_JOBS ?= $(shell nproc=$(shell nproc); echo $$((nproc / 2)))
```

### Requirements

1. **Default parallel execution**: Every check-* target MUST use `-j$(MAKE_JOBS)`. No exceptions.
2. **MAKE_JOBS respected**: Users can override: `make check-compile MAKE_JOBS=8`
3. **No serial fallbacks**: If something can't parallelize, redesign it until it can.
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

## Test Linking: Mock and Helper Discovery

Tests link against `MODULE_OBJ` (all src/*.o except main.o) plus automatically discovered helpers and mocks.

### The Problem

Tests often define mock implementations of functions that also exist in `MODULE_OBJ`:
```c
// In test file - conflicts with real implementation
res_t ik_db_message_insert(...) { /* mock */ }
```

This causes "multiple definition" linker errors.

### The Solution

1. **`--allow-multiple-definition`**: Added to test link flags. Allows duplicate symbols.
2. **Link order matters**: Mocks/helpers linked BEFORE `MODULE_OBJ`. First definition wins.
3. **Automatic discovery**: Dependencies extracted from `.d` files generated during compilation.

### Dependency Extraction Pattern

The `.d` file (generated by `-MMD -MP`) lists all headers a source file includes. We extract linkable dependencies:

```makefile
# Extract *_helper.o, *_mock.o, and helpers/*_test.o from .d file
define deps_from_d_file_script
grep -oE '[^ \\:]*(_helper|_mock)\.h|[^ \\:]*helpers/[^ \\:]*_test\.h' $(1) 2>/dev/null | \
  sort -u | while read p; do \
    # Normalize ../.. paths
    while echo "$$p" | grep -q '[^/]*/\.\./'; do p=$$(echo "$$p" | sed 's|[^/]*/\.\./||'); done; \
    echo "$(BUILDDIR)/$${p%.h}.o"; \
  done
endef
```

This discovers:
- `*_helper.h` → `*_helper.o` (test helpers)
- `*_mock.h` → `*_mock.o` (mock implementations)
- `helpers/*_test.h` → `helpers/*_test.o` (test suite helpers)

### Link Command Structure

```makefile
$(CC) $(LDFLAGS) -Wl,--allow-multiple-definition \
    -o $@ \
    $<                    # test.o (may contain inline mocks)
    $$deps                # discovered helpers/mocks (override MODULE_OBJ)
    $(MODULE_OBJ)         # real implementations (overridden by earlier defs)
    $(TOOL_LIB_OBJECTS)   # tool library objects
    $(VCR_STUBS)          # VCR weak symbol stubs
    -lcheck -lm ...       # libraries
```

### Test Binary Discovery

Tests are discovered by pattern, excluding helper directories:

```makefile
# Exclude helpers/ - those are test suite helpers, not standalone tests
UNIT_TEST_BINARIES = $(patsubst tests/%.c,$(BUILDDIR)/tests/%,\
    $(shell find tests/unit -name '*_test.c' -not -path '*/helpers/*'))
```

Files in `helpers/` directories (like `helpers/openai_serialize_user_test.c`) are compiled as objects but not built as standalone binaries - they're linked into coordinator tests.

### Multi-Phase Targets

When a target needs prerequisites built first (e.g., check-link needs objects before linking), **do NOT use make prerequisites**. Order-only prerequisites like `target: | $(PREREQS)` build serially. The `-j` flag only applies to the recipe, not prerequisite resolution.

**Solution**: Explicit multi-phase recipes with parallel builds in each phase.

```makefile
check-link:
ifndef FILE
	@# Phase 1: Compile all objects in parallel
	@$(MAKE) -k -j$(MAKE_JOBS) $(ALL_OBJECTS) 2>&1 | grep -E "^(🟢|🔴)" || true
	@# Phase 2: Link all binaries in parallel
	@$(MAKE) -k -j$(MAKE_JOBS) $(ALL_BINARIES) 2>&1 | grep -E "^(🟢|🔴)" || true; \
	# ... count failures and report
endif
```

This pattern applies to ANY target with dependencies - always use explicit parallel invocations, never rely on make's prerequisite system for bulk operations.

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
4. Create `.claude/harness/<name>/run` script (parses 🟢/🔴, outputs JSON)
5. Symlink `.claude/scripts/check-<name>` → `../harness/<name>/run`
6. Follow the 🟢/🔴 output format in make target
7. Support both bulk and FILE= modes
8. Update `make help` to list the new target
9. Document in this file

## References

- Main Makefile: `/Makefile`
- Example check target: `/.make/check-compile.mk`
- Makefile skill: `/.claude/library/makefile/SKILL.md`
- Harness skill: `/.claude/library/harness/SKILL.md`
