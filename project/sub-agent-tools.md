# Sub-Agent Tools

**Status:** Design discussion, not finalized.

## Philosophy

Provide simple, composable primitives that power users can build complex workflows around. Tools must be straightforward enough that LLMs never get confused about how/when to use them.

Design principles:
- **Minimal decision paths** - LLM makes fewer choices
- **Consistent interface** - everything looks the same
- **Composable** - simple primitives combine into complex patterns
- **Stupid simple** - if the LLM can get confused, simplify further

## The Two-Path Model

LLM sees only two ways to take action:

| Path | Purpose |
|------|---------|
| `/bash` | Shell commands |
| `/slash <command>` | Everything else |

Sub-agent operations (`fork`, `send`, `check-mail`, `kill`) are slash commands. Under the hood, some slash commands route to internal tool implementations, but the LLM doesn't know or care.

**Benefits:**
- Minimal cognitive load ("is this a tool or command?" - doesn't matter)
- Consistent syntax for all operations
- Easy to extend (new commands, same pattern)
- System prompt stays simple

## Commands

### /slash fork [PROMPT]

Create a child agent.

| Parameter | Behavior |
|-----------|----------|
| No prompt | Child inherits parent's full context |
| With prompt | Child starts fresh with only that prompt |

**Returns:** Child's UUID

**Child receives:** Parent's UUID

**Behavior:** Non-blocking. Parent continues immediately. Child runs concurrently.

### /slash send UUID MESSAGE

Send a message to another agent.

**Requirements:** Must know the recipient's UUID.

**Primary uses:**
- Child reporting to parent
- Parent sending follow-up instructions
- Sibling coordination (if UUIDs shared)

### /slash check-mail

Check inbox for messages.

**Returns:** Pending messages with sender UUIDs.

**Also:** System injects fake user message "got mail from UUID" when agent goes idle with pending mail. LLM can then decide whether to check.

### /slash kill UUID

Terminate a child agent.

## Child Lifecycle

```
1. Parent: /slash fork "do X"
   └─▶ Child starts, receives parent UUID

2. Child works autonomously

3. Child completes, sends structured message to parent:
   {"status": "idle", "success": true/false, "summary": "..."}

4. Child sits idle, awaiting instructions

5. Parent either:
   - /slash kill <UUID> to terminate
   - /slash send <UUID> "do more" to continue
```

Child doesn't terminate on completion - it idles. This allows reuse without re-forking.

## Structured Completion Message

When a child completes its work, it sends:

```json
{
  "status": "idle",
  "success": true,
  "summary": "What I accomplished and key findings..."
}
```

On failure:

```json
{
  "status": "idle",
  "success": false,
  "error": "What went wrong...",
  "partial": "Any progress made before failure..."
}
```

Parent knows from one message: completion state, outcome, and results.

## System Prompt Guidance (Draft)

The following should be included in agent system prompts:

---

### Sub-Agents

You can fork child agents to work in parallel.

**Commands:**

```
/slash fork [PROMPT]     Create child (inherit context or fresh start)
/slash send UUID MSG     Send message to any agent
/slash check-mail        Check your inbox
/slash kill UUID         Terminate a child
```

**When to fork:**
- Task is self-contained, doesn't need ongoing collaboration
- You want parallel exploration of multiple approaches
- Work benefits from isolated context

**When NOT to fork:**
- Task requires back-and-forth with you
- You need results immediately to continue
- Task is simple enough to do yourself

**Child lifecycle:**
1. Fork creates child, returns UUID
2. Child works, then sends completion message with status/summary
3. Child idles until you kill it or send more work

**Coordinating children:**
- Track UUIDs and what each child is doing
- Check mail when notified or periodically
- Share sibling UUIDs in prompts if children need to coordinate

**Example - parallel research:**
```
/slash fork "Research approach A for the auth system"
/slash fork "Research approach B for the auth system"
# Continue other work...
# Later, check mail for results
/slash check-mail
```

**Example - sibling coordination:**
```
/slash fork "Analyze the database schema. Sibling UUID for API analysis: <UUID>"
```

---

## Open Questions

1. **Prompt structure guidance** - Should we document what makes a good fork prompt?

2. **Mail notification wording** - Exact format of the "got mail" fake user message?

3. **Common patterns** - Document fan-out/gather, pipeline, etc.? Or keep minimal?

4. **Error handling** - What happens if fork fails? If send targets invalid UUID?

5. **Concurrency limits** - Maximum children? Resource management?

## Related

- [minimal-tool-architecture.md](minimal-tool-architecture.md) - Tool philosophy

**Note:** The `opus/` folder contains earlier design explorations that have been superseded by ongoing work in the development branch.
