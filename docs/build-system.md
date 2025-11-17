# Build System

The ikigai build system provides multiple layers of quality checks, fast iteration tools, and multi-distro support.

## Quality Assurance

Multiple layers of checks catch different classes of bugs:

### Compile-Time: Comprehensive Warnings

19 warning flags enabled by default (always, in all build modes), with `-Werror` to treat warnings as errors:

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

### Runtime: Dynamic Analysis

Four different sanitizers and tools, each specialized:

**AddressSanitizer + UndefinedBehaviorSanitizer** (`make check-sanitize`)
- Heap/stack/global buffer overflows
- Use-after-free, use-after-return, double-free
- Memory leaks
- Undefined behavior (null derefs, signed integer overflow, etc.)

**ThreadSanitizer** (`make check-tsan`)
- Data races
- Deadlocks
- Thread safety violations

**Valgrind Memcheck** (`make check-valgrind`)
- Uninitialized memory reads
- Invalid memory access
- Memory leaks (comprehensive, catches what ASan misses)

**Valgrind Helgrind** (`make check-helgrind`)
- Race conditions
- Lock ordering violations
- Incorrect pthread API usage

The combo target `make check-dynamic` runs all four. Clean rebuild between each to avoid conflicts.

### Coverage: 100% Threshold Enforced

```bash
make coverage
```

Generates line and branch coverage report, enforced at 100%.

Uses `lcov` for text-based reports. Shows per-file coverage and summary.

With MOCKABLE seams (see below), most error paths can be tested. OOM branches are excluded from coverage (`LCOV_EXCL_BR_LINE`) since memory allocation failures now cause PANIC (abort) and cannot be tested via injection.

### Code Quality: Complexity Gating

```bash
make lint
```

Enforces multiple code quality metrics:

1. **Cyclomatic complexity**: threshold of 15 (uses `complexity` tool)
   - Functions exceeding this threshold must be refactored
2. **Nesting depth**: maximum 5 levels
   - Excessive nesting makes code hard to understand
3. **File line counts**: maximum 500 lines per file
   - Applies to source files (`src/*.c`, `src/*.h`)
   - Applies to test files (`tests/unit/*/*.c`, `tests/integration/*.c`)
   - Applies to documentation files (`docs/*.md`, `docs/*/*.md`)

Files exceeding these thresholds must be refactored or split.

### Release Build

```bash
make release
```

Enables:
- `-O2` optimization
- `-D_FORTIFY_SOURCE=2` for runtime buffer overflow detection
- `-DNDEBUG` to inline all MOCKABLE functions

The `make ci` target runs: lint → coverage → dynamic analysis → release build.

## Test Infrastructure: MOCKABLE Seams

All external library calls are wrapped to enable testing in debug builds.

### What are link seams?

All external library calls (talloc, yyjson, uuid, b64, POSIX) are wrapped in functions marked `MOCKABLE`:

```c
// wrapper.h
#ifdef NDEBUG
#define MOCKABLE static inline  // Release: inline, zero overhead
#else
#define MOCKABLE __attribute__((weak))  // Debug: overridable by tests
#endif

MOCKABLE void *talloc_zero_(TALLOC_CTX *ctx, size_t size);
```

In **debug builds**, these are weak symbols—tests can override them for non-OOM testing.

In **release builds** (`-DNDEBUG`), the wrappers are `static inline` and defined in the header. The compiler inlines them completely—zero runtime overhead, no symbols in the binary.

Benefits:
- Release builds have zero overhead (wrappers inlined away)
- Industry standard pattern (known as "link seams")
- Single codebase for tests and production

**Note on OOM testing:** Memory allocation failures now cause `PANIC("Out of memory")` which immediately terminates the process. OOM injection testing has been removed since allocation failures are no longer recoverable errors. OOM branches are excluded from coverage metrics (`LCOV_EXCL_BR_LINE`).

See `docs/decisions/link-seams-mocking.md` for full details and `src/wrapper.h` for implementation.

## Multi-Distribution Support

Building for multiple Linux distributions is handled via Docker.

### Supported distributions

Current: **Arch Linux**, **Debian**, and **Fedora**

Each distribution has:
- `distros/<distro>/Dockerfile` - Build environment
- `distros/<distro>/package.sh` - Package creation script
- `distros/<distro>/packaging/` - Distro-specific packaging files

### Build workflow

```bash
# Build Docker images for all distros
make distro-images

# Run full CI on all distros
make distro-check

# Build packages (.deb, .rpm)
make distro-package
```

The `distro-check` target:
1. Builds Docker image for each distro
2. Runs `make ci` inside container (lint → coverage → dynamic analysis → release)
3. Fails if any distro fails

### Handling missing dependencies

Some libraries aren't available on all distros, or we want to avoid version conflicts.

