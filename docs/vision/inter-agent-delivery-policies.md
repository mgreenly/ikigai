# Inter-Agent Communication: Delivery Policies & Mechanics

This document covers agent message handling policies and the simple tool-based message mechanics.

See also:
- [inter-agent-communication.md](inter-agent-communication.md) - Overview and index
- [inter-agent-core.md](inter-agent-core.md) - Core concepts and messaging
- [inter-agent-handlers.md](inter-agent-handlers.md) - Message handler system
- [inter-agent-delivery-timing.md](inter-agent-delivery-timing.md) - Delivery timing approaches
- [inter-agent-implementation.md](inter-agent-implementation.md) - Use cases and implementation

## Agent Message Handling Policy

**Key Insight:** Each agent controls how it handles incoming messages via a policy flag.

### Handling Modes

**1. `interrupt` - Interrupt Me**

Messages are delivered immediately, potentially interrupting active conversation:

```bash
/agent config message-handling interrupt

# Now when messages arrive:
# - Handler runs immediately
# - User sees notification/injection into conversation
# - Best for coordinating agents that need real-time updates
```

**Use case:** Root coordinating agent that needs to stay aware of all helper agents' status.

**2. `queue` - Wait Until Idle**

Messages queue until agent is idle (no active conversation for N seconds):

```bash
/agent config message-handling queue --idle-after=30s

# Messages queue silently
# When user stops typing for 30s, handlers run
# User sees: "Handled 3 messages while you were idle"
```

**Use case:** Implementation agents focused on deep work, don't want interruptions.

**3. `fork` - Fork Me**

Spawn a new agent instance to handle the message while original continues:

```bash
/agent config message-handling fork

# When message arrives:
# 1. System creates oauth-impl-fork-1 (temporary agent)
# 2. Fork handles the message with its own context
# 3. Fork can respond to sender
# 4. Fork auto-merges insights back to parent or auto-closes
# 5. Parent agent continues uninterrupted
```

**Use case:** Agents that need to handle messages without disrupting main work.

### How Fork Works

When a message arrives to an agent with `fork` policy:

```
┌──────────────────────────────────────────┐
│  oauth-impl (original agent)             │
│  Status: Active conversation with user   │
│  Working on: Token refresh implementation│
└──────────────────────────────────────────┘
                    │
                    │ Message arrives from testing-agent
                    │
                    ▼
┌──────────────────────────────────────────┐
│  oauth-impl-fork-1 (spawned)             │
│  Status: Handling message                │
│  Context: Message + sender info          │
│  Lifetime: Temporary (auto-closes)       │
└──────────────────────────────────────────┘
                    │
                    │ 1. Runs handler
                    │ 2. Responds to sender
                    │ 3. Creates memory doc if needed
                    │ 4. Closes itself
                    ▼
           Message handled ✓
```

**Fork characteristics:**
- **Temporary**: Auto-closes after handling message (or after N minutes idle)
- **Isolated**: Own conversation context, doesn't pollute parent
- **No worktree**: Forks don't create git worktrees (unless needed)
- **Limited scope**: Can read parent's state but can't modify it directly
- **Communication**: Can send messages back to parent with findings

**Fork naming:**
- `oauth-impl-fork-1`, `oauth-impl-fork-2`, etc.
- Or semantic: `oauth-impl-testing-response`, `oauth-impl-review-handler`

**Fork outcomes:**

```yaml
# After handling message, fork can:
outcomes:
  - auto_close: true                          # Default: just close
  - create_memory: "findings-from-testing"    # Document findings
  - send_to_parent: "FYI: handled test results" # Notify parent
  - escalate: main                            # Forward to root agent
```

### Hybrid Policies

Agents can have sophisticated policies:

```yaml
# .agents/oauth-impl.yml
message_handling:
  default: queue           # Default behavior

  # Override for specific senders
  from:
    main: interrupt        # Root agent can interrupt
    testing-*: fork        # Testing agents trigger fork

  # Override for specific priorities
  priority:
    urgent: interrupt      # Urgent always interrupts
    high: fork            # High priority forks
    normal: queue         # Normal queues

  # Override for specific tags
  tags:
    blocker: interrupt    # Blockers interrupt
    fyi: queue           # FYIs queue

  # Override for specific times
  schedule:
    weekdays:
      9am-5pm: fork       # During work hours, fork
      5pm-9am: queue      # Off hours, queue
    weekends: queue       # Weekends always queue
```

### Fork vs Queue vs Interrupt Trade-offs

| Aspect | Interrupt | Queue | Fork |
|--------|-----------|-------|------|
| User disruption | High | None | None |
| Real-time response | Immediate | Delayed | Immediate |
| Context preservation | Breaks flow | Preserves flow | Preserves flow |
| Resource usage | Low | Low | Medium (new agent) |
| Complexity | Low | Low | High |
| Best for | Coordination | Deep work | Async collaboration |

### User Control

User can override agent's default policy:

