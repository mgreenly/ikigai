# Code Style

## Description
Code style conventions for ikigai development.

## Details

### Comments

- Use `//` style only (never `/* ... */`)
- Comment why, not what
- Use sparingly

### Numeric Types

- Always use `<inttypes.h>` for numeric types and format specifiers
- Never use primitive types (`int`, `long`, etc.)
- Use explicit sized types: `int8_t`, `int16_t`, `int32_t`, `int64_t`, `uint8_t`, etc.
- Use `size_t` for sizes and counts
- Use `PRId32`, `PRIu64`, etc. for printf format specifiers
- Use `SCNd32`, `SCNu64`, etc. for scanf format specifiers

Example:
```c
#include <inttypes.h>

int32_t count = 42;
uint64_t size = 1024;
printf("Count: %" PRId32 ", Size: %" PRIu64 "\n", count, size);
```

### Include Order

Follow Google C++ style guide for #include ordering:

1. Own header first (e.g., `config.h` in `config.c`) - catches missing dependencies
2. Project headers (`"header.h"`) - alphabetically sorted
3. System/library headers (`<header.h>`) - alphabetically sorted

Example:
```c
#include "config.h"           // Own header

#include "json_allocator.h"   // Project headers (alphabetical)
#include "logger.h"
#include "panic.h"
#include "wrapper.h"

#include <errno.h>            // System headers (alphabetical)
#include <stdlib.h>
#include <string.h>
```

Rationale: Project headers before system headers catches non-self-contained headers early.

### Static Functions

Use static functions judiciously with these guidelines:

**When to use static functions:**
- Complex logic (>5 lines) repeated within a file
- Named code blocks for readability
- Callback implementations (layer callbacks, command handlers)
- Helper functions with clear single responsibility

**When to inline instead:**
- Helper is 1-3 lines and only used once
- Code is trivially simple (single statement)
- For absolute coverage certainty (rare edge cases)

**Coverage handling:**
- Use LCOV_EXCL_BR_LINE on individual unreachable branches
- For larger unreachable blocks, use LCOV_EXCL_START/STOP
- Both markers work correctly in static functions (verified in LCOV 2.0-1)
- **Important:** Avoid mentioning marker keywords in nearby comments (LCOV parses all comments)

**Exception:** MOCKABLE wrapper functions use static by design.

**Example:**
```c
// Helper for validation (avoid saying "LCOV exclusion" here)
static int32_t validate(int32_t input)
{
    if (input < 0) PANIC("Invalid");  // LCOV_EXCL_BR_LINE
    return input;
}
```

**Reference:** See `rel-06/docs/lcov-static-fn-findings.md` for investigation details.

### Test Code Style

Always add a blank line between END_TEST and START_TEST.
