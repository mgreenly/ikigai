# Renaming: workspace → input_buffer

## Current Terminology

The codebase currently uses "workspace" (`ik_workspace_t`) to refer to the editable text area where users type REPL commands.

## The Problem

"Workspace" is ambiguous and doesn't clearly convey what this component does:

- **Too generic**: Could mean the entire REPL environment, a project workspace, or a working directory
- **Not descriptive**: Doesn't indicate this is the *input* side of the REPL
- **Unclear relationship**: The pairing of "scrollback" (output history) and "workspace" (???) doesn't make the data flow obvious

## What It Actually Is

The component is the **editable input buffer** where users type their current REPL command/expression before execution. It provides:

- Multi-line text editing
- Cursor navigation and manipulation
- UTF-8/grapheme cluster support
- Layout calculation for wrapping

It's the "input side" of the REPL, distinct from the scrollback (output/history side).

## Naming Alternatives Considered

1. **`editor`** - Clear for editing functionality, but too generic; doesn't convey REPL context
2. **`prompt`** - REPL-specific, but "prompt" typically means the `>>>` marker, not the editable area
3. **`command_buffer`** - Describes what it becomes, but it's still being edited (not yet a command)
4. **`edit_buffer`** - Generic, less REPL-specific
5. **`input_buffer`** ✓ - Clear, REPL-appropriate, pairs naturally with scrollback

## Chosen Solution: input_buffer

Rename `ik_workspace_t` to `ik_input_buffer_t` and update all related functions and variables.

### Benefits

1. **Clear component roles**:
   - `ik_scrollback_t` - historical output (read-only)
   - `ik_input_buffer_t` - current input being edited (read-write)
   - `ik_repl_t` - complete multiline REPL context

2. **Obvious data flow**: Input buffer → execution → results added to scrollback

3. **Standard terminology**: "Input buffer" is widely used in REPL/terminal implementations

4. **Self-documenting**: New developers immediately understand what `input_buffer` does

## Implementation Scope

Systematic rename across:
- Type definitions: `ik_workspace_t` → `ik_input_buffer_t`
- Functions: `ik_workspace_*` → `ik_input_buffer_*`
- Files: `workspace.{c,h}` → `input_buffer.{c,h}` (and related files)
- Variables: `workspace` → `input_buffer` or `input_buf`
- Tests: Update all test names and documentation

Note: This is a purely mechanical refactoring with no functional changes.

## Implementation Plan

### Phase 1: Preparation

1. **Verify clean build and test state**
   ```bash
   make clean
   make
   make test
   ```
   Ensure all tests pass before starting.

2. **Create a feature branch**
   ```bash
   git checkout -b refactor/workspace-to-input-buffer
   ```

3. **Inventory affected files**
   - Source files (8):
     - `src/workspace.c` → `src/input_buffer.c`
     - `src/workspace.h` → `src/input_buffer.h`
     - `src/workspace_cursor.c` → `src/input_buffer_cursor.c`
     - `src/workspace_cursor.h` → `src/input_buffer_cursor.h`
     - `src/workspace_cursor_pp.c` → `src/input_buffer_cursor_pp.c`
     - `src/workspace_layout.c` → `src/input_buffer_layout.c`
     - `src/workspace_multiline.c` → `src/input_buffer_multiline.c`
     - `src/workspace_pp.c` → `src/input_buffer_pp.c`

   - Test files (21):
     - `tests/unit/workspace/` → `tests/unit/input_buffer/` (15 test files)
     - `tests/unit/workspace_cursor/` → `tests/unit/input_buffer_cursor/` (4 test files)
     - `tests/unit/render/workspace_test.c` → `tests/unit/render/input_buffer_test.c`
     - `tests/unit/repl/*` (multiple files reference workspace)

   - Files with references to update:
     - `src/repl.c`, `src/repl.h`
     - `src/render.c`, `src/render.h`
     - `src/pp_helpers.h`
     - All test files
     - Build files (`Makefile`, etc.)

### Phase 2: Renaming Strategy

Use a **systematic bottom-up approach** to minimize build breakage:

1. Start with leaf components (cursor)
2. Move to core workspace files
3. Update consumers (repl, render)
4. Rename test files last

### Phase 3: Step-by-Step Execution

#### Step 1: Rename cursor component files

