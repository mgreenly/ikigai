# Task: Vendor fzy Library

## Target
Feature: Autocompletion - fzy Integration

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md

### Pre-read Docs
None

### Pre-read Source (patterns)
- src/vendor/ (existing vendor directory structure, if any)
- Makefile (CLIENT_SOURCES, build configuration)

### Pre-read Tests (patterns)
None

## Pre-conditions
- `make check` passes
- `make lint` passes

## Task
Vendor the fzy fuzzy matching library source files into the project. This provides the core fuzzy matching algorithm without external dependencies.

fzy source files needed:
- match.h (header with API)
- match.c (implementation)

Source: https://github.com/jhawthorn/fzy (MIT License)

## TDD Cycle

### Red
1. Create `src/vendor/fzy/` directory
2. Download and place `match.h` from fzy repository
3. Download and place `match.c` from fzy repository
4. Add simple compilation test in `tests/unit/vendor/fzy_compile_test.c`:
   ```c
   #include "../../../src/vendor/fzy/match.h"
   #include <check.h>

   START_TEST(test_fzy_compiles)
   {
       // Just verify fzy compiles and links
       int result = has_match("abc", "abc");
       ck_assert_int_eq(result, 1);
   }
   END_TEST

   // ... suite boilerplate
   ```
5. Run `make check` - expect compilation failure (not in Makefile yet)

### Green
1. Update Makefile to include vendor source:
   ```make
   CLIENT_SOURCES += src/vendor/fzy/match.c
   ```
2. Create test directory and update test Makefile if needed
3. Run `make check` - expect pass

### Refactor
1. Add license header comment to vendor files noting MIT license and source
2. Verify `make lint` passes (may need to exclude vendor files from lint)
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint` passes
- fzy source files exist in `src/vendor/fzy/`
- `has_match()` and `match()` functions are available
- No external dependencies added
