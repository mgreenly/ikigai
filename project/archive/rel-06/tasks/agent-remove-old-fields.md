# Task: Remove Migrated Fields from ik_agent_ctx_t

## Target

Refactoring #1: Decompose ik_agent_ctx_t God Object - Phase 4 (Cleanup)

## Model

sonnet/thinking (verify no remaining access)

## Pre-read

### Skills

- default
- tdd
- style
- errors
- naming
- scm

### Docs

- project/memory.md (talloc ownership)
- rel-06/refactor.md (Section 1: Decompose ik_agent_ctx_t - Target Structure)

### Source Files

- src/agent.h (current ik_agent_ctx_t with all fields)
- src/agent_identity.h (identity sub-context)
- src/agent_display.h (display sub-context)
- src/agent_llm.h (LLM sub-context with state enum)
- src/agent_tool_executor.h (tool executor sub-context)

### Test Patterns

- tests/unit/agent/*.c (all agent tests)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make helgrind` passes
- Identity field migration complete (from agent-migrate-identity-access.md)
- Display field migration complete (from agent-migrate-display-access.md)
- LLM field migration complete (from agent-migrate-llm-access.md)
- Tool executor field migration complete (from agent-migrate-tool-access.md)
- NO remaining direct access to legacy fields anywhere in codebase

## Task

Remove all migrated fields from `ik_agent_ctx_t`, leaving only sub-context pointers and remaining state.

### What

Remove these fields from `ik_agent_ctx_t`:

**Identity fields (5):**
- `char *uuid;`
- `char *name;`
- `char *parent_uuid;`
- `int64_t created_at;`
- `int64_t fork_message_id;`

**Display fields (11):**
- `ik_scrollback_t *scrollback;`
- `ik_layer_cake_t *layer_cake;`
- `ik_layer_t *scrollback_layer;`
- `ik_layer_t *spinner_layer;`
- `ik_layer_t *separator_layer;`
- `ik_layer_t *input_layer;`
- `ik_layer_t *completion_layer;`
- `size_t viewport_offset;`
- `ik_spinner_state_t spinner_state;`
- `bool separator_visible;`
- `bool input_buffer_visible;`
- `const char *input_text;`
- `size_t input_text_len;`

**LLM fields (9):**
- `struct ik_openai_multi *multi;`
- `int curl_still_running;`
- `ik_agent_state_t state;`
- `char *assistant_response;`
- `char *streaming_line_buffer;`
- `char *http_error_message;`
- `char *response_model;`
- `char *response_finish_reason;`
- `int32_t response_completion_tokens;`

**Tool execution fields (8):**
- `ik_tool_call_t *pending_tool_call;`
- `pthread_t tool_thread;`
- `pthread_mutex_t tool_thread_mutex;`
- `bool tool_thread_running;`
- `bool tool_thread_complete;`
- `TALLOC_CTX *tool_thread_ctx;`
- `char *tool_thread_result;`
- `int32_t tool_iteration_count;`

Also remove:
- `ik_agent_state_t` enum from agent.h (now in agent_llm.h)

### How

**Step 1: Verify no remaining access**

Run comprehensive grep to ensure no legacy field access remains:

```bash
# Identity fields
grep -rn "agent->uuid[^_]" src/ tests/ --include="*.c" --include="*.h"
grep -rn "agent->name[^_]" src/ tests/ --include="*.c" --include="*.h"
grep -rn "agent->parent_uuid" src/ tests/ --include="*.c" --include="*.h"
grep -rn "agent->created_at" src/ tests/ --include="*.c" --include="*.h"
grep -rn "agent->fork_message_id" src/ tests/ --include="*.c" --include="*.h"

# Display fields
grep -rn "agent->scrollback[^_]" src/ tests/ --include="*.c" --include="*.h"
grep -rn "agent->layer_cake" src/ tests/ --include="*.c" --include="*.h"
grep -rn "agent->scrollback_layer\|agent->spinner_layer\|agent->separator_layer\|agent->input_layer\|agent->completion_layer" src/ tests/ --include="*.c" --include="*.h"
grep -rn "agent->viewport_offset\|agent->spinner_state\|agent->separator_visible\|agent->input_buffer_visible" src/ tests/ --include="*.c" --include="*.h"

# LLM fields
grep -rn "agent->multi[^_]" src/ tests/ --include="*.c" --include="*.h"
grep -rn "agent->curl_still_running" src/ tests/ --include="*.c" --include="*.h"
grep -rn "agent->state[^_]" src/ tests/ --include="*.c" --include="*.h" | grep -v "spinner"
grep -rn "agent->assistant_response\|agent->streaming_line_buffer\|agent->http_error_message" src/ tests/ --include="*.c" --include="*.h"

# Tool execution fields
grep -rn "agent->pending_tool_call\|agent->tool_thread" src/ tests/ --include="*.c" --include="*.h"
```

If ANY matches found (outside comments/strings), DO NOT proceed - fix remaining access first.

**Step 2: Update agent.h**

Replace the massive struct with the slim composition:

```c
// src/agent.h - AFTER cleanup
#pragma once

#include "agent_display.h"
#include "agent_identity.h"
#include "agent_llm.h"
#include "agent_tool_executor.h"
#include "error.h"

#include <talloc.h>
#include <stdbool.h>

// Forward declarations
typedef struct ik_shared_ctx ik_shared_ctx_t;
typedef struct ik_input_buffer_t ik_input_buffer_t;
typedef struct ik_openai_conversation ik_openai_conversation_t;
struct ik_repl_ctx_t;

// Mark structure for conversation checkpoints
typedef struct {
    size_t message_index;
    char *label;
    char *timestamp;
} ik_mark_t;

/**
 * Per-agent context for ikigai.
 *
 * Contains all state specific to one agent, organized into
 * focused sub-contexts for different concerns.
 *
 * Ownership: Created as child of ik_repl_ctx_t.
 */
