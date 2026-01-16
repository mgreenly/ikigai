# Task Tool

**Status:** Design discussion, not finalized.

**Implementation:** External tool (rel-12)

**Dependencies:** Requires rel-11 (Shared Files) for `ik://` URI scheme

## Problem

Claude Code's `TodoWrite` tool requires rewriting the entire list on every update. This wastes context - more items means more tokens per operation. The LLM must track state mentally and reconstruct it each time.

## Solution

Simple queue operations with Redis-inspired naming implemented as an external tool. System holds state; LLM just issues commands. Constant token cost per operation regardless of list size.

The `task` external tool takes an `agent_id` parameter and stores task details in `ik://system/agents/{agent_id}/tasks.json`.

## Philosophy

- **Minimal operations** - only what's needed, no extras
- **Textbook semantics** - use well-known patterns
- **Redis-inspired naming** - battle-tested, widely recognized
- **L/R convention** - left (front), right (back)

## Commands

| Command | Action | Redis equiv |
|---------|--------|-------------|
| `todo-lpush "item"` | Add to front | LPUSH |
| `todo-rpush "item"` | Add to back | RPUSH |
| `todo-lpop` | Pop from front | LPOP |
| `todo-rpop` | Pop from back | RPOP |
| `todo-lpeek` | View front | LINDEX 0 |
| `todo-rpeek` | View back | LINDEX -1 |
| `todo-list` | View all items | LRANGE |
| `todo-count` | Get item count | LLEN |

Eight operations. Full symmetric deque with inspection.

### Naming Convention

```
L = left (front of list)
R = right (back of list)
```

Once learned, the pattern is intuitive and symmetric.

## Implementation as External Tool

The `task` tool is implemented as an external tool that:
- Takes `agent_id` as a required parameter
- Stores data in `ik://system/agents/{agent_id}/tasks.json`
- Supports Redis-style deque operations
- Returns JSON responses

Example tool calls:
```json
{"tool": "task", "agent_id": "abc123", "operation": "rpush", "item": "Fix authentication bug"}
{"tool": "task", "agent_id": "abc123", "operation": "lpop"}
{"tool": "task", "agent_id": "abc123", "operation": "list"}
```

## What's Covered

Full symmetric deque operations:
- Add to either end (lpush, rpush)
- Remove from either end (lpop, rpop)
- Peek at either end (lpeek, rpeek)
- View all items (list)
- Get count (count)

## What's Not Covered

- Insert at arbitrary index (middle insertion)
- Remove at arbitrary index (middle deletion)
- Get by arbitrary index

For a task queue, this is sufficient. Work happens at the ends, not the middle. If reprioritization is needed, use `list` to see state, then rebuild.

## Comparison to TodoWrite

| Aspect | TodoWrite | Todo Tools |
|--------|-----------|------------|
| State management | LLM rewrites full list | System holds state |
| Token cost | O(n) per operation | O(1) per operation |
| Mental overhead | Track and reconstruct | Just issue commands |
| Operations | Single write operation | Six focused operations |

## System Prompt Guidance (Draft)

```markdown
### Task List

Manage your work queue with Redis-style list commands.

L = left (front), R = right (back)

Commands:
/slash todo-lpush "item"   Add to front (urgent)
/slash todo-rpush "item"   Add to back (normal)
/slash todo-lpop           Pop from front
/slash todo-rpop           Pop from back
/slash todo-lpeek          View front without removing
/slash todo-rpeek          View back without removing
/slash todo-list           View all items
/slash todo-count          Get count

Typical workflow:
1. Add tasks: todo-rpush for normal, todo-lpush for urgent
2. Check next: todo-lpeek to see what's up
3. Work: todo-lpop to take next task
4. Review: todo-list or todo-count for progress
```

## Design Decisions

1. **Per-agent scope:** Each agent has its own task list via `agent_id` parameter
2. **Persistence:** Stored in shared file system (`ik://system/agents/{agent_id}/tasks.json`)
3. **Integration with sub-agents:** Parent can access child's list by passing child's agent_id
4. **Single list per agent:** One default list; multiple lists not in initial scope

## Related

- [shared-files.md](shared-files.md) - Shared file system (dependency)
- [sub-agent-tools.md](sub-agent-tools.md) - Sub-agent fork/send/mail tools
- [external-tool-architecture.md](external-tool-architecture.md) - External tools architecture