The Makefile supports selective static linking:

```make
CLIENT_LIBS ?= -ltalloc -luuid -lb64 -lpthread -lutf8proc
CLIENT_STATIC_LIBS ?=
```

Distro-specific makefiles can override:

```make
# mk/alpine.mk (hypothetical)
CLIENT_STATIC_LIBS = -lb64  # Not available in Alpine repos
```

The linker invocation:

```make
$(CC) $(LDFLAGS) -o $@ $^ -Wl,-Bstatic $(CLIENT_STATIC_LIBS) -Wl,-Bdynamic $(CLIENT_LIBS)
```

This links `CLIENT_STATIC_LIBS` statically and `CLIENT_LIBS` dynamically.

When a distro is missing dependencies, they are built from source during the Docker image build and linked statically.

## Efficiency for Development

Tools selected for speed and terse output:

- **Check framework**: Only prints failures
- **lcov**: Text-based coverage reports
- **complexity**: Single-line output per function
- **uncrustify**: Fast formatter with K&R style (120-char lines)
- **Sanitizers**: 2-5x slowdown (vs 100x for Valgrind)

### Incremental builds

The Makefile uses automatic dependency generation:

```make
DEP_FLAGS = -MMD -MP
```

GCC generates `.d` files listing each source file's dependencies (headers). Make includes these:

```make
-include $(wildcard build/*.d)
```

Only recompile what changed. No manual dependency tracking.

### Parallel test execution

Tests are independent executables. Run them in parallel:

```bash
make -j$(nproc) check
```

Each test runs in isolation with its own talloc context.

### Coverage data preservation

```make
.SECONDARY:
```

Prevents Make from deleting intermediate files (like `.gcno` coverage files). Coverage data persists across incremental builds.

## Build Modes

Five build modes for different purposes:

| Mode | Use Case | Flags |
|------|----------|-------|
| `debug` | Default development | `-O0 -g3 -fno-omit-frame-pointer -DDEBUG` |
| `release` | Production | `-O2 -g -DNDEBUG -D_FORTIFY_SOURCE=2` |
| `sanitize` | ASan + UBSan | `debug + -fsanitize=address,undefined` |
| `tsan` | ThreadSanitizer | `debug + -fsanitize=thread` |
| `valgrind` | Valgrind runs | `-O0 -g3 -fno-omit-frame-pointer -DDEBUG` |

Note: All modes include `-Werror` (warnings as errors) in the base warning flags.

Use with any target:

```bash
make all BUILD=release
make check BUILD=sanitize
```

### Security hardening

All modes include:

```make
SECURITY_FLAGS = -fstack-protector-strong
```

Release mode adds:

```make
-D_FORTIFY_SOURCE=2  # Runtime buffer overflow detection
```

## CI Integration

The `make ci` target runs the full validation pipeline:

```bash
make ci
```

Executes:
1. `make lint` - Complexity checks
2. `make coverage` - 100% coverage enforcement
3. `make check-dynamic` - All sanitizers + Valgrind
4. `make release` - Build with `-Werror`

If any step fails, CI fails.

This is what runs in `distro-check` for each distribution.

## Zero-Overhead Abstractions

The MOCKABLE pattern provides testability with no production cost.

Release builds:
- Inline all wrappers (`static inline`)
- Optimize aggressively (`-O2`)
- Strip debug info (`-DNDEBUG`)

Debug/test builds:
- Keep all symbols for debugging
- Enable sanitizers
- Maintain weak symbols for test overrides

Verification:

```bash
# Debug: wrapper functions are weak symbols
make BUILD=debug
nm build/wrapper.o | grep ik_talloc
# Shows: W talloc_zero_

# Release: wrappers inlined, no symbols
make BUILD=release
nm build/wrapper.o | grep ik_talloc
# Shows: (nothing - inlined away)
```

## Example Workflows

### Quick iteration

```bash
make              # Build (debug mode)
make check        # Run tests
# Edit code
make check        # Incremental rebuild + test
```

### Before committing

```bash
make ci           # Full validation
```

### Release validation

```bash
make distro-check  # Test on all supported distros
make distro-package # Build packages
```

### Debugging a specific issue

```bash
# Memory issue?
make check-valgrind

# Race condition?
make check-tsan

# Undefined behavior?
make check-sanitize

# All of the above?
make check-dynamic
```

## Summary

The build system provides:

- **19 warning flags** at compile time
- **4 dynamic analysis tools** (ASan/UBSan, TSan, Valgrind Memcheck, Helgrind)
- **100% coverage threshold** enforced
- **Code quality metrics** (complexity ≤15, nesting ≤5, files ≤500 lines)
- **MOCKABLE seams** for testing error paths
- **Multi-distro validation** (Arch, Debian, Fedora)
- **Fast, terse tools** for quick iteration
