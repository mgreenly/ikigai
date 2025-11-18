# Mark and Rewind

**Checkpoint and rollback for iterative exploration.** Like git for your conversation.

## The Problem

When working with AI agents, you often try multiple approaches, explore risky refactors, test different solutions, and need to backtrack from dead ends.

**Without mark/rewind:** Start new conversation from scratch, re-establish context, try again.

**With mark/rewind:** `/mark` checkpoint, try approach, `/rewind` instant rollback, try different approach from same starting point.

## Core Concepts

### Marks

**What:** Checkpoints in your conversation history

**Properties:** Named or anonymous, stack-based (LIFO), multiple marks allowed, visible in scrollback as separators

### Rewind

**What:** Rollback to a checkpoint

**How:** Truncate message array to mark position

**Effect:** Restores conversation context, removes messages after mark, rebuilds scrollback, non-destructive (DB keeps everything)

### Non-Destructive

**Critical:** Nothing is deleted from database. All messages preserved, complete audit trail, can view rewound-from messages with `/history`.

## Quick Start

### Anonymous Mark

```bash
[You] Let's try refactoring this module

/mark
⟡ Checkpoint #1 created

[Conversation continues...]

[You] Wait, that didn't work. Go back.

/rewind
Rewound to Checkpoint #1 ⟡

[You] Let's try a different approach...
```

### Named Mark

```bash
[You] Let's explore different OAuth implementations

/mark before-oauth
⟡ Mark: before-oauth

[Try JWT approach...]

[You] That's too complex. Rewind.

/rewind before-oauth
Rewound to Mark: before-oauth ⟡

[You] Try session-based approach instead
```

### Multiple Marks

```bash
/mark approach-a
[Try approach A...]

/mark approach-b
[Try approach B...]

# Rewind to specific mark
/rewind approach-a
```

## Use Cases

### Risky Refactors

```bash
[You] Let's refactor the error handling module

/mark before-refactor

[You] Refactor all error handling to use ik_result_t
[Makes changes...]

[You] This breaks too much. Rewind.

/rewind before-refactor

[You] Let's take a more incremental approach
```

### Exploring Alternatives

```bash
[You] I'm not sure which data structure to use

/mark before-data-structure

[You] Try using a hash table
[Implementation and analysis...]

/rewind before-data-structure

[You] Try using a binary search tree instead
[Different implementation and comparison...]
```

### Iterative Design

```bash
/mark v1
[Design simple API...]

/mark v2
[Add OAuth support...]

/mark v3
[Add rate limiting...]

# Review versions
/rewind v2
[Review v2 design]

# Or start over
/rewind v1
```

### Avoiding Dead Ends

```bash
[You] Debug this memory leak

/mark before-debugging

[Long debugging session that's wrong approach...]

/rewind before-debugging

[Fresh debugging approach with no wasted context]
```

## Mark Stack Behavior

Marks form a LIFO (Last In, First Out) stack:

```bash
/mark a
/mark b
/mark c

Current state: [a] → [b] → [c] → [current]

/rewind     # Rewind to [c]
/rewind     # Rewind to [b]
```

**After rewind:**

```bash
/mark a
/mark b
/rewind     # Now at [a]

Mark stack: [a] → [current]
# Mark [b] is removed from stack (but preserved in database)

/mark c     # New branch from [a]
Mark stack: [a] → [c] → [current]
```

## Commands

### /mark [label]

Create checkpoint.

**Syntax:**
```bash
/mark                   # Anonymous (numbered)
/mark label             # Named
/mark "multi word"      # Multi-word label (quoted)
```

**Effect:** Creates mark at current position, adds to mark stack, persisted to database, visible in scrollback.

**No limit** on mark count.

### /rewind [label]

Rollback to checkpoint.

**Syntax:**
```bash
/rewind                 # Rewind to most recent mark
/rewind label           # Rewind to specific label
```

**Effect:** Truncates messages to mark position, removes marks after target (including target), rebuilds scrollback, persists rewind operation to DB.

**Error Cases:**
```bash
/rewind
⚠ Error: No marks to rewind to

/rewind nonexistent
⚠ Error: Mark 'nonexistent' not found

Available marks:
  • before-refactor
  • approach-a
```

## Key Bindings

**Ctrl-m:** Create anonymous mark instantly

**Ctrl-Shift-M:** Prompts for descriptive label

**Ctrl-r:** Rewind to most recent mark

**Ctrl-Shift-R:** Fuzzy-find mark to rewind to

## Scrollback Display

Marks appear as visual separators:

```bash
[You] Let's implement OAuth

[Assistant] I'll design the OAuth flow...

⟡ Mark: before-refactor ─────────────────────

[You] Now let's refactor for better error handling

⟡ Checkpoint #1 ─────────────────────────────

[You] Let's try a different approach
```

## Database Persistence

### Mark Messages

Marks stored as special message type:

```sql
INSERT INTO messages (session_id, role, content, created_at)
VALUES (?, 'mark', 'before-refactor', NOW());
```

**Fields:** `role` = "mark", `content` = label (or NULL for anonymous)

### Rewind Messages

