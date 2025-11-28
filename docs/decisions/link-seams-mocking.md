# Weak Symbol Wrappers for Mocking

**Decision**: Wrap external library calls and system calls in MOCKABLE functions that are weak symbols in debug builds and inlined in release builds.

## Implementation

```c
// wrapper.h
#ifdef NDEBUG
#define MOCKABLE static inline  // Release: inline, zero overhead
#else
#define MOCKABLE __attribute__((weak))  // Debug: overridable by tests
#endif

// Wrappers for system calls and libraries
MOCKABLE int posix_select_(int nfds, fd_set *readfds, ...);
MOCKABLE void *talloc_zero_(TALLOC_CTX *ctx, size_t size);
MOCKABLE CURLM *curl_multi_init_(void);
```

Production code calls wrappers:
```c
int ready = posix_select_(max_fd + 1, &read_fds, &write_fds, &exc_fds, &timeout);
```

Tests override weak symbols:
```c
// test.c
int posix_select_(int nfds, fd_set *readfds, ...) {
    return 1;  // Mock: always ready
}
```

## Why Not GNU `--wrap`?

Most Linux C projects use the GNU linker's `--wrap` flag for mocking:

```bash
# Production code calls select() directly
gcc -Wl,--wrap=select test.c

# Test provides __wrap_select()
int __wrap_select(...) { return 1; }
```

We rejected `--wrap` for three reasons:

1. **macOS incompatible**: macOS's `ld` doesn't support `--wrap`, limiting portability
2. **LTO bugs**: `--wrap` has known issues with Link Time Optimization—the linker may inline or eliminate wrapped symbols before applying `--wrap`, breaking mocks
3. **Implicit**: Wrapper indirection happens at link time with no visible marker in source code

## Rationale

**Testing benefits**:
- Inject allocation failures (OOM)
- Simulate system call errors (`select()` returning -1, `EINTR`, etc.)
- Mock curl multi-handle behavior
- Test error paths unreachable in normal execution

**Cross-platform**:
- Works on Linux, macOS, BSD (weak symbols are standard)
- No LTO compatibility issues
- Future-proof for any platform with weak symbol support

**Explicit**:
- `posix_*_()` and `curl_*_()` naming makes mockable calls greppable
- Developers immediately see what's testable
- MOCKABLE macro documents intent

**Zero overhead**:
- Release builds (`-O2 -DNDEBUG`) inline wrappers completely
- No function call overhead, no symbols in binary
- Verify: `nm build/wrapper.o` shows no wrapper symbols in release

## Design

All wrappers live in `wrapper.c`/`wrapper.h`, organized by library:
- talloc wrappers
- yyjson wrappers
- libcurl wrappers
- POSIX system call wrappers

This centralizes external dependencies for easy auditing.

## Build Modes

- `make all`/`make check`: Debug build with weak symbols (testable)
- `make release`: `-O2 -DNDEBUG`, wrappers inlined (zero overhead)

## Trade-offs

**Pros**:
- Enables comprehensive error path testing
- Zero production overhead
- Cross-platform (Linux, macOS, BSD)
- Explicit, greppable wrapper names
- No LTO issues
- Industry-standard pattern (link seams)

**Cons**:
- Requires `_` suffix throughout production code
- Wrapper layer adds slight indirection in debug builds
- Wrapper functions need maintenance when adding new library calls (rare)

## Alternatives Considered

- **GNU `--wrap`**: Clean production code but Linux-only, LTO-incompatible, implicit
- **LD_PRELOAD**: Runtime-only, fragile, not suitable for unit tests
- **Function pointers**: Requires major refactoring, runtime overhead
- **CMock/CMocka**: External framework dependency, build complexity

## Scaling

As new libraries are integrated, add wrappers to `wrapper.c` in clearly marked sections. Most libraries only need 3-6 function wrappers.
