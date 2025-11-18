# Mark and Rewind

**Checkpoint and rollback for iterative exploration.** Like git for your conversation.

## The Problem

When working with AI agents, you often:
- Try multiple approaches
- Explore risky refactors
- Test different solutions
- Need to backtrack from dead ends

**Without mark/rewind:**
1. AI suggests approach A
2. You realize it's not working
3. Start new conversation from scratch
4. Re-establish context
5. Try approach B

**With mark/rewind:**
1. `/mark` - Checkpoint before exploration
2. Try approach A
3. Doesn't work
4. `/rewind` - Instant rollback to checkpoint
5. Try approach B from same starting point

## Core Concepts

### Marks

**What:** Checkpoints in your conversation history

**Where:** Stored in session message array

**When:** Created with `/mark [label]`

**Properties:**
- Named or anonymous
- Stack-based (LIFO)
- Multiple marks allowed
- Visible in scrollback as separators

### Rewind

**What:** Rollback to a checkpoint

**How:** Truncate message array to mark position

**Effect:**
- Restores conversation context
- Removes messages after mark
- Rebuilds scrollback
- Non-destructive (DB keeps everything)

### Non-Destructive

**Critical:** Nothing is deleted from database

- All messages preserved
- Complete audit trail
- Can view rewound-from messages with `/history`
- Rewind operations themselves are logged

## Quick Start

### Anonymous Mark

```bash
[You]
Let's try refactoring this module

/mark

⟡ Checkpoint #1 created

[You]
Actually, let's use a different approach...
[Conversation continues...]

[You]
Wait, that didn't work. Go back.

/rewind

Rewound to Checkpoint #1 ⟡

[You]
Let's try a different refactoring approach...
```

### Named Mark

```bash
[You]
Let's explore different OAuth implementations

/mark before-oauth

⟡ Mark: before-oauth

[You]
Try JWT-based approach
[Conversation explores JWT...]

[You]
That's too complex. Rewind.

/rewind before-oauth

Rewound to Mark: before-oauth ⟡

[You]
Try session-based approach instead
```

### Multiple Marks

```bash
/mark approach-a
[Try approach A...]

/mark approach-b
[Try approach B...]

/mark approach-c
[Try approach C...]

# Rewind to specific mark
/rewind approach-a

# Or rewind step by step
/rewind  # Back to approach-b
/rewind  # Back to approach-a
/rewind  # Back to before all approaches
```

## Use Cases

### Risky Refactors

```bash
[You]
Let's refactor the error handling module

/mark before-refactor

[You]
Refactor all error handling to use ik_result_t

[Assistant]
I'll refactor src/error.c...
[Makes changes]

[You]
Hmm, this breaks too much existing code. Rewind.

/rewind before-refactor

[You]
Let's take a more incremental approach instead
```

### Exploring Alternatives

```bash
[You]
I'm not sure which data structure to use

/mark before-data-structure

[You]
Try using a hash table

[Assistant]
I'll implement with a hash table...
[Implementation discussion...]

[You]
What's the performance?

[Assistant]
O(1) lookup but high memory usage...

/rewind before-data-structure

[You]
Try using a binary search tree instead

[Assistant]
I'll implement with a BST...
[Different implementation...]

[You]
What's the performance?

[Assistant]
O(log n) lookup but lower memory usage...

# Compare, choose best approach
```

### Iterative Design

```bash
/mark v1

[You]
Design a simple API with basic auth

[Assistant]
Here's a simple design...

/mark v2

[You]
Add OAuth support

[Assistant]
Extended design with OAuth...

/mark v3

[You]
Add rate limiting

[Assistant]
Design with OAuth and rate limiting...

# Review versions
/rewind v2
[Review v2 design]

/rewind v3
[Return to latest]

# Or start over
/rewind v1
[You]
Actually, let's redesign from scratch
```

### Avoiding Dead Ends

```bash
[You]
Debug this memory leak

/mark before-debugging

[Assistant]
Let me analyze... I think it's in the parser module
[Long debugging session...]
[Turns out to be wrong approach]

/rewind before-debugging

[Assistant]
Let me analyze from the beginning...
[No wasted context from dead end]
[Fresh debugging approach]
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
/rewind     # Rewind to [a]
```

**After rewind:**

```bash
/mark a
/mark b
/rewind     # Now at [a]

Mark stack: [a] → [current]
# Mark [b] is removed from stack
# (But preserved in database)

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

**Examples:**
```bash
/mark
⟡ Checkpoint #1

/mark before-refactor
⟡ Mark: before-refactor

/mark "before big changes"
⟡ Mark: before big changes
```

**Effect:**
- Creates mark at current position
- Adds to mark stack
- Persisted to database
- Visible in scrollback

**Limit:** No hardcoded limit on mark count

### /rewind [label]

Rollback to checkpoint.

**Syntax:**
```bash
/rewind                 # Rewind to most recent mark
/rewind label           # Rewind to specific label
/rewind "multi word"    # Multi-word label (quoted)
```

**Examples:**
```bash
/rewind
Rewound to Checkpoint #3 ⟡