```bash
# Rename files
git mv src/workspace_cursor.h src/input_buffer_cursor.h
git mv src/workspace_cursor.c src/input_buffer_cursor.c
git mv src/workspace_cursor_pp.c src/input_buffer_cursor_pp.c

# Update content in these files:
# - ik_cursor_t stays the same (it's already generic)
# - Update header guards: IKIGAI_WORKSPACE_CURSOR_H → IKIGAI_INPUT_BUFFER_CURSOR_H
# - Update #include "workspace_cursor.h" → #include "input_buffer_cursor.h"
# - Update function comments mentioning "workspace"
```

#### Step 2: Rename core workspace files

```bash
# Rename files
git mv src/workspace.h src/input_buffer.h
git mv src/workspace.c src/input_buffer.c
git mv src/workspace_layout.c src/input_buffer_layout.c
git mv src/workspace_multiline.c src/input_buffer_multiline.c
git mv src/workspace_pp.c src/input_buffer_pp.c
```

In each file, perform systematic text replacements:
- `ik_workspace_t` → `ik_input_buffer_t`
- `ik_workspace_create` → `ik_input_buffer_create`
- `ik_workspace_` → `ik_input_buffer_`
- `IKIGAI_WORKSPACE_H` → `IKIGAI_INPUT_BUFFER_H`
- `#include "workspace` → `#include "input_buffer`
- Documentation/comments: "workspace" → "input buffer"

#### Step 3: Update consumers (repl, render)

Update files that use the workspace API:
- `src/repl.h`: `ik_workspace_t *workspace` → `ik_input_buffer_t *input_buffer`
- `src/repl.c`: Update all workspace references
- `src/render.h`: Update function parameter names
- `src/render.c`: Update `ik_render_workspace()` → `ik_render_input_buffer()`
  - Update parameter `workspace_text` → `input_text`
  - Update parameter `workspace_cursor_offset` → `input_cursor_offset`

#### Step 4: Update test files and directories

```bash
# Rename test directories
git mv tests/unit/workspace tests/unit/input_buffer
git mv tests/unit/workspace_cursor tests/unit/input_buffer_cursor

# Rename individual test files
git mv tests/unit/render/workspace_test.c tests/unit/render/input_buffer_test.c

# Update test file contents:
# - Test function names: test_workspace_* → test_input_buffer_*
# - Variable names: workspace → input_buffer or input_buf
# - All ik_workspace_* calls → ik_input_buffer_*
# - Comments and test descriptions
```

#### Step 5: Update build system

Update `Makefile` or build configuration:
- Object file names: `workspace.o` → `input_buffer.o`
- Source file paths
- Test target names

#### Step 6: Update documentation

- Update any README files
- Update design documents
- Update code comments with "workspace" terminology

### Phase 4: Verification

1. **Build verification**
   ```bash
   make clean
   make
   ```
   Ensure no compilation errors.

2. **Test verification**
   ```bash
   make test
   ```
   All tests should pass with identical results to pre-refactor state.

3. **Visual inspection**
   - Search for remaining "workspace" references:
     ```bash
     grep -r "workspace" src/ tests/ --exclude-dir=.git
     ```
   - Verify they are either intentional (e.g., in unrelated contexts) or updated

4. **Code review**
   - Review git diff to ensure no accidental functional changes
   - Verify only identifiers and comments changed

### Phase 5: Commit and Review

```bash
# Stage all changes
git add -A

# Commit with descriptive message
git commit -m "Refactor: Rename workspace → input_buffer for clarity

Systematic rename of workspace terminology to input_buffer throughout
the codebase. This clarifies that this component is the editable input
side of the REPL, distinct from scrollback (output history).

Changes:
- Renamed all workspace files to input_buffer
- Updated type: ik_workspace_t → ik_input_buffer_t
- Updated all function names: ik_workspace_* → ik_input_buffer_*
- Renamed test directories and files
- Updated render API: workspace_text → input_text

No functional changes. All tests pass."

# Run final verification
make clean && make test
```

### Rollback Plan

If issues arise during refactoring:

```bash
# Discard all changes and return to clean state
git reset --hard HEAD
git clean -fd

# Or, if partially committed, revert specific commits
git revert <commit-hash>
```

### Estimated Effort

- **Preparation**: 15 minutes
- **File renaming and content updates**: 2-3 hours
- **Build/test fixes**: 30-60 minutes
- **Verification**: 30 minutes
- **Total**: 3.5-5 hours

### Success Criteria

- ✓ All files renamed consistently
- ✓ Build succeeds with no warnings
- ✓ All tests pass
- ✓ No "workspace" references remain (except in this documentation)
- ✓ Git history shows clean, atomic rename commit
