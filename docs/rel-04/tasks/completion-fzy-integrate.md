# Task: Integrate fzy into Completion Module

## Target
Feature: Autocompletion - fzy Integration

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md

### Pre-read Docs
None

### Pre-read Source (patterns)
- src/completion.h (current completion API)
- src/completion.c (current strncmp-based matching)
- src/fzy_wrapper.h (fzy wrapper API)
- src/commands.c (command registry)

### Pre-read Tests (patterns)
- tests/unit/completion/matching_test.c (existing completion tests)

## Pre-conditions
- `make check` passes
- fzy wrapper complete (completion-fzy-wrapper task)

## Task
Replace the strncmp-based prefix matching in completion.c with fzy fuzzy matching. This changes the completion algorithm from strict prefix matching to fuzzy matching with score-based ordering.

Key changes:
1. Use fzy for filtering/scoring instead of strncmp
2. Sort by fzy score instead of alphabetically
3. Keep max 10 results limit
4. Update tests to reflect new behavior

Note: fzy is case-insensitive by default, which changes behavior from current case-sensitive matching.

## TDD Cycle

### Red
1. Update test expectations in `tests/unit/completion/matching_test.c`:
   - `test_case_sensitive_matching`: "/M" should NOW match "mark", "model" (fzy is case-insensitive)
   - `test_multiple_matches_sorted`: Results sorted by score, not alphabetically
   - Add new test for fuzzy matching:
     ```c
     START_TEST(test_fuzzy_matching)
     {
         // "ml" should match "model" (m...l)
         ik_completion_t *comp = ik_completion_create_for_commands(ctx, "/ml");
         ck_assert_ptr_nonnull(comp);
         ck_assert_uint_ge(comp->count, 1);
         // model should be in results
         bool found_model = false;
         for (size_t i = 0; i < comp->count; i++) {
             if (strcmp(comp->candidates[i], "model") == 0) {
                 found_model = true;
                 break;
             }
         }
         ck_assert(found_model);
     }
     END_TEST
     ```

2. Run `make check` - expect failures (still using strncmp)

### Green
1. Modify `ik_completion_create_for_commands()` in `src/completion.c`:
   ```c
   ik_completion_t *ik_completion_create_for_commands(TALLOC_CTX *ctx,
                                                       const char *prefix)
   {
       assert(ctx != NULL);
       assert(prefix != NULL);
       assert(prefix[0] == '/');

       // Skip the leading '/' to get the search string
       const char *search = prefix + 1;

       // Get all registered commands
       size_t cmd_count;
       const ik_command_t *commands = ik_cmd_get_all(&cmd_count);

       // Build candidate array
       const char **candidates = talloc_array(ctx, const char *, cmd_count);
       if (candidates == NULL) PANIC("OOM");

       for (size_t i = 0; i < cmd_count; i++) {
           candidates[i] = commands[i].name;
       }

       // Use fzy to filter and score
       size_t match_count = 0;
       ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, cmd_count,
                                                 search, MAX_COMPLETIONS, &match_count);
       talloc_free(candidates);

       if (results == NULL || match_count == 0) {
           return NULL;
       }

       // Build completion context from fzy results
       ik_completion_t *comp = talloc_zero(ctx, ik_completion_t);
       if (!comp) PANIC("OOM");

       comp->candidates = talloc_array(comp, char *, match_count);
       if (!comp->candidates) PANIC("OOM");

       comp->prefix = talloc_strdup(comp, prefix);
       if (!comp->prefix) PANIC("OOM");

       for (size_t i = 0; i < match_count; i++) {
           comp->candidates[i] = talloc_strdup(comp, results[i].candidate);
           if (!comp->candidates[i]) PANIC("OOM");
       }

       comp->count = match_count;
       comp->current = 0;

       talloc_free(results);
       return comp;
   }
   ```

2. Add `#include "fzy_wrapper.h"` to completion.c
3. Run `make check` - expect pass

### Refactor
1. Apply same fzy integration to `ik_completion_create_for_arguments()` (future task)
2. Remove now-unused strncmp comparison function
3. Run `make lint` - verify passes
4. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `make lint` passes
- Completion uses fzy for fuzzy matching
- Results sorted by fzy score (best match first)
- Case-insensitive matching enabled
- Max 10 results maintained
