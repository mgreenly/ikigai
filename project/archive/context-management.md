# Context Management

## Overview

Ikigai conversations can run indefinitely - across hours, days, or weeks, surviving multiple application restarts. Context persists until explicitly modified by user command. This document describes the toolkit for managing conversation context.

**Core principle**: Context changes only through explicit user action, never implicitly.

---

## The Problem

Context growth is inevitable in long-running conversations:
- Token limits constrain what can be sent to the LLM
- Model performance degrades as context fills (typically around 80% capacity)
- Not all historical messages remain relevant to current work

Users need control over what stays in context and what gets removed.

---

## Context vs Database

**Database**: Immutable event log. Every message, tool call, and command is recorded permanently.

**Context (Memory)**: The filtered view of conversation history sent to the LLM. Can be modified through context management commands.

Commands modify context, not the database. The full history remains queryable.

---

## Available Commands

| Command | Purpose | Scope |
|---------|---------|-------|
| `/clear` | Reset to empty context | Nuclear - everything gone |
| `/mark` | Create checkpoint | Marker for potential rollback |
| `/rewind` | Rollback to last mark | Remove everything after mark |
| `/forget` | Remove specific content | Selective removal by criteria |
| `/remember` | Keep only specific content | Selective retention (inverse of forget) |

---

## /clear - Nuclear Reset

Start fresh. All context removed, conversation begins anew.

```
> /clear
Context cleared. Starting fresh.
```

**Use when**: Starting a completely different task, or context is unrecoverably cluttered.

**Database impact**: None. Messages remain in database, just removed from active context.

---

## /mark and /rewind - Checkpointing

Create savepoints and rollback to them.

```
> /mark
Checkpoint created.

... conversation continues ...

> /rewind
Rewound to last checkpoint.
```

**Use when**: Trying experimental approaches, want ability to undo and try differently.

**Implementation**: Mark events stored in database. Rewind removes messages after the mark from context.

**Note**: Marks are database events but never sent to the LLM. They're invisible markers in the event stream.

---

## /forget - Selective Removal

Remove messages matching specific criteria while keeping the rest.

### Workflow

```
User: "/forget database schema discussions"
User: "Show me what you'll forget first"
LLM: [uses filter_context tool with action: "preview"]
LLM: "I found 12 messages about database schema:"
     - Message 45-67: Initial schema design discussion
     - Message 312-340: Schema refactoring conversation
     - Message 678-690: Column naming debate
     
     Total: 12 messages, ~4500 tokens
     
     Should I proceed to forget these?

User: "Yes, proceed"

LLM: [uses filter_context tool with action: "apply"]
LLM: "Done. I've forgotten those 12 messages about database schema."

[Planning conversation (messages 88-95) automatically removed from context]
```

### Characteristics

**LLM-based matching**: The LLM applies semantic understanding to find matches. "Database schema discussions" catches related conversations even if they don't use exact keywords.

**Interactive**: User can review what will be forgotten before confirming.

**Self-cleaning**: The entire planning conversation disappears after the filter is applied.

**Composable**: Multiple `/forget` commands compound. Each filter removes additional messages.

### Use Cases

- Remove debugging sessions after issue is resolved
- Forget exploratory conversations that led nowhere
- Clean up failed attempts before the working solution
- Remove outdated information after refactoring

---

## /remember - Selective Retention

Inverse of `/forget`. Keep only messages matching criteria, forget everything else.

### Workflow

```
User: "/remember only the final implementation and API decisions"

LLM: [uses filter_context tool with action: "preview", mode: "remember"]
LLM: "I found 18 messages containing final implementation and API decisions:"
     - Message 450-465: Final API contract definition
     - Message 890-895: Implementation strategy we settled on
     
     Everything else (512 messages) will be forgotten.
     Should I proceed?

User: "Yes"

LLM: [uses filter_context tool with action: "apply", mode: "remember"]
LLM: "Done. Context reduced to 18 messages."
```

### Use Cases

- After long feature development, keep only the final implementation
- Distill a lengthy discussion down to decisions made
- Prepare for new phase of work with minimal relevant context

---

## Ephemeral Mode - High Context Pressure

When context usage is high (80%+), the planning conversation itself might overflow context. Ikigai enters **ephemeral mode** to handle this.

### How It Works

Instead of reviewing the full history at once, the LLM reviews it in chunks:

```
Context at 85% (170K/200K tokens)

User: "/forget database discussions"

[Ephemeral mode activated]

Iteration 1:
  Input: system prompt + chunk 1 (messages 1-200) + filter request
  Output: matched IDs [45-67]
  
Iteration 2:
  Input: system prompt + chunk 2 (messages 201-400) + filter request  
  Output: matched IDs [312-340]

...continue for all chunks...

Aggregate matched_ids: [45-67, 312-340, 678-690]
User confirms, filter applied
```

### Chunk Boundaries

Chunks split on **exchange boundaries** - user input through complete assistant response. This keeps semantic units together.

### Adaptive Chunking

The number of chunks adapts to context pressure:

| Context Usage | Strategy |
|--------------|----------|
| < 50% | No chunking - review all at once |
| 50-80% | 2 chunks |
| 80-90% | 3-4 chunks |
| 90%+ | 5+ chunks or force `/clear` only |

### Implementation Detail