/rewind before-refactor
Rewound to Mark: before-refactor ⟡

/rewind "before big changes"
Rewound to Mark: before big changes ⟡
```

**Effect:**
- Truncates messages to mark position
- Removes marks after target (including target)
- Rebuilds scrollback
- Persists rewind operation to DB

**Error Cases:**
```bash
/rewind
⚠ Error: No marks to rewind to

/rewind nonexistent
⚠ Error: Mark 'nonexistent' not found

Available marks:
  • before-refactor
  • approach-a
  • checkpoint-1
```

## Key Bindings

### Quick Mark (Ctrl-m)

Create anonymous mark instantly:

```bash
[Conversation flowing...]
Ctrl-m
⟡ Checkpoint #1 created
[Continue conversation...]
```

**Perfect for:** Quick checkpoints without thinking about names

### Named Mark (Ctrl-Shift-M)

Prompts for descriptive label:

```bash
Ctrl-Shift-M

┌─────────────────────────────────────┐
│ Mark Label:                         │
│ before-refactor_                    │
└─────────────────────────────────────┘

Enter → ⟡ Mark: before-refactor
```

**Perfect for:** Important checkpoints you'll reference later

### Quick Rewind (Ctrl-r)

Rewind to most recent mark:

```bash
[Deep in conversation...]
Ctrl-r
Rewound to Checkpoint #1 ⟡
```

**Perfect for:** Simple undo

### Named Rewind (Ctrl-Shift-R)

Fuzzy-find mark to rewind to:

```bash
Ctrl-Shift-R

┌─────────────────────────────────────┐
│ Rewind to:                          │
│                                     │
│ > before-refactor                   │
│   approach-a                        │
│   checkpoint-1 (anonymous)          │
│   checkpoint-2 (anonymous)          │
└─────────────────────────────────────┘

Type to filter, Enter to rewind
```

**Perfect for:** Jumping back to specific checkpoint

## Scrollback Display

Marks appear as visual separators:

```bash
[You]
Let's implement OAuth

[Assistant]
I'll design the OAuth flow...

⟡ Mark: before-refactor ─────────────────────

[You]
Now let's refactor for better error handling

[Assistant]
I'll refactor the error handling...

⟡ Checkpoint #1 ─────────────────────────────

[You]
Let's try a different approach
```

**Visual hierarchy:**
- Named marks: Full label displayed
- Anonymous marks: Numbered
- Separator line for clarity

## Database Persistence

### Mark Messages

Marks stored as special message type:

```sql
INSERT INTO messages (session_id, role, content, created_at)
VALUES (?, 'mark', 'before-refactor', NOW());
```

**Fields:**
- `role` = "mark"
- `content` = label (or NULL for anonymous)
- Message ID used to identify mark position

### Rewind Messages

Rewinds stored as audit trail:

```sql
INSERT INTO messages (session_id, role, content, created_at)
VALUES (?, 'rewind', '{"target_mark": "before-refactor", "message_count": 15}', NOW());
```

**Fields:**
- `role` = "rewind"
- `content` = JSON with rewind details
- Preserves audit trail

### Viewing History

```bash
/history

Session History:
[1] user: Let's implement OAuth
[2] assistant: I'll design the OAuth flow...
[3] mark: before-refactor
[4] user: Now let's refactor
[5] assistant: I'll refactor...
[6] mark: checkpoint-1
[7] user: Try different approach
[8] rewind: checkpoint-1  ← Rewound here
[5] assistant: I'll refactor...  ← Back to here
[9] user: Different approach
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
[Back to beginning]

[Try completely different approach]
```

### Mark Before Risky Operations

```bash
# Always mark before risky operations
/mark before-major-refactor
[You]
Refactor the entire module structure

/mark before-dependency-upgrade
[You]
Upgrade all dependencies

/mark before-architecture-change
[You]
Change to event-driven architecture
```

### Comparative Analysis

```bash
/mark baseline

[You]
Implement with approach A
[Get performance numbers]

/rewind baseline

[You]
Implement with approach B
[Get performance numbers]

/rewind baseline

[You]
Implement with approach C
[Get performance numbers]

# Then choose best based on data
```

### Exploration Journal

```bash
# Document exploration path
/mark 2025-01-15-start
[Exploration begins]

/mark 2025-01-15-idea-1
[Try first idea]

/mark 2025-01-15-idea-2
[Try second idea]

# Create memory doc with findings
[You]
Create memory doc summarizing all approaches tried

# Rewind to best approach
/rewind 2025-01-15-idea-1
```

## Best Practices

### 1. Mark Before Risky Changes

```bash
# Good
/mark before-refactor
[You]
Refactor this complex module

# Risky
[You]
Refactor this complex module
# (No mark - can't easily undo)
```

### 2. Use Descriptive Labels

```bash
# Good
/mark before-database-migration
/mark approach-a-async
/mark working-implementation

# Less useful
/mark test
/mark temp
/mark x
```

### 3. Clean Up Old Marks

```bash
# After successful completion
/rewind approach-a-async
/mark final-implementation

