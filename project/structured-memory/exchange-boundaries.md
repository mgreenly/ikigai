# Exchange Boundaries

The sliding window evicts messages in atomic units called "exchanges" to maintain semantic coherence.

## What is an Exchange?

An exchange is the complete interaction from user input through final assistant response:

```
Exchange N:
  ├─ Message 1: user input
  ├─ Message 2: assistant tool_call (optional)
  ├─ Message 3: tool_result (optional)
  ├─ Message 4: assistant tool_call (optional)
  ├─ Message 5: tool_result (optional)
  ├─ ...
  └─ Message N: assistant final response
```

**All messages in an exchange are evicted together.** Never orphan tool calls or results.

## Why Exchange Boundaries?

### Problem: Partial Eviction

**Without exchange boundaries**:
```
[Evict oldest 5k tokens to make room]

Remaining context:
  ❌ Message 142: tool_result "File written successfully"
  ✓  Message 143: assistant "I've updated the auth module..."
  ✓  Message 144: user "Now add tests"

Missing context:
  Message 140: user "Implement authentication"
  Message 141: assistant tool_call file_write(src/auth.c)
```

**Problem**: Tool result without corresponding call is nonsensical.

### Solution: Atomic Eviction

**With exchange boundaries**:
```
[Evict entire Exchange 23 (messages 140-143)]

Remaining context:
  ✓  Message 144: user "Now add tests"
  ✓  Message 145: assistant tool_call file_write(tests/auth_test.c)
  ✓  Message 146: tool_result "success"
  ✓  Message 147: assistant "Tests added..."
```

**Result**: Every message has full context. No orphaned tool calls.

## Exchange Structure Examples

### Simple Exchange (No Tools)

```
Exchange 1 (2.1k tokens):
  ├─ msg_1: user "What's the current status?"
  └─ msg_2: assistant "We've implemented auth and tests..."
```

### Single Tool Call

```
Exchange 2 (4.5k tokens):
  ├─ msg_3: user "Show me the auth code"
  ├─ msg_4: assistant tool_call file_read(src/auth.c)
  ├─ msg_5: tool_result [file contents]
  └─ msg_6: assistant "Here's the auth implementation..."
```

### Multiple Tool Calls

```
Exchange 3 (8.2k tokens):
  ├─ msg_7: user "Fix all linting errors"
  ├─ msg_8: assistant tool_call bash("npm run lint")
  ├─ msg_9: tool_result [5 errors listed]
  ├─ msg_10: assistant tool_call file_edit(src/a.js, ...)
  ├─ msg_11: tool_result "success"
  ├─ msg_12: assistant tool_call file_edit(src/b.js, ...)
  ├─ msg_13: tool_result "success"
  ├─ msg_14: assistant tool_call bash("npm run lint")
  ├─ msg_15: tool_result "All clear"
  └─ msg_16: assistant "Fixed all 5 linting errors."
```

All 10 messages evicted together when this exchange falls off window.

## Boundary Detection

### Algorithm

```c
typedef struct {
    int start_msg_id;   // First message (user input)
    int end_msg_id;     // Last message (assistant response)
    int token_count;    // Total tokens in exchange
} ik_exchange_t;

ik_exchange_t* detect_exchanges(ik_message_t *messages, int count) {
    ik_exchange_t *exchanges = array_new(sizeof(ik_exchange_t));

    int exchange_start = -1;
    int exchange_tokens = 0;

    for (int i = 0; i < count; i++) {
        ik_message_t *msg = &messages[i];

        if (msg->role == IK_ROLE_USER) {
            // Start of new exchange
            if (exchange_start >= 0) {
                // Save previous exchange
                array_append(exchanges, &(ik_exchange_t){
                    .start_msg_id = exchange_start,
                    .end_msg_id = i - 1,
                    .token_count = exchange_tokens
                });
            }

            exchange_start = i;
            exchange_tokens = msg->token_count;

        } else {
            // Part of current exchange
            exchange_tokens += msg->token_count;
        }
    }

    // Save final exchange
    if (exchange_start >= 0) {
        array_append(exchanges, &(ik_exchange_t){
            .start_msg_id = exchange_start,
            .end_msg_id = count - 1,
            .token_count = exchange_tokens
        });
    }

    return exchanges;
}
```

**Rule**: User message starts exchange, next user message ends previous exchange.

### Edge Cases

**Interrupted exchange** (user sends before agent finishes):
```
Exchange 5 (incomplete):
  ├─ msg_20: user "Implement feature X"
  ├─ msg_21: assistant tool_call file_read(...)
  └─ msg_22: tool_result [content]

[User interrupts]

Exchange 6:
  ├─ msg_23: user "Actually, do Y instead"
  └─ msg_24: assistant "Sure, working on Y..."
```

Exchange 5 remains incomplete but is still a valid boundary. Gets evicted atomically.

**System messages** (marks, rewinds):
```
Exchange 7:
  ├─ msg_25: user "Add logging"
  ├─ msg_26: system mark_created
  ├─ msg_27: assistant tool_call file_edit(...)
  ├─ msg_28: tool_result "success"
  └─ msg_29: assistant "Logging added"
```