Ephemeral mode is an implementation detail. Users may see chunk progress in future iterations but it doesn't affect the final outcome.

---

## Context Limits and Forcing Function

### Token Limits

Different models have different context windows:
- GPT-4: 128K tokens
- Claude Sonnet/Opus: 200K tokens

Ikigai tracks context usage and displays it to the user.

### UI Indicator

```
┌─────────────────────────────────────────┐
│ Context: 80K/200K tokens (40%) ⚪        │  ← Normal
└─────────────────────────────────────────┘

┌─────────────────────────────────────────┐
│ Context: 160K/200K tokens (80%) 🟡      │  ← Model degrading
└─────────────────────────────────────────┘

┌─────────────────────────────────────────┐
│ Context: 190K/200K tokens (95%) 🔴      │  ← Critical
└─────────────────────────────────────────┘
```

### Hard Limit

At 100% capacity, user input is blocked:

```
⚠️  Context full. Use /clear to continue.
```

### Practical Behavior

In practice, humans notice model degradation around 80% and intervene naturally. The UI indicator makes the problem visible before it becomes critical.

---

## Filter Events in Database

Filter events are stored in the database for replay and auditability.

### Schema

```sql
CREATE TABLE filter_events (
  id SERIAL PRIMARY KEY,
  session_id INTEGER REFERENCES sessions(id),
  created_at TIMESTAMP,
  mode TEXT,  -- 'forget' or 'remember'
  criteria TEXT,  -- "database schema discussions"
  
  -- Messages affected by the filter
  filtered_message_ids INTEGER[],
  
  -- The planning conversation (also removed)
  planning_start_id INTEGER,
  planning_end_id INTEGER,
  
  -- Metadata
  context_size_before INTEGER,
  context_size_after INTEGER
);
```

### Replay Semantics

When loading a session from the database:

```c
// Pseudocode
messages = load_all_messages(session_id);
filters = load_filter_events(session_id);  // in order

for (filter in filters) {
    // Remove filtered messages
    messages = remove_ids(messages, filter.filtered_message_ids);
    
    // Remove planning conversation
    messages = remove_range(messages, 
                           filter.planning_start_id,
                           filter.planning_end_id);
}

return messages;  // Final context for LLM
```

Events apply in serial order. Each filter operates on the result of previous filters.

---

## Interaction with /mark and /rewind

Filter events and checkpoint events coexist without conflict:

```
Timeline of events:
1. Messages 1-100
2. /mark (checkpoint created)
3. Messages 101-150
4. /forget (removes messages 50-75, planning was 140-145)
5. Messages 151-200
6. /rewind (to mark at step 2)

Result after replay:
- Load 1-100
- Mark (not sent to LLM, just a boundary marker)
- Load 101-150
- Apply forget filter (messages 50-75 removed, messages 140-145 removed)
- Load 151-200
- Apply rewind (removes messages 101-200)
- Final context: messages 1-49, 76-100
```

Each operation is independent. They compose naturally through serial execution.

---

## Tool Interface

The LLM uses a `filter_context` tool to execute filtering operations.

### Preview Phase

```json
{
  "action": "preview",
  "criteria": "messages about database schema",
  "mode": "forget"
}
```

Returns:
```json
{
  "matching_messages": [
    {"id": 142, "role": "user", "preview": "Can you explain the schema?"},
    {"id": 143, "role": "assistant", "preview": "Sure! The schema..."}
  ],
  "message_count": 12,
  "token_estimate": 4500
}
```

### Apply Phase

```json
{
  "action": "apply",
  "mode": "forget",
  "message_ids": [142, 143, 144, ...],
  "planning_conversation_ids": [201, 202, 203, 204]
}
```

Returns:
```json
{
  "filter_event_id": 17,
  "messages_filtered": 12,
  "context_before": 523,
  "context_after": 511
}
```

### Tool Responsibilities

- Query messages matching semantic criteria
- Return previews for user review
- Create filter events in database
- Track planning conversation boundaries
- Report context size changes

---

## Future: /recall - Historical Search

Filter events remove messages from context but leave them in the database. A future `/recall` command could search historical (forgotten) messages:

```
User: "/recall what did we discuss about database schema?"

LLM: [searches all messages including forgotten ones]
LLM: "I found 3 conversations about database schema in the forgotten history:
     - March 15: Initial schema design (12 messages)
     - March 18: Refactoring discussion (8 messages)
     - March 20: Column naming (5 messages)"
```

This provides access to forgotten information without polluting active context.

---

## Design Philosophy

### Explicit Over Implicit

Context never changes automatically. Users always know why context changed and when.

### Transparent Collaboration

The LLM shows what it will forget before applying changes. Users review and approve.

### Auditability

All context changes recorded as events in the database. Full history queryable.

### Composability

Commands work together naturally. Multiple filters compound. Filters and checkpoints coexist.

### User Control

The user decides what's relevant, what to keep, what to forget. The LLM executes their intent.

---

## Summary

| Command | When to Use |
|---------|-------------|
| `/clear` | Starting completely fresh |
| `/mark` | Before trying something experimental |
| `/rewind` | Undo and try differently |
| `/forget XYZ` | Remove specific topics, keep the rest |
| `/remember XYZ` | Keep only specific topics, forget the rest |

Together, these commands give users complete control over conversation context in long-running sessions.
