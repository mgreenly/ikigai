# Fix: Unify ik_openai_msg_t to ik_msg_t

## Agent
model: sonnet

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `naming.md` - Naming conventions

## Context

**IMPORTANT: This is a CONSOLIDATION task, not a simple rename.**

Two nearly-identical message types exist:

| Type | Location | Discriminator Field |
|------|----------|---------------------|
| `ik_msg_t` | `src/msg.h` | `kind` |
| `ik_openai_msg_t` | `src/openai/client.h` | `role` |

Both have identical `content` and `data_json` fields. The ONLY structural difference is `kind` vs `role`.

**Design intent:** `ik_msg_t` is the ONE canonical message type for:
- Database storage
- In-memory representation
- Rendering to scrollback
- All LLM providers (not just OpenAI)

The `ik_openai_msg_t` type should NOT exist. OpenAI module should use `ik_msg_t` internally and convert to wire format only during API serialization.

## High-Level Goal

**Replace all uses of `ik_openai_msg_t` with `ik_msg_t`.**

This fix handles the TYPE change only. A follow-up fix will change `->role` to `->kind`.

## Pre-conditions

- Build passes: `make BUILD=debug`
- Tests pass: `make check`

## Task

### Step 1: Find all occurrences

```bash
grep -r "ik_openai_msg_t" src/ tests/ --include="*.c" --include="*.h" | wc -l
```

Expect ~100+ occurrences across ~30 files.

### Step 2: Update src/openai/client.h

1. Add `#include "msg.h"` near the top includes
2. DELETE the `ik_openai_msg_t` typedef (lines ~33-37)
3. Keep the comment block but update it to reference `ik_msg_t`
4. Change all function signatures from `ik_openai_msg_t` to `ik_msg_t`

### Step 3: Update src/openai/client_msg.c

1. Add `#include "msg.h"`
2. Change all `ik_openai_msg_t` to `ik_msg_t`

### Step 4: Update remaining src/ files

For each file using `ik_openai_msg_t`:
1. Add `#include "msg.h"` if not present
2. Change `ik_openai_msg_t` to `ik_msg_t`

Files to update (use grep to find them):
- `src/openai/client.c`
- `src/openai/client_serialize.c`
- `src/marks.c`
- `src/repl_tool.c`
- `src/repl_actions.c`
- `src/repl_event_handlers.c`
- Any others found by grep

### Step 5: Update test files

For each test file using `ik_openai_msg_t`:
1. Add `#include "msg.h"` if not present
2. Change `ik_openai_msg_t` to `ik_msg_t`

### Step 6: Update ik_openai_conversation_t

In `src/openai/client.h`, the conversation struct uses `ik_openai_msg_t **messages`. Update to `ik_msg_t **messages`.

### Step 7: Verify

```bash
# Should return 0 results (only docs files allowed)
grep -r "ik_openai_msg_t" src/ tests/ --include="*.c" --include="*.h"

# Must compile
make clean && make BUILD=debug

# Tests will likely FAIL at this point - that's expected
# The ->role field doesn't exist on ik_msg_t (it has ->kind)
# The next fix handles that
```

## Post-conditions

- Zero occurrences of `ik_openai_msg_t` in src/ and tests/ (*.c, *.h files)
- Code compiles (may have warnings about ->role)
- Tests may fail (expected - next fix addresses ->role → ->kind)

## Notes

- This is a mechanical search-replace task
- Do NOT change `->role` to `->kind` in this fix (that's the next fix)
- Do NOT rename functions yet (that's a future fix)
- Focus ONLY on the type name change
- If the scope is too large, use sub-agents or the todo list to track progress
