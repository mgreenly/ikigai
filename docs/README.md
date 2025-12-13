# Ikigai Documentation Index

This documentation is primarily for AI agents and secondarily for humans.

## Project Overview

**Ikigai** is an experiment to build a Linux focused, terminal based coding agent with hierarchical sub-agents, RAG accessible permanent memory, progressive tool discovery and a dynamic sliding context window.

A desktop terminal application that combines:
- Direct LLM API integration (OpenAI, Anthropic, Google, X.AI)
- Local tool execution (file operations, shell commands, code analysis)
- PostgreSQL-backed conversation history with full-text search
- Terminal UI with direct rendering and scrollback

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

**Tasks**:
- Direct terminal rendering (single write per frame)
- UTF-8 support (emoji, CJK, combining characters)
- Multi-line input with readline-style shortcuts
- Scrollback buffer with O(1) arithmetic reflow
- Viewport scrolling (Page Up/Down, auto-scroll)
- 100% test coverage

### rel-02: LLM Integration

**Objective**: Stream LLM responses directly to the terminal

**Tasks**:
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

**Tasks**:
- PostgreSQL schema with event stream model
- Automatic migration system (runs on startup)
- Session lifecycle with active detection (Model B)
- Message persistence at 5 integration points
- Replay algorithm for state reconstruction
- Integration testing with transaction isolation
- Session restoration on launch
- Memory-only fallback mode

### rel-04: Local Tool Execution

**Objective**: Enable file operations, shell commands, and code analysis

**Tasks**:
- Tool interface design
- File operations (read, write, search)
- Shell command execution
- Code analysis (tree-sitter integration)
- Results flow back to conversation

### rel-05: Agent Process Foundation

**Objective**: Prepare architecture for multi-agent support with per-agent state isolation

**Tasks**:
- Agent context structure (`ik_agent_ctx_t`) with UUID-based identity
- State extraction from monolithic repl_ctx to agent_ctx (display, input, conversation, LLM, tool, spinner)
- CSI u keyboard protocol (Kitty) with XKB layout-aware key translation
- Scroll detector state machine with burst absorption for smooth scrolling

### Future: Background Agents

**Objective**: Support multiple concurrent agents with human and LLM spawning

**Tasks**:
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

**Objective**: Support multiple LLM providers with unified interface

**Tasks**:
- Abstract provider interface
- OpenAI, Anthropic, Google, X.AI implementations
- Provider switching via config or runtime command
- Unified conversation format

### Future: Codebase Refactor

**Objective**: Improve code organization, reduce complexity, and clean up technical debt

**Tasks**:
- Reorganize source into subfolder-per-module structure with one public header each
- Standardize naming conventions and design patterns across modules
- Remove unnecessary abstractions (e.g., `layer_wrappers.c`)
- Consolidate and clean up test structure
- Improve dependency injection consistency

### Future: Rich Tool Use

**Objective**: Expand tool capabilities and improve tool execution experience

**Tasks**:
- Parallel tool execution
- Better tool run inspection and visibility

**Tool Priorities** (compared against Claude Code's 18 tools, we have 5):

| Priority | Tool | Rationale |
|----------|------|-----------|
| High | Edit | Surgical text replacement, less context than file_write |
| High | BashOutput + KillShell | Async bash monitoring and termination |
| High | TodoWrite | Task tracking for complex multi-step work |
| Medium | AskUserQuestion | Interactive clarification from model |
| Medium | WebFetch | Fetch and process web content (docs, APIs) |
| Medium | WebSearch | General web search |
| Low | SlashCommand | Model self-invocation of commands |
| Low | Skill | Direct skill loading by model |

### Future: User Experience

**Objective**: Polish configuration, discoverability, and customization workflows

**Tasks**:
- Separate credentials from config ([design](backlog/config-credentials-split.md))
- Layered resolution for commands, skills, and personas ([design](backlog/agents-layered-resolution.md))

### Far Future: Vision

v2.0 focuses on intelligent context management through comprehensive RAG infrastructure. Automated indexing and summarization will maintain conversation history, with retrieval systems constructing optimal context for each LLM request. The model gains tools to search and retrieve historical messages, while the architecture introduces concurrency foundations for background processing of RAG operations.

---

**Note**: This documentation is maintained for AI agents and developers. Keep it concise, accurate, and current.
