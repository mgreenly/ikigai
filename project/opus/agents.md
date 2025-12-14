# Agents

**Status:** Early design discussion, not finalized.

## Core Concept

All agents share the same fundamental structure: a name, a queue, an inbox, and conversation history. They differ in how they're created, whether users can interact with them, and their lifetime.

## Agent Types

| Type | Created by | User input | Lifetime | Worktree |
|------|------------|------------|----------|----------|
| Main | Startup | Yes | Forever | Main branch |
| Helper | User (`/agent new`) | Yes | Until closed | Optional |
| Sub-agent | Agent (`/push` + `/run`) | No | Until task done | Inherits parent |

### Main Agent

- Always exists, created on startup
- Default name "main" (can be renamed)
- Cannot be closed
- Works on main branch
- User's home base for coordination

### Helper Agents

- Created by user: `/agent new [name] [--worktree]`
- Full user interaction (input + output)
- Persistent until explicitly closed
- Optional git worktree for isolation
- Use cases: feature development, research, experiments

### Sub-Agents

- Created by agents via `/push` + `/run`
- No user input, just initial prompt
- Ephemeral: finish when task done
- Read-only view for user (prompt, tool use, output)
- Results auto-delivered to parent's inbox

## Shared Capabilities

All agent types have:

- **Name** - unique identifier
- **Queue** - can dispatch sub-agents
- **Inbox** - can receive messages
- **Conversation history** - persisted
- **Full tool access** - same capabilities

This means:
- Helper agents can spawn sub-agents
- Sub-agents can spawn sub-sub-agents
- Any agent can message any agent it knows about

## UI Model

All agents are tabs. Navigate with alt-tab or fuzzy search.

**Main/Helper agents:**
- Full interaction: input dialog + output
- User can send messages, use slash commands

**Sub-agents:**
- Read-only view while running
- See: original prompt, tool use, responses, final message
- No input dialog

## Creating Agents

### Helper Agents (User-Created)

```
/agent new research              # No worktree
/agent new oauth-impl --worktree # With worktree
```

With worktree:
- Creates branch
- Creates worktree in `.worktrees/<name>`
- Switches to worktree directory

### Sub-Agents (Agent-Created)

```
/push name=worker "Research the API"
/run
```

See [agent-queues.md](agent-queues.md) for full queue/inbox operations.

## Switching Agents

```
Ctrl-\           # Open agent switcher (fuzzy search)
Ctrl-\ Ctrl-\    # Toggle between current and last
Ctrl-\ m         # Jump to main
/agent <name>    # Switch by name (fuzzy match)
```

## Listing Agents

```
/agent list

Agents:
• main (current) - Branch: main
  Messages: 45 | Last activity: now

• oauth-impl [helper] - Branch: oauth-impl
  Worktree: .worktrees/oauth-impl
  Messages: 23 | Ahead: 3 commits

• research [helper] - No worktree
  Messages: 8

• worker-a [sub-agent] - Running
  Parent: main | Started: 30s ago

• worker-b [sub-agent] - Finished
  Parent: main | Result in inbox
```

## Closing Agents

### Helper Agents

```
/agent close                # Prompt for merge, cleanup worktree
/agent close --no-merge     # Discard work
/agent close --keep-branch  # Merge but keep branch
```

### Sub-Agents

Sub-agents close automatically when their task completes. Their final message is delivered to parent's inbox.

To terminate early:
```
/kill worker-a              # Hard termination
```

## Agent Identity

- Every agent has a unique name/identifier
- Agents only automatically know their own identity
- Parent can pass identities to children via prompts
- Children can pass known identities to their children

```
/push name=coordinator "Spawn workers. Your ID is 'coordinator'. Main is 'main'."
```

## Context Isolation

Each agent has completely isolated context:

- **Independent conversation history** - no pollution between agents
- **Independent working directory** - worktree-backed agents have own directory
- **Independent scrollback** - `/clear` in one doesn't affect others

**Shared across agents:**
- Memory documents (future feature)
- Agent identities (explicitly passed)

## Communication

### Between Parent and Sub-Agent

Results flow automatically via inbox when sub-agent finishes.

Progress updates via explicit `/send`:
```
# Sub-agent can send progress to parent
/send main "Phase 1 complete"
```

### Between Siblings

Sub-agents can coordinate if they know each other's IDs:
```
/push name=analyzer "Analyze code. Share with 'implementer'."
/push name=implementer "Wait for 'analyzer', then implement."
/run
# analyzer does: /send implementer "Found: ..."
# implementer does: /receive from=analyzer
```

### Between Helper Agents

Helper agents can message each other too:
```
# In oauth-impl agent
/send research "Need info on token refresh patterns"

# In research agent
/check from=oauth-impl
# Respond...
/send oauth-impl "Here's what I found: ..."
```

## Workflows

### Feature Development with Worktree

```
/agent new oauth --worktree
# Implement feature in isolation
# Commit changes
/agent close                # Merge to main
```

### Research Without Worktree

```
/agent new research
# Research, create memory docs
/agent close                # No merge needed
```

### Delegated Work

```
# Main agent spawns workers
/push name=a "Research approach A"
/push name=b "Research approach B"
/run
# Continue other work
/receive                    # Get results
```

### Coordinated Pipeline

```
/agent new impl --worktree

# In impl agent, spawn sub-agents
/push name=analyze "Analyze requirements"
/run
/receive

/push name=design "Design based on analysis"
/run
/receive

/push name=test "Write tests for design"
/run
/receive
```

## Related

- [agent-queues.md](agent-queues.md) - Queue and inbox operations
- [vision/multi-agent.md](../vision/multi-agent.md) - Original multi-agent vision (to be consolidated)
