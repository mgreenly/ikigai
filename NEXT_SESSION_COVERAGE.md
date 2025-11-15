# Complete Remaining Coverage Gaps to 100%

## Current Status

**Coverage**: 99.9% lines (1742/1744), 100% functions (129/129), 99.5% branches (567/570)
**Target**: 100.0% lines, 100% functions, 100% branches
**Gap**: 2 uncovered lines, 3 uncovered branches - ALL in error paths requiring OOM injection or edge cases

---

## Task: Achieve 100% Coverage

All remaining gaps require **OOM (Out Of Memory) injection** or are potentially unreachable edge cases. You need to either:
1. Implement OOM injection testing to cover these paths, OR
2. Verify they are truly unreachable and request permission to add LCOV exclusions

---

## Remaining Gaps (Exact Locations)

### Gap 1: render.c Line 361 + Branch 360
**File**: `src/render.c`
**Function**: `ik_render_combined()`
**Location**: Lines 357-362
```c
if (workspace_text_len > 0) {
    res_t result = calculate_cursor_screen_position(ctx, workspace_text, workspace_text_len,
                                                    workspace_cursor_offset, ctx->cols,
                                                    &workspace_cursor_pos);
    if (is_err(&result)) {  // Branch 360:0:0 - FALSE branch uncovered
        return result;      // Line 361 - UNCOVERED
    }
}
```

**What triggers it**:
- `calculate_cursor_screen_position()` returns an error
- This happens when: invalid UTF-8 sequence OR OOM allocating temporary buffers

**How to test**:
- Option 1: OOM injection - make `ik_talloc_wrapper()` fail during cursor calculation
- Option 2: Invalid UTF-8 - pass malformed UTF-8 in workspace_text (but this should be impossible if workspace validates input)

**Check**: Does `calculate_cursor_screen_position()` have any non-OOM error paths?

---

### Gap 2: render.c Line 424
**File**: `src/render.c`
**Function**: `ik_render_combined()`
**Location**: Lines 419-426
```c
// Write scrollback lines
for (size_t i = scrollback_start_line; i < scrollback_end_line; i++) {
    const char *line_text = NULL;
    size_t line_len = 0;
    res_t result = ik_scrollback_get_line_text(scrollback, i, &line_text, &line_len);
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE - branch excluded but...
        talloc_free(framebuffer);
        return result;  // Line 424 - UNCOVERED (line 425 has LCOV_EXCL_LINE)
    }
    // ...
}
```

**What triggers it**:
- `ik_scrollback_get_line_text()` returns an error
- This function has no documented error paths - check the implementation!

**How to test**:
- Check if `ik_scrollback_get_line_text()` can actually fail
- If it can't fail (returns `void` or always succeeds), this is dead code
- If it CAN fail, likely OOM injection needed

**Action**: Read `src/scrollback.c::ik_scrollback_get_line_text()` and check its signature and implementation

---

### Gap 3: repl.c Branch 259
**File**: `src/repl.c`
**Function**: `ik_repl_handle_slash_command()`
**Location**: Lines 257-259
```c
const char *output = ik_format_get_string(buf);
size_t output_len = strlen(output);
if (output_len > 0) {  // Branch 259:0:1 - FALSE branch uncovered
    // Split output by newlines...
}
```

**What triggers it**:
- `ik_pp_workspace()` produces empty output (output_len == 0)
- Is this possible? The function ALWAYS outputs at minimum: `ik_workspace_t @ 0xADDRESS\n...`

**How to test**:
- Read `src/workspace_pp.c::ik_pp_workspace()`
- Confirm it ALWAYS produces output
- If confirmed impossible, this may warrant LCOV_EXCL_BR_LINE

**Likely verdict**: Unreachable - request permission to add `LCOV_EXCL_BR_LINE`

---

### Gap 4: repl.c Branch 266
**File**: `src/repl.c`
**Function**: `ik_repl_handle_slash_command()`
**Location**: Lines 262-269
```c
for (size_t i = 0; i <= output_len; i++) {
    if (i == output_len || output[i] == '\n') {
        // Found end of line or end of string
        size_t line_len = i - line_start;
        if (line_len > 0 || i < output_len) {  // Branch 266:0:2 - FALSE branch uncovered
            result = ik_scrollback_append_line(repl->scrollback, output + line_start, line_len);
            if (is_err(&result))PANIC("allocation failed");  // LCOV_EXCL_BR_LINE
        }
        line_start = i + 1;
    }
}
```

