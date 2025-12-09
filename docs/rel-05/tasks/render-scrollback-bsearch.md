# Task: Binary Search for Scrollback Line Lookup

## Target
Render Performance Optimization - O(log n) scrollback line lookup instead of O(n)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- docs/memory.md (talloc ownership)

## Pre-read Source (patterns)
- src/scrollback.c (lines 346-359 - linear scan for physical row lookup)
- src/scrollback.h (ik_scrollback_t structure, ik_line_layout_t)

## Pre-read Tests (patterns)
- tests/unit/scrollback/*.c (scrollback test patterns)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `ik_scrollback_get_line_at_physical_row()` uses O(n) linear scan

## Task
Replace linear scan with binary search for finding logical line at a physical row. Current code iterates through all lines:

```c
for (size_t i = 0; i < scrollback->count; i++) {
    size_t line_physical_lines = scrollback->layouts[i].physical_lines;
    if (physical_row < current_row + line_physical_lines) {
        // found
    }
    current_row += line_physical_lines;
}
```

This is O(n) and becomes slow with thousands of lines. Maintain a prefix sum array of physical line counts to enable O(log n) binary search.

## TDD Cycle

### Red
1. Add prefix sum array to `ik_scrollback_t` in `src/scrollback.h`:
   ```c
   // Prefix sum of physical lines for O(log n) lookup
   // prefix_sums[i] = total physical lines for lines 0..i-1
   // prefix_sums[0] = 0, prefix_sums[count] = total physical lines
   size_t *prefix_sums;
   ```

2. Create test in `tests/unit/scrollback/bsearch_test.c`:
   - Test lookup in empty scrollback
   - Test lookup in single-line scrollback
   - Test lookup finds correct line in multi-line scrollback
   - Test lookup with wrapped lines (multiple physical per logical)
   - Test lookup at physical row 0
   - Test lookup at last physical row
   - Test lookup after line insertion updates prefix sums
   - Test performance with 10000 lines (should be fast)

3. Run `make check` - expect test failures

### Green
1. Add `ik_scrollback_rebuild_prefix_sums()` function:
   ```c
   void ik_scrollback_rebuild_prefix_sums(ik_scrollback_t *scrollback)
   {
       if (scrollback->prefix_sums == NULL) {
           scrollback->prefix_sums = talloc_array_(scrollback, size_t,
                                                    scrollback->capacity + 1);
       }
       scrollback->prefix_sums[0] = 0;
       for (size_t i = 0; i < scrollback->count; i++) {
           scrollback->prefix_sums[i + 1] = scrollback->prefix_sums[i] +
                                             scrollback->layouts[i].physical_lines;
       }
   }
   ```

2. Call `ik_scrollback_rebuild_prefix_sums()` after:
   - Adding lines
   - Layout recalculation (terminal resize)
   - Any modification to physical_lines

3. Implement binary search in `ik_scrollback_get_line_at_physical_row()`:
   ```c
   // Binary search: find largest i where prefix_sums[i] <= physical_row
   size_t lo = 0, hi = scrollback->count;
   while (lo < hi) {
       size_t mid = lo + (hi - lo + 1) / 2;
       if (scrollback->prefix_sums[mid] <= physical_row) {
           lo = mid;
       } else {
           hi = mid - 1;
       }
   }
   // lo is the logical line index
   *offset_within_line = physical_row - scrollback->prefix_sums[lo];
   return lo;
   ```

4. Run `make check` - expect pass

### Refactor
1. Consider incremental prefix sum update on single line insert (avoid full rebuild)
2. Verify memory is freed properly
3. Run `make lint` - verify clean

## Post-conditions
- Working tree is clean (all changes committed)
- `make check` passes
- `ik_scrollback_t` has `prefix_sums` array
- Line lookup is O(log n) via binary search
- Prefix sums are maintained on modifications
- Performance test confirms fast lookup with large scrollbacks
