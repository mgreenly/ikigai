# Build System

The ikigai build system uses a modular architecture with the Makefile orchestrating compilation and `.make/*.mk` files defining quality check targets. The `.claude/harness/` system provides automated fix loops.

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
.claude/harness/
  <name>/run               # Check harness (calls make target)
  fix-<name>/run           # Fix harness (spawns sub-agents)
```

## The 11 Check Targets

Each check target exists at two levels forming a check-fix workflow:

### Level 1: Make Target

`make check-<name>` - Core implementation in `.make/check-<name>.mk`

| Target | Purpose |
|--------|---------|
| `check-compile` | Compile all source files to .o files |
| `check-link` | Link all binaries (main, tools, tests) |
| `check-filesize` | Verify source files under 16KB |
| `check-complexity` | Verify cyclomatic complexity under 15 |
| `check-unit` | Run unit tests |
| `check-integration` | Run integration tests |
| `check-coverage` | Check code coverage meets 90% threshold |
| `check-sanitize` | Run tests with AddressSanitizer/UBSan |
| `check-tsan` | Run tests with ThreadSanitizer |
| `check-valgrind` | Run tests under Valgrind Memcheck |
| `check-helgrind` | Run tests under Valgrind Helgrind |

### Level 2: Harness Integration

`.claude/harness/<name>/run` - Entry point for automated systems
`.claude/harness/fix-<name>/run` - Spawns sub-agents to fix failures

## Output Format

All check-* targets use consistent 🟢/🔴 output.

### Bulk Mode (default)

One line per file. Green or red circle:

```
🟢 src/main.c
🟢 src/config.c
🔴 src/agent.c
✅ All files compiled
```

Or on failure:
```
🟢 src/main.c
🔴 src/agent.c
❌ 1 files failed to compile
```

### Single File Mode (FILE=path)

```bash
make check-compile FILE=src/main.c
```

Success: `🟢 src/main.c`
Failure: `🔴 src/agent.c:42:5: error: 'unknown_var' undeclared`

## Build Modes

| Mode | Use Case | Flags |
|------|----------|-------|
| `debug` | Default development | `-O0 -g3 -fno-omit-frame-pointer -DDEBUG` |
| `release` | Production | `-O2 -g -DNDEBUG -D_FORTIFY_SOURCE=2` |
| `sanitize` | ASan + UBSan | `debug + -fsanitize=address,undefined` |
| `tsan` | ThreadSanitizer | `debug + -fsanitize=thread` |
| `valgrind` | Valgrind runs | `-O0 -g3 -fno-omit-frame-pointer -DDEBUG` |

Usage: `make check-compile BUILD=release`

## Warning Flags

19 warning flags enabled by default with `-Werror`:

```
-Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes
-Wwrite-strings -Wformat=2 -Wconversion -Wcast-qual -Wundef
-Wdate-time -Winit-self -Wstrict-overflow=2 -Wimplicit-fallthrough
-Walloca -Wvla -Wnull-dereference -Wdouble-promotion -Werror
```

These catch:
- Type mismatches and implicit conversions
- Null dereferences and undefined behavior
- Format string vulnerabilities
- Variable shadowing and uninitialized variables
- VLAs and alloca (we use talloc instead)

## Parallelization

All check-* targets run in parallel by default:

```makefile
MAKE_JOBS ?= $(shell nproc=$(shell nproc); echo $$((nproc / 2)))
```

- Default: half of available cores
- Override: `make check-compile MAKE_JOBS=8`
- Output synchronized with `--output-sync=line`

## Source Discovery

Never hardcode file lists. All source discovery uses pattern-based `find`:

```makefile
SRC_FILES = $(shell find src -name '*.c' -not -path '*/vendor/*')
TEST_FILES = $(shell find tests -name '*.c')
VENDOR_FILES = $(shell find src/vendor -name '*.c')
```

## Test Infrastructure: MOCKABLE Seams

External library calls are wrapped for testability:

```c
// wrapper.h
#ifdef NDEBUG
#define MOCKABLE static inline  // Release: inline, zero overhead
#else
#define MOCKABLE __attribute__((weak))  // Debug: overridable by tests
#endif

MOCKABLE void *talloc_zero_(TALLOC_CTX *ctx, size_t size);
```

- **Debug builds**: Weak symbols, tests can override
- **Release builds**: Inlined away, zero overhead

OOM branches are excluded from coverage (`LCOV_EXCL_BR_LINE`) since allocation failures cause `PANIC()`.

## Test Linking

Tests link against `MODULE_OBJ` (all src/*.o except main.o) plus automatically discovered helpers and mocks.

### Mock/Helper Discovery

Dependencies extracted from `.d` files:
- `*_helper.h` → `*_helper.o`
- `*_mock.h` → `*_mock.o`
- `helpers/*_test.h` → `helpers/*_test.o`

### Link Order

```makefile
$(CC) $(LDFLAGS) -Wl,--allow-multiple-definition \
    -o $@ \
    $<              # test.o (may contain inline mocks)
    $$deps          # discovered helpers/mocks (override MODULE_OBJ)
    $(MODULE_OBJ)   # real implementations
    $(LDLIBS)
```

`--allow-multiple-definition` + link order = mocks override real implementations.

## Security Hardening

All modes:
```makefile
SECURITY_FLAGS = -fstack-protector-strong
```

Release mode adds:
```makefile
-D_FORTIFY_SOURCE=2  # Runtime buffer overflow detection
```

## Example Workflows

### Quick iteration
```bash
make              # Build
make check-unit   # Run unit tests
```

### Single file check
```bash
make check-compile FILE=src/main.c
make check-coverage FILE=src/main.c
```

### Full quality check
```bash
make check-compile && make check-link && make check-unit && \
make check-integration && make check-coverage && make check-sanitize
```

### Debugging specific issues
```bash
make check-valgrind   # Memory issues
make check-tsan       # Race conditions
make check-sanitize   # Undefined behavior
```