**What triggers FALSE branch**: `line_len == 0 && i == output_len`
- This means: empty line at END of output (trailing newline creates zero-length final "line")
- The current test `test_pp_output_trailing_newline` SHOULD cover this but doesn't register

**How to test**:
- Verify test is actually running: `./build/tests/unit/repl/repl_slash_command_test`
- Check if pp output ends with `\n` (it does: confirmed earlier)
- This may be a compiler optimization or lcov tracking issue
- Try explicit test with string ending in multiple newlines: "test\n\n"

**Possible fix**: Create test with workspace that generates pp output ending in exactly "\n" to force the edge case

---

## OOM Injection Strategy

The codebase uses talloc for all allocations. You need to:

### Step 1: Check Existing OOM Infrastructure
```bash
grep -r "OOM\|oom_inject\|fail_alloc" tests/
```

Look for existing OOM injection patterns in `tests/test_utils.h` or `tests/test_utils.c`

### Step 2: If No Infrastructure Exists, Create It

**Pattern A: Wrapper Override**
Create a test-only talloc wrapper that can be configured to fail:
```c
// In test_utils.h
extern bool ik_test_oom_enabled;
extern size_t ik_test_oom_countdown; // Fail on Nth allocation

// In test_utils.c
bool ik_test_oom_enabled = false;
size_t ik_test_oom_countdown = 0;

void *ik_test_talloc_wrapper(const void *ctx, size_t size) {
    if (ik_test_oom_enabled && ik_test_oom_countdown == 0) {
        return NULL;  // Simulate OOM
    }
    if (ik_test_oom_enabled && ik_test_oom_countdown > 0) {
        ik_test_oom_countdown--;
    }
    return talloc_size(ctx, size);
}
```

Then modify `src/wrapper.h` to allow test overrides (may require conditional compilation)

**Pattern B: Environment Variable**
Some codebases use `MALLOC_FAILURE_RATE` environment variables

**Pattern C: LD_PRELOAD**
Use LD_PRELOAD to intercept malloc/talloc (complex, avoid if possible)

### Step 3: Write OOM Tests

Example for Gap 1 (render.c:361):
```c
START_TEST(test_render_combined_cursor_position_oom)
{
    void *ctx = talloc_new(NULL);
    ik_render_ctx_t *render_ctx = NULL;

    res_t res = ik_render_create(ctx, 24, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = NULL;
    res = ik_scrollback_create(ctx, 80, &scrollback);
    ck_assert(is_ok(&res));

    // Trigger OOM during cursor position calculation
    // This requires workspace_text_len > 0
    const char *text = "test";

    // Enable OOM on next allocation
    ik_test_oom_enabled = true;
    ik_test_oom_countdown = X;  // Determine X experimentally

    res = ik_render_combined(render_ctx, scrollback, 0, 0, text, 4, 0);

    ik_test_oom_enabled = false;

    // Should get error, not PANIC
    ck_assert(is_err(&res));

    talloc_free(ctx);
}
END_TEST
```

---

## Action Plan

### Task 1: Investigate Non-OOM Paths (30 min)
1. Read `src/render.c::calculate_cursor_screen_position()` - can it fail without OOM?
2. Read `src/scrollback.c::ik_scrollback_get_line_text()` - can it fail? What's its signature?
3. Read `src/workspace_pp.c::ik_pp_workspace()` - can it produce empty output?
4. Read `src/format.c::ik_format_get_string()` - can it return empty string?

**Deliverable**: List which gaps are truly OOM-only vs potentially testable another way

### Task 2: Check Existing OOM Infrastructure (15 min)
1. Search codebase for existing OOM injection patterns
2. Check if `tests/test_utils.h` has OOM helpers
3. Look at how other error injection is done (if any)

**Deliverable**: Document existing patterns or confirm none exist

