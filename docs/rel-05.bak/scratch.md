# rel-05 Scratch Notes - Sub-Agent Architecture

**Status:** Architectural design in progress

## Vision

rel-05 primary goal: Add internal sub-agents to ikigai (within the C binary, NOT external TypeScript agents).

### Three-Phase Implementation

1. **Manual top-level agents**: User manually spawns additional top-level agents via `/spawn` command
2. **Agent-spawned sub-agents**: Agents create their own sub-agents (delegation pattern)
3. **Inter-agent communication**: Mailbox/inbox system for agent coordination (time permitting)

This document focuses on **Phase 1: Manual top-level agents**.

---

## Agent Naming Scheme

Agent IDs follow path-like structure with serial incrementing:

- Initial agent on launch: `0/`
- Next manually created: `1/`, `2/`, `3/`...
- Sub-agents: `0/0`, `0/1`, `0/2` (children of agent `0/`)
- Nested sub-agents: `1/1/3` (grandchild of agent `1/`)

**Properties:**
- Serial IDs never reused (even if agent killed)
- Forward slashes separate hierarchy levels
- Enables future hierarchical spawning

---

## User Experience

### Creating Agents

```bash
/spawn                  # Creates new top-level agent (1/, 2/, etc.)
```

Future: hotkey support for quick spawn.

### Switching Agents

```bash
Ctrl+Left Arrow         # Switch to previous agent in history
Ctrl+Right Arrow        # Switch to next agent in history
```

### Agent History Stack

Track which agents user has visited:
- Stack: `[current_agent, previous_agent, older_agent, ...]`
- Navigate back/forward through history
- When killing current agent: auto-switch to previous in stack

### Killing Agents

When killing the current agent:
1. Switch to previous agent in history
2. Kill the agent you just left
3. If no previous agent, stay on a default agent (probably `0/`)

---

## Current Architecture Analysis

### Monolithic `ik_repl_ctx_t`

Currently owns EVERYTHING:
- Terminal I/O (term, render, input_buffer, input_parser)
- Display (scrollback, layer_cake, all layers)
- Conversation state (conversation, marks, cfg)
- LLM interaction (multi, assistant_response, streaming buffers)
- Database (db_ctx, current_session_id)
- Tool execution (pending_tool_call, tool_thread)

**Problem:** No separation between shared infrastructure and per-agent state.

### Layer System (Good Foundation)

Layers use raw pointers - don't own data, just reference it:
- `scrollback_layer` → wraps `ik_scrollback_t *`
- `input_layer` → wraps raw pointers to text/len/visibility
- `separator_layer` → wraps visibility flag
- `spinner_layer` → wraps `ik_spinner_state_t *`

**Benefit:** Layers already designed for swappable backing state.

---

## Proposed Architecture: Dependency Injection + Agent Contexts

### Phase 0: DI Refactor - Create `ik_shared_ctx_t`

Extract shared infrastructure into explicit context:

```c
typedef struct ik_shared_ctx_t {
    // Shared infrastructure (read-mostly, thread-safe)
    ik_cfg_t *cfg;                  // Global config
    ik_db_ctx_t *db_ctx;            // Database connection pool/handle
    int64_t session_id;             // All agents share one session

    // Shared I/O (only current agent uses, but all need reference)
    ik_term_ctx_t *term;            // Single terminal
    ik_render_ctx_t *render;        // Rendering context

    // Shared history
    ik_history_t *history;          // Command history shared across agents

    // Debug infrastructure
    ik_debug_pipe_manager_t *debug_mgr;
    bool debug_enabled;

    // Agent coordination (protected by mutex)
    pthread_mutex_t agent_list_mutex;

} ik_shared_ctx_t;
```

**Why this first?**
- Establishes clear ownership boundaries
- Makes threading model explicit
- Easy to test (inject mock shared context)
- Cleaner agent creation API

### Phase 1: Extract `ik_agent_ctx_t`

Per-agent state (each agent owns its conversation and display):

