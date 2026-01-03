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

### Memory Partitioning

Context is explicitly partitioned into layers with defined budgets:

| Layer | Budget | Purpose |
|-------|--------|---------|
| **Pinned Blocks** | 100k tokens | Curated persistent knowledge |
| **Auto-Summary** | 10k tokens | Compressed history index |
| **Sliding Window** | 90k tokens | Recent conversation |
| **Archival** | unlimited | Everything forever |

**Sliding Window**: FIFO eviction when full. Everything preserved in archival first.

**Auto-Summary**: Background agents compress evicted history by recency (this session → yesterday → this week). Headlines only - triggers `/recall` for full content.

**Pinned Blocks**: Knowledge that compounds across sessions. User-controlled via `/pin` and `/unpin`.

**Archival**: Zero context cost. PostgreSQL storage. Searchable via `/recall`.

**Agent Self-Management**: Agents can manage their own context via slash commands:
- `/mark` - Create checkpoint before risky operations
- `/rewind` - Rollback to checkpoint if approach fails
- `/forget` - Remove irrelevant content to free space
- `/remember` - Extract valuable knowledge to pinned blocks

Agents don't just receive context - they actively shape it.

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
| `/check-mail` | Read incoming messages |
| `/kill` | Terminate agent |

Agents coordinate through mailbox messaging and shared StoredAssets (`ikigai://` URIs).

### External Tools

The default mechanism. Any executable ikigai can access:
- System-installed tools
- Project scripts
- Tools packaged with ikigai

If it's in the path, reference it in config. Extending capabilities is trivial - no special protocol, no registration, no restart.

Minimal internal tools. External tools are first-class. Leverage model strengths at bash and Unix patterns.

## Design Philosophy

Simple primitives that compose. Each does one thing well. Power comes from composition.

You control exactly what memory, knowledge, capabilities, and communication channels each agent receives with every request.

## Why This Matters

LLM output quality depends on context quality.

Most tools treat context as an afterthought - dump messages into a window and hope. Ikigai makes context engineering a first-class concern with explicit partitioning, defined budgets, skill sets, tool sets, and composable agent primitives.

## See Also

- [structured-memory/](structured-memory/) - Memory layer architecture
- [context-management.md](context-management.md) - Context commands
- [minimal-tool-architecture.md](minimal-tool-architecture.md) - Tool philosophy
- [agent-process-model.md](agent-process-model.md) - Agent hierarchy
