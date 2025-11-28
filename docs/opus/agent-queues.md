# Agent Queues and Inboxes

**Status:** Early design discussion, not finalized.

## Core Concept

Two primitives for agent orchestration:

1. **Queue** - dispatch work to sub-agents
2. **Inbox** - receive messages from other agents

Every agent has a name, a queue, and an inbox. Work definition is separate from execution, following an I/O multiplexing pattern similar to `select()`/`poll()` in systems programming.

## Entities

| Entity | Lifetime | Queue | Inbox |
|--------|----------|-------|-------|
| Main agent | Persistent | Yes | Yes |
| Sub-agent | Until task complete | Yes | Yes (while alive) |

**User:** The user is not a separate entity. Slash commands act on behalf of the main agent. When user types `/push`, it adds to main agent's queue. When user types `/check`, it checks main agent's inbox.

## Agent Identity

- Every agent has a unique name/identifier
- Main agent has a name (e.g., `main` by default)
- Sub-agents have names (explicit via `name=` or auto-generated)
- Agents only automatically know their own identity
- Parent can pass identities to sub-agents via task prompts
- Sub-agents can pass known identities to their sub-agents

## Queue Operations

### /push

Add a task to the queue.

```
/push [model=MODEL] [name=NAME] PROMPT
```

- `model` - Optional, defaults to current model
- `name` - Optional, auto-generated if not provided
- `PROMPT` - The task description

Returns immediately. No execution.

### /run

Start executing tasks from the queue.

```
/run              # Start all queued tasks
/run N            # Start up to N tasks
```

Tasks begin executing asynchronously. Command returns immediately.

### /queue

Show queue state.

```
/queue

Queued:
  - task-a
  - task-b

Running:
  - task-c (started 30s ago)
  - task-d (started 12s ago)

Finished:
  - task-e
  - task-f
```

Note: Queue only tracks status (queued/running/finished). Results are delivered via inbox.

## Inbox Operations

### /send

Send a message to another agent's inbox.

```
/send AGENT_ID MESSAGE
```

### /receive

Block until message arrives.

```
/receive                    # Wait for any message (FIFO)
/receive --lifo             # Wait for any message (LIFO)
/receive from=ID            # Wait for message from specific agent
/receive from=ID --lifo     # LIFO from specific agent
```

### /check

Non-blocking check for messages.

```
/check                      # Return ready messages (FIFO)
/check --lifo               # Return ready messages (LIFO)
/check from=ID              # Check for message from specific agent
/check from=ID --lifo       # LIFO from specific agent
```

Returns messages if available, otherwise indicates nothing ready.

### /inbox

Show inbox state.

```
/inbox

Messages:
  - from: worker-a (received 30s ago)
  - from: worker-b (received 12s ago)
  - from: worker-c (received 5s ago)
```

## Tool Equivalents

Agents use the same operations via tools:

```json
{"type": "tool_use", "name": "push", "input": {"name": "research", "prompt": "..."}}
{"type": "tool_use", "name": "run", "input": {"count": 3}}
{"type": "tool_use", "name": "send", "input": {"to": "agent-id", "message": "..."}}
{"type": "tool_use", "name": "receive", "input": {"from": "worker-a"}}
{"type": "tool_use", "name": "check", "input": {}}
```

## Result Delivery

**Automatic:** When a sub-agent finishes, its final assistant message is automatically delivered to the parent's inbox. The sub-agent doesn't need to do anything special.

**Sub-agent perspective:**
- Work normally using tools
- Optionally send progress messages via `/send`
- When done, just be done - final message is automatic

**System behavior:**
1. Sub-agent conversation ends (no more tool calls, final text produced)
2. System wraps final assistant message as a message from that agent
3. Message delivered to parent's inbox

**Parent perspective:**
- Push tasks, run them
- Check inbox for results (and any explicit messages)
- Results arrive automatically

## Task Lifecycle

```
queued → running → finished
                      ↓
            (result auto-delivered to parent inbox)
```

## Message Lifecycle

```
sent → in recipient inbox → received/checked (collected)
```

Messages persist in inbox until explicitly collected via `/receive` or `/check`.

## Result Format

Results arrive as messages. The message content is the sub-agent's final output.

On failure, the message indicates the error:

```json
{
  "from": "task-name",
  "success": false,
  "error": "...",
  "partial_output": "..."
}
```

Failed tasks still deliver messages. Errors don't get lost.

## Workflows

### Sequential (blocking)

```
/push "Research the API"
/run
/receive
# Use result, then continue
/push "Implement based on research"
/run
/receive
```

### Parallel

```
/push name=a "Research approach A"
/push name=b "Research approach B"
/push name=c "Research approach C"
/run
/receive              # Get first result
/receive              # Get second result
/receive              # Get third result
```

### Async with continued work

```
/push name=slow "Long running research"
/run
# Continue working on other things...
# ...
/check               # Anything ready? Not yet.
# ...
/check               # Anything ready? Yes!
```

### Batched with concurrency limit

```
/push "Refactor file1.c"
/push "Refactor file2.c"
/push "Refactor file3.c"
/push "Refactor file4.c"
/push "Refactor file5.c"
/run 2               # Run 2 at a time
# Collect as they finish
/receive
/receive
/receive
/receive
/receive
```

### Out-of-order collection

```
/push name=a "Task A"
/push name=b "Task B"
/push name=c "Task C"
/run
# All finish
/receive from=b      # Get b's result specifically
/receive from=c      # Get c's result specifically
/receive             # Get remaining (a)
```

### Progress updates

```
/push name=worker "Long task. My ID is 'main', send progress updates."
/run
# Worker sends: /send main "Phase 1 complete"
/check               # Get progress update
# Worker sends: /send main "Phase 2 complete"
/check               # Get progress update
# Worker finishes
/receive             # Get final result (auto-delivered)
```

### Sibling coordination

```
/push name=analyzer "Analyze the codebase. Share findings with 'implementer'."
/push name=implementer "Wait for analysis from 'analyzer', then implement."
/run
# analyzer does: /send implementer "Found patterns: ..."
# implementer does: /receive from=analyzer, then works
/receive             # Get analyzer result
/receive             # Get implementer result
```

## Design Decisions

**Sub-agent context:** Blank. Sub-agents start with only their task prompt. No parent system message, no history. They read what their prompt tells them to read.

**Nesting depth:** No practical limit. Sub-agents can spawn sub-agents freely.

**Sub-agent tools:** Full access to all tools, same as parent.

**Queue manipulation:** Inspect and remove for now. Future expansion possible (reorder, edit).

**Timeout/cancellation:** Background implementation must support hard abrupt termination. Timeout is a parameter on `/push` or `/run` that triggers hard termination.

```
/push timeout=300 "Long task"    # 5 minute timeout
```

**Inbox limits:** No artificial limits. Underlying system capabilities determine practical limits.

**Progress visibility:** For now, use `/queue` and `/inbox` commands to check status. No special UI.

Future direction: treat all agents (main and sub-agents) like browser tabs. Alt-tab through the list. For sub-agents, no input dialog but can view:
- Original prompt
- Tool use and responses
- Normal output
- Final message

## Related

- [task_queue.md](../task_queue.md) - Task document patterns for complex decomposition
- [vision/multi-agent.md](../vision/multi-agent.md) - Persistent parallel agents (different concept)