```c
typedef struct ik_agent_ctx_t {
    // Identity
    char *agent_id;                    // "0/", "1/", "0/0", etc.
    size_t numeric_id;                 // Just the number part

    // Display state (per-agent)
    ik_scrollback_t *scrollback;       // Agent's conversation history display
    ik_layer_cake_t *layer_cake;       // Agent's layer stack
    ik_layer_t *scrollback_layer;
    ik_layer_t *spinner_layer;
    ik_layer_t *separator_layer;
    ik_layer_t *input_layer;

    // Input state (per-agent - preserves partial composition)
    ik_input_buffer_t *input_buffer;   // Each agent has own input buffer

    // Conversation state (per-agent)
    ik_openai_conversation_t *conversation;  // Messages for LLM
    ik_mark_t **marks;
    size_t mark_count;

    // LLM interaction state (per-agent)
    struct ik_openai_multi *multi;      // curl_multi handle
    int curl_still_running;
    ik_repl_state_t state;              // IDLE / WAITING_FOR_LLM / EXECUTING_TOOL
    char *assistant_response;           // Accumulated response
    char *streaming_line_buffer;
    char *http_error_message;
    char *response_model;
    char *response_finish_reason;
    int32_t response_completion_tokens;

    // Tool execution (per-agent)
    ik_tool_call_t *pending_tool_call;
    pthread_t tool_thread;
    pthread_mutex_t tool_thread_mutex;
    bool tool_thread_running;
    bool tool_thread_complete;
    TALLOC_CTX *tool_thread_ctx;
    char *tool_thread_result;
    int32_t tool_iteration_count;

    // Spinner state (per-agent)
    ik_spinner_state_t spinner_state;

    // Viewport state (per-agent)
    size_t viewport_offset;

    // Layer visibility flags (referenced by layers via raw pointers)
    bool separator_visible;
    bool input_buffer_visible;

    // Thread for this agent (Phase 2)
    pthread_t agent_thread;
    pthread_mutex_t agent_mutex;        // Protects scrollback access
    bool thread_running;

} ik_agent_ctx_t;
```

### Modified `ik_repl_ctx_t` (Session/Coordinator)

```c
typedef struct ik_repl_ctx_t {
    // Shared infrastructure
    ik_shared_ctx_t *shared;           // DI: all shared resources

    // Input parser (shared - stateless)
    ik_input_parser_t *input_parser;

    // Agent management
    ik_agent_ctx_t **agents;            // Array of all agents
    size_t agent_count;
    size_t agent_capacity;
    size_t current_agent_idx;           // Which agent is attached to input/render
    size_t next_agent_serial;           // For "0/", "1/", "2/" numbering

    // Exit flag
    atomic_bool quit;

} ik_repl_ctx_t;
```

---

## Key Design Decisions

### Input Buffer: Per-Agent
Each agent has its own `ik_input_buffer_t`. When switching agents, partially composed input is preserved.

### Command History: Shared
Arrow up/down history is shared across all agents (for now). May revisit in future releases.

### Tool/LLM Execution: Per-Agent Background
Each agent continues tool execution and LLM conversations in background when you switch away. No blocking.

### Layer Cake: Per-Agent
Each agent owns its own layer_cake. Enables future customization (different agents could show different layers).

### Threading Model: All Agents Always Run

**CRITICAL PRINCIPLE:** All agents execute continuously. Switching only changes I/O attachment.

Each agent runs its event loop:
1. Process any pending LLM streaming chunks (curl_multi)
2. Process any pending tool completion (pthread)
3. Current agent: also polls terminal for input
4. Non-current agents: no terminal input, but still process LLM/tool events

**There is no "background" vs "foreground" distinction.** All agents are always running. The only difference is which agent receives keyboard input and renders to terminal.

---

## Database Schema Changes

Add columns to messages table:

```sql
ALTER TABLE messages ADD COLUMN agent_id TEXT NOT NULL DEFAULT '0/';
```

- `session_id` - shared by all agents in one ikigai launch
- `agent_id` - identifies which agent owns the message ("0/", "1/", "0/0", etc.)

Partition messages by `(session_id, agent_id)` for agent conversation history.

---

## Rendering/Switching Mechanics

### Switching Flow

