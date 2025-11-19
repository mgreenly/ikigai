# ikigai Vision

## Overview

ikigai is your **personal super agent** - a coordinated system of AI agent threads that work together through shared memory and inter-agent communication. Think of it as a team of expert developers collaborating seamlessly, but all within a single terminal-native environment.

**The Core Innovation:**

Multiple agent threads coordinate through a **shared PostgreSQL database** that serves as both memory and communication backbone:

- **Coordinated agent threads** - Agents work in parallel on different tasks while maintaining awareness of each other
- **Shared memory store** - Memory documents provide persistent knowledge accessible to all agents
- **Inter-agent communication** - Agents can message each other, delegate work, and coordinate automatically
- **Database as source of truth** - All conversations, memory, and agent state persist in PostgreSQL
- **Multi-client support** - Run multiple terminal clients across different projects, all connected to the same agent ecosystem
- **Git-native workflow** - Agents can work in separate worktrees with automatic branch management

Instead of a single agent with limited context, ikigai gives you a **coordinated team** where research agents feed implementation agents, testing agents validate work, and all agents build on shared institutional knowledge - all orchestrated through the database.

## Design Philosophy

**Coordinated Multi-Agent System:**
- Multiple agent threads work simultaneously on different aspects of your project
- Agents maintain isolated context but coordinate through shared database
- Inter-agent messaging enables delegation and collaboration
- Memory documents serve as persistent shared knowledge base
- Like having a team of specialists who can communicate and build on each other's work

**Database as Coordination Hub:**
- Shared PostgreSQL database stores all agents, conversations, memory, and messages
- Agents discover each other through database queries
- Memory documents persist knowledge across all agents and sessions
- Message identity tracking enables powerful RAG queries
- All conversation history tagged with rich metadata for future retrieval

**Inter-Agent Communication:**
- Agents can send messages to each other for delegation and coordination
- Research agents notify implementation agents when findings are ready
- Testing agents report results back to feature agents
- Root agent coordinates work across multiple helper agents
- Start with manual message processing, evolve to automatic handlers

**Git-Native Workflow:**
- Each agent can work in its own git worktree on its own branch
- Root agent stays on main branch in primary directory
- Helper agents work on feature branches in `.worktrees/`
- Physical isolation prevents conflicts while enabling parallel work

**Power User First:**
- Keyboard-driven navigation with fast agent switching
- Slash commands for explicit actions and coordination
- Maximum control and transparency of agent interactions
- Terminal-native for speed and efficiency

## Quick Navigation

### Core Vision Documents

- [database-architecture.md](database-architecture.md) - Multi-client database architecture and message identity
- [commands.md](commands.md) - Complete command reference
- [keybindings.md](keybindings.md) - Keyboard navigation and shortcuts
- [multi-agent.md](multi-agent.md) - Multi-agent conversation design
- [inter-agent-communication.md](inter-agent-communication.md) - Agent-to-agent messaging, handlers, and automation
- [mark-rewind.md](mark-rewind.md) - Checkpoint/rollback system
- [git-integration.md](git-integration.md) - Git worktree workflows
- [why-ikigai.md](why-ikigai.md) - The vision and sales pitch

### Workflow Examples

- [workflows.md](workflows.md) - Real-world usage patterns and examples
- [why-ikigai.md](why-ikigai.md) - Vision, philosophy, and comparisons

## Core Concepts

### Coordinated Agent Threads

**Root Agent:**
- Always exists, typically on main branch in primary worktree
- Coordinates work across helper agents
- Receives status updates and escalations from helper agents
- Maintains overall project context

**Helper Agents:**
- Created for specific tasks (research, implementation, testing, review)
- Work in parallel on different aspects of the project
- Can work in separate worktrees on feature branches
- Communicate with each other and with root agent

**Agent Coordination:**
- Each agent has isolated conversation context
- Agents discover each other through database queries
- Share knowledge through memory documents
- Communicate through inter-agent messages
- Can delegate work and notify each other of progress

### Memory Documents

The **shared knowledge base** that enables coordination:

- Markdown documents stored in PostgreSQL (not in git)
- Accessible to all agents across all clients and projects
- Persistent - survive agent destruction and session restarts
- Referenced with `#mem-<id>` or `#mem-<alias>`
- Research agents create them, implementation agents consume them
- Build up institutional knowledge over time

**Example workflow:**
1. Research agent investigates OAuth patterns → creates `#mem-oauth/patterns`
2. Implementation agent reads memory doc → implements feature using documented patterns
3. Testing agent references memory doc → validates implementation matches patterns

### Inter-Agent Messages

Agents **communicate and delegate** through messages:

