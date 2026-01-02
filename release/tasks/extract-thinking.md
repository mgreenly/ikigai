# Task: Extract Thinking Blocks from Response

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided.

**Model:** sonnet
**Depends on:** agent-context.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - For PANIC patterns
- `/load style` - For code style conventions

**Source:**
- `src/repl_callbacks.c` - Response processing (lines 200-230)
- `src/providers/provider.h` - Content block types (lines 145-175)
- `src/agent.h` - Agent context with new fields

**Plan:**
- `release/plan/thinking-signatures.md` - Section 9 (Response Processing)

## Libraries

- `talloc` - Already used for memory management

## Preconditions

- [ ] Git workspace is clean
- [ ] `agent-context.md` task completed (pending thinking fields exist)

## Objective

Extract thinking blocks from API response when there's a tool call, storing them in agent context for later use in message construction.

## Interface

### Modify `extract_tool_calls`

**File:** `src/repl_callbacks.c` (line 208)

This is a static function with only one call site (line 289), so no rename is needed. Expand the function to also extract thinking blocks.

Add thinking block extraction before tool call extraction:

```c
static void extract_tool_calls(ik_agent_ctx_t *agent, const ik_response_t *response)
{
    // Clear any previous pending thinking
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

    // Clear any previous pending tool call
    if (agent->pending_tool_call != NULL) {
        talloc_free(agent->pending_tool_call);
        agent->pending_tool_call = NULL;
    }

    for (size_t i = 0; i < response->content_count; i++) {
        ik_content_block_t *block = &response->content_blocks[i];

        if (block->type == IK_CONTENT_THINKING) {
            if (block->data.thinking.text != NULL) {
                agent->pending_thinking_text = talloc_strdup(agent, block->data.thinking.text);
                if (agent->pending_thinking_text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
            if (block->data.thinking.signature != NULL) {
                agent->pending_thinking_signature = talloc_strdup(agent, block->data.thinking.signature);
                if (agent->pending_thinking_signature == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
        } else if (block->type == IK_CONTENT_REDACTED_THINKING) {
            if (block->data.redacted_thinking.data != NULL) {
                agent->pending_redacted_data = talloc_strdup(agent, block->data.redacted_thinking.data);
                if (agent->pending_redacted_data == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
        } else if (block->type == IK_CONTENT_TOOL_CALL) {
            agent->pending_tool_call = ik_tool_call_create(agent,
                                                           block->data.tool_call.id,
                                                           block->data.tool_call.name,
                                                           block->data.tool_call.arguments);
            if (agent->pending_tool_call == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            break;  // Only handle first tool call
        }
    }
}
```

## Behaviors

- Thinking blocks are extracted BEFORE tool call (preserves order)
- Only the first thinking block is stored (Anthropic sends one per response)
- Redacted thinking and regular thinking are mutually exclusive
- Previous pending values are cleared before extraction

## Test Scenarios

Add tests in `tests/unit/repl_callbacks_test.c` (or new file):

1. `test_extract_thinking_block` - Verify thinking text and signature stored
2. `test_extract_redacted_thinking` - Verify redacted data stored
3. `test_extract_thinking_with_tool_call` - Verify both stored
4. `test_extract_clears_previous` - Verify old values are freed

## Completion

```bash
git add -A
git commit -m "$(cat <<'EOF'
task(extract-thinking.md): success - extract thinking blocks from response

Renamed extract_tool_calls to extract_response_blocks.
Now extracts thinking blocks alongside tool calls from response.
EOF
)"
```

Report status: `/task-done extract-thinking.md`

## Postconditions

- [ ] Compiles without warnings
- [ ] All tests pass (`make check`)
- [ ] All changes committed
- [ ] Git workspace is clean
