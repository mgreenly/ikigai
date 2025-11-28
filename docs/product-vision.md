# Ikigai: A Platform for Autonomous Agent Development

## Overview

Ikigai is a development platform for building, deploying, and managing autonomous AI agents. At its core is a PostgreSQL-backed task queue and mailbox system that enables agents to work independently, communicate with each other, and serve both technical and non-technical users through multiple interfaces.

The platform consists of three layers:

1. **Ikigai Terminal** - A C-based developer interface for writing and debugging agents
2. **Autonomous Agent Runtime** - TypeScript agents that execute tasks independently
3. **User Interfaces** - Present and future frontends for interacting with agent capabilities

What distinguishes Ikigai from other agent frameworks is its database-first architecture. PostgreSQL isn't just persistence—it's the coordination layer. Task queues, mailboxes, conversation history, and agent state all live in the database, allowing any interface to participate in the agent ecosystem without complex API layers or message broker infrastructure.

## The Developer Experience

Ikigai Terminal is purpose-built for developers creating autonomous agents. Unlike traditional coding assistants that wait for instructions, Ikigai understands the agent development lifecycle:

**Writing Agents**: The terminal provides direct LLM integration with multiple providers (OpenAI, Anthropic, Google, X.AI), file operations, shell commands, and code analysis tools. Developers describe what they want to build, and Ikigai generates TypeScript agents that follow platform conventions for queue processing, mailbox communication, and fault tolerance.

**Debugging Agents**: Full conversation history with PostgreSQL-backed search enables developers to review how agents made decisions, trace message flows between agents, and identify failure patterns. Session restoration means context is never lost across terminal restarts.

**Managing Agents**: The terminal interface provides visibility into active agents, pending tasks, and mailbox contents. Developers can inject tasks, inspect messages, and intervene when autonomous agents need guidance.

The key insight: Ikigai itself is an AI agent that specializes in building other AI agents for this specific platform. It knows the queue schema, the mailbox patterns, the TypeScript idioms for long-running workers, and the supervision strategies that make agents resilient.

## The Autonomous Agent Runtime

TypeScript autonomous agents form the working layer of the platform. These are long-running processes that:

- Pull tasks from PostgreSQL queues using priority and scheduling rules
- Execute work autonomously (API calls, data processing, code generation, system integration)
- Post results to mailboxes for other agents or user interfaces to consume
- Handle failures gracefully with retry logic and dead-letter queues
- Communicate with other agents through message passing

The runtime environment is event-driven. Agents respond to:
- New tasks in their queue
- Messages in their mailbox
- Timer events for scheduled work
- System signals for coordination

This architecture enables true autonomy. Once deployed, agents work continuously without human intervention, only escalating to humans when they encounter genuine uncertainty or failure conditions.

Because agents share a common PostgreSQL foundation, they can:
- Query conversation history for context
- Use full-text search to find relevant past decisions
- Coordinate through transactional mailbox updates
- Leverage PostgreSQL's LISTEN/NOTIFY for real-time coordination

Future versions will add pgvector for RAG capabilities, allowing agents to build and query semantic indexes of code, documentation, and conversation history.

## The Platform Vision

The PostgreSQL foundation enables Ikigai to evolve beyond developer tooling into a full agent platform:

**Phase 1: Developer Adoption**
Developers use Ikigai Terminal to rapidly build autonomous agents. The platform handles the undifferentiated heavy lifting—task queues, message passing, state management, conversation persistence—letting developers focus on agent logic and capabilities.

**Phase 2: Multi-Interface Access**
As agents prove valuable, non-technical users need access. A web dashboard lets product managers create tasks, view agent status, and consume results—all reading from the same PostgreSQL tables. No API layer needed initially; interfaces query the database directly with appropriate access controls.

**Phase 3: Agent Ecosystem**
Multiple specialized agents coordinate through mailboxes. A monitoring agent detects issues and posts tasks for a remediation agent. A code analysis agent feeds findings to a refactoring agent. A documentation agent reads from mailboxes to stay current with system changes. The platform becomes an ecosystem where agents compose.

**Phase 4: External Integration**
As the agent ecosystem matures, external systems integrate via well-defined queue and mailbox contracts. Third-party tools post tasks. Enterprise systems consume results. The PostgreSQL interface becomes stable enough for broad integration.

## Technical Architecture

**Database Schema**:
- `sessions` - Conversation context and state
- `messages` - Full message history with full-text search
- `task_queue` - Pending work with priority and scheduling
- `mailbox` - Inter-agent and agent-to-user messaging
- `agent_state` - Runtime status and health monitoring

**Ikigai Terminal (C)**:
- Direct terminal rendering with UTF-8 support
- Multi-line input with readline-style editing
- Scrollback buffer with efficient reflow
- Streaming LLM responses directly to terminal
- Session restoration from database
- Tool execution (file ops, shell commands, code analysis)

**Agent Runtime (TypeScript)**:
- Event-driven main loop responding to multiple signal types
- Worker pool pattern for concurrent task processing
- Supervision trees for fault isolation and recovery
- Streaming API integration for LLM calls
- Shared libraries for queue/mailbox patterns

**Deployment Model**:
- PostgreSQL instance (self-hosted or managed)
- Ikigai Terminal runs locally for developers
- Autonomous agents deploy as services (Docker, systemd, cloud functions)
- Future web interfaces deploy separately but share database

## Why This Approach Works

**Database as Platform**: By making PostgreSQL the coordination layer, Ikigai avoids the complexity of message brokers, API gateways, and distributed state management. ACID transactions ensure consistency. LISTEN/NOTIFY enables real-time updates. Full-text search provides rich querying. The database does what databases do best.

**Language Pragmatism**: C for the terminal UI provides performance and stability for a long-running system process. TypeScript for agents provides rapid iteration, excellent AI ecosystem integration, and a large developer pool. Each language handles what it's best at.

**Autonomous-First Design**: Unlike chatbot frameworks adapted for task execution, Ikigai is designed from the ground up for autonomous operation. The event loop, mailbox communication, and supervision patterns are first-class concepts, not afterthoughts.

**Developer-Led Growth**: By starting with a terminal interface that developers love, Ikigai ensures early adopters are the people who will build the agent ecosystem. As they create valuable agents, demand for end-user interfaces follows naturally.

## Current Status and Roadmap

**Current (v0.3.0)**: Production-ready terminal with PostgreSQL integration, conversation persistence, and session restoration. LLM streaming from multiple providers. Basic tool execution.

**Near-term (v0.4.0-v0.5.0)**: Complete tool execution framework, agent-script architecture, task system for autonomous operation.

**Mid-term (v1.0)**: Mature autonomous agent development experience, formalized queue/mailbox schema, TypeScript agent templates and libraries, comprehensive documentation.

**Long-term (v2.0+)**: RAG infrastructure with pgvector, web dashboard for non-technical users, agent marketplace, enterprise features (RBAC, audit logging, multi-tenancy).

Ikigai is building the infrastructure for the next generation of AI applications—not chatbots that wait for input, but autonomous agents that work continuously toward goals, coordinate through message passing, and serve users through interfaces appropriate to their needs.
