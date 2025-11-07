# Build System

The ikigai build system is designed for **fast iteration with rigorous quality gates**. Every tool provides instant, actionable feedback. Every check catches real bugs.

## Philosophy

The build system enables a tight development loop:

1. **Write code** - Comprehensive warnings catch bugs at compile time
2. **Run tests** - Fast unit/integration tests with Check framework
3. **Deep validation** - Sanitizers, Valgrind, and coverage gates catch what tests miss
4. **Ship confidently** - Release builds optimize aggressively with all checks compiled away

This isn't about bureaucracy. It's about catching bugs early when they're cheap to fix, and shipping code you trust.

## Quality Assurance: Layered Defense

The build system provides multiple layers of bug detection, each catching different classes of errors:

### Compile-Time: Comprehensive Warnings

19 warning flags enabled by default (always, in all build modes):

```
-Wall -Wextra -Wshadow -Wstrict-prototypes -Wmissing-prototypes
-Wwrite-strings -Wformat=2 -Wconversion -Wcast-qual -Wundef
-Wdate-time -Winit-self -Wstrict-overflow=2 -Wimplicit-fallthrough
-Walloca -Wvla -Wnull-dereference -Wdouble-promotion
```

These catch:
- Type mismatches and implicit conversions
- Null dereferences and undefined behavior
- Format string vulnerabilities
- Variable shadowing and uninitialized variables
- VLAs and alloca (we use talloc instead)

**Result**: Many bugs never make it past compilation.

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

**Result**: Memory errors, races, and undefined behavior caught before they reach users.

### Coverage: 100% Threshold Enforced

```bash
make coverage
```

Generates line and branch coverage report, **enforced at 100%**.

Uses `lcov` for fast, text-based reports (no HTML generation overhead). Shows per-file coverage and summary.

**Why 100%?** Because untested code is untested error paths. With MOCKABLE seams (see below), we can test every allocation failure, every parse error, every edge case.

**Result**: Every code path is validated. No dark corners where bugs hide.

### Code Quality: Complexity Gating

```bash
make lint
```

Uses the `complexity` tool to enforce cyclomatic complexity threshold of 15.

Functions exceeding this threshold must be refactored. Complexity correlates with bugs—simpler functions are easier to test, understand, and maintain.

**Result**: Code stays maintainable and testable.

### Release: Turn Errors Into Build Failures

```bash
make release
```

Adds `-Werror` to turn all warnings into errors. If it compiles in release mode, it's clean.

Also enables:
- `-O2` optimization
- `-D_FORTIFY_SOURCE=2` for runtime buffer overflow detection
- `-DNDEBUG` to inline all MOCKABLE functions (zero overhead)

The `make ci` target enforces this: lint → coverage → dynamic analysis → release build.

**Result**: No warnings, no skipped tests, no compromises in production code.

## Test Infrastructure: MOCKABLE Seams

Testing C code is hard because you can't easily inject failures into external libraries. How do you test OOM handling if you can't make `talloc_zero()` return NULL?

**Solution**: Link seams via the `MOCKABLE` pattern.

### What are link seams?

All external library calls (talloc, jansson, uuid, b64) are wrapped in functions marked `MOCKABLE`:

```c
// wrapper.h
#ifdef NDEBUG
#define MOCKABLE static inline  // Release: inline, zero overhead
#else
#define MOCKABLE __attribute__((weak))  // Debug: overridable by tests
#endif

MOCKABLE void *ik_talloc_zero_wrapper(TALLOC_CTX *ctx, size_t size);
```

In **debug builds**, these are weak symbols—tests can override them:

```c
// In test file
void *ik_talloc_zero_wrapper(TALLOC_CTX *ctx, size_t size) {
    return NULL;  // Inject allocation failure
}
```

In **release builds** (`-DNDEBUG`), the wrappers are `static inline` and defined in the header. The compiler inlines them completely—**zero runtime overhead**, no symbols in the binary.

### Why this pattern?

