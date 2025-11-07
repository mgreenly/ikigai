# Why Link Seams for External Library Mocking?

**Decision**: Wrap external library calls (talloc, jansson, etc.) in MOCKABLE functions that are weak symbols in debug/test builds and inline in release builds.

**Rationale**:
- **Comprehensive testing**: Enables injection of allocation failures (OOM), parse failures, and other error conditions that are difficult to trigger naturally
- **Zero overhead in production**: Release builds (`make release` with `-DNDEBUG`) inline wrappers completely—no function call overhead, no symbols in binary
- **Industry-standard pattern**: Known as "link seams" (Michael Feathers) or "test seams," used by Linux kernel (KUnit) and other C projects
- **Conditional compilation**: Single codebase supports both test hooks and production performance

**Implementation**:
```c
// wrapper.h
#ifdef NDEBUG
#define MOCKABLE static inline  // Release: inline, zero overhead
#else
#define MOCKABLE __attribute__((weak))  // Debug: overridable by tests
#endif

// Debug build: wrapper.c compiles weak symbol implementations
// Release build: wrapper.c is empty, functions defined inline in header

MOCKABLE void *ik_talloc_zero_wrapper(TALLOC_CTX *ctx, size_t size);
```

**Testing example**:
```c
// Test can override wrapper to inject OOM
void *ik_talloc_zero_wrapper(TALLOC_CTX *ctx, size_t size) {
    return NULL;  // Simulate allocation failure
}
```

**Build modes**:
- `make all` / `make check`: Debug build with weak symbols (testable)
- `make release`: `-O3 -DNDEBUG`, wrappers inlined (zero overhead)

**Verification**:
- Debug: `nm build/wrapper.o` shows `W ik_talloc_*_wrapper` (weak symbols)
- Release: `nm build/wrapper.o` shows no wrapper symbols (inlined away)

**Design choice**: All external library wrappers live in single `wrapper.c/wrapper.h` file, organized by library (talloc, jansson, etc.). Most libraries only have 3-6 functions we call, so consolidation keeps the codebase simple.

**Alternative considered**: GNU linker `--wrap` flag. Rejected because:
- Requires linker flags for every wrapped function
- Less transparent (magic happening at link time)
- Conditional compilation is more explicit and easier to understand

**Trade-offs**:
- **Pro**: Enables testing of unreachable error paths (OOM, parse failures)
- **Pro**: Zero production overhead with conditional compilation
- **Pro**: Simple, explicit pattern easy to extend
- **Con**: Slight code indirection (but MOCKABLE macro makes intent clear)
- **Con**: Wrapper functions need maintenance when adding new library calls (but this is rare)

**Scaling**: As new libraries are integrated, add their wrappers to `wrapper.c` in clearly marked sections. This centralizes all external dependencies in one place for easy auditing and testing.
