# Todo List Tools

**Status:** Design discussion, not finalized.

## Problem

Claude Code's `TodoWrite` tool requires rewriting the entire list on every update. This wastes context - more items means more tokens per operation. The LLM must track state mentally and reconstruct it each time.

## Solution

Simple queue operations with Redis-inspired naming. System holds state; LLM just issues commands. Constant token cost per operation regardless of list size.

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

## Usage via Two-Path Model

These are slash commands, consistent with ikigai's minimal tool architecture:

```
/slash todo-rpush "Fix authentication bug"
/slash todo-rpush "Update tests"
/slash todo-rpush "Write documentation"
/slash todo-lpop
```

Under the hood, these route to internal tooling with a queue structure in the database attached to the current agent.

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

## Open Questions

1. **Multiple lists?** Single default vs named lists (`todo-rpush --list=refactor "item"`)
2. **Per-agent scope?** Each agent has own list, or shared?
3. **Persistence?** Survives session restart?
4. **Integration with sub-agents?** Can parent see child's list?

## Related

- [sub-agent-tools.md](sub-agent-tools.md) - Sub-agent fork/send/mail tools
- [minimal-tool-architecture.md](minimal-tool-architecture.md) - Tool philosophy
