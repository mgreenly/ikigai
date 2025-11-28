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

Rename `ik_workspace_t` to `ik_input_buffer_t` and reorganize files into `src/input_buffer/` subdirectory with shorter, focused filenames.

### Benefits

1. **Clear component roles**:
   - `ik_scrollback_t` - historical output (read-only)
   - `ik_input_buffer_t` - current input being edited (read-write)
   - `ik_repl_t` - complete multiline REPL context

2. **Obvious data flow**: Input buffer → execution → results added to scrollback

3. **Standard terminology**: "Input buffer" is widely used in REPL/terminal implementations

4. **Self-documenting**: New developers immediately understand what `input_buffer` does

5. **Better organization**: Subdirectory groups related files, shorter names within namespace

## Symbol Naming Strategy

### Decision: Full Module Prefix (Option 2)

All public symbols use the full `ik_input_buffer_*` prefix, including cursor operations:

```c
// Types
ik_input_buffer_t
ik_input_buffer_cursor_t

// Core functions
ik_input_buffer_create()
ik_input_buffer_insert_codepoint()
ik_input_buffer_clear()

// Cursor functions (note: full prefix, not ik_cursor_*)
ik_input_buffer_cursor_create()
ik_input_buffer_cursor_move_left()
ik_input_buffer_cursor_move_right()

// Layout functions
ik_input_buffer_ensure_layout()
ik_input_buffer_get_physical_lines()
```

**Rationale:**

1. **One subdirectory = One module**: The `src/input_buffer/` directory represents a single cohesive module
2. **Clear ownership in global namespace**: All `ik_input_buffer_*` symbols obviously belong to this module
3. **Prevents conflicts**: No ambiguity about which module owns which symbol
4. **Consistency**: Follows the principle documented in `docs/naming.md`
5. **Cursor is not generic**: The cursor is tightly coupled to the input buffer's text representation

**Alternative considered and rejected:**

