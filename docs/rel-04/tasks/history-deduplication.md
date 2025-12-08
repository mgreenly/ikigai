# Task: History Deduplication on Submission

## Target
Feature: Command History - Deduplication Logic

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md

### Pre-read Docs
- docs/backlog/readline-features.md (deduplication specification)

### Pre-read Source (patterns)
- src/history.h (ik_history_add function)
- src/history.c (history add implementation)

### Pre-read Tests (patterns)
- tests/unit/history/core_test.c (history operation tests)

## Pre-conditions
- `make check` passes
- History add/load/save functions exist
- History navigation works
- Input submission triggers history append

## Task
Modify history entry addition to implement deduplication rules:

**Rules**:
1. **Identical reuse**: If user submits same command as already exists in history, remove old occurrence and add to end
2. **Consecutive duplicates**: If user submits same command as the most recent entry, do NOT add duplicate
3. **Edited history**: If user browses to history entry and modifies it, the modified version is added as new entry (original unchanged)

This ensures clean history without consecutive duplicates while preserving user's ability to reuse commands.

## TDD Cycle

### Red
1. Add tests to `tests/unit/history/core_test.c`:
   ```c
   START_TEST(test_history_dedup_consecutive_identical)
   {
       // Add "hello"
       // Add "hello" again
       // Verify count == 1 (duplicate not added)
   }
   END_TEST

   START_TEST(test_history_dedup_reuse_moves_to_end)
   {
       // Add "first", "second", "third"
       // Add "second" again
       // Verify order: "first", "third", "second"
       // Verify count == 3 (not 4)
   }
   END_TEST

   START_TEST(test_history_dedup_edited_creates_new)
   {
       // Add "original"
       // Add "edited" (different content)
       // Verify count == 2
       // Verify both entries exist
   }
   END_TEST

   START_TEST(test_history_dedup_case_sensitive)
   {
       // Add "Hello"
       // Add "hello" (different case)
       // Verify count == 2 (case matters)
   }
   END_TEST

   START_TEST(test_history_dedup_whitespace_significant)
   {
       // Add "hello"
       // Add "hello " (trailing space)
       // Verify count == 2 (whitespace matters)
   }
   END_TEST
   ```

2. Run `make check` - expect test failures (deduplication not implemented)

### Green
1. Modify `ik_history_add()` in `src/history.c`:
   ```c
   res_t ik_history_add(ik_history_t *hist, const char *entry) {
       assert(hist != NULL);
       assert(entry != NULL);

       // Rule 2: Skip if identical to most recent entry
       if (hist->count > 0) {
           if (strcmp(hist->entries[hist->count - 1], entry) == 0) {
               return OK(NULL);  // Skip consecutive duplicate
           }
       }

       // Rule 1: Check if entry exists anywhere in history
       for (size_t i = 0; i < hist->count; i++) {
           if (strcmp(hist->entries[i], entry) == 0) {
               // Found existing - remove it
               talloc_free(hist->entries[i]);
               // Shift remaining entries left
               for (size_t j = i; j < hist->count - 1; j++) {
                   hist->entries[j] = hist->entries[j + 1];
               }
               hist->count--;
               break;
           }
       }

       // Add to end (existing add logic)
       if (hist->count >= hist->capacity) {
           // Remove oldest
           talloc_free(hist->entries[0]);
           for (size_t i = 0; i < hist->count - 1; i++) {
               hist->entries[i] = hist->entries[i + 1];
           }
           hist->count--;
       }

       hist->entries[hist->count] = talloc_strdup(hist, entry);
       if (hist->entries[hist->count] == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
       hist->count++;

       return OK(NULL);
   }
   ```

2. Run `make check` - expect pass

### Refactor
1. Consider performance: linear search for duplicates is O(n), acceptable for history size
2. Ensure talloc ownership is correct (freed entries are removed from parent)
3. Run `make check` and `make lint` - verify still green

## Post-conditions
- `make check` passes
- Consecutive identical entries are not added
- Reusing existing command moves it to end (removes old position)
- Edited history entries create new entries
- Case and whitespace are significant in comparison
- 100% test coverage maintained
