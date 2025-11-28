# Inter-Agent Communication: Implementation

This document covers use cases, design questions, and implementation phases.

See also:
- [inter-agent-communication.md](inter-agent-communication.md) - Overview and index
- [inter-agent-core.md](inter-agent-core.md) - Core concepts and messaging
- [inter-agent-handlers.md](inter-agent-handlers.md) - Message handler system
- [inter-agent-delivery.md](inter-agent-delivery.md) - Message delivery mechanics

## Use Cases

### 1. Research → Implementation Handoff

**Scenario:** User asks root agent to implement OAuth. Root agent delegates.

```bash
# In root agent (main)
[You] Implement OAuth 2.0 authentication

[Assistant] I'll coordinate this across multiple agents:
1. Creating research agent to gather OAuth patterns
2. Creating implementation agent to build the feature

/agent new oauth-research
[You] Research OAuth 2.0 patterns, best practices, and security considerations.
      Create memory doc when done and notify oauth-impl agent.

/agent new oauth-impl --worktree
[You] Wait for research from oauth-research agent, then implement OAuth 2.0.

# oauth-research completes and sends message
# oauth-impl has handler that auto-responds, acknowledges, creates plan
# oauth-impl notifies user "Research received, starting implementation"
```

### 2. Continuous Testing Agent

**Scenario:** A testing agent monitors implementation agents and runs tests.

```yaml
# testing-agent handler
handlers:
  - name: code-committed
    filters:
      tags: [code-complete, ready-for-test]
    prompt: |
      {sender_name} reports code completion: {message_content}

      1. Switch to their worktree
      2. Run the test suite
      3. Report results back to them
      4. If failures, include details
    actions:
      - respond: true
      - notify_user: true
```

Implementation agent sends message when done:
```bash
/send testing-agent --tag=ready-for-test "OAuth implementation complete in feature/oauth branch"
```

Testing agent automatically:
1. Receives message
2. Handler fires
3. Runs tests
4. Sends results back

### 3. Code Review Workflow

**Scenario:** Review agent automatically reviews PRs.

```yaml
# review-agent handler
handlers:
  - name: review-request
    filters:
      tags: [review-request]
      from_agent: "*-impl"      # Any implementation agent
    prompt: |
      Review request from {sender_name}:
      {message_content}

      1. Check out their branch
      2. Review code changes
      3. Check for security issues, best practices
      4. Provide detailed feedback
      5. Approve or request changes
    actions:
      - respond: true
      - create_memory: "review-{sender_name}-{timestamp}"
```

### 4. Blocker Escalation

**Scenario:** Helper agents escalate blockers to root agent.

```yaml
# main (root) agent handler
handlers:
  - name: blocker-escalation
    filters:
      tags: [blocker]
      priority: [high, urgent]
    prompt: |
      BLOCKER from {sender_name}:
      {message_content}

      1. Understand the blocker
      2. Provide solution or decision
      3. Unblock the agent so they can continue
    actions:
      - respond: true
      - notify_user: urgent      # Interrupt user
      - priority: urgent
```

Any agent can escalate:
```bash
/send main --priority=urgent --tag=blocker "Need decision: store refresh tokens in DB or filesystem?"
```

### 5. Cross-Project Collaboration

**Scenario:** Agent in project-a needs info from project-b.

```bash
# In project-a agent
/send @project-b/main "Do you have a memory doc about OAuth patterns? Need to implement similar feature."

# project-b agent has handler that searches memory docs and responds
# Response: "Yes! See #mem-oauth/patterns-v2. Also #mem-oauth/security-checklist"
```

## Implementation Philosophy: Manual First

**Start manual, evolve to automatic.**

Rather than building complex automatic handlers first, start with simple manual message processing and learn how it's actually used.

### Phase 1: Manual Message Mode

**Core principle:** Messages arrive and queue. Agent processes them only when user explicitly says so.

```bash
# Messages queue automatically, shown in status line
[Agent: oauth-impl | 3 msgs] [Branch: feature/oauth]

# User decides when to process
/messages next         # Process next message
/messages wait         # Process all queued messages
/messages show         # Show queued messages without processing

# Agent treats message like user input and acts on it
```