### Task 3A: If OOM Infrastructure Exists - Use It (2 hours)
1. Write OOM injection tests for gaps 1 and 2
2. Run tests and verify coverage increases
3. Update coverage.md

### Task 3B: If No Infrastructure - Build Minimal Version (4 hours)
1. Design simple OOM injection mechanism (wrapper or environment-based)
2. Implement in `tests/test_utils.c/h`
3. Write OOM tests for gaps 1 and 2
4. Run tests and verify coverage
5. Update coverage.md

### Task 4: Handle Edge Cases (1 hour)
1. For gap 3 (branch 259): Confirm impossible → add `LCOV_EXCL_BR_LINE` with permission
2. For gap 4 (branch 266): Debug why test doesn't cover it, try alternative approach
3. Update coverage.md

### Task 5: Verify 100% Coverage (30 min)
```bash
make clean
make coverage
# Should show 100.0% for all metrics
```

---

## Important Files to Read

1. **Error Handling Philosophy**: `docs/error_handling.md` - understand when OOM causes PANIC vs error
2. **Test Utilities**: `tests/test_utils.h` and `tests/test_utils.c` - check for existing helpers
3. **Coverage Exclusions**: `docs/lcov_exclusion_strategy.md` - when to use LCOV_EXCL markers
4. **Memory Management**: `docs/memory.md` - how talloc is used

---

## Success Criteria

- [ ] Coverage at 100.0% lines (1744/1744)
- [ ] Coverage at 100.0% functions (129/129)
- [ ] Coverage at 100.0% branches (570/570)
- [ ] `make coverage` passes with no errors
- [ ] `make check` passes (all tests pass)
- [ ] `make lint` passes (no complexity violations)
- [ ] All new test code follows TDD principles (tests written first)
- [ ] coverage.md updated to show 100% achieved
- [ ] Any LCOV exclusions justified and documented

---

## Questions to Answer

1. Can `calculate_cursor_screen_position()` fail without OOM? (Check for UTF-8 validation errors)
2. Can `ik_scrollback_get_line_text()` fail? (Check function signature - might return `void`)
3. Can `ik_pp_workspace()` produce empty output? (Likely NO - always outputs structure)
4. Does OOM injection infrastructure already exist in tests/?
5. Is branch 266 a compiler optimization issue preventing coverage tracking?

---

## Quick Start Commands

```bash
# Start fresh
make clean

# Check what remains uncovered
make coverage 2>&1 | tail -50
grep "^DA:" coverage/coverage.info | grep ",0$"
grep "^BRDA:" coverage/coverage.info | grep ",0$"

# Read key functions
cat src/render.c | sed -n '57,90p'  # calculate_cursor_screen_position
grep -A 20 "ik_scrollback_get_line_text" src/scrollback.c
cat src/workspace_pp.c  # Check if can return empty

# Search for OOM patterns
grep -r "OOM\|oom\|fail.*alloc" tests/

# Run specific tests
./build/tests/unit/repl/repl_slash_command_test
./build/tests/unit/render/render_combined_edge_test
```

---

## Expected Outcome

After this session, you should have either:

**Option A (Preferred)**: 100% coverage with OOM injection tests in place
- 2 new tests covering render.c lines 361, 424
- Branch 259 marked with LCOV_EXCL_BR_LINE (if confirmed impossible)
- Branch 266 covered or explained why coverage doesn't register

**Option B (If OOM infrastructure too complex)**: Request permission to exclude
- Document that gaps are OOM-only error paths
- Confirm OOM paths lead to PANIC in production (per error_handling.md)
- Request user permission to add LCOV_EXCL_LINE markers
- Update docs/lcov_exclusion_strategy.md with justification

---

## Context for Next Session

Per `AGENTS.md`:
- **NEVER change LCOV_EXCL_COVERAGE without permission**
- Follow strict TDD: write failing test FIRST, then minimal code
- All tests must pass before commit
- Coverage requirement is ABSOLUTE: 100% or justify exclusions

The codebase follows a PANIC-on-OOM philosophy (see `docs/error_handling.md`), so OOM error paths may be defensive programming that never executes in practice.