Rewinds stored as audit trail:

```sql
INSERT INTO messages (session_id, role, content, created_at)
VALUES (?, 'rewind', '{"target_mark": "before-refactor", "message_count": 15}', NOW());
```

### Viewing History

```bash
/history

Session History:
[1] user: Let's implement OAuth
[2] assistant: I'll design the OAuth flow...
[3] mark: before-refactor
[4] user: Now let's refactor
[5] assistant: I'll refactor...
[6] rewind: checkpoint-1  ← Rewound here
[7] user: Different approach
```

All messages preserved, including rewound-from messages.

## Advanced Patterns

### Nested Exploration

```bash
/mark start
[Try approach A]

/mark approach-a-variant-1
[Explore variant 1]

/rewind approach-a-variant-1
[Back to approach A]

/mark approach-a-variant-2
[Explore variant 2]

/rewind start
[Try completely different approach]
```

### Comparative Analysis

```bash
/mark baseline

[Try approach A, get metrics]

/rewind baseline

[Try approach B, get metrics]

/rewind baseline

[Try approach C, get metrics]

# Choose best based on data
```

### Exploration Journal

```bash
/mark 2025-01-15-start
[Exploration begins]

/mark 2025-01-15-idea-1
[Try first idea]

/mark 2025-01-15-idea-2
[Try second idea]

# Create memory doc with findings
[You] Create memory doc summarizing all approaches

# Rewind to best approach
/rewind 2025-01-15-idea-1
```

## Best Practices

### 1. Mark Before Risky Changes

Always mark before risky refactors or complex changes.

### 2. Use Descriptive Labels

Use labels like `before-database-migration`, `approach-a-async`, `working-implementation` instead of `test`, `temp`, `x`.

### 3. Clean Up Old Marks

After successful completion, rewind to final state. Old marks removed from stack (but preserved in database).

### 4. Document Why

Add context when creating marks so you understand them later.

### 5. Use with Multi-Agent

Each agent has own independent mark stacks with no interference.

## Comparison with Git

| Feature | mark/rewind | git commit/reset |
|---------|-------------|------------------|
| Scope | Conversation | Code |
| Speed | Instant | Fast |
| Granularity | Message-level | Commit-level |
| Persistence | Database | Repository |
| Destructive | No (audit trail) | Depends on reset type |

**Think of mark/rewind as:** Git for your conversation, lightweight exploration tool, complementary to actual git.

**Use both:**
```bash
# Mark conversation
/mark before-refactor

# Git commit code
[You] Commit this working version

# Try experimental approach
[You] Try risky refactor

# Doesn't work, rollback both
/rewind before-refactor
[You] Git reset to working version
```

## Implementation Notes

### Mark Structure

```c
typedef struct ik_mark_t {
    size_t message_index;
    int64_t db_message_id;
    char *label;
    time_t created_at;
} ik_mark_t;
```

### Mark Stack

```c
struct ik_agent_t {
    ik_mark_t **marks;
    size_t mark_count;
};
```

### Creating Mark

```c
res_t create_mark(ik_agent_t *agent, const char *label) {
    ik_message_t *msg = talloc_zero(agent, ik_message_t);
    msg->role = talloc_strdup(msg, "mark");
    msg->content = label ? talloc_strdup(msg, label) : NULL;

    TRY(ik_db_message_insert(agent->db, agent->session_id, msg));

    ik_mark_t *mark = talloc_zero(agent, ik_mark_t);
    mark->message_index = agent->message_count;
    mark->db_message_id = msg->id;
    mark->label = label ? talloc_strdup(mark, label) : NULL;

    agent->marks = talloc_realloc(agent, agent->marks, ik_mark_t *, agent->mark_count + 1);
    agent->marks[agent->mark_count++] = mark;

    ik_scrollback_append_mark(agent->scrollback, label);
    return OK(mark);
}
```

### Rewinding

```c
res_t rewind_to_mark(ik_agent_t *agent, const char *label) {
    ik_mark_t *target = find_mark(agent, label);
    if (!target)
        return ERR(ERR_NOT_FOUND, "Mark not found");

    size_t target_index = target->message_index;
    for (size_t i = target_index; i < agent->message_count; i++)
        talloc_free(agent->messages[i]);

    agent->message_count = target_index;

    for (size_t i = 0; i < agent->mark_count; i++) {
        if (agent->marks[i]->message_index >= target_index) {
            agent->mark_count = i;
            break;
        }
    }

    ik_scrollback_clear(agent->scrollback);
    for (size_t i = 0; i < agent->message_count; i++)
        ik_format_and_append_message(agent->scrollback, agent->messages[i]);

    return OK(target);
}
```

## Future Enhancements

**Mark Annotations:** Add descriptions to marks for better context.

**Visual Mark Tree:** See exploration paths visually.

**Mark Diffing:** Compare state changes between marks.

**Persistent Marks:** Save important marks across sessions.

## Related Documentation

- [multi-agent.md](multi-agent.md) - Multi-agent workflows
- [commands.md](commands.md) - Command reference
- [workflows.md](workflows.md) - Example workflows