1. User presses Ctrl+Left or Ctrl+Right
2. Detach current agent's layer_cake from render
3. Find target agent in history/list
4. Attach target agent's layer_cake to render
5. Update `current_agent_idx`
6. Update agent history stack
7. Render new agent's scrollback + input

### What Gets Swapped

- Scrollback buffer (agent's conversation history)
- Input buffer (agent's partial composition)
- Layer cake (references agent's display state)

### What Stays Shared

- Terminal (ik_term_ctx_t)
- Render context (ik_render_ctx_t)
- Database connection
- Config
- Command history

---

## Implementation Phases

### Phase 0: DI Refactor (Foundation)
**Goal:** Establish clean separation of shared vs per-agent state

Tasks:
1. Create `ik_shared_ctx_t` with minimal fields
2. Refactor `ik_repl_init()` to create shared context first
3. Update functions to take `ik_shared_ctx_t *shared` where appropriate
4. Keep `ik_repl_ctx_t` as "main agent" for now
5. Verify everything still works (`make check`)

**Deliverable:** Clean DI pattern established, no behavioral changes.

### Phase 1: Multi-Agent Foundation
**Goal:** Multiple agents in memory, all running concurrently, manual switching works

Tasks:
1. Extract agent-specific state into `ik_agent_ctx_t` (including per-agent curl_multi)
2. Refactor existing code to use agent context
3. Add agent array management in `ik_repl_ctx_t`
4. Main loop iterates all agents for curl_multi_perform() (all agents always active)
5. Implement `/spawn` command (creates new agent, starts its event loop, max 20)
6. Implement Ctrl+Left/Right circular navigation (wraps around)
7. Implement `/kill` command (agent 0 cannot be killed)
8. Update separator to display current agent ID
9. Implement `/agents` command (list all agents with state)
10. Database: add `agent_id` column to messages table
11. Lazy SIGWINCH: reflow only current agent, others reflow on switch

**CRITICAL:** Switching agents only changes which agent receives input and renders. All agents continue executing. There is no "active" vs "inactive" distinction.

**Deliverable:** Can create multiple agents, switch between them. Non-visible agents continue LLM streaming and tool execution. Switching back shows accumulated results.

### Phase 2: Agent-Spawned Sub-Agents
**Goal:** Agents can spawn their own sub-agents (delegation pattern)

Tasks:
1. Implement hierarchical agent IDs (`0/0`, `0/1`, `1/0/0`)
2. Agent can issue `/spawn` programmatically (tool call)
3. Parent-child relationships tracked
4. Sub-agent results flow back to parent
5. Lifecycle management (kill parent = kill children?)

**Note:** Threading is NOT Phase 2. Phase 1 already has all agents running concurrently via existing curl_multi and pthread patterns. Phase 2 is about agent hierarchy, not concurrency.

**Deliverable:** Agent 0 can delegate task to sub-agent 0/0, receive results back.

### Phase 3: Mailboxes/Inboxes (If Time Permits)
**Goal:** Agents can send/receive messages

Tasks:
- `/send agent-id message`
- `/receive` (blocking)
- `/check` (non-blocking)
- Database: inbox table or message queue
- Cross-agent communication primitives

**Deliverable:** Parent agent can spawn sub-agent, receive result via inbox.

---

## Open Questions

1. **Database connection threading**: Single shared connection with mutex? Connection pool? Per-agent connections?
2. **Terminal resize**: How does SIGWINCH propagate to all agent scrollback buffers?
3. **Agent cleanup**: When agent finishes, keep in memory for history viewing? Or delete immediately?
4. **Error handling**: If background agent crashes, how does user see the error?
5. **Resource limits**: Max agents per session?

---

## References

- `docs/opus/agents.md` - Early design discussions on agent types
- `docs/opus/agent-queues.md` - Queue/inbox operation designs
- `docs/TODO.md` - rel-05 focus: sub-agents and concurrency
- `docs/autonomous-agents.md` - External agent vision (NOT this release)

---

## Next Steps

1. Create task files for Phase 0 (DI refactor)
2. Create task files for Phase 1 (multi-agent foundation)
3. Start implementation with Phase 0
