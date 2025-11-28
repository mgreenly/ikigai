# Database Architecture

**Core Principle:** A shared PostgreSQL database is the single source of truth for all agents, conversations, and knowledge across all ikigai client instances.

## Multi-Client Architecture

### Clients are Views, Database is Truth

ikigai supports **multiple independent client processes** connecting to the same shared database:

```
┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐
│  Client 1       │       │  Client 2       │       │  Client 3       │
│  ~/project-a    │       │  ~/project-b    │       │  ~/project-c    │
│  (terminal UI)  │       │  (terminal UI)  │       │  (terminal UI)  │
└────────┬────────┘       └────────┬────────┘       └────────┬────────┘
         │                         │                         │
         └─────────────────────────┼─────────────────────────┘
                                   │
                          ┌────────▼────────┐
                          │   PostgreSQL    │
                          │    Database     │
                          │                 │
                          │  • Agents       │
                          │  • Messages     │
                          │  • Memory docs  │
                          │  • Marks        │
                          └─────────────────┘
```

**Key characteristics:**
- Each client is just a terminal UI connected to the shared database
- Clients differentiated primarily by their working directory / project
- All agents are first-class database entities, not tied to a specific client
- Any client could potentially access any agent (though typically scoped to their project)
- Client processes can start/stop independently without affecting database state

### Why This Matters

**Persistence Beyond Client Lifecycle:**
- Close a client, agents and conversations persist in the database
- Open a new client on a different project, create new agents there
- All knowledge accumulates in the shared database

**True Multi-Project Support:**
- Work on project-a in one client
- Work on project-b in another client
- Both sets of agents coexist in the same database
- RAG tools can search across all projects or scope to specific ones

**Future Flexibility:**
- Database could be local PostgreSQL
- Or remote PostgreSQL instance
- Eventually could evolve to distributed storage system (PostgreSQL + document store + vector DB)
- For now: focus on single PostgreSQL instance providing all functionality

## Message Identity Model

Every message in the system has a **multi-layered identity** that enables powerful RAG queries later.

### Identity Layers

**Physical/Environmental Identity:**

- **User** - Who is running the client (e.g., "mgreenly")
  - From environment or configuration
  - Ties work to specific human users

- **Machine** - Which computer/host
  - Hostname, possibly IP address
  - Useful for distinguishing work environments (laptop vs workstation vs server)

- **Path** - Absolute working directory
  - `/home/mgreenly/projects/ikigai/main`
  - Multiple checkouts of same repo have different paths

- **Git Repo** - Repository identity
  - Remote URL (e.g., `git@github.com:mgreenly/ikigai.git`)
  - Determined by default push remote
  - Links messages to canonical repository

**Logical/Organizational Identity:**

- **Project** - Logical grouping of related work
  - Defaults to git repository name if inside a git worktree/bare repo
  - Falls back to `@username` or similar if not in a git repo
  - Controllable via `/project` command (future)
  - Scopes work: "all conversations about ikigai" vs "all conversations about other-project"

- **Agent** - The agent conducting the conversation
  - Agent name, branch, worktree path
  - Persistent database entity

- **Context Span** - Period between start/stop/clear (internal concept)
  - The mechanical boundary of "current context"
  - Not explicitly named in user-facing commands
  - Just the reality of how conversations work
  - Tracked internally for understanding conversation flow

- **Tags/Focus** - User-applied labels for organizing work
  - `/tag oauth-implementation` marks current period of work
  - `/focus "refactoring database layer"` sets what you're working on
  - Multiple tags can apply to same message
  - Searchable later: "show me all oauth-implementation tagged work"

### RAG Query Examples

The identity model enables powerful queries:

```sql
-- All work on ikigai project
WHERE project = 'ikigai'

-- Specific agent's work
WHERE agent_id = 42 AND project = 'ikigai'

-- Tagged work across all agents
WHERE tags CONTAINS 'oauth-implementation'

-- Work from specific machine/environment
WHERE machine = 'workstation' AND user = 'mgreenly'

-- Work in specific path (useful for multiple checkouts)
WHERE path = '/home/mgreenly/projects/ikigai/main'

-- Combinations
WHERE project = 'ikigai'
  AND tags CONTAINS 'refactoring'
  AND timestamp > '2024-01-01'
```

### Identity Capture

Message identity is captured at message creation time:

