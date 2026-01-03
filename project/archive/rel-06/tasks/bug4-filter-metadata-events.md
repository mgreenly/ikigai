# Task: Filter metadata events from OpenAI API serialization

## Target
Bug 4: Application crashes with "Failed to add content field to message" when metadata events exist in conversation history

## Macro Context

**What:** When sending a message to the LLM, the application crashes if the conversation history contains metadata events like `agent_killed`, `clear`, `mark`, or `rewind` because these events have NULL content.

**Why this matters:**
- Users cannot interact with the LLM after performing agent operations (kill, mark, rewind, clear)
- The crash occurs during JSON serialization when trying to add NULL content to OpenAI API request
- Metadata events are internal REPL events that should never be sent to the LLM

**How the bug manifests:**
```
$ ./bin/ikigai
> what is 2 + 2
FATAL: Failed to add content field to message
  at src/openai/client.c:174
Aborted
```

**Database state showing the problem:**
```sql
SELECT id, kind, content FROM messages ORDER BY created_at;
 id |     kind     |               content
----+--------------+-------------------------------------
  5 | clear        |
  6 | system       | You are a helpful coding assistant.
  7 | agent_killed |                           -- NULL content causes crash
  8 | user         | what is 1 + 1
```

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/database.md
- .agents/skills/errors.md
- .agents/skills/git.md
- .agents/skills/makefile.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md

## Pre-read Docs
- src/msg.h (lines 1-35 - ik_msg_t structure and kind discriminator documentation)

## Pre-read Source
- src/msg.h (READ - canonical message structure with kind values)
- src/openai/client.c (MODIFY - lines 132-177, ik_openai_serialize_request function)
- src/db/message.c (READ - lines 15-28, valid_kinds array showing all event kinds)

## Source Files to MODIFY
- src/msg.h (add helper function declaration)
- src/msg.c (NEW FILE - implement helper function)
- src/openai/client.c (skip non-conversation messages during serialization)
- tests/unit/openai/client_test.c (add tests for metadata filtering)

## Investigation Context

**Root Cause Analysis:**

The serialization loop in `src/openai/client.c:156-177` processes ALL messages without filtering:
```c
for (size_t i = 0; i < request->conv->message_count; i++) {
    ik_msg_t *msg = get_message_at_index(request->conv->messages, i);

    // Creates message object for EVERY message, including metadata
    yyjson_mut_val *msg_obj = yyjson_mut_arr_add_obj(doc, messages_arr);

    // ... later tries to add content field
    if (!yyjson_mut_obj_add_str(doc, msg_obj, "content", msg->content)) {
        PANIC("Failed to add content field to message"); // CRASHES when content is NULL
    }
}
```

**Message Kinds Classification (from src/msg.h):**

*Conversation kinds* (should be sent to LLM):
- `"system"` - System message (role-based)
- `"user"` - User message (role-based)
- `"assistant"` - Assistant message (role-based)
- `"tool_call"` - Tool call message (structured data)
- `"tool_result"` - Tool result message (structured data)

*Metadata kinds* (internal REPL events, should NOT be sent to LLM):
- `"clear"` - Context reset events (NULL content)
- `"mark"` - Checkpoint metadata (may have NULL content)
- `"rewind"` - Navigation metadata (may have NULL content)
- `"agent_killed"` - Agent termination tracking (NULL content)

**Why yyjson_mut_obj_add_str fails:**
From yyjson documentation: `yyjson_mut_obj_add_str()` returns false if the value is NULL, causing the PANIC.

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Database contains at least one metadata event (`agent_killed`, `clear`, `mark`, or `rewind`)

## Task

### Part 1: Create helper function to identify conversation messages
1. Create `src/msg.c` as a new file
2. Implement `ik_msg_is_conversation_kind(const char *kind)` that returns:
   - `true` for: "system", "user", "assistant", "tool_call", "tool_result"
   - `false` for: "clear", "mark", "rewind", "agent_killed"
   - `false` for NULL or unknown kinds
