# Sub-Agent Tools

**Status:** Design finalized. See `.claude/data/scratch.md` for full design decisions (rel-11).

## Philosophy

Provide simple, composable primitives that power users can build complex workflows around. Tools must be straightforward enough that LLMs never get confused about how/when to use them.

Design principles:
- **Minimal decision paths** - LLM makes fewer choices
- **Consistent interface** - everything looks the same
- **Composable** - simple primitives combine into complex patterns
- **Stupid simple** - if the LLM can get confused, simplify further

## Internal Tools

Each tool gets its own registry entry with a precise schema. The LLM sees them in a single alphabetized list alongside external tools — no distinction.

| Tool name (LLM sees) | Backed by command | Purpose |
|---|---|---|
| `fork` | `/fork` | Create a child agent |
| `kill` | `/kill` | Terminate an agent |
| `mail_send` | `/mail-send` | Send message to another agent |
| `mail_check` | `/mail-check` | Check inbox |
| `mail_read` | `/mail-read` | Read a specific message |
| `mail_delete` | `/mail-delete` | Delete a message |
| `mail_filter` | `/mail-filter` | Filter messages by criteria |

## Human-Only Commands

These are NOT internal tools. They never appear in the tool list.

| Command | Purpose |
|---|---|
| `/capture` | Enter capture mode for composing sub-agent tasks |
| `/cancel` | Exit capture mode without forking |

## Slash Command vs Tool Behavioral Differences

Slash commands are user-initiated on the main thread. Internal tools are agent-initiated on a worker thread.

### fork

**Slash command (human):**
- `/fork` with active capture — creates child with captured content, switches UI to child
- `/fork` without capture — creates idle child, switches UI to child
- No prompt argument. Human provides the task via `/capture` ... `/fork` or by typing after switching.

**Tool (agent):**
- `fork(prompt: "...")` — creates child with full parent context + prompt, no UI switch
- `prompt` is required. Agent must tell the child what to do.

### kill

**Slash command:** Kills target, switches UI away if it was the current agent.

**Tool:** Kills target, returns success/failure, no UI side effects.

### Mail commands

Behavioral difference is only rendering (scrollback vs JSON return). No control flow divergence.

## Capture Mode

**Problem**: When a human types a task for a sub-agent, the parent's LLM would execute it.

**Solution**: `/capture` enters capture mode. User input is displayed in scrollback and persisted to DB but never sent to the parent's LLM. `/fork` ends capture and creates the child with the captured content. `/cancel` ends capture without forking.

```
/capture                          ← event rendered in scrollback
Enumerate all the *.md files,     ← rendered in scrollback, not sent to LLM
count their words and build       ← rendered in scrollback, not sent to LLM
a summary table.                  ← rendered in scrollback, not sent to LLM
/fork                             ← child created with captured content
```

- Each input persisted with `kind="capture"` — excluded from LLM conversation
- `/cancel` ends capture without forking; captured text stays in scrollback
- Captured content is immutable once rendered

## Child Lifecycle

```
1. Parent: fork(prompt: "do X")
   └─▶ Child starts, receives parent UUID

2. Child works autonomously

3. Child completes, sends structured message to parent:
   {"status": "idle", "success": true/false, "summary": "..."}

4. Child sits idle, awaiting instructions

5. Parent either:
   - kill(uuid) to terminate
   - mail_send(uuid, "do more") to continue
```

Child doesn't terminate on completion - it idles. This allows reuse without re-forking.

## Open Questions

1. **Prompt structure guidance** - Should we document what makes a good fork prompt?
2. **Mail notification wording** - Exact format of the "got mail" fake user message?
3. **Common patterns** - Document fan-out/gather, pipeline, etc.? Or keep minimal?
4. **Error handling** - What happens if fork fails? If kill targets invalid UUID?
5. **Concurrency limits** - Maximum children? Resource management?

## Related

- [external-tool-architecture.md](external-tool-architecture.md) - External tools architecture (unified registry)
- `.claude/data/scratch.md` - Full rel-11 design decisions (threading, dispatch, DB, registry changes)
