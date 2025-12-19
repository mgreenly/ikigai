# Task: Update session_restore.c

## Target
Gap 0: Message Type Unification (PREREQUISITE for PR1)

## Macro Context

This is step 6 of the message type unification. Now that `ik_replay_context_t.messages` uses `ik_msg_t**`, we can simplify `session_restore.c` by removing the `ik_msg_from_db_()` conversion calls.

**Before:** session_restore.c called `ik_msg_from_db_()` to convert `ik_message_t` to `ik_msg_t`
**After:** session_restore.c uses `ik_msg_t` directly from replay_ctx (no conversion needed)

**What changes:**
- Remove `ik_msg_from_db_()` calls
- Use `replay_ctx->messages[i]` directly (already `ik_msg_t*`)
- Filter non-conversation kinds inline (clear, mark, rewind)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- rel-06/gap.md (Gap 0 section)

## Pre-read Source
- src/msg.h (READ - ik_msg_t definition)
- src/db/replay.h (READ - updated ik_replay_context_t)
- src/repl/session_restore.c (READ - current implementation with conversion)

## Source Files to MODIFY
- src/repl/session_restore.c (MODIFY - remove conversion, use ik_msg_t directly)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- Code compiles (`make` succeeds)
- Prior tasks completed:
  - gap0-migrate-replay.md (ik_replay_context_t uses ik_msg_t**)
  - gap0-migrate-agent-replay.md
  - gap0-migrate-message.md

## Task

### Part 1: Update scrollback population (lines 92-107)

```c
// BEFORE (lines 93-106)
for (size_t i = 0; i < replay_ctx->count; i++) {
    ik_message_t *msg = replay_ctx->messages[i];

    // Use universal event renderer for consistent display
    res_t render_res = ik_event_render(
        repl->current->scrollback,
        msg->kind,
        msg->content,
        msg->data_json
    );
    ...
}

// AFTER - just change ik_message_t to ik_msg_t
for (size_t i = 0; i < replay_ctx->count; i++) {
    ik_msg_t *msg = replay_ctx->messages[i];

    // Use universal event renderer for consistent display
    res_t render_res = ik_event_render(
        repl->current->scrollback,
        msg->kind,
        msg->content,
        msg->data_json
    );
    ...
}
```

### Part 2: Update conversation rebuild (lines 109-130)

This is the key change - remove the conversion:

```c
// BEFORE (lines 109-130)
for (size_t i = 0; i < replay_ctx->count; i++) {
    ik_message_t *db_msg = replay_ctx->messages[i];

    // Convert DB format to canonical format
    res_t msg_res = ik_msg_from_db_(tmp, db_msg);
    if (is_err(&msg_res)) {
        talloc_steal(repl, msg_res.err);
        talloc_free(tmp);
        return msg_res;
    }

    // Add to conversation if not skipped (NULL means skip)
    ik_msg_t *msg = msg_res.ok;
    if (msg != NULL) {
        res_t add_res = ik_openai_conversation_add_msg_(repl->current->conversation, msg);
        ...
    }
}

// AFTER - use ik_msg_t directly, filter non-conversation kinds inline
for (size_t i = 0; i < replay_ctx->count; i++) {
    ik_msg_t *msg = replay_ctx->messages[i];

    // Skip non-conversation kinds (these don't go to LLM)
    if (strcmp(msg->kind, "clear") == 0 ||
        strcmp(msg->kind, "mark") == 0 ||
        strcmp(msg->kind, "rewind") == 0) {
        continue;
    }

    // Add to conversation
    res_t add_res = ik_openai_conversation_add_msg_(repl->current->conversation, msg);
    if (is_err(&add_res)) {
        talloc_free(tmp);
        return add_res;
    }
}
```

### Part 3: Remove unused include

The `#include "../msg.h"` should already be present. Verify it includes msg.h and not just for ik_msg_from_db.

## TDD Cycle

### Red
1. Run `make` to confirm current compilation state

### Green
1. Update line 94: `ik_message_t *msg` -> `ik_msg_t *msg`
2. Update line 111: `ik_message_t *db_msg` -> `ik_msg_t *msg`
3. Remove lines 113-119 (the ik_msg_from_db_ conversion)
4. Add inline filtering for clear/mark/rewind kinds
5. Remove the `ik_msg_t *msg = msg_res.ok;` assignment (msg already defined)
6. Run `make` - should compile
7. Run `make lint` - should pass

### Refactor
No refactoring needed.

## Sub-agent Usage
- Use haiku sub-agents to run `make` and `make lint`

## Post-conditions
- Code compiles (`make` succeeds)
- `make lint` passes
- session_restore.c no longer calls ik_msg_from_db_()
- session_restore.c uses ik_msg_t directly
- Working tree is clean (all changes committed)