# Old marks removed from stack
# (But preserved in database)
```

### 4. Document Why

```bash
# Good
[You]
This is a risky refactor that might break things

/mark before-risky-refactor

# Context clear when you rewind later
```

### 5. Use with Multi-Agent

```bash
# Each agent has own marks
[Agent: main]
/mark main-checkpoint

[Agent: oauth-impl]
/mark oauth-checkpoint

# Independent mark stacks
# No interference
```

## Comparison with Git

| Feature | mark/rewind | git commit/reset |
|---------|-------------|------------------|
| Scope | Conversation | Code |
| Speed | Instant | Fast |
| Granularity | Message-level | Commit-level |
| Persistence | Database | Repository |
| Destructive | No (audit trail) | Soft reset: No, Hard reset: Yes |
| UI | Built-in | External |

**Think of mark/rewind as:**
- Git for your conversation
- Lightweight exploration tool
- Complementary to actual git

**Use both:**
```bash
# Mark conversation
/mark before-refactor

# Git commit code
[You]
Commit this working version
[Assistant commits]

# Try experimental approach
[You]
Try risky refactor

# Doesn't work, rollback both
/rewind before-refactor
[You]
Git reset to working version
```

## Implementation Notes

### Mark Structure

```c
typedef struct ik_mark_t {
    size_t message_index;       // Position in session_messages[]
    int64_t db_message_id;      // Database ID
    char *label;                // User label (or NULL)
    time_t created_at;          // Timestamp
} ik_mark_t;
```

### Mark Stack

```c
struct ik_agent_t {
    // ...
    ik_mark_t **marks;          // Array of marks (stack)
    size_t mark_count;          // Current count
};
```

### Creating Mark

```c
res_t create_mark(ik_agent_t *agent, const char *label) {
    // 1. Create mark message
    ik_message_t *msg = talloc_zero(agent, ik_message_t);
    msg->role = talloc_strdup(msg, "mark");
    msg->content = label ? talloc_strdup(msg, label) : NULL;

    // 2. Persist to database
    TRY(ik_db_message_insert(agent->db, agent->session_id, msg));

    // 3. Add to mark stack
    ik_mark_t *mark = talloc_zero(agent, ik_mark_t);
    mark->message_index = agent->message_count;
    mark->db_message_id = msg->id;
    mark->label = label ? talloc_strdup(mark, label) : NULL;
    mark->created_at = time(NULL);

    // 4. Append to stack
    agent->marks = talloc_realloc(agent, agent->marks,
                                   ik_mark_t *, agent->mark_count + 1);
    agent->marks[agent->mark_count++] = mark;

    // 5. Display in scrollback
    ik_scrollback_append_mark(agent->scrollback, label);

    return OK(mark);
}
```

### Rewinding

```c
res_t rewind_to_mark(ik_agent_t *agent, const char *label) {
    // 1. Find target mark
    ik_mark_t *target = find_mark(agent, label);
    if (!target)
        return ERR(ERR_NOT_FOUND, "Mark not found");

    // 2. Truncate messages
    size_t target_index = target->message_index;
    for (size_t i = target_index; i < agent->message_count; i++) {
        talloc_free(agent->messages[i]);
    }
    agent->message_count = target_index;

    // 3. Remove marks at and after target
    for (size_t i = 0; i < agent->mark_count; i++) {
        if (agent->marks[i]->message_index >= target_index) {
            talloc_free(agent->marks[i]);
            agent->mark_count = i;
            break;
        }
    }

    // 4. Rebuild scrollback
    ik_scrollback_clear(agent->scrollback);
    for (size_t i = 0; i < agent->message_count; i++) {
        ik_format_and_append_message(agent->scrollback, agent->messages[i]);
    }

    // 5. Persist rewind operation
    ik_message_t *rewind_msg = create_rewind_message(target);
    TRY(ik_db_message_insert(agent->db, agent->session_id, rewind_msg));

    return OK(target);
}
```

## Future Enhancements

### Mark Annotations

```bash
/mark before-refactor "This is working but slow"

# Later
/marks list

Marks:
⟡ before-refactor - "This is working but slow"
  Created: 10 minutes ago
  Messages after: 15
```

### Visual Mark Tree

```bash
/marks tree

⟡ start
  ├─⟡ approach-a
  │  ├─⟡ variant-1 (rewound)
  │  └─⟡ variant-2 (rewound)
  └─⟡ approach-b
     └─⟡ current (you are here)
```

### Mark Diffing

```bash
/mark diff before-refactor

Changes since mark 'before-refactor':
  • 15 messages exchanged
  • 3 files modified
  • 2 commits made
  • 1 memory doc created
```

### Persistent Marks Across Sessions

```bash
# Save important marks permanently
/mark save before-refactor

# Load in new session
/mark load before-refactor
```

## Related Documentation

- [multi-agent.md](multi-agent.md) - Multi-agent workflows
- [commands.md](commands.md) - Command reference
- [workflows.md](workflows.md) - Example workflows
- [../v1-conversation-management.md](../v1-conversation-management.md) - Implementation details