**Example flow:**

```
[oauth-impl agent]
[You] Implement token refresh

[Agent starts working...]

[Status line updates: oauth-impl | 1 msg]

[You finish current work, then:]
/messages next

[System] Message from research-agent (2 minutes ago):
         OAuth security patterns documented in #mem-oauth/security

[Assistant] I'll review the security patterns from research-agent...
[Reads memory doc, incorporates patterns, continues work...]
```

**Benefits:**
- User stays in control
- No surprising automatic behavior
- Learn actual usage patterns before automating
- Simple to implement and understand
- Easy to debug

### Phase 2: Opt-In Automation

Once manual mode works well, add **opt-in** automation:

```bash
# Agent responds automatically only when explicitly configured
/agent config auto-respond research-agent    # Auto-respond to this sender
/agent config auto-respond --tag=fyi         # Auto-respond to FYI messages

# Still defaults to manual queue for everything else
```

### Phase 3: Handler Patterns

After learning common manual workflows, codify them as handlers:

```bash
# User notices they always respond the same way to testing-agent
# So they create a handler
/handler new test-results

# Handler captures the pattern
# But still requires manual approval initially
/handler test-results --require-approval
```

### Phase 4: Full Automation (Future)

Only after extensive use of manual and opt-in modes, consider full automation with all the sophisticated features (fork, complex filters, etc.).

**Key insight:** We need to **use the system manually** to understand how inter-agent communication actually works in practice before we try to automate it.

## Implementation Phases (Revised)

**Phase 1: Manual Messaging**
- Agent business cards (`/agents` commands)
- Point-to-point messages (`/send`)
- Manual message queue (`/messages next`, `/messages wait`)
- Messages treated like user input
- Status line indicators for pending messages

**Phase 2: Message Management**
- View message details (`/messages show`)
- Mark messages as read/unread
- Delete/archive messages
- Filter message list
- Message threading (replies)

**Phase 3: Opt-In Automation**
- Simple auto-respond rules (`/agent config auto-respond <sender>`)
- Priority-based auto-processing
- Tag-based routing
- Still requires explicit configuration

**Phase 4: Basic Handlers (Future)**
- File-based handler definitions
- Simple prompt templates
- Manual handler invocation
- Handler testing tools

**Phase 5: Advanced Automation (Future)**
- Automatic handler execution
- Complex filter expressions
- Fork mode
- Broadcast messaging
- Agent specialization

**Phase 6: Ecosystem (Future)**
- Database-stored handlers
- Handler marketplace
- Workflow orchestration
- Agent reputation

## Open Design Questions

### Coordination & Creation Patterns

**Agent Creation & Coordination:**
- Who creates helper agents? User manually, or main agent orchestrates?
- Can agents create sub-agents?
- If so, what's the ownership model? (agent tree? flat list?)
- Does user explicitly coordinate agents, or do agents coordinate themselves?
- How much autonomy should agents have in delegation?

**Discovery & Awareness:**
- How do agents discover each other?
- Should agents be able to query for other agents by capability/tag/role?
- Can agents see all agents, or only related ones (same project, etc.)?
- Should agents be aware of what other agents are working on?

### Message Flows

**Notification & Delivery:**
- How does a receiving agent know a message arrived?
  - Status line indicator only?
  - Active notification?
  - Depends on priority?
- When research agent finishes, how does impl agent find out?
  - Manual: user switches and tells impl agent
  - Message: research sends message to impl
  - Discovery: impl queries for completed research
  - Broadcast: research announces completion
- Should agents be able to wait/block for messages from specific agents?

**Message Processing:**
- `/messages next` vs `/messages wait` vs automatic processing?
- Can user preview message without agent processing it?
- Can user modify/edit incoming messages before agent processes them?
- What if agent is processing a message and another arrives?
- Should there be message priorities that affect queue ordering?

### User Interaction Models

