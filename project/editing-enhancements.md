# Editing Enhancements

## Current State

ikigai has three file tools: `file_read`, `file_edit` (exact substring replace), and `file_write` (create/overwrite). The LLM falls back to `bash` for operations not covered by these.

`file_edit` uses exact string matching with uniqueness enforcement — the same approach Claude Code, Cursor, and Aider converge on. This is the right primitive. Patch/diff-based editing (unified diff format) is worse because LLMs generate incorrect diffs frequently.

## Recommended Improvements

### 1. Diff Preview in Terminal UI

When `file_edit` succeeds, show the human a colored before/after diff in the scrollback rather than just a success message. This lets the user see exactly what changed without running `jj diff` manually.

The tool already knows old_string and new_string — render them as a mini inline diff with surrounding context lines (3-5 lines above/below from the file).

### 2. Edit Confirmation Mode

Add an optional confirmation gate where the user can approve or reject a file edit before it's applied. This could be:

- A per-agent setting (e.g., `confirm_edits = true`)
- A tool parameter (e.g., `"confirm": true`)
- A global config option

When enabled, the diff preview is shown and the user presses y/n before the write happens. When disabled (default), edits apply immediately as they do now.

### 3. Explicit File Delete Tool

Currently the LLM must use `bash rm` to delete files. A dedicated `file_delete` tool would:

- Show the user exactly which file is being deleted
- Integrate with confirmation mode
- Return structured JSON (success/error) instead of bash exit codes
- Prevent accidental deletion of directories (file-only)

### 4. Explicit File Rename/Move Tool

Same rationale as delete — a `file_rename` tool that:

- Takes `old_path` and `new_path`
- Validates source exists and destination doesn't
- Returns structured JSON
- Integrates with confirmation mode

### 5. Stale Context Detection

`file_edit` already fails with NOT_FOUND when the target string doesn't exist, which naturally catches most stale context issues. An enhancement would be to track file mtimes:

- When `file_read` runs, record the mtime of the file
- When `file_edit` runs on the same file, compare current mtime to recorded mtime
- If the file changed between read and edit, warn (but still apply if the old_string matches)

This catches the case where the file was modified externally but the exact substring still happens to match — the edit would succeed silently even though the surrounding context may have shifted in meaning.

## Non-Recommendations

- **Patch/unified diff editing** — LLMs produce incorrect diffs too often. Exact string matching is more reliable.
- **Line-number-based editing** — Fragile; line numbers shift after every edit. Substring matching is position-independent.
- **Structured AST editing** — Over-engineered for the value. String replacement works across all languages without parsers.
