# Ikigai Documentation Index

This documentation is primarily for AI agents and secondarily for humans.

## Project Overview

**Ikigai** is an agentic orchestration platform - an operating environment where you develop, deploy, and test living agents through the terminal (Ikigai) on your laptop, then deploy them to a production runtime PaaS, Iki-Genba.


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
- **[build-system.md](build-system.md)** - Modular build system, check-* targets, harness integration
- **[build-system-roadmap.md](build-system-roadmap.md)** - Future: Make as dependency query engine
- **[lcov_exclusion_strategy.md](lcov_exclusion_strategy.md)** - Coverage exclusion guidelines

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
- `/toolset` command to filter which tools are visible to the LLM during a session
- UI improvements (braille spinner, flicker elimination)
- Complete readline/editline control key support (Ctrl+K kill-to-end, etc.)


### rel-11: Internal Tools (complete)

**Objective**: Expose agent operations as LLM-callable tools via in-process execution

**Features**:
- Internal tool type in unified registry (single alphabetized tool list, no LLM distinction)
- 4 internal tools: `fork`, `kill`, `send`, `wait`
- Two-phase execution: worker thread does real work, main thread handles `repl->agents[]` manipulation
- `wait` tool with fan-in semantics: wait for multiple sub-agents, structured per-agent results, PG LISTEN/NOTIFY wake-up
- `/capture` and `/cancel` human-only commands for composing sub-agent tasks without LLM seeing the content
- `/reap` human-only command for removing dead agents from nav rotation (kill marks dead, reap cleans up)
- Pending prompt mechanism for fork tool (worker creates child, main loop starts its LLM stream)


### rel-12: HTTP API for External Agents (future)

**Objective**: Enable external processes to spawn and communicate with agents

**Features**:
- Fork null agent (starting point on which you set model, tools, etc...)
- Support basic message exchange (no tools)
- Build minimal external agent to demo


### rel-13: Dynamic Sliding Context Window (future)

**Objective**: Manage context window efficiently with automatic history management

**Features**:
- History clipping
- Recent summary


### rel-14: Parallel Tool Execution (future)

**Objective**: Parallelize read only tool calls

**Features**:
- Parallelize read only tool calls


### rel-15: Per-Agent Configuration (future)

**Objective**: Implement a runtime per-agent configuration system

**Features**:
- Default configuration defined purely in code (no config files)
- `/config get|set KEY=value` slash commands for modifying the current agent's config
- Deep copy inheritance on fork (children inherit parent's config at fork time)
- Support for named config templates managed via `/config template` commands
- Extended fork capabilities (`--from`, `--config`) to use templates or other agents as config sources


### rel-16: Additional AI Providers (future)

**Objective**: Expand provider support beyond core three

**Features**:
- Support X.AI (Grok)
- Support Ollama (local LLMs)
- Evaluate OpenRouter


### rel-17: Token Estimation (future)

**Objective**: Local token counting for pre-send estimates and context window warnings

**Features**:
- Implement `libikigai-tokenizer` library with BPE algorithm
- Embed vocabularies for OpenAI, Google, Meta, xAI (~15-20MB)
- Fallback estimation for Anthropic (cl100k_base, ~80-90% accuracy)
- Display `~NUMBER` during composition, exact count after response


### rel-18: User Experience (future)

**Objective**: Polish configuration, discoverability, and customization workflows

**Features**:
- lots of tab auto completion
- improved status bar


### rel-19: Codebase Refactor & MVP Release (future)

**Objective**: Improve code organization, reduce complexity, and clean up technical debt

**Features**:
- Reorganize source into subfolder-per-module structure with one public header each
- Standardize naming conventions and design patterns across modules
- Remove unnecessary abstractions (e.g., `layer_wrappers.c`)
- Consolidate and clean up test structure
- Improve dependency injection consistency