```c
typedef struct ik_message_identity_t {
    // Physical/Environmental
    char *user;           // "mgreenly"
    char *machine;        // "laptop" or hostname
    char *path;           // "/home/mgreenly/projects/ikigai/main"
    char *git_remote;     // "git@github.com:mgreenly/ikigai.git"

    // Logical/Organizational
    char *project;        // "ikigai" (derived or explicit)
    int64_t agent_id;     // Foreign key to agents table
    int64_t context_span_id;  // Internal tracking
    char **tags;          // ["oauth", "refactoring"]
    char *focus;          // "implementing authentication"

    // Temporal
    timestamp_t created_at;
} ik_message_identity_t;
```

## Project Concept

**Project** is a logical boundary for organizing work.

### Default Project Determination

When an agent is created:

1. **Inside git repo?** → Project = repository name from remote URL
   - `git@github.com:mgreenly/ikigai.git` → project = `ikigai`

2. **Not in git repo?** → Project = `@username`
   - Running outside any repo → project = `@mgreenly`

3. **Explicit override:** `/project set my-project-name` (future command)

### Project Use Cases

**Scoping RAG queries:**
- "Show me all discussions about database design in the ikigai project"
- "What patterns did we establish for error handling in project X?"

**Multi-project workflows:**
- Client 1 working on `ikigai` project
- Client 2 working on `other-tool` project
- Both store agents/messages in same database
- RAG tools can scope to one project or search across all

**Organization:**
- Groups related work together
- Even if spread across multiple agents, branches, timeframes
- Survives beyond any individual agent or conversation

## Memory Documents

**Memory documents are markdown files that live in the database, not the filesystem.**

Think of them as a **shared wiki** accessible to all agents across all clients and projects.

### Key Properties

**Database-Native:**
- Stored in PostgreSQL, not in git repositories
- Full-text search across all memory docs
- Tagging/categorization in database
- Referenced by ID or alias: `#mem-124` or `#mem-oauth/patterns`
- Versioning handled at database level (future)

**Shared Knowledge Base:**
- Any agent (from any client, in any project) can access them
- Survive beyond any individual conversation or agent
- Document patterns, decisions, research that spans projects
- Like a wiki that all your AI agents share

**Not in Git:**
- Memory docs are about the code, not part of the code
- No repository clutter
- No merge conflicts when multiple agents update them
- Can reference specific commits/branches without being in them

### Use Cases

**Cross-Project Patterns:**
- Document a useful pattern in project A
- Reference it when building project B
- Accumulate wisdom across all work

**Research Results:**
- Agent researches a topic, creates memory doc
- Any future agent can access that research
- No need to repeat expensive API/research work

**Architectural Decisions:**
- Document why you made certain choices
- Future agents understand context and rationale
- Prevents re-litigating decided questions

**API Documentation:**
- Create custom docs for APIs you use frequently
- Agents can reference these instead of fetching docs repeatedly
- Build up institutional knowledge

### Memory Doc Structure (Conceptual)

```c
typedef struct ik_memory_doc_t {
    int64_t id;              // Unique ID
    char *alias;             // "oauth/patterns" or NULL
    char *title;             // "OAuth 2.0 Implementation Patterns"
    char *content;           // Full markdown content
    char **tags;             // ["oauth", "security", "patterns"]
    char *project;           // "ikigai" or NULL for cross-project
    int64_t created_by_agent_id;
    timestamp_t created_at;
    timestamp_t updated_at;
    char *git_commit_ref;    // Optional reference to relevant commit
} ik_memory_doc_t;
```

## Context Span (Internal Concept)

**Context span** is the period between start/stop/clear events. It's an internal tracking concept, not a user-facing feature.

### What Defines a Context Span

- **Start:** Agent is created or context is cleared
- **End:** Agent stops (client closes) or `/clear` command issued
- **Purpose:** Track the mechanical boundaries of conversation context
- **Not user-facing:** No `/session` commands, no explicit naming

### Why Track It Internally

**Understanding conversation flow:**
- RAG can see natural breaks in conversation
- "This set of messages was all in one continuous context"
- vs "These messages span multiple context resets"

**Debugging and analysis:**
- How often do users clear context?
- How long do context spans typically last?
- Correlation between span length and conversation quality

