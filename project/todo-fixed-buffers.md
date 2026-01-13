# TODO: Replace Fixed-Size Buffers with Dynamic Allocation

## Problem

The codebase uses fixed-size stack buffers (`char buf[N]`) for paths and strings throughout tests and some production code. This pattern:

1. **Risks buffer overflow** - If actual content exceeds buffer size, silent truncation or overflow occurs
2. **Requires guessing sizes** - Developers pick arbitrary sizes (256, 512, 1024) hoping they're "big enough"
3. **Inconsistent with production patterns** - Production code already uses `talloc_asprintf()` in many places

## Solution

Replace `snprintf()` into fixed buffers with `talloc_asprintf()`:

```c
// Before (fragile):
char test_dir[256];
snprintf(test_dir, sizeof(test_dir), "/tmp/tool_discovery_test_%d", getpid());

// After (robust):
char *test_dir = talloc_asprintf(test_ctx, "/tmp/tool_discovery_test_%d", getpid());
```

Benefits:
- Auto-sizes to actual content length
- No buffer overflow risk
- Consistent with existing production patterns (see `src/tool_discovery.c:135`)
- Memory freed automatically via talloc hierarchy

## Scope

### High Priority (Tests)

Test files have the most instances and lowest risk of breakage:

| File | Buffer Count | Notes |
|------|--------------|-------|
| `tests/test_utils.c` | ~15 | Core test utilities, high impact |
| `tests/unit/tools/tool_discovery_test.c` | ~10 | Path buffers for test dirs |
| `tests/unit/commands/cmd_kill_cascade_test.c` | 6 | `args[128]` buffers |
| `tests/unit/commands/cmd_filter_mail_timestamp_test.c` | 9 | `args[64]` buffers |
| `tests/unit/terminal/terminal_pty_*.c` | 3 | PTY name and read buffers |
| `tests/helpers/vcr.c` | 1 | `path[512]` |

### Medium Priority (Production)

Production code that could benefit:

| File | Buffer Count | Notes |
|------|--------------|-------|
| `src/logger.c` | 7 | Path construction for log files |
| `src/tool_discovery.c` | 1 | 8KB read buffer (may need different approach) |
| `src/commands_kill.c` | 3 | Small message buffers |
| `src/commands_mail.c` | 3 | UUID and body buffers |
| `src/panic.c` | 3 | Intentionally simple - may keep as-is |

### Exceptions (Keep Fixed)

Some fixed buffers are intentional:

- `src/error.h:37` - `char msg[256]` in `err_t` struct (embedded, not dynamically allocated)
- `src/uuid.c:18` - `unsigned char bytes[16]` (fixed by UUID spec)
- `src/ansi/color_test.c` - Tests that specifically verify buffer behavior
- `src/panic.c` - Panic handler should minimize allocations

## Approach

### Phase 1: Test Infrastructure
1. Start with `tests/test_utils.c` - most reused, highest leverage
2. Convert path-building helpers to return `char*` from talloc
3. Update callers

### Phase 2: Individual Test Files
1. Work through test files one at a time
2. Ensure each test has a talloc context for dynamic allocations
3. Replace `char buf[N]; snprintf(...)` with `talloc_asprintf()`

### Phase 3: Production Code
1. Review each production file case-by-case
2. Some may need talloc context passed in
3. Consider whether callers need updating

## Considerations

### Talloc Context Availability

Dynamic allocation requires a talloc context. Most test fixtures already have one:
```c
static TALLOC_CTX *test_ctx;
static void setup(void) { test_ctx = talloc_new(NULL); }
```

For functions without a context, options:
1. Add context parameter
2. Use `talloc_autofree_context()` for truly temporary allocations
3. Keep fixed buffer if allocation is inappropriate (e.g., panic handler)

### Performance

Stack allocation is faster than heap allocation. However:
- Test code performance is irrelevant
- Production hot paths should be evaluated individually
- Most path-building happens at startup/initialization, not in tight loops

### Static Buffers

Some patterns use `static` buffers to avoid allocation:
```c
static char result[512];  // Returned to caller, reused
```

These need careful handling - may need to change API to take caller-provided buffer or return talloc'd memory.

## Discovery Method

Found via: `grep -r 'char \w+\[\d+\];' src/ tests/`

This catches most cases but misses:
- Buffers in structs (separate consideration)
- Multi-dimensional arrays
- Buffers with computed sizes
