# Task: Decouple ik_mark_create from REPL context

## Target
Refactoring #4: Standardize Initialization Patterns (DI Violations - Phase 3)

## Pre-read

### Skills
- scm
- di
- errors
- naming
- style
- tdd

### Source patterns
- src/marks.h
- src/marks.c
- src/repl.h
- src/agent.h

### Test patterns
- tests/unit/commands/mark_*.c
- tests/**/test_mark*.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `ik_mark_create` takes `ik_repl_ctx_t *repl` as first parameter (current state)
- Phase 2 tasks (naming standardization) have been completed

## What
Change `ik_mark_create` signature to follow DI principles: take explicit dependencies rather than reaching into REPL internals.

Current signature (DI violation):
```c
res_t ik_mark_create(ik_repl_ctx_t *repl, const char *label);
```

The function reaches into `repl->current->conversation`, `repl->current->marks`, `repl->current->scrollback`, which couples marks tightly to REPL structure.

Proposed signature (explicit dependencies):
```c
res_t ik_mark_create(TALLOC_CTX *ctx, ik_openai_conversation_t *conv, const char *label, ik_mark_t **out);
```

However, this is a COMPLEX refactoring because:
1. Marks are stored in `repl->current->marks` array (agent context)
2. The function renders to scrollback (`repl->current->scrollback`)
3. Tests create entire REPL contexts just to test marks

## How

This task requires careful analysis and may need to be split into subtasks.

### Step 1: Analyze mark dependencies
The current `ik_mark_create` function:
1. Creates a mark struct allocated on `repl`
2. Records `repl->current->conversation->message_count` as `mark->message_index`
3. Adds mark to `repl->current->marks` array
4. Renders mark event to `repl->current->scrollback`

### Step 2: Design decoupled interface
Option A - Minimal change (recommended for now):
Keep the signature but document the coupling as technical debt.

Option B - Full decoupling:
```c
// Create mark object (pure allocation, no side effects)
ik_mark_t *ik_mark_create_obj(TALLOC_CTX *ctx, size_t message_index, const char *label);

// Caller handles:
// 1. Adding to marks array
// 2. Rendering to scrollback
```

Option C - Pass explicit dependencies:
```c
res_t ik_mark_create(TALLOC_CTX *ctx,
                     ik_openai_conversation_t *conv,
                     ik_mark_t ***marks_ptr,      // Pointer to marks array
                     size_t *mark_count_ptr,      // Pointer to mark count
                     ik_scrollback_t *scrollback, // For rendering
                     const char *label,
                     ik_mark_t **out);
```

### Step 3: Implementation decision
Given the complexity, this task should:
1. Document the DI violation with a `// TODO(DI):` comment
2. Defer full refactoring to a future release
3. OR proceed with Option B if the scope is acceptable

### Step 4: If proceeding with refactoring
1. Create `ik_mark_create_obj` that just creates the mark struct
2. Move array manipulation to callers
3. Move scrollback rendering to callers
4. Update `ik_mark_create` to be a convenience wrapper that calls the new function
5. Update tests

### Step 5: Verify
Run `make check` to ensure no compilation errors or test failures.

## Why
The current signature violates DI principles:
1. **Hidden dependencies**: Function signature doesn't reveal it needs conversation, marks array, and scrollback
2. **Tight coupling**: Marks module knows internal structure of REPL and agent contexts
3. **Testability**: Tests must create full REPL context just to test mark creation

Per di.md: "Pass dependencies as function parameters - Never load/create internally"

## TDD Cycle

### Red
If proceeding with refactoring:
1. Add test for new `ik_mark_create_obj` function
2. Test should fail (function doesn't exist)

### Green
Implement `ik_mark_create_obj` with minimal logic.

### Refactor
Migrate callers from old to new interface.

## Post-conditions
- `make check` passes with no errors
- Either:
  - DI violation is documented with TODO comment (minimal change), OR
  - New decoupled interface is implemented and tested
- Working tree is clean (changes committed)

## Complexity
High - this is an architectural change that affects multiple modules

## Notes
- This may be too complex for a single task
- Consider documenting as technical debt for a future release
- The marks module is used by the `/mark` and `/rewind` commands
- Similar DI issues exist in `ik_mark_find` and `ik_mark_rewind_to`
- Use sub-agents to find all callers before making changes:
  ```bash
  grep -rn "ik_mark_create\|ik_mark_find\|ik_mark_rewind" src/ tests/
  ```