System messages included in exchange, evicted with it.

## Eviction Process

```c
void update_sliding_window(ik_agent_ctx_t *agent) {
    // 1. Detect exchange boundaries
    ik_message_t *messages = get_all_messages(agent);
    ik_exchange_t *exchanges = detect_exchanges(messages);

    // 2. Calculate total tokens
    int total_tokens = sum_exchange_tokens(exchanges);

    // 3. Evict oldest exchanges until under budget
    while (total_tokens > MAX_SLIDING_WINDOW_TOKENS) {
        ik_exchange_t *oldest = &exchanges[0];

        // Mark all messages in exchange as evicted
        db_mark_messages_evicted(
            oldest->start_msg_id,
            oldest->end_msg_id
        );

        // Add exchange summary to auto-summary queue
        enqueue_summary_addition(agent, oldest);

        // Update running total
        total_tokens -= oldest->token_count;

        // Remove from array
        array_remove_first(exchanges);
    }
}
```

**Eviction is atomic**: All messages in exchange marked together.

## Database Representation

```sql
-- Messages table tracks eviction status
CREATE TABLE messages (
  id SERIAL PRIMARY KEY,
  agent_id UUID,
  role TEXT,
  content TEXT,
  token_count INTEGER,
  created_at TIMESTAMP,

  -- Sliding window tracking
  in_sliding_window BOOLEAN DEFAULT TRUE,
  evicted_at TIMESTAMP,
  exchange_id INTEGER  -- Groups messages by exchange
);

-- Optional: Explicit exchange tracking
CREATE TABLE exchanges (
  id SERIAL PRIMARY KEY,
  agent_id UUID,
  start_msg_id INTEGER,
  end_msg_id INTEGER,
  token_count INTEGER,
  created_at TIMESTAMP,
  evicted_at TIMESTAMP
);
```

**Query for sliding window**:
```sql
SELECT * FROM messages
WHERE agent_id = $1
  AND in_sliding_window = TRUE
ORDER BY created_at;
```

**Query for archival**:
```sql
SELECT * FROM messages
WHERE agent_id = $1
  AND in_sliding_window = FALSE
ORDER BY created_at;
```

## Building LLM Request

```c
ik_result_t build_llm_request(ik_agent_ctx_t *agent) {
    // Get sliding window messages
    ik_message_t *messages = db_get_sliding_window_messages(agent);

    // Verify exchange completeness (debug build)
    #ifdef DEBUG
    assert_no_orphaned_tool_calls(messages);
    #endif

    // Build request with complete exchanges
    json_t *request = json_object();
    json_object_set_new(request, "messages",
                       serialize_messages(messages));

    return OK(request);
}
```

**Invariant**: Sliding window always contains complete exchanges.

## Token Estimation

Exchanges need accurate token counts:

```c
void on_message_received(ik_agent_ctx_t *agent, ik_message_t *msg) {
    // Store message
    db_insert_message(agent, msg);

    // Update current exchange token count
    agent->current_exchange_tokens += msg->token_count;

    if (msg->role == IK_ROLE_ASSISTANT && !msg->has_tool_calls) {
        // Exchange complete
        db_finalize_exchange(agent, agent->current_exchange_tokens);
        agent->current_exchange_tokens = 0;

        // Trigger eviction check
        update_sliding_window(agent);
    }
}
```

**Token counts tracked incrementally** as exchange builds.

## Visualization

Users can see exchange boundaries:

```bash
> /history

Sliding Window (82k/90k tokens):

[Outside window - in archival]
  Exchange 1: "Implement auth" (evicted 3h ago, 4.2k)
  Exchange 2: "Add tests" (evicted 2h ago, 3.8k)

[Inside window - currently visible]
  Exchange 3: "Refactor errors" (1h ago, 8.5k)
    ├─ User: "Refactor error handling to use Result<T>"
    ├─ Tool: file_read src/errors.h
    ├─ Tool: file_edit src/errors.c
    └─ Assistant: "Refactored to Result<T> pattern"

  Exchange 4: "Update docs" (30min ago, 5.2k)
    ├─ User: "Document the new error handling"
    ├─ Tool: file_write docs/errors.md
    └─ Assistant: "Documentation updated"

  Exchange 5: "Add logging" (current, 2.1k)
    └─ User: "Add structured logging"
```

**Exchanges are the natural granularity** for viewing conversation history.

## Benefits

**Semantic coherence**:
- Tool calls never orphaned
- Complete thoughts preserved
- Natural conversation units

**Predictable behavior**:
- User knows eviction happens at exchange boundaries
- No mid-conversation context loss
- Clean break points

**Summary quality**:
- Background agents summarize complete interactions
- "User asked X, agent did Y" makes sense
- No partial tool sequences to explain

**Debugging**:
- Exchange is atomic unit for replay
- Easy to identify problematic exchanges
- Natural granularity for logging

---

Exchange boundaries ensure the sliding window maintains semantic integrity even as old messages evict.