- Point-to-point messages (`/send oauth-impl "research complete"`)
- Messages queue in database, processed manually or automatically
- Agents can send messages autonomously when prompted
- Enable workflows like research→implementation→testing handoffs
- Start with manual message processing, evolve to handlers

**Example:**
```
[research-agent] Completes research, sends message to oauth-impl
[oauth-impl]     Receives message, reads memory doc, starts implementation
[oauth-impl]     Completes work, sends to testing-agent
[testing-agent]  Runs tests, reports results back
```

### Worktrees

- Git worktrees provide physical isolation for parallel agent work
- Optional: agents can work without worktrees (research, analysis)
- Automatic cleanup when agents close
- Natural mapping: agent → branch → worktree → task

### Mark/Rewind

- In-session checkpoints for safe exploration
- Non-destructive (all messages preserved in DB)
- Named or anonymous marks
- LIFO stack for nested exploration

## Status Line

Shows current context and coordination state at a glance:

```
[Agent: main | +3] [Branch: main] [Worktree: ~/projects/ikigai/main]
[Agent: oauth-impl | 2 msgs] [Branch: feature-oauth] [Worktree: .worktrees/oauth-impl]
[Agent: research-oauth] [Branch: main] [Project: ikigai]
```

Indicators:
- **Agent name** - Current agent you're interacting with
- **Active helper count** - `+3` means 3 other agents exist in your ecosystem
- **Message count** - `2 msgs` means 2 unread inter-agent messages queued
- **Git branch** - Current branch the agent is working on
- **Worktree path** - Physical location of agent's workspace
- **Mark count** - Shows when conversation checkpoints exist

The status line keeps you aware of your agent ecosystem and coordination state without cluttering the interface.

## Command Categories

### Agent Management & Coordination
- `/agent new [name]` - Create helper agent
- `/agent list` - Show all agents in your ecosystem
- `/agent <name>` - Switch to agent (instant context switch)
- `/agent close` - Close current helper
- `/agent rename <name>` - Rename current agent
- `/agents active` - Show currently running agents
- `/agents find --tag=<tag>` - Discover agents by capability/tag

### Inter-Agent Communication
- `/send <agent> <message>` - Send message to another agent
- `/send <agent> --priority=high <message>` - Send priority message
- `/messages next` - Process next queued message
- `/messages show` - View message queue
- `/messages wait` - Process all queued messages

### Memory Documents (Shared Knowledge)
- `/memory list` - List all documents across all agents
- `/memory show <id|alias>` - Display document
- `/memory merge <id|alias>` - Pull into current context
- `/memory create <alias>` - Create new memory document
- `/memory delete <id|alias>` - Remove document

### Conversation Control
- `/mark [label]` - Create checkpoint
- `/rewind [label]` - Rollback to checkpoint
- `/clear` - Clear session context

### Git Integration
- `/agent new <name> --worktree` - Create agent with isolated worktree
- `/agent merge` - Merge current branch to main
- `/branch` - Show branch status

## Key Bindings

### Agent Navigation
- `Ctrl-\` - Agent switcher (fuzzy find)
- `Ctrl-\ Ctrl-\` - Toggle to last agent
- `Ctrl-\ m` - Jump to root agent (main)

### Mark/Rewind
- `Ctrl-m` - Quick mark (anonymous)
- `Ctrl-r` - Quick rewind (to last mark)

### Scrollback
- `Page Up/Down` - Scroll display
- `Ctrl-Home/End` - Jump to top/bottom
- `Ctrl-l` - Redraw screen

See [keybindings.md](keybindings.md) for complete reference.

## Design Principles

### 1. Coordination Through Shared Memory

The database is the coordination hub:
- All agents read from and write to shared PostgreSQL database
- Memory documents serve as persistent knowledge base
- Inter-agent messages enable explicit coordination
- Conversation history tagged with rich metadata for RAG
- Knowledge accumulates across all agents and sessions

### 2. Explicit Communication Over Implicit

Agent interactions are explicit and visible:
- `/send agent-name "message"` for inter-agent communication
- `/messages next` to process incoming messages
- `/memory merge #mem-doc` to pull in shared knowledge
- User stays in control of coordination
- No hidden automation until explicitly configured

### 3. Isolated Context, Shared Knowledge

Balance between isolation and collaboration:
- Each agent has own conversation history and context
- Own working directory (when using worktrees)
- Share knowledge through memory documents and messages
- `/clear` on one agent doesn't affect others
- Agents coordinate explicitly, not through shared context

### 4. Git-Aware By Default

ikigai understands git as a coordination mechanism:
- Show current branch in status line
- Agents can own separate worktrees for parallel work
- Memory docs can reference commits/branches
- Physical isolation through git enables parallel work