**Not a replacement for tags:**
- Tags/focus are user-facing organizational tools
- Context span is mechanical tracking
- Both exist, serve different purposes

## Tags and Focus

**Tags** and **focus** are user-facing tools for organizing and describing work.

### Tags

Mark current period of work with searchable keywords:

```bash
/tag oauth-implementation
/tag refactoring
/tag bug-fix

# Messages from this point forward tagged with these labels
```

**Multiple tags:** A message can have multiple tags
**Persistent:** Tags stay active until changed
**Searchable:** RAG queries can filter by tags

### Focus

Describe what you're currently working on:

```bash
/focus "implementing OAuth 2.0 authentication"
/focus "refactoring database layer for performance"

# More descriptive than tags, natural language
```

**Human-readable:** Full sentences describing intent
**Context for RAG:** "What was I doing when I discussed X?"
**Optional:** Tags might be enough for many users

### Use in RAG

```bash
# User query: "What did we decide about OAuth?"
# RAG searches:
WHERE project = 'ikigai'
  AND (tags CONTAINS 'oauth' OR focus LIKE '%oauth%')
ORDER BY created_at DESC
LIMIT 20
```

Returns relevant messages tagged or focused on OAuth work.

## Future Evolution

### Storage System Abstraction

While focusing on PostgreSQL now, the system could evolve:

**Phase 1: Single PostgreSQL (now)**
- All data in PostgreSQL
- Simple, reliable, proven
- Sufficient for v1.0 and beyond

**Phase 2: Abstracted Storage System (future)**
- Storage layer becomes logical concept
- Behind the scenes could be:
  - PostgreSQL for transactional data (agents, metadata)
  - Document store for memory documents (MongoDB, etc.)
  - Vector DB for semantic search (pgvector, Pinecone)
  - Object storage for artifacts (S3, MinIO)
  - Cache layer for active conversations (Redis)

**Key principle:** All clients share same storage configuration
- Whether one database or composite system
- All ikigai clients configured to connect to same backend(s)
- Storage abstraction hides complexity from clients

### Multi-Tenant Support

Future consideration: Multiple users sharing same database:

- User identity becomes primary scope
- Projects scoped to users
- Memory docs can be private or shared
- Permissions and access control

Not required for initial versions (single-user focus).

## Implementation Notes

### Database Schema Sketch

```sql
-- Agents table
CREATE TABLE agents (
    id BIGSERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    project TEXT,
    user_id TEXT,
    created_at TIMESTAMP DEFAULT NOW()
);

-- Messages table
CREATE TABLE messages (
    id BIGSERIAL PRIMARY KEY,
    agent_id BIGINT REFERENCES agents(id),
    role TEXT NOT NULL,  -- 'user' or 'assistant'
    content TEXT NOT NULL,

    -- Identity tracking
    user_name TEXT,
    machine TEXT,
    path TEXT,
    git_remote TEXT,
    project TEXT,
    context_span_id BIGINT,
    tags TEXT[],
    focus TEXT,

    created_at TIMESTAMP DEFAULT NOW()
);

-- Memory documents table
CREATE TABLE memory_documents (
    id BIGSERIAL PRIMARY KEY,
    alias TEXT UNIQUE,
    title TEXT NOT NULL,
    content TEXT NOT NULL,
    tags TEXT[],
    project TEXT,
    created_by_agent_id BIGINT REFERENCES agents(id),
    created_at TIMESTAMP DEFAULT NOW(),
    updated_at TIMESTAMP DEFAULT NOW()
);

-- Context spans table (internal tracking)
CREATE TABLE context_spans (
    id BIGSERIAL PRIMARY KEY,
    agent_id BIGINT REFERENCES agents(id),
    started_at TIMESTAMP DEFAULT NOW(),
    ended_at TIMESTAMP
);
```

### Client Connection

Clients connect to PostgreSQL using connection string from config:

```bash
# ~/.config/ikigai/config
database_url = "postgresql://localhost/ikigai"

# Or environment variable
export IKIGAI_DATABASE_URL="postgresql://user:pass@host/dbname"
```

All clients using same database URL connect to same backend.

## Related Documentation

- [README.md](README.md) - Vision overview
- [multi-agent.md](multi-agent.md) - Multi-agent workflows
- [workflows.md](workflows.md) - Example usage patterns
- [../v1-database-design.md](../v1-database-design.md) - Detailed database schema