typedef struct ik_agent_ctx {
    // Reference to shared infrastructure
    ik_shared_ctx_t *shared;

    // Backpointer to REPL context
    struct ik_repl_ctx_t *repl;

    // Focused sub-contexts (owned)
    ik_agent_identity_t *identity;
    ik_agent_display_t *display;
    ik_agent_llm_t *llm;
    ik_agent_tool_executor_t *tool_executor;

    // Remaining state (not extracted to sub-contexts)
    ik_input_buffer_t *input_buffer;
    ik_completion_t *completion;
    ik_openai_conversation_t *conversation;
    ik_mark_t **marks;
    size_t mark_count;
} ik_agent_ctx_t;

// ... function declarations unchanged ...
```

**Step 3: Update agent.c**

Remove field initialization for removed fields in `ik_agent_create()` and `ik_agent_restore()`:

```c
// Before
agent->uuid = ik_generate_uuid(agent);
agent->identity->uuid = ...;

// After
agent->identity->uuid = ik_generate_uuid(agent->identity);
// No legacy field assignment
```

**Step 4: Clean up includes**

Some files may have included pthread.h only for the mutex type. Review and remove unnecessary includes.

### Why

This is the final cleanup step that achieves the refactoring goal:

1. **From 35+ fields to 9:** Massive reduction in complexity
2. **Single responsibility:** Each sub-context has one purpose
3. **Clear ownership:** Sub-contexts own their data
4. **Improved testability:** Can mock sub-contexts independently

### Target Structure Verification

After cleanup, verify struct matches target from refactor.md:

```c
typedef struct ik_agent_ctx {
    ik_shared_ctx_t *shared;
    struct ik_repl_ctx_t *repl;

    // Focused sub-contexts (owned)
    ik_agent_identity_t *identity;
    ik_agent_display_t *display;
    ik_agent_llm_t *llm;
    ik_agent_tool_executor_t *tool_executor;

    // Remaining state
    ik_input_buffer_t *input_buffer;
    ik_completion_t *completion;
    ik_openai_conversation_t *conversation;
    ik_mark_t **marks;
    size_t mark_count;
} ik_agent_ctx_t;
```

## TDD Cycle

### Red

No new tests - existing tests should continue to pass. Removal is validated by:
1. Compilation succeeds (no missing fields)
2. All tests pass
3. Helgrind passes (no thread issues)

### Green

1. Run verification grep commands (Step 1)
2. If any legacy access found, STOP and report - migration incomplete
3. Update `src/agent.h` to remove fields
4. Update `src/agent.c` to remove field initialization
5. Remove state enum from agent.h (keep in agent_llm.h)
6. Run `make check`
7. Run `make helgrind`

### Verify

- `make check` passes
- `make lint` passes
- `make helgrind` passes
- Struct has 9 fields (vs. original 35+)
- No compilation warnings about unused variables
- Full application still works (manual verification)

## Post-conditions

- `ik_agent_ctx_t` has only 9 fields
- All legacy fields removed
- State enum only in agent_llm.h
- Sub-contexts own all previously duplicated data
- All existing tests pass
- `make check` passes
- `make lint` passes
- `make helgrind` passes
- Working tree is clean (all changes committed)

## Rollback

If issues arise after field removal:
1. `git checkout -- src/agent.h src/agent.c`
2. Identify which legacy access was missed
3. Complete migration for that access
4. Try removal again

**Note:** This rollback returns to the "transition state" where both legacy and new fields exist.

## Sub-agent Usage

- Use haiku sub-agents for running verification grep commands
- Use haiku sub-agents for `make check`, `make lint`, `make helgrind`
- Use haiku sub-agents for git commits

## Final Verification Checklist

Before committing, verify:

- [ ] `grep -rn "agent->uuid[^_]" src/` returns nothing
- [ ] `grep -rn "agent->scrollback[^_]" src/` returns nothing
- [ ] `grep -rn "agent->state[^_]" src/ | grep -v spinner` returns nothing
- [ ] `grep -rn "agent->tool_thread_mutex" src/` returns nothing
- [ ] `make check` passes
- [ ] `make lint` passes
- [ ] `make helgrind` passes
- [ ] Struct field count is 9 (shared, repl, identity, display, llm, tool_executor, input_buffer, completion, conversation, marks, mark_count)
- [ ] Manual test: application starts, LLM works, tools work
