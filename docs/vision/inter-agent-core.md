# Inter-Agent Communication: Core Concepts

This document covers the fundamental concepts of agent-to-agent communication.

See also:
- [inter-agent-communication.md](inter-agent-communication.md) - Overview and index
- [inter-agent-handlers.md](inter-agent-handlers.md) - Message handler system
- [inter-agent-delivery.md](inter-agent-delivery.md) - Message delivery mechanics
- [inter-agent-implementation.md](inter-agent-implementation.md) - Use cases and implementation

## The Concept

Agents are no longer just isolated conversations sharing a memory store. They become **active participants** that can:
- Advertise their identity and capabilities
- Send messages to other agents
- Respond to messages automatically based on filters
- Coordinate work without constant human intervention

Think of it as **agent-to-agent RPC** + **event-driven automation** + **filtered subscriptions**.

## Agent Business Cards

Every agent has an **identity** that other agents can discover and query.

### Core Identity

```yaml
name: oauth-impl
project: ikigai
role: Feature Implementation
status: active
created_at: 2024-01-15T10:30:00Z
current_focus: "Implementing OAuth 2.0 authentication"
tags:
  - oauth
  - authentication
  - security
capabilities:
  - code_implementation
  - testing
  - git_operations
location:
  branch: feature/oauth
  worktree: .worktrees/oauth-impl
  path: /home/mgreenly/projects/ikigai/main/.worktrees/oauth-impl
owner:
  user: mgreenly
  machine: workstation
```

### Querying Agents

```bash
# List all agents
/agents list

# List agents by project
/agents list --project=ikigai

# List agents by capability
/agents list --capability=testing

# Show specific agent's card
/agents show oauth-impl

# Show all agents working on authentication
/agents find --tag=authentication
```

### Agent Roles (Optional Categorization)

Agents can declare their role to help with discovery:

- **Root** - Main coordinating agent
- **Feature** - Implementing a specific feature
- **Research** - Gathering information, creating memory docs
- **Testing** - Writing/running tests
- **Review** - Code review and quality checks
- **Refactor** - Improving existing code
- **Custom** - User-defined roles

## Inter-Agent Messaging

Agents can send messages to other agents. Messages are **asynchronous** and stored in the database.

### Sending Messages

From within a conversation:

```bash
# Send message to specific agent
/send oauth-impl "OAuth research complete. Memory doc at #mem-oauth/patterns ready for your implementation."

# Send to multiple agents
/send testing,review "Feature implementation complete. Ready for testing and review."

# Send with metadata
/send oauth-impl --priority=high --tag=blocker "Need decision on refresh token storage before proceeding."
```

Or agents can send messages autonomously (when prompted by user):

```
[You] Research OAuth patterns and notify the oauth-impl agent when done.
[Assistant researches, creates memory doc, sends message to oauth-impl agent]
Sent message to oauth-impl: "Research complete. See #mem-oauth/patterns"
```

### Receiving Messages

**Key Principle:** Messages from other agents are treated like user input - they can cause the receiving agent to do actual work.

When a message arrives, it enters the agent's conversation context just like user input:

```
[System] 📨 Message from research-agent:
         OAuth research complete. Memory doc at #mem-oauth/patterns ready for implementation.
         Tags: oauth, research-complete
         Priority: normal

[Assistant] I received the OAuth research from research-agent. Let me review #mem-oauth/patterns and start implementing...

[Assistant reads memory doc, creates implementation plan, writes code, runs tests...]
```

**The agent treats the message as a prompt and acts on it**, potentially:
- Writing code
- Running tests
- Creating files
- Making git commits
- Creating memory docs
- Sending messages to other agents
- Making decisions
- Any other work it would do for a user

This is **agent-driven automation** - agents can truly delegate work to each other.

### Message Structure

```c
typedef struct ik_agent_message_t {
    int64_t id;
    int64_t from_agent_id;      // Sending agent
    int64_t to_agent_id;        // Receiving agent (or NULL for broadcast)
    char *subject;              // Brief subject line
    char *content;              // Full message content
    char **tags;                // Message tags for filtering
    char *priority;             // "low", "normal", "high", "urgent"
    timestamp_t sent_at;
    timestamp_t read_at;        // NULL if unread
    char *in_reply_to;          // NULL or message_id being replied to

    // Metadata
    char *project;              // Scope to project
    json_t *metadata;           // Arbitrary structured data
} ik_agent_message_t;
```

## Agent Discovery

How do agents find out about each other?

### Active Agents

```bash
/agents active

# Shows currently running agents (with active client connections)
# vs agents that exist in database but no active client
```

### Agent Registry

```bash
/agents registry

# Shows all agents ever created in database
# With metadata: project, last active, message count, etc.
```

### Capability-Based Discovery

```bash
/agents find --capability=testing --project=ikigai

# Returns agents that:
# - Have 'testing' in their capabilities
# - Are associated with ikigai project
# - May be active or dormant
```

### Smart Discovery

```bash
[You] I need help with OAuth testing

[Assistant] Found these agents that might help:
- testing-oauth (active, project: ikigai, 23 messages)
- oauth-impl (active, knows about OAuth implementation)

Would you like me to send a message to one of them?
```

## Broadcast Messages

In addition to point-to-point messages, agents can broadcast to multiple recipients.

### Topic-Based Broadcast

```bash
# Broadcast to all agents with specific tags
/broadcast --tags=oauth,implementation "Breaking: OAuth library upgraded to v3.0. Review #mem-oauth/migration-guide"

# All agents tagged with 'oauth' AND 'implementation' receive the message
```

### Project-Wide Broadcast

```bash
# Broadcast to all agents in a project
/broadcast --project=ikigai "FYI: Switching to new error handling pattern. See #mem-errors/new-pattern"
```

### Filtered Broadcast

Handlers can filter broadcasts just like direct messages:

```yaml
handlers:
  - name: security-alerts
    filters:
      is_broadcast: true
      tags: [security, alert]
    prompt: |
      Security alert broadcast: {message_content}

      1. Review the security issue
      2. Check if your code is affected
      3. Respond with impact assessment
```

## Future Possibilities

### Handler Marketplace

Share handler definitions:

```bash
/handler install @community/test-automation
/handler install @mgreenly/security-review
```

### Agent Specialization

Agents could advertise specialized knowledge:

```yaml
specializations:
  - OAuth 2.0 implementation
  - PostgreSQL query optimization
  - React component patterns
```

Other agents can query: "Which agent knows about OAuth?"

### Negotiation Protocols

Agents could negotiate:

```
Agent A: "Can you review my PR?"
Agent B: "I'm busy with high-priority work. Can it wait 2 hours?"
Agent A: "Yes, I'll mark it for later review."
```

### Workflow Orchestration

Complex multi-agent workflows:

```yaml
workflow: feature-implementation
steps:
  - agent: research
    outputs: [memory-doc]
    notify: implementation

  - agent: implementation
    depends_on: [research]
    outputs: [code, tests]
    notify: testing

  - agent: testing
    depends_on: [implementation]
    outputs: [test-results]
    notify: [implementation, review]

  - agent: review
    depends_on: [testing]
    outputs: [approval]
    notify: main
```

### Agent Reputation

Track agent reliability:

- Response time
- Quality of responses
- Success rate of automated actions
- User satisfaction ratings

Use reputation for prioritization and discovery.
