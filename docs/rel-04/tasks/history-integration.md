# Task: History Integration with REPL Lifecycle

## Target
Feature: Command History - End-to-End Integration

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/errors.md

### Pre-read Docs
- docs/backlog/readline-features.md (full feature specification)

### Pre-read Source (patterns)
- src/repl.h (ik_repl_ctx_t, initialization)
- src/repl.c (ik_repl_init, ik_repl_cleanup, ik_repl_submit_line)
- src/history.h (all history functions)
- src/config.h (history_size field)

### Pre-read Tests (patterns)
- tests/integration/repl_test.c (REPL lifecycle tests if they exist)
- tests/unit/repl/submit_test.c (input submission tests)

## Pre-conditions
- `make check` passes
- All history functions implemented and tested
- History navigation works
- History deduplication works
- Config has history_size field

## Task
Integrate history into REPL lifecycle:

1. **Initialization** (`ik_repl_init`):
   - Create history structure with capacity from config
   - Load history from `.ikigai/history` file
   - Handle load errors gracefully (log warning, continue with empty history)

2. **Submission** (`ik_repl_submit_line`):
   - After processing user input, add to history with deduplication
   - Append to `.ikigai/history` file
   - Exit browsing mode if active

3. **Cleanup** (`ik_repl_cleanup`):
   - History automatically freed via talloc (already owned by repl context)
   - Optionally save full history on exit (defensive save)

4. **Edge cases**:
   - Empty input should not be added to history
   - Slash commands should be added to history
   - Multi-line input should be preserved in history

## TDD Cycle

### Red
1. Create `tests/integration/history_lifecycle_test.c`:
   ```c
   START_TEST(test_history_loads_on_init)
   {
       // Setup: Create .ikigai/history with entries
       // Initialize REPL
       // Verify history loaded (navigate with Up arrow)
       // Cleanup
   }
   END_TEST

   START_TEST(test_history_saves_on_submit)
   {
       // Initialize REPL
       // Submit command
       // Verify .ikigai/history file updated
       // Verify entry in correct JSONL format
   }
   END_TEST

   START_TEST(test_history_survives_repl_restart)
   {
       // Session 1: Submit "command1", cleanup
       // Session 2: Init REPL, verify "command1" in history
   }
   END_TEST

   START_TEST(test_history_respects_config_capacity)
   {
       // Config with history_size=5
       // Submit 10 commands
       // Verify only last 5 in history
       // Verify file has only 5 entries
   }
   END_TEST

   START_TEST(test_history_empty_input_not_saved)
   {
       // Submit empty string
       // Verify not added to history
   }
   END_TEST

   START_TEST(test_history_multiline_preserved)
   {
       // Submit "line1\nline2"
       // Reload REPL
       // Navigate to history
       // Verify newline preserved
   }
   END_TEST

   START_TEST(test_history_file_corrupt_continues)
   {
       // Create .ikigai/history with invalid JSON
       // Initialize REPL
       // Verify REPL starts successfully (empty history or partial load)
   }
   END_TEST
   ```

2. Run `make check` - expect test failures

### Green
1. Modify `src/repl.c`:

   **In `ik_repl_init()`**:
   ```c
   // Create history (after config loaded)
   repl->history = ik_history_create(repl, cfg->history_size);

   // Ensure directory exists
   res_t dir_res = ik_history_ensure_directory();
   if (!dir_res.ok) {
       // Log warning but continue
       fprintf(stderr, "Warning: Could not create history directory: %s\n",
               dir_res.err->msg);
   }

   // Load history from file
   res_t load_res = ik_history_load(repl, repl->history);
   if (!load_res.ok) {
       // Log warning but continue with empty history
       fprintf(stderr, "Warning: Could not load history: %s\n",
               load_res.err->msg);
   }
   ```

   **In `ik_repl_submit_line()` (or wherever input is processed)**:
   ```c
   // After getting input text, before clearing buffer
   size_t len;
   const char *text = ik_input_buffer_get_text(repl->input_buffer, &len);

   // Don't save empty input
   if (len > 0) {
       // Stop browsing if active
       if (ik_history_is_browsing(repl->history)) {
           ik_history_stop_browsing(repl->history);
       }

       // Add to history (with deduplication)
       res_t add_res = ik_history_add(repl->history, text);
       if (add_res.ok) {
           // Append to file
           res_t append_res = ik_history_append_entry(text);
           if (!append_res.ok) {
               // Log warning but continue
               fprintf(stderr, "Warning: Could not save to history file: %s\n",
                       append_res.err->msg);
           }
       }
   }
   ```

   **In `ik_repl_cleanup()`**:
   ```c
   // Defensive full save (optional, since we append on each entry)
   // This handles case where file was deleted during session
   if (repl->history != NULL) {
       res_t save_res = ik_history_save(repl->history);
       if (!save_res.ok) {
           fprintf(stderr, "Warning: Could not save history on exit: %s\n",
                   save_res.err->msg);
       }
   }
   // Actual cleanup via talloc is automatic
   ```

2. Run `make check` - expect pass

### Refactor
1. Consider using logger instead of fprintf for warnings (once logger available)
2. Verify error handling doesn't break REPL workflow
3. Test with very large history files (performance)
4. Run `make check` and `make lint` - verify still green

## Post-conditions
- `make check` passes
- History loads automatically on REPL startup
- Commands are saved incrementally to file
- History persists across REPL restarts
- Config history_size is respected
- Empty input is not saved to history
- Multi-line input is preserved correctly
- Corrupt history files don't crash REPL (graceful degradation)
- 100% test coverage maintained

## Success Criteria
The command history feature is fully functional:
- ✓ Up/Down navigation at cursor position 0
- ✓ Persistent storage in `.ikigai/history` (JSONL)
- ✓ Deduplication (no consecutive duplicates, reuse moves to end)
- ✓ Configurable capacity via `history_size`
- ✓ Graceful error handling
- ✓ Multi-line command support
