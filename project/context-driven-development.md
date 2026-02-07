# Context Driven Development (CDD)

Ikigai is a Context Driven Development platform.

**CDD**: Engineer the context provided to the LLM with every request.

## Core Focus

Define exactly what the LLM sees when it processes your request. This is the central value proposition.

## Context Dimensions

Ikigai provides explicit control over every dimension of LLM context:

| Dimension | Mechanism | Controls |
|-----------|-----------|----------|
| **Memory** | Partitioned layers with budgets | What history the agent sees |
| **Knowledge** | Skill Sets | What domain knowledge the agent has |
| **Capabilities** | Tool Sets | What actions the agent can take |
| **Communication** | Agent hierarchy | What other agents it can interact with |

### Pinned Documents

The system prompt is assembled from pinned documents. User-controlled via `/pin` and `/unpin`:

- `/pin PATH` - Add a document to the system prompt
- `/unpin PATH` - Remove a document from the system prompt
- `/pin` (no args) - List currently pinned documents

Documents are concatenated in FIFO order. Both filesystem paths and `ik://` URIs are supported.

Forked agents inherit their parent's pinned documents at fork time.

### Skill Sets

Define what knowledge an agent loads.

A skill set is a named collection of skills. Skills are markdown files teaching domain knowledge and introducing relevant tools. `/skillset developer` loads database, errors, git, TDD, style, and other development skills.

- **Preload**: Skills loaded immediately when skill set activates
- **Advertise**: Skills available on-demand via `/load`

Different roles need different knowledge. A planner needs research skills. An implementor needs coding skills. Skill sets control what knowledge is in context.

### Tool Sets

Define what tools an agent advertises.

A tool set is a named collection of tools. Each agent gets precisely the capabilities it needs. File read but not write. Spawn but not kill.

Tools are the authorization model. No complex permissions - just curated tool sets per agent.

### Agent Hierarchy

Unix/Erlang-inspired process model:

| Primitive | Purpose |
|-----------|---------|
| `/fork` | Create child agent |
| `/send` | Message another agent |
| `/wait` | Wait for messages (blocking, with fan-in) |
| `/kill` | Terminate agent |

Agents coordinate through mailbox messaging and shared StoredAssets (`ikigai://` URIs).

### External Tools

The default mechanism. Any executable ikigai can access:
- System-installed tools
- Project scripts
- Tools packaged with ikigai

If it's in the path, reference it in config. Extending capabilities is trivial - no special protocol, no registration, no restart.

External tools for user-extensible capabilities. Internal tools only for agent operations (fork, kill, send, wait) requiring in-process shared state access.

## Design Philosophy

Simple primitives that compose. Each does one thing well. Power comes from composition.

You control exactly what memory, knowledge, capabilities, and communication channels each agent receives with every request.

## Why This Matters

LLM output quality depends on context quality.

Most tools treat context as an afterthought - dump messages into a window and hope. Ikigai makes context engineering a first-class concern with explicit partitioning, defined budgets, skill sets, tool sets, and composable agent primitives.

## See Also

- [context-management.md](context-management.md) - Context commands
- [external-tool-architecture.md](external-tool-architecture.md) - External tools architecture
- [agent-process-model.md](agent-process-model.md) - Agent hierarchy
