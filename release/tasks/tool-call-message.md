# Task: Create Tool Call Message with Thinking Blocks

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided.

**Model:** sonnet
**Depends on:** extract-thinking.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - For PANIC patterns
- `/load style` - For code style conventions

**Source:**
- `src/repl_tool.c` - Tool execution (lines 60-125)
- `src/message.h` - Message creation functions
- `src/message.c` - Message implementation (lines 30-55)
- `src/agent.h` - Agent context with pending thinking fields

**Plan:**
- `release/plan/thinking-signatures.md` - Section 9 (Tool Call Message Creation)

## Libraries

- `talloc` - Already used for memory management

## Preconditions

- [ ] Git workspace is clean
- [ ] `extract-thinking.md` task completed (thinking blocks extracted to agent)

## Objective

Modify tool call message creation to include thinking blocks (when present) before the tool_call block.

## Interface

### New Helper Function

**File:** `src/message.c`

Add function to create multi-block assistant message:

```c
/**
 * Create assistant message with thinking and tool call blocks
 *
 * Creates a message containing:
 * 1. Thinking block (if thinking_text != NULL)
 * 2. Tool call block
 *
 * @param ctx              Talloc parent
 * @param thinking_text    Thinking text (may be NULL)
 * @param thinking_sig     Thinking signature (may be NULL)
 * @param redacted_data    Redacted thinking data (may be NULL)
 * @param tool_id          Tool call ID
 * @param tool_name        Tool name
 * @param tool_args        Tool arguments JSON
 * @return                 Allocated message
 */
ik_message_t *ik_message_create_tool_call_with_thinking(
    TALLOC_CTX *ctx,
    const char *thinking_text,
    const char *thinking_sig,
    const char *redacted_data,
    const char *tool_id,
    const char *tool_name,
    const char *tool_args);
```

**Declaration:** Add to `src/message.h`

### Implementation

```c
ik_message_t *ik_message_create_tool_call_with_thinking(
    TALLOC_CTX *ctx,
    const char *thinking_text,
    const char *thinking_sig,
    const char *redacted_data,
    const char *tool_id,
    const char *tool_name,
    const char *tool_args)
{
    assert(tool_id != NULL);    // LCOV_EXCL_BR_LINE
    assert(tool_name != NULL);  // LCOV_EXCL_BR_LINE
    assert(tool_args != NULL);  // LCOV_EXCL_BR_LINE

    // Count blocks
    size_t block_count = 1;  // tool_call always present
    if (thinking_text != NULL) block_count++;
    if (redacted_data != NULL) block_count++;

    ik_message_t *msg = talloc_zero(ctx, ik_message_t);
    if (!msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    msg->role = IK_ROLE_ASSISTANT;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, (unsigned int)block_count);
    if (!msg->content_blocks) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    msg->content_count = block_count;
    msg->provider_metadata = NULL;

    size_t idx = 0;

    // Add thinking block first (if present)
    if (thinking_text != NULL) {
        msg->content_blocks[idx].type = IK_CONTENT_THINKING;
        msg->content_blocks[idx].data.thinking.text = talloc_strdup(msg, thinking_text);
        if (!msg->content_blocks[idx].data.thinking.text) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        if (thinking_sig != NULL) {
            msg->content_blocks[idx].data.thinking.signature = talloc_strdup(msg, thinking_sig);
            if (!msg->content_blocks[idx].data.thinking.signature) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        } else {
            msg->content_blocks[idx].data.thinking.signature = NULL;
        }
        idx++;
    }

    // Add redacted thinking (if present)
    if (redacted_data != NULL) {
        msg->content_blocks[idx].type = IK_CONTENT_REDACTED_THINKING;
        msg->content_blocks[idx].data.redacted_thinking.data = talloc_strdup(msg, redacted_data);
        if (!msg->content_blocks[idx].data.redacted_thinking.data) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        idx++;
    }

    // Add tool_call - populate directly (don't use helper which allocates separately)
    msg->content_blocks[idx].type = IK_CONTENT_TOOL_CALL;
    msg->content_blocks[idx].data.tool_call.id = talloc_strdup(msg, tool_id);
    if (!msg->content_blocks[idx].data.tool_call.id) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    msg->content_blocks[idx].data.tool_call.name = talloc_strdup(msg, tool_name);
    if (!msg->content_blocks[idx].data.tool_call.name) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    msg->content_blocks[idx].data.tool_call.arguments = talloc_strdup(msg, tool_args);
    if (!msg->content_blocks[idx].data.tool_call.arguments) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return msg;
}
```

### Update Tool Execution

**File:** `src/repl_tool.c`

**IMPORTANT:** There is only ONE function to update: `ik_agent_complete_tool_execution` (line 203). This handles both sync and async paths - the wrapper `ik_repl_complete_tool_execution` just calls it.

In `ik_agent_complete_tool_execution`, at line 223, replace:

```c
ik_message_t *tc_msg = ik_message_create_tool_call(agent, tc->id, tc->name, tc->arguments);
```

With:

```c
ik_message_t *tc_msg = ik_message_create_tool_call_with_thinking(
    agent,
    agent->pending_thinking_text,
    agent->pending_thinking_signature,
    agent->pending_redacted_data,
    tc->id, tc->name, tc->arguments);

// Clear pending thinking after use
if (agent->pending_thinking_text != NULL) {
    talloc_free(agent->pending_thinking_text);
    agent->pending_thinking_text = NULL;
}
if (agent->pending_thinking_signature != NULL) {
    talloc_free(agent->pending_thinking_signature);
    agent->pending_thinking_signature = NULL;
}
if (agent->pending_redacted_data != NULL) {
    talloc_free(agent->pending_redacted_data);
    agent->pending_redacted_data = NULL;
}
```

## Behaviors

- Thinking block appears BEFORE tool_call (matches Anthropic response order)
- Signature is optional (may be NULL for testing)
- Thinking and redacted_thinking don't coexist (but code handles if they did)
- Pending thinking fields cleared after message creation

## Test Scenarios

Add tests in `tests/unit/message_test.c`:

1. `test_create_tool_call_with_thinking` - Verify 2 blocks (thinking + tool_call)
2. `test_create_tool_call_with_redacted` - Verify 2 blocks (redacted + tool_call)
3. `test_create_tool_call_no_thinking` - Verify 1 block (tool_call only)
4. `test_create_tool_call_with_signature` - Verify signature copied

## Completion

```bash
git add -A
git commit -m "$(cat <<'EOF'
task(tool-call-message.md): success - include thinking in tool call messages

Added ik_message_create_tool_call_with_thinking function.
Tool call messages now include thinking blocks with signatures.
EOF
)"
```

Report status: `/task-done tool-call-message.md`

## Postconditions

- [ ] Compiles without warnings
- [ ] All tests pass (`make check`)
- [ ] All changes committed
- [ ] Git workspace is clean
