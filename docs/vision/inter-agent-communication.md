# Inter-Agent Communication

**Core Innovation:** Agents can discover each other, communicate asynchronously, and automate responses through filtered message handlers.

This is a multi-document overview. The content has been split into focused documents:

## Documentation Structure

### 1. [Core Concepts](inter-agent-core.md)
Start here to understand the fundamentals.

**Topics:**
- What is inter-agent communication?
- Agent business cards and identity
- Sending and receiving messages
- Agent discovery mechanisms
- Broadcast messaging
- Future possibilities

**Key concepts:** Agent identity, message structure, discovery, broadcasts.

### 2. [Message Handlers](inter-agent-handlers.md)
Deep dive into the handler system for automated responses.

**Topics:**
- Handler concept and architecture
- Defining handlers with filters and prompts
- Handler context and template variables
- Handler actions (respond, forward, execute)
- Filter expressions (simple, pattern, boolean, compound)
- Handler locations (files vs database)
- Handler development (testing, debugging, dry-run)
- Security and privacy considerations
- Handler prompt patterns (work request, test & report, code review, etc.)

**Key concepts:** Filters, actions, prompt patterns, automation.

### 3. [Message Delivery](inter-agent-delivery.md)
How messages are delivered and processed. *(Split into two sub-documents)*

**3a. [Delivery Timing](inter-agent-delivery-timing.md)**
- Message delivery timing (the hard problem)
- Delivery approaches (queue, interrupt, background, injection, micro-interrupt)
- Handler action restrictions based on agent state
- Recommended hybrid approach

**3b. [Delivery Policies & Mechanics](inter-agent-delivery-policies.md)**
- Agent message handling policies (interrupt, queue, fork)
- Fork mode mechanics and characteristics
- Hybrid policies and user control
- Visual indicators and user experience
- Simple tool-based message mechanics
- Message tools (ReadMessage, SendMessage, ListMessages, ReplyToMessage)

**Key concepts:** Delivery timing, fork mode, handling policies, tools.

### 4. [Implementation](inter-agent-implementation.md)
Use cases, design questions, and implementation roadmap.

**Topics:**
- Concrete use cases (research handoff, testing agent, code review, blocker escalation, cross-project)
- Implementation philosophy (manual first)
- Phase 1: Manual message mode
- Phase 2: Opt-in automation
- Phase 3: Handler patterns
- Phase 4: Full automation
- Implementation phases (manual messaging → message management → automation → ecosystem)
- Open design questions (coordination, message flows, UX, technical concerns)

**Key concepts:** Use cases, phased implementation, design questions.

## Quick Start

If you're new to inter-agent communication:

1. **Start with [Core Concepts](inter-agent-core.md)** to understand what agents are and how they communicate
2. **Read [Implementation](inter-agent-implementation.md)** to see concrete use cases and understand the phased approach
3. **Refer to [Message Delivery](inter-agent-delivery.md)** when you need to understand timing and policies
4. **Deep dive into [Handlers](inter-agent-handlers.md)** when you're ready to implement automation

## Design Principles

1. **Manual First**: Start with manual message processing, evolve to automation based on actual usage
2. **User Control**: User stays in control of when and how messages are processed
3. **Simple Tools**: Messages are data, agents use tools to read/respond naturally
4. **Progressive Disclosure**: Start simple, add complexity as needed
5. **Transparent**: User can see and intercept all inter-agent communication

## Current Status

**Phase 1 Target**: Manual messaging
- Agent business cards
- Point-to-point messages
- Manual message queue (`/messages next`, `/messages wait`)
- Status line indicators

**Future Phases**: See [Implementation](inter-agent-implementation.md) for the full roadmap.

## Related Documentation

- [multi-agent.md](multi-agent.md) - Multi-agent conversation design
- [database-architecture.md](database-architecture.md) - How agents are stored
- [workflows.md](workflows.md) - Multi-agent workflows
- [commands.md](commands.md) - Command reference
