# Ikigai Documentation Index

This documentation is primarily for AI agents and secondarily for humans.

## Project Overview

**Ikigai** is an agent platform - an operating environment where you build, deploy, and coordinate living agents. Design agents interactively through the terminal (ikigai), deploy them to the runtime (iki-genba), and let them coordinate through process primitives (fork, mailbox, signals) and structured memory.

An agent platform combining:
- **ikigai** (terminal): Interactive agent builder with direct LLM integration
- **iki-genba** (runtime): Autonomous agent deployment environment
- **Structured Memory**: 4-layer context system (pinned blocks, auto-summary, sliding window, archival)
- **Agent Process Model**: Fork, signals, mailbox communication, process tree
- **Unified Interface**: `ikigai://` URIs, same tools for files and StoredAssets

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
- **[context-driven-development.md](context-driven-development.md)** - CDD: What Ikigai is and why context engineering matters
- **[decisions/](decisions/)** - Architecture Decision Records (ADRs) for key design choices
- **[repl/](repl/)** - REPL terminal interface documentation (rel-01 - complete)

### Development Standards
- **[git-workflow.md](git-workflow.md)** - Git workflow with release branches and worktrees
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

## Roadmap to MVP

### rel-01: REPL Terminal Foundation (complete)

**Objective**: Terminal interface with direct rendering and UTF-8 support

**Features**:
- Direct terminal rendering (single write per frame)
- UTF-8 support (emoji, CJK, combining characters)
- Multi-line input with readline-style shortcuts
- Scrollback buffer with O(1) arithmetic reflow
- Viewport scrolling (Page Up/Down, auto-scroll)
- 100% test coverage


### rel-02: LLM Integration (complete)

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


### rel-03: Database Integration (complete)

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


### rel-04: Local Tool Execution (complete)

**Objective**: Enable file operations and shell commands

**Features**:
- Tool interface design
- File operations (read, write, search)
- Shell command execution
- Results flow back to conversation


### rel-05: Agent Process Foundation (complete)

**Objective**: Prepare architecture for multi-agent support with per-agent state isolation

**Features**:
- Agent context structure (`ik_agent_ctx_t`) with UUID-based identity
- State extraction from monolithic repl_ctx to agent_ctx (display, input, conversation, LLM, tool, spinner)
- CSI u keyboard protocol (Kitty) with XKB layout-aware key translation
- Scroll detector state machine with burst absorption for smooth scrolling


### rel-06: Background Agents (complete)

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


### rel-07: Multi-LLM Provider Support (complete)

**Objective**: Abstract support for multiple AI providers with unified internal interface

**Features**:
- Provider abstraction layer with normalized streaming, errors, and tool calling
- Core 3 providers: OpenAI (refactor existing), Anthropic, Google
- Extended thinking abstraction (`/model MODEL/THINKING` with none/low/med/high levels)
- Per-message provider/model tracking in event history for cost and replay
- Separate config.json (settings) and credentials.json (API keys)


### rel-08: External Tool Architecture (complete)

**Objective**: Zero-overhead custom tools via JSON protocol

**Features**:
- External tool infrastructure (discovery, registry, execution)
- 6 production tools: `bash`, `grep`, `glob`, `file_read`, `file_write`, `file_edit`
- JSON I/O protocol with `--schema` self-description
- 3-tier discovery: system (`/usr/share/ikigai/tools/`), user (`~/.ikigai/tools/`), project (`.ikigai/tools/`)
- Directory precedence: project overrides user overrides system
- Commands: `/tool [NAME]` (inspect), `/refresh` (reload all tools)
- 100% test coverage maintained


### rel-09: Web Tools (complete)

**Objective**: Web access as external tools following rel-08 architecture

**Features**:
- `web_fetch` - Fetch URL content, convert HTML to markdown
- `web_search` - Brave Search API (2,000 free queries/month)
- Each tool manages its own credentials


### rel-10: Pinned Documents, URI Mapping & List Tool (in progress)

**Objective**: System prompt assembly, internal URI scheme, and per-agent list management

**Features**:
- `/pin` and `/unpin` commands for managing pinned documents
- Document cache with `ik://` URI support
- Bidirectional `ik://` to filesystem path translation
- `list` tool with deque operations (lpush, rpush, lpop, rpop, lpeek, rpeek, list, count)
- UI improvements (braille spinner, flicker elimination)
- Complete readline/editline control key support (Ctrl+K kill-to-end, etc.)


### rel-11: System prompts, skills and tools (future)

**Objective**: Layered primitives for tool/prompt configuration

**Features**:
- `/toolset` - Control which tools are available to agents
- `/skillset` - Control which prompts/memory are pinned
- `/mode` - Named bundles of toolset + skillset (e.g., planning, research, review)
- User-defined modes in `~/.ikigai/modes/` or `.ikigai/modes/`
- `!` prefix for user-defined prompt commands (distinct from `/` tools)


### rel-12: Internal Tools (future)

**Objective**: Expose slash commands as tools for agent self-management

**Features**:
- Agents can invoke `/fork`, `/kill` for spawning and terminating sub-agents
- Agents can invoke `/send`, `/check-mail`, `/read-mail` for inter-agent communication
- Agents can invoke `/pin`, `/unpin` for managing their own context
- Tool wrappers translate slash commands to LLM-callable tool definitions


### rel-13: Tool Sets (future)

**Objective**: Named collections of tools for task-specific and model-optimized configurations

**Features**:
- Full tool index with all available tools registered
- Named tool collections (sets) with include/exclude lists
- Active set switching at runtime
- Per-model defaults (match tools to model training data)
- Per-task profiles (coding, research, file-heavy workflows)


### rel-14: Token Estimation (future)

**Objective**: Local token counting for pre-send estimates and context window warnings

**Features**:
- Implement `libikigai-tokenizer` library with BPE algorithm
- Embed vocabularies for OpenAI, Google, Meta, xAI (~15-20MB)
- Fallback estimation for Anthropic (cl100k_base, ~80-90% accuracy)
- Display `~NUMBER` during composition, exact count after response


### rel-15: User Experience (future)

**Objective**: Polish configuration, discoverability, and customization workflows

**Features**:
- Separate credentials from config ([design](backlog/config-credentials-split.md))
- Layered resolution for commands, skills, and personas ([design](backlog/agents-layered-resolution.md))
- Status bar showing live agent count and total memory usage (via `talloc_total_size`)


### rel-16: Codebase Refactor & MVP Release (future)

**Objective**: Improve code organization, reduce complexity, and clean up technical debt

**Features**:
- Reorganize source into subfolder-per-module structure with one public header each
- Standardize naming conventions and design patterns across modules
- Remove unnecessary abstractions (e.g., `layer_wrappers.c`)
- Consolidate and clean up test structure
- Improve dependency injection consistency

### Far Future: Vision

Intelligent context management through comprehensive RAG infrastructure. Automated indexing and summarization will maintain conversation history, with retrieval systems constructing optimal context for each LLM request. The model gains tools to search and retrieve historical messages, while the architecture introduces concurrency foundations for background processing of RAG operations.

### Far Future: Interactive Shell

`bash_interactive` tool for long-running interactive shell sessions. PTY management, streaming I/O, session persistence. Enables REPL workflows, debugging sessions, and processes requiring ongoing input.

### Far Future: LSP Integration

Language Server Protocol support for code intelligence. Go to definition, find references, hover info, workspace symbols. Enables precise code navigation without regex guessing.

### Far Future: Notebook Support

Jupyter notebook editing with cell-level operations. Read, replace, insert, and delete cells. Enables data science and scientific computing workflows.

---
