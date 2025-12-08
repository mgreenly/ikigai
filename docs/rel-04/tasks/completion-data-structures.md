# Task: Completion Data Structures and Matching Logic

## Target
Feature: Tab Completion - Core Data Structures

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/di.md

### Pre-read Docs
- docs/memory.md (talloc ownership)
- docs/backlog/readline-features.md (completion specification)

### Pre-read Source (patterns)
- src/commands.h (ik_command_t, ik_cmd_get_all)
- src/repl.h (ik_repl_ctx_t ownership pattern)

### Pre-read Tests (patterns)
- tests/unit/commands/dispatch_test.c (command registry tests)

## Pre-conditions
- `make check` passes
- IK_INPUT_TAB action exists
- Command registry works (ik_cmd_get_all returns all commands)

## Task
Create completion data structures and matching logic:

**Data structure** (`ik_completion_t`):
- Array of candidate strings (commands or arguments)
- Count of candidates
- Current selection index (for arrow key navigation)
- Original input that triggered completion (for re-matching as user types)

**Matching logic**:
- Given input text starting with `/`, find all commands that match prefix
- Case-sensitive matching
- Alphabetically sorted results
- Maximum 10 suggestions
- Return NULL if no matches (completion not possible)

**Ownership**: Completion context is created dynamically when TAB pressed, freed when dismissed.

## TDD Cycle

### Red
1. Create `src/completion.h`:
   ```c
   typedef struct {
       char **candidates;    // Array of matching strings (talloc'd)
       size_t count;         // Number of matches
       size_t current;       // Currently selected index (0-based)
       char *prefix;         // Original prefix that triggered completion
   } ik_completion_t;

   // Create completion context for command matching
   // Returns NULL if no matches found
   // Limit results to max 10, alphabetically sorted
   ik_completion_t *ik_completion_create_for_commands(TALLOC_CTX *ctx,
                                                       const char *prefix);

   // Get currently selected candidate
   const char *ik_completion_get_current(const ik_completion_t *comp);

   // Move selection to next candidate (wraps around)
   void ik_completion_next(ik_completion_t *comp);

   // Move selection to previous candidate (wraps around)
   void ik_completion_prev(ik_completion_t *comp);

   // Check if input still matches prefix (for re-triggering)
   bool ik_completion_matches_prefix(const ik_completion_t *comp,
                                      const char *current_input);
   ```

2. Create `tests/unit/completion/matching_test.c`:
   ```c
   START_TEST(test_completion_single_match)
   {
       // Commands: /clear, /help, /mark, /model
       // Prefix: "/cl"
       // Expect: 1 match: "clear"
       ik_completion_t *comp = ik_completion_create_for_commands(NULL, "/cl");
       ck_assert_ptr_nonnull(comp);
       ck_assert_int_eq(comp->count, 1);
       ck_assert_str_eq(comp->candidates[0], "clear");
       talloc_free(comp);
   }
   END_TEST

   START_TEST(test_completion_multiple_matches_sorted)
   {
       // Prefix: "/m"
       // Expect: "mark", "model" (alphabetically sorted)
       ik_completion_t *comp = ik_completion_create_for_commands(NULL, "/m");
       ck_assert_ptr_nonnull(comp);
       ck_assert_int_eq(comp->count, 2);
       ck_assert_str_eq(comp->candidates[0], "mark");
       ck_assert_str_eq(comp->candidates[1], "model");
   }
   END_TEST

   START_TEST(test_completion_no_matches)
   {
       // Prefix: "/xyz"
       // Expect: NULL (no matches)
       ik_completion_t *comp = ik_completion_create_for_commands(NULL, "/xyz");
       ck_assert_ptr_null(comp);
   }
   END_TEST

   START_TEST(test_completion_max_10_results)
   {
       // If we have >10 commands matching prefix
       // Expect: Only first 10 (alphabetically)
       // (May not be testable with current command set)
   }
   END_TEST

   START_TEST(test_completion_navigation)
   {
       // Create completion with ["mark", "model"]
       ik_completion_t *comp = ik_completion_create_for_commands(NULL, "/m");

       // Initially selected: index 0 ("mark")
       ck_assert_str_eq(ik_completion_get_current(comp), "mark");

       // Next -> index 1 ("model")
       ik_completion_next(comp);
       ck_assert_str_eq(ik_completion_get_current(comp), "model");

       // Next -> wraps to index 0 ("mark")
       ik_completion_next(comp);
       ck_assert_str_eq(ik_completion_get_current(comp), "mark");

       // Prev -> wraps to index 1 ("model")
       ik_completion_prev(comp);
       ck_assert_str_eq(ik_completion_get_current(comp), "model");

       talloc_free(comp);
   }
   END_TEST

   START_TEST(test_completion_prefix_matching)
   {
       ik_completion_t *comp = ik_completion_create_for_commands(NULL, "/m");

       ck_assert(ik_completion_matches_prefix(comp, "/m"));
       ck_assert(ik_completion_matches_prefix(comp, "/ma"));
       ck_assert(ik_completion_matches_prefix(comp, "/mar"));
       ck_assert(!ik_completion_matches_prefix(comp, "/h"));  // Different prefix
       ck_assert(!ik_completion_matches_prefix(comp, "m"));   // Missing slash

       talloc_free(comp);
   }
   END_TEST
   ```

