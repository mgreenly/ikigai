# Task: Store Thinking Data in Database

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided.

**Model:** sonnet
**Depends on:** tool-call-message.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - For error handling patterns
- `/load style` - For code style conventions

**Source:**
- `src/repl_tool.c` - Database persistence (lines 112-118)
- `src/db/message.h` - Message insert API
- `src/db/message.c` - Message insert implementation
- `src/agent.h` - Agent context with pending thinking fields

**Plan:**
- `release/plan/thinking-signatures.md` - Section 10 (Database Persistence)

## Libraries

- `yyjson` - Already used for JSON handling

## Preconditions

- [ ] Git workspace is clean
- [ ] `tool-call-message.md` task completed (thinking in messages works)

## Objective

Store thinking block data in the `data_json` field when persisting tool_call messages to database.

## Interface

### Update Database Insert (Inline JSON Building)

**File:** `src/repl_tool.c`

**IMPORTANT:** There is only ONE function to update: `ik_agent_complete_tool_execution` (line 203). This handles both sync and async paths.

**Note:** Do NOT use a static helper function - LCOV exclusion markers inside static functions are not reliably honored. Inline the JSON building directly.

In `ik_agent_complete_tool_execution`, at lines 257-262, replace:

```c
// 4. Persist to database
if (agent->shared->db_ctx != NULL && agent->shared->session_id > 0) {
    ik_db_message_insert_(agent->shared->db_ctx, agent->shared->session_id,
                          agent->uuid, "tool_call", formatted_call, "{}");
    ik_db_message_insert_(agent->shared->db_ctx, agent->shared->session_id,
                          agent->uuid, "tool_result", formatted_result, "{}");
}
```

With:

```c
// 4. Persist to database
if (agent->shared->db_ctx != NULL && agent->shared->session_id > 0) {
    // Build data_json with tool call details and thinking (inline - no static function)
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(doc, root);

    // Tool call fields
    yyjson_mut_obj_add_str(doc, root, "tool_call_id", tc->id);
    yyjson_mut_obj_add_str(doc, root, "tool_name", tc->name);
    yyjson_mut_obj_add_str(doc, root, "tool_args", tc->arguments);

    // Thinking block (if present)
    if (agent->pending_thinking_text != NULL) {
        yyjson_mut_val *thinking_obj = yyjson_mut_obj(doc);
        if (thinking_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        yyjson_mut_obj_add_str(doc, thinking_obj, "text", agent->pending_thinking_text);
        if (agent->pending_thinking_signature != NULL) {
            yyjson_mut_obj_add_str(doc, thinking_obj, "signature", agent->pending_thinking_signature);
        }
        yyjson_mut_obj_add_val(doc, root, "thinking", thinking_obj);
    }

    // Redacted thinking (if present)
    if (agent->pending_redacted_data != NULL) {
        yyjson_mut_val *redacted_obj = yyjson_mut_obj(doc);
        if (redacted_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        yyjson_mut_obj_add_str(doc, redacted_obj, "data", agent->pending_redacted_data);
        yyjson_mut_obj_add_val(doc, root, "redacted_thinking", redacted_obj);
    }

    char *json = yyjson_mut_write(doc, 0, NULL);
    char *data_json = talloc_strdup(agent, json);
    free(json);
    yyjson_mut_doc_free(doc);
    if (data_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    ik_db_message_insert_(agent->shared->db_ctx, agent->shared->session_id,
                          agent->uuid, "tool_call", formatted_call, data_json);
    ik_db_message_insert_(agent->shared->db_ctx, agent->shared->session_id,
                          agent->uuid, "tool_result", formatted_result, "{}");
    talloc_free(data_json);
}
```

## Behaviors

- data_json now contains tool call details + thinking (if present)
- Existing messages without thinking continue to work (backwards compatible)
- JSON format matches plan specification

## Database Schema

No schema changes required. The `data` column already supports JSONB.

**Example stored JSON (with thinking):**
```json
{
  "tool_call_id": "toolu_01abc123",
  "tool_name": "bash",
  "tool_args": "{\"command\": \"ls\"}",
  "thinking": {
    "text": "Let me list the files...",
    "signature": "EqQBCgIYAhIM..."
  }
}
```

**Example stored JSON (without thinking):**
```json
{
  "tool_call_id": "toolu_01abc123",
  "tool_name": "bash",
  "tool_args": "{\"command\": \"ls\"}"
}
```

## Test Scenarios

Add tests in `tests/unit/repl_tool_test.c` (or new file):

1. `test_build_tool_call_data_json_with_thinking` - Verify JSON includes thinking
2. `test_build_tool_call_data_json_with_signature` - Verify signature included
3. `test_build_tool_call_data_json_no_thinking` - Verify clean JSON without thinking
4. `test_build_tool_call_data_json_redacted` - Verify redacted_thinking in JSON

## Completion

```bash
git add -A
git commit -m "$(cat <<'EOF'
task(db-persistence.md): success - store thinking data in database

Tool call messages now persist thinking text and signature in
data_json field for database storage and restoration.
EOF
)"
```

Report status: `/task-done db-persistence.md`

## Postconditions

- [ ] Compiles without warnings
- [ ] All tests pass (`make check`)
- [ ] All changes committed
- [ ] Git workspace is clean
