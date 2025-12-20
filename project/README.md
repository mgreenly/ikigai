# Ikigai Documentation Index

This documentation is primarily for AI agents and secondarily for humans.

## Project Overview

**Ikigai** is an agent platform - an operating environment where you build, deploy, and coordinate living agents. Design agents interactively through the terminal (ikigai), deploy them to the runtime (iki-genba), and let them coordinate through process primitives (fork, mailbox, signals) and structured memory.

An agent platform combining:
- **ikigai** (terminal): Interactive agent builder with direct LLM integration
- **iki-genba** (runtime): Autonomous agent deployment environment
- **Structured Memory**: 4-layer context system (pinned blocks, auto-summary, sliding window, archival)
- **Agent Process Model**: Fork, signals, mailbox communication, process tree
- **Unified Interface**: `ikigai://` URIs, same tools for files and memory blocks

## Quick Start

- **Source code**: `src/`
- **Build target**: `bin/ikigai`
- **Tests**: `tests/unit/`, `tests/integration/`, `tests/performance/`
- **Build**: `make` (debug), `make release`, `make check` (run tests)
- **Standard**: C17 with K&R style (120-char width)

See [build-system.md](build-system.md) for comprehensive build documentation.

## Documentation

### Architecture & Design
- **[architecture.md](architecture.md)** - System architecture, dependencies, and design principles
- **[decisions/](decisions/)** - Architecture Decision Records (ADRs) for key design choices
- **[repl/](repl/)** - REPL terminal interface documentation (rel-01 - complete)

### Development Standards
- **[naming.md](naming.md)** - Naming conventions and approved abbreviations
- **[memory.md](memory.md)** - Memory management with talloc, ownership rules, and patterns
- **[error_handling.md](error_handling.md)** - Error handling philosophy and decision framework
- **[error_patterns.md](error_patterns.md)** - Error handling patterns and best practices
- **[error_testing.md](error_testing.md)** - Error handling testing strategy
- **[memory+error_handling.md](memory+error_handling.md)** - Integration of memory and error handling
- **[generic-types.md](generic-types.md)** - Generic base implementation with type-safe wrappers

### Build & Testing
- **[build-system.md](build-system.md)** - Build system, quality gates, testing infrastructure, multi-distro support
- **[lcov_exclusion_strategy.md](lcov_exclusion_strategy.md)** - Coverage exclusion guidelines

### Agent Tools & Automation
- **[agent-scripts.md](agent-scripts.md)** - Script-based tools architecture (vs MCP), JSON output contract, discovery workflow

### Internal Analysis
- **[considerations.md](considerations.md)** - Candidate features and changes to consider for the future

## Roadmap

### rel-01: REPL Terminal Foundation

**Objective**: Production-ready terminal interface with direct rendering and UTF-8 support

**Features**:
- Direct terminal rendering (single write per frame)
- UTF-8 support (emoji, CJK, combining characters)
- Multi-line input with readline-style shortcuts
- Scrollback buffer with O(1) arithmetic reflow
- Viewport scrolling (Page Up/Down, auto-scroll)
- 100% test coverage

### rel-02: LLM Integration

**Objective**: Stream LLM responses directly to the terminal

**Features**:
- OpenAI API client with libcurl streaming
- Display AI responses in scrollback as chunks arrive
- Basic conversation flow (user input → API call → streamed response)
- Status indicators (loading spinner, error handling)
- Layer architecture (scrollback, spinner, separator, input)
- Slash commands (/clear, /mark, /rewind, /help, /model, /system)
- In-memory conversation state with checkpoint/rollback
- Mock verification test suite

### rel-03: Database Integration (PostgreSQL)

**Objective**: Persistent conversation history with optional database mode

**Features**:
- PostgreSQL schema with event stream model
- Automatic migration system (runs on startup)
- Session lifecycle with active detection (Model B)
- Message persistence at 5 integration points
- Replay algorithm for state reconstruction
- Integration testing with transaction isolation
- Session restoration on launch
- Memory-only fallback mode

### rel-04: Local Tool Execution

**Objective**: Enable file operations and shell commands

**Features**:
- Tool interface design
- File operations (read, write, search)
- Shell command execution
- Results flow back to conversation

### rel-05: Agent Process Foundation

**Objective**: Prepare architecture for multi-agent support with per-agent state isolation

**Features**:
- Agent context structure (`ik_agent_ctx_t`) with UUID-based identity
- State extraction from monolithic repl_ctx to agent_ctx (display, input, conversation, LLM, tool, spinner)
- CSI u keyboard protocol (Kitty) with XKB layout-aware key translation
- Scroll detector state machine with burst absorption for smooth scrolling

### Future: Background Agents

**Objective**: Support multiple concurrent agents with human and LLM spawning

**Features**:
- Multiple top-level agents (human-spawned via command or hotkey)
- LLM-spawned sub-agents for parallel task execution
- Refactor existing single-agent code for multi-agent architecture
- Hot-key navigation between active agents (e.g., Alt+1, Alt+2)
- Agent lifecycle management (spawn, switch, terminate)
- Slash commands for agent control (/agents, /spawn, /switch, /kill)
- Visual indicator showing current agent and status of all agents
- Inter-agent communication (optional: message passing between agents)

**Rationale**: Complex tasks benefit from parallel execution. Human users may want multiple conversations active simultaneously. LLMs can delegate subtasks to specialized sub-agents, improving throughput and enabling divide-and-conquer workflows.

### Future: Multi-LLM Provider Support

**Objective**: Abstract support for multiple AI providers with unified internal interface