3. Run `make check` - expect test failures

### Green
1. Create `src/completion.c`:
   ```c
   #include "completion.h"
   #include "commands.h"
   #include <stdlib.h>
   #include <string.h>
   #include <assert.h>

   ik_completion_t *ik_completion_create_for_commands(TALLOC_CTX *ctx,
                                                       const char *prefix) {
       assert(prefix != NULL);
       assert(prefix[0] == '/');

       size_t cmd_count;
       const ik_command_t *commands = ik_cmd_get_all(&cmd_count);

       // Skip leading '/'
       const char *search = prefix + 1;
       size_t search_len = strlen(search);

       // First pass: count matches
       size_t match_count = 0;
       for (size_t i = 0; i < cmd_count; i++) {
           if (strncmp(commands[i].name, search, search_len) == 0) {
               match_count++;
           }
       }

       if (match_count == 0) {
           return NULL;  // No matches
       }

       // Create completion context
       ik_completion_t *comp = talloc_zero(ctx, ik_completion_t);
       if (comp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       comp->prefix = talloc_strdup(comp, prefix);
       if (comp->prefix == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       // Limit to 10 matches
       comp->count = match_count > 10 ? 10 : match_count;
       comp->candidates = talloc_array(comp, char *, comp->count);
       if (comp->candidates == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       // Second pass: collect matches (already sorted by command registry)
       size_t idx = 0;
       for (size_t i = 0; i < cmd_count && idx < comp->count; i++) {
           if (strncmp(commands[i].name, search, search_len) == 0) {
               comp->candidates[idx] = talloc_strdup(comp->candidates,
                                                     commands[i].name);
               if (comp->candidates[idx] == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
               idx++;
           }
       }

       comp->current = 0;  // Start with first match selected
       return comp;
   }

   const char *ik_completion_get_current(const ik_completion_t *comp) {
       assert(comp != NULL);
       assert(comp->count > 0);
       return comp->candidates[comp->current];
   }

   void ik_completion_next(ik_completion_t *comp) {
       assert(comp != NULL);
       comp->current = (comp->current + 1) % comp->count;
   }

   void ik_completion_prev(ik_completion_t *comp) {
       assert(comp != NULL);
       if (comp->current == 0) {
           comp->current = comp->count - 1;
       } else {
           comp->current--;
       }
   }

   bool ik_completion_matches_prefix(const ik_completion_t *comp,
                                      const char *current_input) {
       assert(comp != NULL);
       assert(current_input != NULL);

       size_t prefix_len = strlen(comp->prefix);
       return strncmp(current_input, comp->prefix, prefix_len) == 0;
   }
   ```

2. Add to Makefile if needed

3. Run `make check` - expect pass

### Refactor
1. Consider: Should qsort be used for alphabetical sorting if command registry not sorted?
2. Ensure all talloc allocations have correct parent context
3. Verify edge cases: empty prefix, single character prefix
4. Run `make check` and `make lint` - verify still green

## Post-conditions
- `make check` passes
- `ik_completion_t` structure exists
- Command matching works (prefix-based, case-sensitive)
- Results are alphabetically sorted
- Maximum 10 results enforced
- Navigation functions (next/prev) work with wraparound
- Prefix matching validation works
- No matches returns NULL
- 100% test coverage maintained