**Control & Visibility:**
- How much should user see of inter-agent communication?
  - Everything (transparent)?
  - Only summaries (agent handled 3 messages)?
  - User configurable?
- When agents are working autonomously, how does user monitor/intervene?
- Can user intercept messages between agents?
- Should user be able to "overhear" agent conversations?

**Manual vs Automatic:**
- Start with everything manual (`/messages next`) and evolve?
- Or build automatic from start with manual override?
- What triggers the transition from manual to automatic?
- How does user configure automation preferences?

### Communication Patterns

**Message Types:**
- Point-to-point: agent A → agent B
- Broadcast: agent A → all agents with tag X
- Publish/subscribe: agents subscribe to topics
- Request/response: agent A asks, expects reply from agent B
- Fire-and-forget: agent A sends, doesn't care about reply
- Which patterns are essential vs nice-to-have?

**Agent Business Cards:**
- What information goes on a business card?
  - Name, role, capabilities, current focus, tags?
  - Status (active/idle/busy)?
  - Message preferences?
- How/when are business cards created?
- Can agents update their own business cards?
- How long do business cards persist (agent lifetime? forever?)

### Technical Concerns

**Synchronous vs Asynchronous:**
- Should agents ever be able to wait for responses?
- Or always async message passing?
- Can handlers be synchronous (block until response)?

**Message Threading:**
- Can messages be part of threads (like email)?
- Reply chains, conversations between agents?
- Or always independent messages?

**Handler Language & Definition:**
- YAML as shown in examples?
- Custom DSL?
- Python/Lua scripts?
- Just structured prompts?
- Database-stored or file-based?
- How does user edit/test handlers?

**Handler Execution Context:**
- Same LLM model as agent?
- Smaller/faster model for handlers?
- Different system prompt?
- Access to full agent context or limited?

**Message Persistence:**
- How long are inter-agent messages kept?
- Are they part of RAG corpus?
- Can they be searched like conversation history?
- Do they live in same messages table or separate?

**Broadcast Scaling:**
- What happens when broadcasting to 50 agents?
- Rate limiting?
- Priority queues?
- Should broadcasts be opt-in (subscribe) vs opt-out?

**Agent Lifecycle & Messages:**
- Can dormant agents (no active client) receive messages?
- Do handlers run even if agent has no active client?
- How to handle agent deletion with pending messages?
- What happens to messages sent to closed/deleted agents?

**Cross-User & Security:**
- Can agents from different users communicate?
- Shared project agents across users?
- Security implications of inter-agent messaging?
- Message authentication/verification?
- Can malicious agent spam others with messages?

### UX & Workflow

**Fork Mode:**
- How does fork actually work in practice?
- What context does fork inherit from parent?
- How does fork communicate findings back to parent?
- When does fork auto-close vs stay around?
- Can user switch to a fork and interact with it?

**Message Queue UI:**
- What does `/messages show` actually display?
- How are messages sorted (time? priority? sender?)
- Can user delete/archive messages?
- Can user mark messages as read/unread?
- Keyboard shortcuts for message navigation?

**Status Line:**
- What message indicators in status line?
  - Count: `| 3 msgs`
  - Priority: `| 🔴 urgent`
  - Sender: `| msg from testing`
- How much info before it's cluttered?
- Should status line be clickable to show messages?

### Future Considerations

**Agent Reputation:**
- Should system track agent reliability/quality?
- How is reputation calculated?
- Does reputation affect message routing/priority?
- User rating of agent responses?

**Workflow Orchestration:**
- Should there be explicit workflow definitions?
- Or emergent workflows from agent interactions?
- Can workflows be saved/reused?
- Template workflows for common patterns?

**Handler Marketplace:**
- Can users share handler definitions?
- Trusted handler sources?
- Versioning of handlers?
- Handler dependencies?

## Related Documentation

- [multi-agent.md](multi-agent.md) - Multi-agent conversation design
- [database-architecture.md](database-architecture.md) - How agents are stored
- [workflows.md](workflows.md) - Multi-agent workflows
- [commands.md](commands.md) - Command reference