Keeping cursor symbols generic (`ik_cursor_*`) would be shorter but:
- Implies the cursor is reusable across modules (it's not)
- Breaks the "one subdirectory = one module" principle
- Creates ambiguity about module ownership
- Inconsistent with C library conventions (e.g., `gtk_window_*`, `SDL_Window*`)

This follows the general naming principle: **subdirectories provide code organization, symbol prefixes provide namespace in C's flat symbol table**.

## Implementation Scope

Systematic rename and reorganization:
- Type definitions: `ik_workspace_t` → `ik_input_buffer_t`
- Functions: `ik_workspace_*` → `ik_input_buffer_*`
- Files: Move to `src/input_buffer/` subdirectory with shorter names
- Variables: `workspace` → `input_buffer` or `input_buf`
- Tests: Update directory structure and all test names

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

   **Source files** - Move to `src/input_buffer/` subdirectory with shorter names:
   ```
   src/workspace.c              → src/input_buffer/core.c
   src/workspace.h              → src/input_buffer/core.h
   src/workspace_cursor.c       → src/input_buffer/cursor.c
   src/workspace_cursor.h       → src/input_buffer/cursor.h
   src/workspace_cursor_pp.c    → src/input_buffer/cursor_pp.c
   src/workspace_layout.c       → src/input_buffer/layout.c
   src/workspace_multiline.c    → src/input_buffer/multiline.c
   src/workspace_pp.c           → src/input_buffer/pp.c
   ```

   **Test files** - Reorganize to match new structure:
   ```
   tests/unit/workspace/               → tests/unit/input_buffer/
   tests/unit/workspace_cursor/        → tests/unit/input_buffer/cursor/
   tests/unit/render/workspace_test.c  → tests/unit/render/input_buffer_test.c
   ```

   **Files with references to update**:
   - `src/repl.c`, `src/repl.h`
   - `src/render.c`, `src/render.h`
   - `src/pp_helpers.h`
   - All test files
   - Build files (`Makefile`, etc.)

### Phase 2: Renaming Strategy

Use a **systematic approach**:

1. Create new directory structure
2. Move and rename files (use `git mv`)
3. Update file contents (includes, guards, function names)
4. Update consumers (repl, render)
5. Update tests
6. Update build system

### Phase 3: Step-by-Step Execution

#### Step 1: Create directory and move files

```bash
# Create directory
mkdir -p src/input_buffer

# Move and rename files
git mv src/workspace_cursor.h src/input_buffer/cursor.h
git mv src/workspace_cursor.c src/input_buffer/cursor.c
git mv src/workspace_cursor_pp.c src/input_buffer/cursor_pp.c
git mv src/workspace.h src/input_buffer/core.h
git mv src/workspace.c src/input_buffer/core.c
git mv src/workspace_layout.c src/input_buffer/layout.c
git mv src/workspace_multiline.c src/input_buffer/multiline.c
git mv src/workspace_pp.c src/input_buffer/pp.c
```

#### Step 2: Update cursor files

**`src/input_buffer/cursor.h`**:
- Update header guard: `IKIGAI_WORKSPACE_CURSOR_H` → `IKIGAI_INPUT_BUFFER_CURSOR_H`
- Rename type: `ik_cursor_t` → `ik_input_buffer_cursor_t`
- Rename all functions: `ik_cursor_*` → `ik_input_buffer_cursor_*`
  - `ik_cursor_create()` → `ik_input_buffer_cursor_create()`
  - `ik_cursor_move_left()` → `ik_input_buffer_cursor_move_left()`
  - `ik_cursor_move_right()` → `ik_input_buffer_cursor_move_right()`
  - etc.
- Update documentation comments

**`src/input_buffer/cursor.c`**:
- Update `#include "workspace_cursor.h"` → `#include "cursor.h"`
- Update all function implementations to use new names
- Update type references: `ik_cursor_t` → `ik_input_buffer_cursor_t`
- Update documentation comments

**`src/input_buffer/cursor_pp.c`**:
- Update includes
- Rename: `ik_pp_cursor()` → `ik_pp_input_buffer_cursor()`
- Update type references
- Update documentation

#### Step 3: Update core input_buffer files

**`src/input_buffer/core.h`**:
- Update header guard: `IKIGAI_WORKSPACE_H` → `IKIGAI_INPUT_BUFFER_CORE_H`
- Update `#include "workspace_cursor.h"` → `#include "cursor.h"`
- Rename type: `ik_workspace_t` → `ik_input_buffer_t`
- Update struct field: `ik_cursor_t *cursor` → `ik_input_buffer_cursor_t *cursor`
- Rename all functions: `ik_workspace_*` → `ik_input_buffer_*`
- Update all documentation: "workspace" → "input buffer"

**`src/input_buffer/core.c`**:
- Update `#include "workspace.h"` → `#include "core.h"`
- Update `#include "workspace_cursor.h"` → `#include "cursor.h"`
- Rename all function implementations: `ik_workspace_*` → `ik_input_buffer_*`
- Update cursor API calls: `ik_cursor_*` → `ik_input_buffer_cursor_*`
- Update type references
- Update all comments

**`src/input_buffer/layout.c`**:
- Update includes to use shorter names
- Rename functions: `ik_workspace_*` → `ik_input_buffer_*`
- Update type references: `ik_workspace_t` → `ik_input_buffer_t`

**`src/input_buffer/multiline.c`**:
- Update includes
- Rename functions: `ik_workspace_*` → `ik_input_buffer_*`
- Update cursor API calls: `ik_cursor_*` → `ik_input_buffer_cursor_*`
- Update type references

**`src/input_buffer/pp.c`**:
- Update includes
- Rename `ik_pp_workspace` → `ik_pp_input_buffer`
- Update function parameter type: `ik_workspace_t` → `ik_input_buffer_t`
- Update cursor pp call: `ik_pp_cursor()` → `ik_pp_input_buffer_cursor()`

#### Step 4: Update consumers (repl, render)

**`src/repl.h`**:
- Update `#include "workspace.h"` → `#include "input_buffer/core.h"`
- Update struct field: `ik_workspace_t *workspace` → `ik_input_buffer_t *input_buffer`

**`src/repl.c`**:
- Update include
- Update all variable references: `workspace` → `input_buffer`
- Update all function calls: `ik_workspace_*` → `ik_input_buffer_*`

**`src/render.h`**:
- Rename function: `ik_render_workspace()` → `ik_render_input_buffer()`
- Update parameters: `workspace_text` → `input_text`, `workspace_cursor_offset` → `input_cursor_offset`
- Update `ik_render_combined()` parameters similarly

**`src/render.c`**:
- Update function implementations to match header changes
- Update internal variable names and comments

**`src/pp_helpers.h`**:
- Update any forward declarations or includes

#### Step 5: Update test structure

```bash
# Rename test directories
git mv tests/unit/workspace tests/unit/input_buffer
git mv tests/unit/workspace_cursor tests/unit/input_buffer/cursor

# Rename render test file
git mv tests/unit/render/workspace_test.c tests/unit/render/input_buffer_test.c
```

**Update all test files**:
- Update `#include "workspace.h"` → `#include "input_buffer/core.h"`
- Update `#include "workspace_cursor.h"` → `#include "input_buffer/cursor.h"`
- Rename test functions: `test_workspace_*` → `test_input_buffer_*`
- Update variable names: `workspace` → `input_buffer` or `buf`
- Update type references:
  - `ik_workspace_t` → `ik_input_buffer_t`
  - `ik_cursor_t` → `ik_input_buffer_cursor_t`
- Update all API calls:
  - `ik_workspace_*` → `ik_input_buffer_*`
  - `ik_cursor_*` → `ik_input_buffer_cursor_*`
- Update test descriptions and comments

#### Step 6: Update build system

**Makefile** (or build configuration):
- Update source paths: `src/workspace*.c` → `src/input_buffer/*.c`
- Update object file paths
- Update include paths if needed (add `-Isrc` for subdirectory includes)
- Update test compilation rules

#### Step 7: Update documentation

- Search for "workspace" in comments and docs
- Update any README files
- Update design documents

### Phase 4: Verification

1. **Build verification**
   ```bash
   make clean
   make
   ```
   Should compile with no errors or warnings.

2. **Test verification**
   ```bash
   make test
   ```
   All tests should pass identically to pre-refactor.

3. **Visual inspection**
   ```bash
   # Check for remaining "workspace" references
   grep -r "workspace" src/ tests/ --exclude-dir=.git | grep -v "input_buffer"

   # Should only find intentional references (if any)
   ```

4. **Code review**
   ```bash
   git diff --stat
   git diff
   ```
   Verify only renaming/reorganization, no functional changes.

### Phase 5: Commit

```bash
# Stage all changes
git add -A

# Commit with descriptive message
git commit -m "Refactor: Rename workspace → input_buffer, reorganize into subdirectory

Systematic rename and reorganization for clarity:

Structure changes:
- Created src/input_buffer/ subdirectory
- Moved workspace*.{c,h} → input_buffer/{core,cursor,layout,multiline,pp}.{c,h}
- Reorganized tests to match new structure

API changes:
- Renamed type: ik_workspace_t → ik_input_buffer_t
- Renamed cursor type: ik_cursor_t → ik_input_buffer_cursor_t
- Renamed functions: ik_workspace_* → ik_input_buffer_*
- Renamed cursor functions: ik_cursor_* → ik_input_buffer_cursor_*
- Updated render API: ik_render_workspace → ik_render_input_buffer
- Updated includes to use subdirectory: \"input_buffer/core.h\"

Naming principle:
All symbols from src/input_buffer/ use ik_input_buffer_* prefix
(one subdirectory = one module). Documented in docs/naming.md.

Rationale:
Clarifies that this component is the editable input side of the REPL,
distinct from scrollback (output history). Subdirectory organization
provides namespace, allowing shorter, more focused filenames.

No functional changes. All tests pass."

# Verify build and tests
make clean && make test
```

### Rollback Plan

If issues arise:

```bash
# Discard all changes
git reset --hard HEAD
git clean -fd

# Or revert specific commit
git revert <commit-hash>
```

### Estimated Effort

- **Preparation**: 15 minutes
- **Directory creation and file moves**: 30 minutes
- **Content updates**: 2-3 hours
- **Build/test fixes**: 30-60 minutes
- **Verification**: 30 minutes
- **Total**: 4-5.5 hours

### Success Criteria

- ✓ New `src/input_buffer/` directory with 8 files
- ✓ All files renamed with shorter, focused names
- ✓ All symbols use consistent `ik_input_buffer_*` prefix (including cursor functions)
- ✓ Build succeeds with no warnings
- ✓ All tests pass
- ✓ No "workspace" or "ik_cursor_" references remain (except in this doc and git history)
- ✓ Git history shows clean rename commit
- ✓ Includes use subdirectory: `#include "input_buffer/core.h"`
