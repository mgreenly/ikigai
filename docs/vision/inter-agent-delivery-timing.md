# Inter-Agent Communication: Message Delivery Timing

This document covers the hard problem of message delivery timing and different approaches.

See also:
- [inter-agent-communication.md](inter-agent-communication.md) - Overview and index
- [inter-agent-core.md](inter-agent-core.md) - Core concepts and messaging
- [inter-agent-handlers.md](inter-agent-handlers.md) - Message handler system
- [inter-agent-delivery-policies.md](inter-agent-delivery-policies.md) - Agent handling policies
- [inter-agent-implementation.md](inter-agent-implementation.md) - Use cases and implementation

## Message Delivery Timing

**The Hard Problem:** What happens when a message arrives while an agent is actively conversing with the user?

This is the most challenging aspect of inter-agent messaging because it involves interrupting or coordinating with an active human interaction.

### Scenarios

**1. Agent is Idle (Easy Case)**

Agent has no active client or conversation is paused. Message arrives:
- Handler runs immediately
- Handler can take actions freely
- Response sent back to sender
- When user next interacts, they see: "While you were away, handled message from X"

**2. Agent is Active (Hard Case)**

User is actively conversing with agent. Message arrives mid-conversation:
- Can't just interrupt the conversation
- Can't ignore the message (defeats the purpose)
- Handler might want to take actions that affect conversation state
- User experience matters

### Possible Approaches

#### Approach 1: Queue Until Idle

**Mechanism:**
- Messages queue when agent is active
- Handler runs when agent becomes idle (user pauses/disconnects)
- Message shows as "pending" in status line: `[Agent: oauth-impl | 2 pending]`

**Pros:**
- Never interrupts user
- Clean separation between user interaction and inter-agent messaging
- Handlers run in quiet time

**Cons:**
- Urgent messages might be delayed
- No real-time collaboration between agents
- "Idle" is hard to detect (user thinking? went to lunch?)

#### Approach 2: Priority-Based Interruption

**Mechanism:**
- Low/normal priority: queue until idle
- High priority: show notification, don't interrupt
- Urgent priority: interrupt with prominent alert

```
[You] Implement OAuth token refresh

[Agent is typing response...]

[URGENT MESSAGE from testing-agent]
Critical security issue found in OAuth implementation!
See #mem-oauth/security-vuln-CVE-2024-1234

[Continue?] [Handle Message] [Read Details]
```

**Pros:**
- Urgent issues get immediate attention
- User stays in control
- Clear priority levels

**Cons:**
- Interruptions are disruptive
- User has to context-switch
- Might interrupt at bad time (mid-thought)

#### Approach 3: Background Handler Execution

**Mechanism:**
- Handler runs in background regardless of agent state
- Handler can take "safe" actions (respond, log, create memory doc)
- Handler CANNOT take "disruptive" actions (tag, focus, execute) during active conversation
- User sees summary at next natural break

```
[You] Implement OAuth token refresh

[Assistant provides implementation...]

[Assistant] (By the way, while we were talking, I received and handled 2 messages from testing-agent and research-agent. See /messages for details.)
```

**Pros:**
- Doesn't interrupt user
- Handlers still run in real-time
- Agents can collaborate in background

**Cons:**
- Limited actions during active conversation
- User might miss important messages
- Complex state management

#### Approach 4: Conversation Injection (Subtle)

**Mechanism:**
- Message arrives during active conversation
- If priority is high/urgent AND related to current work:
  - Agent naturally incorporates message into response
  - Attributed to sending agent

```
[You] Implement OAuth token refresh logic

[Assistant] I'm implementing the OAuth token refresh logic.

Actually, I just received a message from research-agent with relevant information: they've documented the recommended refresh token pattern in #mem-oauth/refresh-patterns. Let me incorporate that approach...

[Implementation incorporating the research...]
```

**Pros:**
- Seamless integration
- Relevant information arrives at perfect time
- Feels natural, not disruptive

**Cons:**
- Only works for contextually relevant messages
- Agent must decide relevance (complex)
- Could be confusing (where did this info come from?)

#### Approach 5: Micro-Interruption with Auto-Resume

**Mechanism:**
- Message arrives during conversation
- Brief notification appears: `[📨 from testing-agent: urgent test failure]`
- User can:
  - Ignore (handler queues for later)
  - Quick peek (show message without switching context)
  - Handle now (pause current conversation, handle message, auto-resume)

**Pros:**
- User awareness without full interruption
- Progressive disclosure (can ignore or dive in)
- Auto-resume preserves conversation flow

**Cons:**
- Still somewhat disruptive
- UI complexity
- Need to manage pause/resume state

### Recommended Hybrid Approach

Combine multiple strategies based on message metadata and agent state:

```python
def deliver_message(msg, agent):
    if agent.is_idle():
        # Easy case: run handler immediately
        run_handler(msg)

    elif msg.priority == "urgent":
        # Interrupt with option to handle now or later
        show_urgent_notification(msg)
        user_choice = prompt_user("[Handle Now] [Later]")
        if user_choice == "now":
            pause_conversation()
            run_handler(msg)
            resume_conversation()
        else:
            queue_message(msg)

    elif msg.priority == "high":
        # Non-intrusive notification
        show_notification_banner(msg)
        queue_message(msg)
        # Handler runs when agent becomes idle

    elif is_contextually_relevant(msg, agent.current_conversation):
        # Inject into conversation naturally
        inject_into_conversation(msg)

    else:
        # Normal/low priority: queue silently
        queue_message(msg)
        update_status_line(f"[{agent.name} | {len(queue)} pending]")
```

### Handler Action Restrictions

Some actions should be restricted based on agent state:

**Always Safe (Even During Active Conversation):**
- `respond: true` - Send reply to sender
- `log: message` - Log to system
- `create_memory: title` - Create memory document
- `notify_user: true` - Show non-intrusive notification

**Restricted During Active Conversation:**
- `tag: value` - Could confuse current conversation context
- `focus: value` - Changes agent's declared focus
- `execute: /command` - Might interfere with current work
- `mark: label` - Conversation checkpoint
- `priority: urgent` - Priority changes

**Never Automated (Always Requires User Confirmation):**
- `execute: /git commit` - Git operations
- `execute: /agent close` - Agent lifecycle
- `execute: /clear` - Context clearing

### Message Context Windows

Perhaps handlers have different "context windows" based on delivery timing:

**Idle Delivery:**
- Full context available
- Can access entire conversation history
- Can take any allowed action

**Active Delivery (Queued):**
- Limited context (message content + sender info only)
- Cannot access current conversation (it's in progress)
- Restricted actions

**Active Delivery (Urgent Interrupt):**
- Full context snapshot at time of interruption
- User must approve disruptive actions
- Can prompt user for decisions

### Implementation Complexity

This is complex because it requires:
- **State tracking**: Is agent idle? Active? How long idle?
- **Priority evaluation**: What counts as urgent?
- **Relevance detection**: Is message related to current work?
- **UI coordination**: Notifications, status updates, interruption handling
- **Action restriction**: Prevent handlers from disrupting active conversations
- **User preferences**: Respect user's workflow preferences

**Recommendation:** Start simple (Approach 1: Queue Until Idle), evolve to hybrid as we learn usage patterns.
