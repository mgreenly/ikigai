# Task: Fix Duplicate Error Messages

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided below.

**Model:** sonnet/thinking
**Depends on:** None

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Pre-Read

**Skills:**
- `/load errors` - Result type patterns (OK/ERR, is_err, talloc_free)
- `/load style` - Code style conventions

**Source:**
- `src/repl_actions_llm.c` - File to modify (lines 65-83)
- `src/commands.c` - Reference only (shows dispatcher already displays errors)

**Plan:**
- `cdd/plan/duplicate-errors.md` - Full analysis

## Libraries

Use only existing project libraries. Do not introduce new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)

## Objective

Remove duplicate error display from `handle_slash_cmd_()` in `src/repl_actions_llm.c`.

**Problem:** Error messages display twice because:
1. `ik_cmd_dispatch()` adds error to scrollback AND returns ERR
2. `handle_slash_cmd_()` sees ERR and adds error to scrollback again

**Solution:** Remove the caller's error display code. The dispatcher already handles it.

## Changes Required

### src/repl_actions_llm.c

Find this code block (approximately lines 70-82):

```c
    } else {
        res_t result = ik_cmd_dispatch(repl, repl, command_text);
        if (is_err(&result)) {
            const char *err_msg = error_message(result.err);
            char *display_msg = talloc_asprintf(repl, "Error: %s", err_msg);
            if (display_msg != NULL) {
                ik_scrollback_append_line(repl->current->scrollback,
                                          display_msg, strlen(display_msg));
                talloc_free(display_msg);
            }
            talloc_free(result.err);
        }
    }
```

Replace with:

```c
    } else {
        res_t result = ik_cmd_dispatch(repl, repl, command_text);
        if (is_err(&result)) {
            talloc_free(result.err);
        }
    }
```

**Key points:**
- Keep the `talloc_free(result.err)` to prevent memory leak
- Remove all the scrollback_append code (lines that format and display the error)
- The dispatcher in `commands.c` already displays the error before returning ERR

## Test Scenarios

Existing tests in `tests/unit/commands/dispatch_test.c` call `ik_cmd_dispatch()` directly. No new tests needed since we're removing dead code.

Run `make check` to verify no regressions.

## Completion

After completing work (whether success, partial, or failed), commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(duplicate-errors.md): success - removed duplicate error display

Removed caller-side error display from handle_slash_cmd_() since
ik_cmd_dispatch() already displays errors before returning ERR.
EOF
)"
```

Report status to orchestration:
- Success: `/task-done duplicate-errors.md`
- Partial/Failed: `/task-fail duplicate-errors.md`

## Postconditions

- [ ] Compiles without warnings
- [ ] All tests pass
- [ ] `make check` passes
- [ ] All changes committed using commit message template
- [ ] Working copy is clean (no uncommitted changes)