### 5. Power User Efficiency

Fast, keyboard-driven coordination:
- Instant agent switching (Ctrl-\ like browser tabs)
- Fuzzy matching for agents/commands
- Quick shortcuts for common coordination tasks
- No unnecessary confirmations
- Terminal-native speed

## Example: Coordinated Multi-Agent Workflow

Here's how the personal super agent concept works in practice:

```bash
# You're in the root agent working on your project
[main] Implement OAuth 2.0 authentication

# Root agent coordinates: creates specialized helper agents
[Assistant] I'll coordinate this across multiple agents:
1. Creating research-oauth to investigate patterns
2. Creating oauth-impl for implementation
3. They'll coordinate through memory docs and messages

# Switch to research agent (Ctrl-\ → research-oauth)
[research-oauth] Research OAuth 2.0 best practices, security patterns,
                 and create memory doc. When done, notify oauth-impl agent.

[Assistant researches, creates #mem-oauth/patterns, #mem-oauth/security]
[Assistant] Research complete. Sending notification...
/send oauth-impl "OAuth research complete. See #mem-oauth/patterns and #mem-oauth/security"

# Switch to implementation agent (Ctrl-\ → oauth-impl)
[oauth-impl] [Status shows: 1 msg]
/messages next

[System] Message from research-oauth (2 minutes ago):
         OAuth research complete. See #mem-oauth/patterns and #mem-oauth/security

[Assistant] I'll review the research and start implementing...
[Reads memory docs, writes code, runs tests]
[Assistant] Implementation complete. Notifying root agent...
/send main "OAuth implementation done in feature/oauth branch. Tests passing."

# Switch back to root agent (Ctrl-\ Ctrl-\ toggles to last)
[main] [Status shows: 1 msg]
/messages next

[System] Message from oauth-impl:
         OAuth implementation done in feature/oauth branch. Tests passing.

[Assistant] Excellent! Let me review the implementation and merge it...
```

**Key observations:**
- **Specialized agents** - Each agent focuses on its specific task
- **Shared knowledge** - Memory docs (`#mem-oauth/patterns`) persist and are accessible to all
- **Explicit coordination** - Messages enable handoffs (research → implementation → review)
- **Fast switching** - Move between agents instantly with Ctrl-\
- **Database persistence** - All agents, messages, and memory persist across sessions
- **User in control** - Manual message processing (`/messages next`) keeps you aware of coordination

This is fundamentally different from a single agent with a large context window - it's a **coordinated team** where agents build on each other's work.

## The Personal Super Agent Advantage

**Traditional single-agent approach:**
- One conversation with one context window
- Agent forgets or loses track when context grows
- No specialization - agent does everything
- No persistent knowledge base
- Work stops when context clears

**ikigai's coordinated multi-agent approach:**
- Multiple specialized agents working in parallel
- Each agent maintains focused context on their task
- Agents coordinate through messages and shared memory
- Persistent knowledge accumulates in database
- Work persists across sessions and context clears
- Institutional knowledge builds over time

**The coordination mechanisms make the difference:**

1. **Shared Memory Database** - All agents contribute to and benefit from a growing knowledge base. Research from months ago is still accessible. Patterns documented in one project inform work in another.

2. **Inter-Agent Communication** - Agents can delegate work to each other. Research agent → Implementation agent → Testing agent, each building on the previous work without the user manually copying context.

3. **Message Identity & RAG** - Every conversation is tagged with rich metadata (project, agent, tags, focus). Future agents can query: "What did we learn about OAuth security?" and get relevant answers from across all agents and sessions.

4. **Multi-Client Architecture** - Multiple terminal clients can connect to the same agent ecosystem. Work on project A in one terminal, project B in another, all agents share the same memory and can collaborate.

**Result:** Instead of constantly re-explaining context and re-researching topics, you build a personal AI team that accumulates knowledge and can tackle increasingly complex projects through coordination.

## Implementation Status

**v1.0 (Current Focus):**
- ✅ Terminal REPL foundation
- 🚧 OpenAI streaming integration
- 🚧 Single agent conversation
- 🚧 Mark/rewind implementation
- ⏳ Database persistence

**v2.0 (Future):**
- ⏳ Multi-agent support
- ⏳ Git worktree integration
- ⏳ Memory document store
- ⏳ Advanced navigation (key bindings)
- ⏳ Multi-LLM support

## Related Documentation

- [../architecture.md](../architecture.md) - Overall architecture
- [../v1-conversation-management.md](../v1-conversation-management.md) - Message lifecycle
- [../v1-database-design.md](../v1-database-design.md) - Database schema
- [../../multi-agent-ux.md](../../multi-agent-ux.md) - Original multi-agent concept