3. Add declaration to `src/msg.h`
4. Follow naming conventions: `ik_msg_*` prefix for message utilities

### Part 2: Filter metadata events during serialization
1. In `src/openai/client.c:156-177`, modify the serialization loop:
   - Check if message kind is a conversation kind before creating message object
   - Skip metadata events (clear, mark, rewind, agent_killed)
   - Only call `yyjson_mut_arr_add_obj()` for conversation messages
2. Preserve existing behavior for conversation messages (system, user, assistant, tool_call, tool_result)

### Part 3: Add comprehensive test coverage
1. Test `ik_msg_is_conversation_kind()` for all kind values
2. Test OpenAI serialization with mixed conversation and metadata events
3. Test that metadata events don't appear in serialized JSON
4. Test that conversation messages are still serialized correctly

## TDD Cycle

### Red
1. Create `tests/unit/msg_test.c` with tests for `ik_msg_is_conversation_kind()`:
   - Test returns true for "system", "user", "assistant", "tool_call", "tool_result"
   - Test returns false for "clear", "mark", "rewind", "agent_killed"
   - Test returns false for NULL
   - Test returns false for unknown kinds like "bogus"

2. Add test to `tests/unit/openai/client_test.c`:
   - Test serialization with metadata events in conversation
   - Verify metadata events are excluded from JSON
   - Verify conversation messages are included

3. Run `make check` - tests should fail (helper function doesn't exist yet)

### Green
1. Create `src/msg.c` with `ik_msg_is_conversation_kind()` implementation
2. Add declaration to `src/msg.h`
3. Update `src/openai/client.c` to skip non-conversation messages
4. Update Makefile to compile `src/msg.c` (add to SOURCES)
5. Run `make check` - tests should pass
6. Run `make lint` - should pass

### Refactor
1. Ensure helper function has clear documentation
2. Consider if other code paths need similar filtering (future work)
3. Add comments explaining why metadata events are filtered

## Sub-agent Usage
- Use Task tool with Explore agent to search for other places that might serialize messages
- Use Task tool to verify no other code assumes all messages are sent to LLM

## Overcoming Obstacles
- If Makefile changes are complex, refer to existing patterns for adding new source files
- If test setup is challenging, refer to `tests/unit/openai/client_test.c` for examples
- If serialization logic is more complex than expected, trace through with debugger

## Post-conditions
- `make` compiles successfully
- `make check` passes (all tests including new ones)
- `make lint` passes
- New file `src/msg.c` exists and is compiled
- `ik_msg_is_conversation_kind()` correctly identifies conversation vs metadata kinds
- OpenAI serialization skips metadata events (clear, mark, rewind, agent_killed)
- Can send user messages without crashing when metadata events exist in history
- Working tree is clean (all changes committed)

## Implementation Notes

**Helper Function Signature:**
```c
// In src/msg.h
/**
 * Check if a message kind should be included in LLM conversation context
 *
 * @param kind Message kind string (e.g., "user", "assistant", "clear")
 * @return true if kind should be sent to LLM, false otherwise
 *
 * Conversation kinds (returns true):
 *   - "system", "user", "assistant", "tool_call", "tool_result"
 *
 * Metadata kinds (returns false):
 *   - "clear", "mark", "rewind", "agent_killed"
 *   - NULL or unknown kinds
 */
bool ik_msg_is_conversation_kind(const char *kind);
```

**Serialization Loop Pattern:**
```c
for (size_t i = 0; i < request->conv->message_count; i++) {
    ik_msg_t *msg = get_message_at_index(request->conv->messages, i);

    // Skip metadata events - they're not part of LLM conversation
    if (!ik_msg_is_conversation_kind(msg->kind)) {
        continue;
    }

    // Existing serialization logic for conversation messages
    yyjson_mut_val *msg_obj = yyjson_mut_arr_add_obj(doc, messages_arr);
    // ... rest of serialization
}
```

**Makefile Update:**
Add `src/msg.c` to the SOURCES list, maintaining alphabetical order.

## Deviations
Document any deviation from this plan with reasoning.
