# Task: Create fzy Wrapper Module

## Target
Feature: Autocompletion - fzy Integration

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md

### Pre-read Docs
None

### Pre-read Source (patterns)
- src/vendor/fzy/match.h (fzy API)
- src/completion.h (current completion structures)

### Pre-read Tests (patterns)
None

## Pre-conditions
- `make check` passes
- fzy library vendored (completion-fzy-vendor task complete)

## Task
Create a talloc-aware wrapper around fzy that:
1. Scores a candidate against a search string
2. Returns results in a talloc-managed structure
3. Provides sorted results by score

The wrapper provides a clean interface between fzy's raw C API and ikigai's talloc-based memory management.

## TDD Cycle

### Red
1. Create `src/fzy_wrapper.h`:
   ```c
   #ifndef IK_FZY_WRAPPER_H
   #define IK_FZY_WRAPPER_H

   #include <talloc.h>
   #include <stddef.h>
   #include <stdbool.h>

   // Scored candidate result
   typedef struct {
       const char *candidate;  // Points to original (not copied)
       double score;           // fzy score (higher = better match)
   } ik_fzy_result_t;

   // Filter and score candidates against a search string
   // Returns array of results sorted by score (descending), limited to max_results
   // Returns NULL if no matches found
   // candidates: NULL-terminated array of strings to search
   // search: the search string to match against
   // max_results: maximum number of results to return (e.g., 10)
   // count_out: set to number of results returned
   ik_fzy_result_t *ik_fzy_filter(TALLOC_CTX *ctx,
                                   const char **candidates,
                                   size_t candidate_count,
                                   const char *search,
                                   size_t max_results,
                                   size_t *count_out);

   #endif
   ```

2. Add test in `tests/unit/fzy/wrapper_test.c`:
   ```c
   START_TEST(test_fzy_filter_basic)
   {
       const char *candidates[] = {"mark", "model", "clear", "help"};
       size_t count = 0;

       ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 4, "m", 10, &count);

       ck_assert_ptr_nonnull(results);
       ck_assert_uint_eq(count, 2);  // mark, model match "m"
       // Results should be sorted by score
       ck_assert(results[0].score >= results[1].score);
   }
   END_TEST

   START_TEST(test_fzy_filter_no_match)
   {
       const char *candidates[] = {"mark", "model"};
       size_t count = 0;

       ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 2, "xyz", 10, &count);

       ck_assert_ptr_null(results);
       ck_assert_uint_eq(count, 0);
   }
   END_TEST

   START_TEST(test_fzy_filter_max_results)
   {
       const char *candidates[] = {"a", "ab", "abc", "abcd", "abcde"};
       size_t count = 0;

       ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 5, "a", 3, &count);

       ck_assert_ptr_nonnull(results);
       ck_assert_uint_eq(count, 3);  // Limited to 3
   }
   END_TEST
   ```

3. Run `make check` - expect failure (not implemented)

### Green
1. Create `src/fzy_wrapper.c`:
   ```c
   #include "fzy_wrapper.h"
   #include "vendor/fzy/match.h"
   #include "panic.h"
   #include <stdlib.h>
   #include <math.h>

   // Internal struct for sorting
   typedef struct {
       size_t index;
       double score;
   } scored_index_t;

   // Comparison for qsort (descending by score)
   static int compare_scores(const void *a, const void *b)
   {
       const scored_index_t *sa = (const scored_index_t *)a;
       const scored_index_t *sb = (const scored_index_t *)b;
       if (sb->score > sa->score) return 1;
       if (sb->score < sa->score) return -1;
       return 0;
   }

   ik_fzy_result_t *ik_fzy_filter(TALLOC_CTX *ctx,
                                   const char **candidates,
                                   size_t candidate_count,
                                   const char *search,
                                   size_t max_results,
                                   size_t *count_out)
   {
       assert(ctx != NULL);
       assert(candidates != NULL);
       assert(search != NULL);
       assert(count_out != NULL);

       *count_out = 0;

       if (candidate_count == 0) return NULL;

       // Score all candidates
       scored_index_t *scored = talloc_array(ctx, scored_index_t, candidate_count);
       if (scored == NULL) PANIC("OOM");

       size_t match_count = 0;
       for (size_t i = 0; i < candidate_count; i++) {
           if (has_match(search, candidates[i])) {
               scored[match_count].index = i;
               scored[match_count].score = match(search, candidates[i]);
               match_count++;
           }
       }

       if (match_count == 0) {
           talloc_free(scored);
           return NULL;
       }

       // Sort by score (descending)
       qsort(scored, match_count, sizeof(scored_index_t), compare_scores);

       // Limit results
       size_t result_count = (match_count < max_results) ? match_count : max_results;

       // Build result array
       ik_fzy_result_t *results = talloc_array(ctx, ik_fzy_result_t, result_count);
       if (results == NULL) PANIC("OOM");

       for (size_t i = 0; i < result_count; i++) {
           results[i].candidate = candidates[scored[i].index];
           results[i].score = scored[i].score;
       }

       talloc_free(scored);
       *count_out = result_count;
       return results;
   }
   ```

2. Update Makefile with new source file
3. Run `make check` - expect pass

### Refactor
1. Add edge case tests (empty search string, single candidate, etc.)
2. Run `make lint` - verify passes
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint` passes
- `ik_fzy_filter()` function available
- Wrapper handles memory management with talloc
- Results sorted by score descending
- Max results limit enforced