```bash
# Override current agent's policy temporarily
/messages mode queue          # Queue until I say otherwise
/messages mode interrupt      # Allow interruptions
/messages mode fork           # Fork for all messages

# Reset to agent's configured default
/messages mode default

# Per-sender override (overrides agent policy)
/messages from testing-agent queue
/messages from main interrupt-urgent-only

# Global quiet mode (highest priority, overrides everything)
/messages quiet              # Queue everything until `/messages resume`
```

### Visual Indicators

Status line shows message state:

```
[Agent: oauth-impl | 2 pending] [Branch: feature/oauth]
                    ^^^^^^^^^^
                    2 unhandled messages

[Agent: oauth-impl | 📨] [Branch: feature/oauth]
                    ^^
                    New message notification

[Agent: oauth-impl | 🔴 urgent] [Branch: feature/oauth]
                    ^^^^^^^^^^^
                    Urgent message waiting
```

### The Ideal Experience

**For low-priority messages:**
1. Message arrives while user is working
2. Silent queue, tiny indicator in status line
3. When conversation pauses (user stops typing for 30s), handler runs
4. User sees: "Handled message from research-agent while you were thinking"

**For urgent messages:**
1. Message arrives with urgent priority
2. Non-modal notification appears: `[📨 URGENT from testing-agent: test failures]`
3. User can click to handle or dismiss
4. If dismissed, queues for later but stays prominent

**For contextually relevant messages:**
1. Message arrives that's directly relevant to current conversation topic
2. Agent naturally mentions: "I just got an update from X about this..."
3. Incorporates information seamlessly
4. Cites source for transparency

## Message Mechanics: Simple Tool-Based Approach

**Core insight:** Messages are delivered by injecting fake user input. Agents use tools to read/respond.

### How It Works

When a message arrives for an agent:

1. Message queues in database
2. Status line updates: `[Agent: oauth-impl | 1 msg]`
3. User decides when to process: `/messages next`
4. System injects fake user message into conversation:
   ```
   [System as User] You have a message from research-agent. Message #2345.
   ```
5. Agent uses tools to read and respond:
   ```
   [Assistant] Let me read that message.
   [Uses ReadMessage(2345)]
   [Message content: "OAuth patterns documented in #mem-oauth/patterns"]

   [Assistant] Great! I'll review the patterns and start implementing...
   [Reads memory doc, writes code, etc.]
   ```

### Message Tools

Agents have access to message tools (like file tools, git tools, etc.):

**`ReadMessage(message_id)`**
- Returns message content, sender info, metadata
- Like reading a file

**`SendMessage(to_agent, content, [metadata])`**
- Sends message to another agent
- Optional priority, tags, etc.

**`ListMessages([filter])`**
- Shows pending messages
- Can filter by sender, unread, priority

**`ReplyToMessage(message_id, content)`**
- Reply to specific message
- Creates message thread

### Example Flow

```
[oauth-impl agent, working on implementation]

[User runs: /messages next]

[System injects as user] You have a message from research-agent. Message #847.

[Assistant] Let me check that message.

[Calls ReadMessage(847)]
Tool result: {
  id: 847,
  from: "research-agent",
  to: "oauth-impl",
  content: "OAuth security patterns documented. See #mem-oauth/security for PKCE flow, token storage recommendations, and common vulnerabilities to avoid.",
  sent_at: "2 minutes ago",
  tags: ["oauth", "security", "research-complete"]
}

[Assistant] Excellent! The research agent has documented OAuth security patterns. Let me review #mem-oauth/security and incorporate these patterns into my implementation.

[Agent reads memory doc, updates code, adds security measures...]

[Assistant] I've incorporated the PKCE flow and secure token storage from the research. Let me send a reply confirming this.

[Calls SendMessage("research-agent", "Thanks for the security patterns. I've incorporated PKCE and secure storage into the implementation. See branch feature/oauth.")]

[Assistant] Done! I've updated the implementation with the security patterns and notified the research agent.
```

### User Commands

**Manual message processing:**
```bash
/messages next          # Process next queued message (injects fake user input)
/messages wait          # Process all queued messages sequentially
/messages show          # Display message queue without processing
/messages show 2345     # Show specific message content
```

**Sending messages:**
```bash
/send oauth-impl "Implementation ready for testing"
/send testing-agent --priority=high "Critical bug found, need retest"
```

Alternatively, user can tell current agent to send:
```bash
[You] Send a message to testing-agent that implementation is ready
[Agent uses SendMessage tool]
```

### No Complex Handler Infrastructure

This approach eliminates the need for:
- Handler YAML files
- Filter expressions
- Template variables
- Handler execution context
- Automatic handler invocation

Instead: **messages are just data, tools read them, agents act naturally**.

The sophistication comes from the agent's understanding and decision-making, not from infrastructure.

### Future Automation

Later, if automation is needed:
- Agent can be configured to auto-process certain messages
- System auto-injects "you have mail" for specific senders/priorities
- Agent still uses same tools, same flow
- Just happens without user typing `/messages next`

But start manual, evolve as needed.
