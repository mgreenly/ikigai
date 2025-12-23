# Task: Update REPL Streaming Callbacks for Provider Abstraction

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided.

**Model:** sonnet/thinking
**Depends on:** repl-provider-routing.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

**Critical Architecture Constraint:** The application uses a select()-based event loop. Streaming callbacks are invoked during `perform()` calls within the event loop - NOT as return values from blocking functions. UI updates happen via callbacks as data arrives during the event loop, enabling incremental rendering.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load source-code` - Map of REPL implementation
- `/load database` - Message persistence patterns

**Source:**
- `src/repl_callbacks.c` - Streaming callbacks (current implementation)
- `src/repl_event_handlers.c` - Event processing and persistence
- `src/repl.c` - Main event loop with select() integration
- `src/providers/types.h` - Stream event types

**Plan:**
- `scratch/plan/streaming.md` - REPL consumption flow, async streaming architecture
- `scratch/plan/architecture.md` - Event loop integration pattern

## Objective

Update REPL streaming callbacks to handle normalized `ik_stream_event_t` types instead of OpenAI-specific events. Callbacks are invoked during `perform()` calls in the REPL event loop. UI updates (scrollback, spinner, etc.) happen incrementally as data arrives through these callbacks. This includes accumulating content deltas, handling thinking content, and persisting messages with provider metadata.

## Interface

Functions to update:

| Function | Signature | Changes |
|----------|-----------|---------|
| `ik_repl_stream_callback` | `res_t (const ik_stream_event_t *event, void *ctx)` | Handle normalized events; invoked during `perform()` |
| `ik_repl_save_message` | `res_t (ik_repl_t *repl, ...)` | Store provider/model/thinking metadata |

Files to update:

- `src/repl_callbacks.c` - Stream event handling (callbacks invoked during perform())
- `src/repl_event_handlers.c` - Message persistence

## Behaviors

### Async Streaming Architecture

**Critical:** Stream callbacks are invoked during `perform()` calls in the REPL event loop. The flow is:

```
1. REPL calls provider->vt->start_stream(ctx, req, stream_cb, ...)
   └── Returns immediately (non-blocking)

2. REPL event loop:
   ┌────────────────────────────────────────────────────────────────┐
   │ while (!done) {                                                │
   │     provider->vt->fdset(ctx, &fds, ...);  // Get provider FDs  │
   │     select(max+1, ...);                   // Wait for activity │
   │                                                                │
   │     provider->vt->perform(ctx, &running);                      │
   │     // ↑ This triggers curl write callbacks as data arrives    │
   │     // ↑ Write callbacks invoke SSE parser                     │
   │     // ↑ SSE parser invokes stream_cb with normalized events   │
   │     // ↑ stream_cb updates UI (scrollback, spinner)            │
   │                                                                │
   │     provider->vt->info_read(ctx, logger);                      │
   │     // ↑ When transfer completes, invokes completion_cb        │
   │ }                                                              │
   └────────────────────────────────────────────────────────────────┘
```

**Key insight:** UI updates happen during `perform()` as data arrives, not as return values. Control returns to the event loop between network activity, enabling incremental rendering.

### Stream Callback Handling

Handle normalized `ik_stream_event_t` types (using the event types from `scratch/plan/streaming.md`):

| Event Type | Handler Action |
|------------|----------------|
| `IK_STREAM_START` | Initialize streaming response object |
| `IK_STREAM_TEXT_DELTA` | Append text to scrollback, trigger UI render |
| `IK_STREAM_THINKING_DELTA` | Append to thinking area (if visible), trigger UI render |
| `IK_STREAM_TOOL_CALL_START` | Initialize tool call accumulator with id and name |
| `IK_STREAM_TOOL_CALL_DELTA` | Append JSON fragments to arguments buffer |
| `IK_STREAM_TOOL_CALL_DONE` | Finalize accumulated JSON, parse, and execute tool |
| `IK_STREAM_DONE` | Save complete response to database, update token counts |
| `IK_STREAM_ERROR` | Display error message in scrollback |

**Text Delta Processing (invoked during perform()):**
```c
case IK_STREAM_TEXT_DELTA:
    // Append delta text to accumulated content
    ik_scrollback_append(repl->scrollback, event->text_delta.text);
    // Trigger UI refresh - control returns to event loop for rendering
    ik_repl_request_render(repl);
    break;
```

**Tool Call Start (invoked during perform()):**
```c
case IK_STREAM_TOOL_CALL_START:
    // Initialize tool call accumulator
    ik_tool_call_t *tc = create_tool_call(repl, event->tool_call_start.index);
    tc->id = talloc_strdup(tc, event->tool_call_start.id);
    tc->name = talloc_strdup(tc, event->tool_call_start.name);
    break;
