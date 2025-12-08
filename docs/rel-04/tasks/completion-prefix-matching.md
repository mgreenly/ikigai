# Task: Completion Prefix Matching

## Target
Feature: Autocompletion - Behavior Fix

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md

### Pre-read Docs
None

### Pre-read Source (patterns)
- src/fzy_wrapper.c (current fuzzy filter - has the bug)
- src/fzy_wrapper.h (wrapper API)
- src/vendor/fzy/match.h (fzy library API - do NOT modify)
- src/completion.c (uses fzy wrapper)

### Pre-read Tests (patterns)
- tests/unit/vendor/fzy_wrapper_test.c

## Pre-conditions
- `make check` passes
- `make lint` passes
- input-layer-newline task complete

## Task
Fix the fzy wrapper to enforce **prefix matching** for command completion. Currently, `/m` incorrectly shows "system" because fzy's subsequence matching finds 'm' at position 5 (syste**m**).

**Current behavior:**
- `/m` shows: mark, model, **system** (wrong - system doesn't start with 'm')
- fzy's `has_match()` does subsequence matching, not prefix matching

**Expected behavior:**
- `/m` shows: mark, model (only commands starting with 'm')
- Standard shell completion behavior is prefix-based

**Fix:** Add a prefix check in `ik_fzy_filter()` before calling `has_match()`. Only consider candidates that start with the search string (case-insensitive).

**Implementation:**
```c
// Before has_match check, add:
if (strncasecmp(candidates[i], search, strlen(search)) != 0) {
    continue;  // Skip candidates that don't start with search
}
```

This keeps fzy for scoring/ranking among matches, but filters to prefix-only candidates first.

**Do NOT modify:** The vendored fzy code in `src/vendor/fzy/`

## TDD Cycle

### Red
1. Add test in `tests/unit/vendor/fzy_wrapper_test.c`:
   ```c
   START_TEST(test_fzy_filter_prefix_only)
   {
       const char *candidates[] = {"mark", "model", "system", "clear"};
       size_t count = 0;

       ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 4, "m", 10, &count);

       // Only mark and model start with 'm'
       // system should NOT match (has 'm' but not at start)
       ck_assert_ptr_nonnull(results);
       ck_assert_uint_eq(count, 2);

       // Verify results are mark and model (in some order by score)
       bool found_mark = false, found_model = false;
       for (size_t i = 0; i < count; i++) {
           if (strcmp(results[i].candidate, "mark") == 0) found_mark = true;
           if (strcmp(results[i].candidate, "model") == 0) found_model = true;
       }
       ck_assert(found_mark);
       ck_assert(found_model);
   }
   END_TEST

   START_TEST(test_fzy_filter_prefix_case_insensitive)
   {
       const char *candidates[] = {"Mark", "MODEL", "System"};
       size_t count = 0;

       ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 3, "m", 10, &count);

       // Case-insensitive prefix match
       ck_assert_ptr_nonnull(results);
       ck_assert_uint_eq(count, 2);  // Mark and MODEL
   }
   END_TEST

   START_TEST(test_fzy_filter_no_prefix_match)
   {
       const char *candidates[] = {"system", "clear", "help"};
       size_t count = 0;

       ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 3, "m", 10, &count);

       // None start with 'm'
       ck_assert_ptr_null(results);
       ck_assert_uint_eq(count, 0);
   }
   END_TEST
   ```

2. Run `make check` - expect failure (system incorrectly included)

### Green
1. Modify `ik_fzy_filter()` in `src/fzy_wrapper.c`:
   ```c
   #include <strings.h>  // for strncasecmp

   // In the loop, before has_match():
   size_t search_len = strlen(search);
   for (size_t i = 0; i < candidate_count; i++) {
       // Prefix check: candidate must start with search string
       if (strncasecmp(candidates[i], search, search_len) != 0) {
           continue;
       }

       if (has_match(search, candidates[i])) {
           scored[match_count].index = i;
           scored[match_count].score = match(search, candidates[i]);
           match_count++;
       }
   }
   ```

2. Run `make check` - expect pass

### Refactor
1. Consider extracting search_len calculation outside the loop
2. Run `make lint` - verify passes
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint` passes
- `ik_fzy_filter()` only returns candidates that start with search string
- Case-insensitive prefix matching works correctly
- Vendored fzy code unchanged