**Features**:
- Provider abstraction layer with normalized streaming, errors, and tool calling
- Core 3 providers: OpenAI (refactor existing), Anthropic, Google
- Extended thinking abstraction (`/model MODEL/THINKING` with none/low/med/high levels)
- Per-message provider/model tracking in event history for cost and replay
- Separate config.json (settings) and credentials.json (API keys)

**Deferred**: xAI, Meta, OpenRouter, multimodal, RAG, prompt caching.

### Future: Web Search Tool/Interface

**Objective**: Enable agents to search the web with zero-setup defaults and flexible provider support

**Features**:
- Abstract search interface (`ik_web_search()`) with structured results
- DuckDuckGo as default provider (no API key required)
- Additional providers: Brave (free tier), Tavily (AI-optimized), Google (paid)
- Provider configuration with credentials separated from config
- No silent fallback - explicit error handling on provider failure
- WebFetch tool for fetching and processing web content (docs, APIs)

**Rationale**: Web search is essential for agents to access current information. DuckDuckGo default ensures zero-friction setup, while additional providers support different needs (privacy, AI-optimization, coverage). Explicit configuration prevents unexpected behavior or costs.

### Future: Minimal Tool Architecture

**Objective**: Small set of internal tools; everything else via bash

**Internal tools** (privileged operations only):
- Shell: `bash`
- Platform: `web_search`, `slash_command`
- Sub-agents: `fork`, `send`, `check-mail`, `kill` ([design](sub-agent-tools.md))
- Task list: Redis-style deque operations ([design](todo-tools.md))

**Everything else through bash**: file ops, search, git, builds. System prompt teaches patterns; output limit (~10KB) prevents context flooding.

**Extension model**: Skills + external scripts replace MCP. Progressive discovery from system prompt → skills → project tools.

**Rationale**: LLMs already excel at bash. See [minimal-tool-architecture.md](minimal-tool-architecture.md).

### Future: Memory Documents

**Objective**: Database-backed document storage accessible to users and agents

**Features**:
- `ikigai:///` URI scheme for database-stored documents
- Slash commands for user access (`/memory edit|list|delete`)
- External editor integration ($EDITOR workflow)
- Prompt expansion with `@` (files) and `@@` (memory) markers
- Fuzzy finder for file and memory document selection
- Background file list with configurable include patterns

**Rationale**: Agents need persistent storage for research, patterns, and decisions that shouldn't clutter the git workspace. Users need intuitive access to reference this knowledge in prompts. See [memory-documents.md](memory-documents.md).

### Future: Codebase Refactor

**Objective**: Improve code organization, reduce complexity, and clean up technical debt

**Features**:
- Reorganize source into subfolder-per-module structure with one public header each
- Standardize naming conventions and design patterns across modules
- Remove unnecessary abstractions (e.g., `layer_wrappers.c`)
- Consolidate and clean up test structure
- Improve dependency injection consistency

### Future: Agent Configuration & Prompts

**Objective**: Clean up .ikigai/.agents structure and system prompt handling

**Features**:
- System prompts: defaults in code, user overrides stored in database
- Standardize directory structure: `personas/`, `skills/`, `commands/`, `scripts/`
- Adopt Claude Code patterns for skills and commands, keep personas concept
- Clear separation between ikigai system-provided and user-defined values

### Future: Docker Compose Deployment

**Objective**: Enable cross-platform evaluation and development via containerized environment

**Features**:
- Dockerfile with build dependencies and PostgreSQL client
- Docker Compose configuration with ikigai and postgres services
- Volume-mounted config for secrets, named volumes for persistence
- Setup and usage documentation

**Rationale**: Currently builds only on Debian. Docker enables evaluation on macOS/Windows and supports contributors on unsupported platforms. See [docker-compose.md](docker-compose.md).

### Future: Token Estimation

**Objective**: Local token counting for pre-send estimates and context window warnings

**Features**:
- Implement `libikigai-tokenizer` library with BPE algorithm
- Embed vocabularies for OpenAI, Google, Meta, xAI (~15-20MB)
- Fallback estimation for Anthropic (cl100k_base, ~80-90% accuracy)
- Display `~NUMBER` during composition, exact count after response

**Rationale**: Currently token counts only appear after API response. Local estimation enables real-time feedback while typing and warnings before hitting context limits. See [project/tokens/](tokens/) for research.

### Future: Tool Sets

**Objective**: Named collections of tools for task-specific and model-optimized configurations

**Features**:
- Full tool index with all available tools registered
- Named tool collections (sets) with include/exclude lists
- Active set switching at runtime
- Per-model defaults (match tools to model training data)
- Per-task profiles (coding, research, file-heavy workflows)

**Rationale**: Builds on Minimal Tool Architecture. Different models perform better with different tool presentations. Task context varies widely - a coding task needs different tools than research. Named sets allow tuning for model training alignment and controlling context usage.

### Future: User Experience

**Objective**: Polish configuration, discoverability, and customization workflows

**Features**:
- Separate credentials from config ([design](backlog/config-credentials-split.md))
- Layered resolution for commands, skills, and personas ([design](backlog/agents-layered-resolution.md))
- Status bar showing live agent count and total memory usage (via `talloc_total_size`)

### Far Future: Interactive Shell

`bash_interactive` tool for long-running interactive shell sessions. PTY management, streaming I/O, session persistence. Enables REPL workflows, debugging sessions, and processes requiring ongoing input.

### Far Future: Vision

v2.0 focuses on intelligent context management through comprehensive RAG infrastructure. Automated indexing and summarization will maintain conversation history, with retrieval systems constructing optimal context for each LLM request. The model gains tools to search and retrieve historical messages, while the architecture introduces concurrency foundations for background processing of RAG operations.

---

**Note**: This documentation is maintained for AI agents and developers. Keep it concise, accurate, and current.