- **Comprehensive testing**: Test every error path (OOM, parse failures, etc.)
- **Zero production overhead**: Release builds inline everything away
- **Industry standard**: Known as "link seams" (Michael Feathers), used by Linux kernel (KUnit)
- **Single codebase**: No `#ifdef` soup, tests and production share the same code

See `docs/decisions/link-seams-mocking.md` for full rationale and `src/wrapper.h` for implementation.

**Examples**:
- `tests/integration/oom_integration_test.c` - Verifies OOM error handling
- All unit tests can inject failures via link seams

**Result**: Every error path is testable. 100% coverage is achievable.

## Multi-Distribution Support

Building for multiple Linux distributions is handled via Docker.

### Supported distributions

Current: **Debian Trixie** and **Fedora** (latest)

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

**Result**: Code validated on multiple distros before release.

### Handling missing dependencies

Some libraries aren't available on all distros, or we want to avoid version conflicts.

The Makefile supports selective static linking:

```make
CLIENT_LIBS ?= -ltalloc -ljansson -luuid -lb64 -lpthread
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

**Result**: Flexible dependency management per distribution.

## Efficiency for Development

Every tool in the build system is chosen for **speed and clarity**:

### Fast tools, terse output

- **Check framework**: Minimal output, fast execution. Only prints failures.
- **lcov**: Text-based coverage reports, no HTML generation overhead
- **complexity**: Instant feedback, single-line output per function
- **uncrustify**: Fast formatter with K&R style (120-char lines)
- **Sanitizers**: Run at near-native speed with ASan/UBSan (2-5x slowdown vs 100x for Valgrind)

### Incremental builds

The Makefile uses automatic dependency generation:

```make
DEP_FLAGS = -MMD -MP
```

GCC generates `.d` files listing each source file's dependencies (headers). Make includes these:

```make
-include $(wildcard build/*.d)
```

**Result**: Only recompile what changed. No manual dependency tracking.

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

**Result**: Fast edit-compile-test cycle. Typical iteration under 1 second for single-file changes.

## Build Modes

Five build modes for different purposes:

| Mode | Use Case | Flags |
|------|----------|-------|
| `debug` | Default development | `-O0 -g3 -DDEBUG` |
| `release` | Production | `-O2 -g -DNDEBUG -Werror -D_FORTIFY_SOURCE=2` |
| `sanitize` | ASan + UBSan | `debug + -fsanitize=address,undefined` |
| `tsan` | ThreadSanitizer | `debug + -fsanitize=thread` |
| `valgrind` | Valgrind runs | `-O0 -g3 -fno-omit-frame-pointer` |

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

**Result**: Defense in depth, even in debug builds.

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

If any step fails, CI fails. No partial success.

This is what runs in `distro-check` for each distribution.

**Result**: Single command validates everything. No manual checklist.

## Zero-Overhead Abstractions

The MOCKABLE pattern demonstrates the philosophy: **pay nothing for testability in production**.

Release builds:
- Inline all wrappers (`static inline`)
- Optimize aggressively (`-O2`)
- Strip debug info (`-DNDEBUG`)

Debug/test builds:
- Keep all symbols for debugging
- Enable sanitizers
- Maintain weak symbols for test overrides

**Verification**:

```bash
# Debug: wrapper functions are weak symbols
make BUILD=debug
nm build/wrapper.o | grep ik_talloc
# Shows: W ik_talloc_zero_wrapper

# Release: wrappers inlined, no symbols
make BUILD=release
nm build/wrapper.o | grep ik_talloc
# Shows: (nothing - inlined away)
```

**Result**: Testing infrastructure disappears in production. No runtime cost.

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

The build system enforces quality through:

- **19 warning flags** catching bugs at compile time
- **4 dynamic analysis tools** catching runtime issues
- **100% coverage threshold** ensuring every path is tested
- **Complexity gating** keeping code maintainable
- **MOCKABLE seams** making every error path testable
- **Multi-distro validation** catching platform-specific issues
- **Fast, terse tools** enabling rapid iteration

Every check is fast. Every failure is actionable. Every bug is caught early.

**Philosophy**: Trust your code because you've validated it thoroughly, not because you hope it works.
