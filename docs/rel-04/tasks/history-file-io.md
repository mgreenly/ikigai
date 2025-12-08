# Task: History File I/O (JSONL Load/Save)

## Target
Feature: Command History - File Persistence

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/errors.md

### Pre-read Docs
- docs/error_handling.md
- docs/return_values.md
- docs/backlog/readline-features.md (JSONL format specification)

### Pre-read Source (patterns)
- src/config.c (yyjson file reading patterns, error handling)
- src/db/message.c (JSONL-style operations if present)
- src/wrapper.h (file I/O wrappers)
- src/history.h (history data structures)
- src/history.c (existing history operations)

### Pre-read Tests (patterns)
- tests/unit/config/load_test.c (file I/O test patterns)

## Pre-conditions
- `make check` passes
- History data structures exist (`ik_history_t`, add/get operations)
- History directory creation function exists
- yyjson library is integrated

## Task
Implement JSONL file I/O for command history persistence in `$PWD/.ikigai/history`:

**Load function** (`ik_history_load`):
- Read `.ikigai/history` file (create if missing)
- Parse each line as JSON object: `{"cmd": "...", "ts": "..."}`
- Add `cmd` field to history structure (ignore `ts` for now)
- Skip malformed lines (log warning but continue)
- Respect capacity limit (if file has more entries, keep most recent)

**Save function** (`ik_history_save`):
- Write entire history to `.ikigai/history`
- Each entry as one line: `{"cmd": "command text", "ts": "ISO-8601-timestamp"}`
- Multi-line commands preserved with JSON `\n` escaping
- Atomic write pattern: write to temp file, rename
- Handle write failures gracefully

**Append function** (`ik_history_append_entry`):
- Append single entry to end of file (for incremental saves)
- Format: `{"cmd": "...", "ts": "..."}\n`
- Open file in append mode, write line, close

## TDD Cycle

### Red
1. Add declarations to `src/history.h`:
   ```c
   // Load history from .ikigai/history (creates file if missing)
   res_t ik_history_load(TALLOC_CTX *ctx, ik_history_t *hist);

   // Save entire history to .ikigai/history (atomic write)
   res_t ik_history_save(const ik_history_t *hist);

   // Append single entry to .ikigai/history
   res_t ik_history_append_entry(const char *entry);
   ```

2. Create `tests/unit/history/file_io_test.c`:
   ```c
   START_TEST(test_history_load_empty_file)
   {
       // Create empty .ikigai/history
       // Load into history structure
       // Verify count == 0
   }
   END_TEST

   START_TEST(test_history_load_valid_entries)
   {
       // Write JSONL: {"cmd": "/clear", "ts": "..."}
       //              {"cmd": "hello\nworld", "ts": "..."}
       // Load and verify both entries present
       // Verify multi-line command preserved
   }
   END_TEST

   START_TEST(test_history_load_respects_capacity)
   {
       // Write 15 entries to file
       // Load into history with capacity=10
       // Verify only last 10 entries loaded
   }
   END_TEST

   START_TEST(test_history_load_malformed_line)
   {
       // Write valid entry, malformed JSON, valid entry
       // Verify malformed line skipped, others loaded
   }
   END_TEST

   START_TEST(test_history_save_atomic_write)
   {
       // Add entries to history
       // Save to file
       // Verify file contents match JSONL format
       // Verify temp file cleaned up
   }
   END_TEST

   START_TEST(test_history_append_entry)
   {
       // Create file with 2 entries
       // Append new entry
       // Verify file now has 3 entries in JSONL format
   }
   END_TEST

   START_TEST(test_history_load_file_missing)
   {
       // No .ikigai/history file exists
       // Load should succeed with empty history
   }
   END_TEST
   ```

3. Run `make check` - expect test failures

### Green
1. Implement `ik_history_load()` in `src/history.c`:
   - Call `ik_history_ensure_directory()` first
   - Use `yyjson_read_file()` to read file
   - If file doesn't exist, return OK (empty history)
   - Parse line-by-line (split on newlines)
   - Extract "cmd" field from each JSON object
   - Add to history with `ik_history_add()`
   - If history exceeds capacity, only keep most recent entries

2. Implement `ik_history_save()`:
   - Build JSONL content in memory buffer
   - For each entry, create JSON object with cmd and timestamp
   - Write to `.ikigai/history.tmp`
   - Use `posix_rename_()` for atomic replacement
   - Clean up temp file on error

3. Implement `ik_history_append_entry()`:
   - Open `.ikigai/history` in append mode
   - Format entry as `{"cmd": "...", "ts": "..."}\n`
   - Write and close
   - Handle file creation if doesn't exist

4. For timestamps, use current time in ISO 8601 format (use strftime)

5. Run `make check` - expect pass

### Refactor
1. Consider extracting JSON formatting to helper (inline if simple)
2. Ensure all file handles are closed properly (even on error)
3. Verify atomic write pattern is correct (write temp, rename)
4. Run `make check` and `make lint` - verify still green

## Post-conditions
- `make check` passes
- History loads from `.ikigai/history` on startup
- History saves to file (atomic write)
- Append function works for incremental saves
- Malformed lines are skipped gracefully
- Capacity limits are respected
- Multi-line commands are preserved via JSON escaping
- Missing file is handled (creates empty history)
- 100% test coverage maintained