```

**Tool Call Delta (invoked during perform()):**
```c
case IK_STREAM_TOOL_CALL_DELTA:
    // Find tool call by index
    ik_tool_call_t *tc = find_tool_call(repl, event->tool_call_delta.index);
    // Append JSON fragment to arguments buffer
    append_args(tc, event->tool_call_delta.arguments);
    break;
```

**Tool Call Done (invoked during perform()):**
```c
case IK_STREAM_TOOL_CALL_DONE:
    // Find tool call by index
    ik_tool_call_t *tc = find_tool_call(repl, event->tool_call_done.index);
    // Finalize and execute
    ik_repl_execute_tool(repl, tc);
    break;
```

**Thinking Delta (invoked during perform()):**
```c
case IK_STREAM_THINKING_DELTA:
    // Accumulate thinking content
    append_thinking(repl->pending_thinking, event->thinking_delta.text);
    // Optional: update thinking display area
    ik_repl_request_render(repl);
    break;
```

**Done Event (invoked via info_read() completion callback):**
```c
case IK_STREAM_DONE:
    // Finalize accumulated content
    ik_repl_finalize_message(repl, event->done.finish_reason);
    // Save to database with metadata
    ik_repl_save_message(repl);
    break;
```

**Error Event (invoked during perform()):**
```c
case IK_STREAM_ERROR:
    // Display error in scrollback
    ik_scrollback_append_error(repl->scrollback, event->error.message);
    // Mark response as failed
    ik_repl_abort_request(repl);
    break;
```

### Message Persistence

Store provider metadata in JSONB `data` column:

| Field | Type | Description |
|-------|------|-------------|
| `provider` | string | Provider name (e.g., "anthropic") |
| `model` | string | Model ID (e.g., "claude-sonnet-4-5-20250929") |
| `thinking_level` | string | Thinking level (e.g., "med") |
| `thinking` | string | Thinking content if present |
| `thinking_tokens` | int | Thinking token count |
| `input_tokens` | int | Input token count |
| `output_tokens` | int | Output token count |
| `total_tokens` | int | Total token count |

**Example JSONB:**
```json
{
  "provider": "anthropic",
  "model": "claude-sonnet-4-5-20250929",
  "thinking_level": "med",
  "thinking": "Let me think about this...",
  "thinking_tokens": 150,
  "input_tokens": 1200,
  "output_tokens": 350,
  "total_tokens": 1550
}
```

Use existing `ik_db_message_insert()` - JSONB column is flexible, no schema changes needed.

### Backward Compatibility

- Remove OpenAI-specific callback handling
- Replace `ik_openai_event_t` with `ik_stream_event_t`
- Streaming behavior identical to before from user perspective
- Message format compatible with existing data

### Event Loop Integration

The REPL's existing event loop pattern (from `src/repl.c`) integrates with providers:

```c
// Before select(), add provider FDs:
if (agent->provider) {
    agent->provider->vt->fdset(agent->provider->ctx,
                               &read_fds, &write_fds, &exc_fds, &max_fd);
}

// After select() returns, process provider I/O:
if (agent->provider) {
    int running;
    agent->provider->vt->perform(agent->provider->ctx, &running);
    // ↑ Stream callbacks invoked here as data arrives
    agent->provider->vt->info_read(agent->provider->ctx, logger);
    // ↑ Completion callbacks invoked here when transfer finishes
}
```

## Test Scenarios

### Stream Callback (Async)
- Text delta: appends to scrollback during perform()
- Multiple text deltas: accumulate correctly
- Tool call start: initializes accumulator
- Tool call delta: accumulates arguments
- Tool call done: finalizes and executes
- Thinking delta: stored separately
- Done event: saves complete message (via info_read/completion callback)
- Error event: displays error, aborts request

### Message Persistence
- Message saved with provider metadata
- Thinking content stored when present
- Token counts stored correctly
- Missing thinking: field omitted (not null)
- JSONB data queryable after save

### UI Updates (Async)
- Scrollback updates during perform() as data arrives
- Render triggered on each delta (UI refresh happens between event loop iterations)
- Final message displayed correctly after completion callback
- Error messages displayed in scrollback during perform()

## Postconditions

- [ ] Stream callback handles all `ik_stream_event_t` types (invoked during perform())
- [ ] Text deltas accumulated correctly
- [ ] Tool calls accumulated correctly (start/delta/done sequence)
- [ ] Thinking content stored separately
- [ ] Messages saved with provider metadata
- [ ] Token counts persisted
- [ ] No OpenAI-specific event handling remains
- [ ] UI updates correctly during streaming (via callbacks during perform())
- [ ] Callbacks are non-blocking (no blocking I/O in stream_cb)
- [ ] Compiles without warnings
- [ ] `make check` passes
- [ ] Changes committed to git with message: `task: repl-streaming-updates.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).