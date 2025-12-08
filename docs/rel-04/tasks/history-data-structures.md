# Task: History Data Structures and Core Operations

## Target
Feature: Command History - Core Data Structures

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/di.md
- .agents/skills/patterns/arena-allocator.md

### Pre-read Docs
- docs/memory.md (talloc ownership patterns)
- docs/error_handling.md
- docs/return_values.md

### Pre-read Source (patterns)
- src/marks.h (checkpoint structure pattern - similar use case)
- src/marks.c (array management with talloc)
- src/repl.h (ik_repl_ctx_t ownership of marks)
- src/scrollback.h (line array management)

### Pre-read Tests (patterns)
- tests/unit/marks/*.c (mark creation and array management tests)

## Pre-conditions
- `make check` passes
- history_size config field exists
- talloc memory management patterns established

## Task
Create `ik_history_t` data structure and core operations for managing command history in memory. This includes:
- Creating history context with configurable size limit
- Adding entries (with automatic oldest entry removal when at capacity)
- Navigating through history (up/down with current position tracking)
- Preserving user's pending input when browsing history
- Getting current entry for display

The history structure should be owned by `ik_repl_ctx_t` and use talloc for memory management.

## TDD Cycle

### Red
1. Create `src/history.h`:
   ```c
   typedef struct {
       char **entries;       // Array of command strings (talloc'd)
       size_t count;         // Current number of entries
       size_t capacity;      // Maximum entries (from config)
       size_t index;         // Current browsing position (count = not browsing)
       char *pending;        // User's pending input before browsing started
   } ik_history_t;

   // Create history context
   ik_history_t *ik_history_create(TALLOC_CTX *ctx, size_t capacity);

   // Add entry to history (appends to end, removes oldest if at capacity)
   res_t ik_history_add(ik_history_t *hist, const char *entry);

   // Start browsing: save pending input and move to last entry
   res_t ik_history_start_browsing(ik_history_t *hist, const char *pending_input);

   // Navigate to previous entry (returns entry or NULL if at beginning)
   const char *ik_history_prev(ik_history_t *hist);

   // Navigate to next entry (returns entry, pending, or NULL)
   const char *ik_history_next(ik_history_t *hist);

   // Stop browsing and return to pending input
   void ik_history_stop_browsing(ik_history_t *hist);

   // Get current entry (for display during browsing)
   const char *ik_history_get_current(const ik_history_t *hist);

   // Check if currently browsing
   bool ik_history_is_browsing(const ik_history_t *hist);
   ```

2. Create `tests/unit/history/core_test.c`:
   - Test history creation with capacity
   - Test adding entries (within capacity)
   - Test adding entries (exceeds capacity, oldest removed)
   - Test browsing workflow (start -> prev -> prev -> next -> stop)
   - Test pending input preservation
   - Test navigation boundaries (can't go before first, after pending)
   - Test empty history browsing (no-op)

3. Run `make check` - expect test failures (implementation missing)

### Green
1. Create `src/history.c` with implementations:
   - `ik_history_create()`: Allocate struct, initialize fields, pre-allocate entries array
   - `ik_history_add()`:
     - If at capacity, free oldest entry and shift array left
     - Append new entry with talloc_strdup
     - Increment count (capped at capacity)
   - `ik_history_start_browsing()`: Save pending input, set index to count-1
   - `ik_history_prev()`: Decrement index (if > 0), return entries[index]
   - `ik_history_next()`:
     - Increment index (if < count)
     - If index == count, return pending input
     - Otherwise return entries[index]
   - `ik_history_stop_browsing()`: Set index to count, free pending
   - `ik_history_get_current()`: Return entries[index] or pending if index == count
   - `ik_history_is_browsing()`: Return index < count

2. Add field to `src/repl.h`:
   ```c
   typedef struct ik_repl_ctx_t {
       // ... existing fields ...
       ik_history_t *history;  // Command history
   } ik_repl_ctx_t;
   ```

3. Initialize in repl init, cleanup is automatic via talloc

4. Run `make check` - expect pass

### Refactor
1. Consider edge cases:
   - Adding empty string to history (should it be allowed?)
   - Very large capacity values
   - Browsing empty history
2. Ensure all allocations use proper talloc context hierarchy
3. Run `make check` and `make lint` - verify still green

## Post-conditions
- `make check` passes
- `ik_history_t` structure exists with core operations
- History is owned by `ik_repl_ctx_t`
- Adding entries works with capacity management
- Navigation (prev/next) works correctly
- Pending input is preserved during browsing
- Empty history is handled gracefully
- 100% test coverage maintained
