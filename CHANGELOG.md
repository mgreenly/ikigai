# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [rel-05] - 2025-12-14

### Added

#### Agent Process Model - Phase 0 (Complete)
- **Agent context structure**: Created `ik_agent_ctx_t` with UUID-based identity (base64url, 22 chars)
- **State extraction**: Migrated per-agent state from monolithic repl_ctx to agent_ctx:
  - Display state (scrollback, viewport, marks)
  - Input state (input_buffer, saved input)
  - Conversation state (messages, history)
  - LLM state (connection, streaming)
  - Tool state (execution context)
  - Completion state (tab completion)
  - Spinner state (activity indicator)
- **Agent integration**: Added `current` agent pointer to repl_ctx
- **Architecture ready**: Single agent works, code organized for multi-agent support

#### Shared Context Architecture (Complete)
- **Shared context structure**: Created `ik_shared_ctx_t` for global/process-wide state
- **State migration**: Moved shared state from repl_ctx to shared_ctx:
  - Configuration (cfg)
  - Terminal interface (term)
  - Render context (render)
  - Database context (db_ctx, session_id)
  - Command history
  - Debug infrastructure
- **Clear separation**: Isolated per-agent state (agent_ctx) from shared state (shared_ctx)
- **Multi-agent ready**: Foundation for multiple agents sharing common infrastructure

#### Task Orchestration Infrastructure (Complete)
- **Script-based orchestration**: Deno scripts in `.ikigai/scripts/tasks/` for task management
- **Task queue management**: `next.ts`, `done.ts`, `escalate.ts`, `session.ts` scripts
- **Model/thinking escalation**: Automatic capability escalation on task failure with progress
- **Orchestration command**: `/orchestrate PATH` for automated task execution
- **Task refactoring**: Removed Agent sections, streamlined task file format

#### CSI u Keyboard Protocol (Kitty Protocol)
- **Protocol support**: Parse CSI u escape sequences for enhanced keyboard input
- **XKB integration**: libxkbcommon for keyboard layout-aware shifted character translation
- **Modified keys**: Support for modified Enter sequences and other special keys

#### Scroll Improvements
- **Scroll detector**: State machine with burst absorption for smooth scrolling
- **Mouse wheel**: Increased scroll lines from 1 to 3 for faster navigation
- **Burst threshold**: Reduced from 20ms to 10ms for more responsive mouse wheel

### Changed

#### Code Quality & Refactoring
- **Tool test helpers**: JSON test helpers for cleaner, more maintainable tool tests
- **Response builders**: Extracted tool response builder utilities to eliminate duplication
- **File reader extraction**: Created `ik_file_read_all` utility for consistent file reading
- **Logger dependency injection**: Added DI pattern to logger for better testability
- **Circular dependency fix**: Broke msg.h → db/replay.h circular dependency
- **Unused code removal**: Removed conversation.h and other unused modules
- **Test refactoring**: Applied JSON helpers across all tool execution tests

#### Build System
- **Release builds**: Fixed MOCKABLE wrapper issues for NDEBUG builds
- **Wrapper headers**: Added inline implementations for pthread, stdlib wrappers in release mode
- **Format warnings**: Fixed format-truncation and format-nonliteral warnings

#### Documentation Organization
- **Directory restructure**: Renamed `docs/` to `project/` for internal design documentation
- **User documentation**: Created new `docs/` directory for user-facing content
- **Reference updates**: Updated all internal references from `docs/` to `project/`
- **Infrastructure docs**: Moved iki-genba from `.agents/` to `.ikigai/` with documentation

#### Code Cleanup
- **Legacy logger**: Removed functions that break alternate buffer mode
- **Scroll debug**: Removed debug logging from viewport actions
- **File organization**: Split test files to comply with 16KB size limit

### Development

#### Testing & Quality Gates
- **Test coverage**: Maintained 100% (lines, functions, branches)
- **Helgrind fixes**: Fixed mutex destroy false positives and double-destroy issues
- **Thread safety**: Improved thread handling for sanitizer compliance
- **New test suites**: scroll_detector, terminal_csi_u, shared context tests
- **Coverage improvements**: Added LCOV exclusions for defensive branches, achieved clean coverage
- **Valgrind fixes**: Fixed log rotation race conditions and talloc false positives

#### Documentation
- **Agent process model**: Comprehensive design document for multi-agent architecture
- **Shared context design**: Documentation for global state architecture
- **Task orchestration**: Complete orchestration system documentation with script APIs
- **Phase 0 tasks**: Complete TDD task files for agent context extraction
- **Refactor tasks**: Task files for code quality improvements and technical debt reduction
- **Skill system**: Updated personas and added git, database, and other skill modules
- **Worktree management**: Documentation for git worktree usage and transaction logs

### Technical Metrics
- **Changes**: 700+ files modified, +40,000/-15,000 lines (estimated)
- **Commits**: 187 commits over development cycle
- **Test coverage**: 100% lines, functions, and branches
- **Code quality**: All lint, format, and sanitizer checks pass

## [rel-04] - 2025-12-07

### Added

#### Local Tool Execution (Complete)
- **Tool dispatcher**: Route tool calls by name to execution handlers
- **File operations**: file_read and file_write tools with output truncation
- **Code search**: glob and grep tools for codebase exploration
- **Shell execution**: bash tool with command execution and error handling
- **Tool loop**: Multi-tool iteration with finish_reason detection and loop limits
- **Conversation integration**: Tool messages added to conversation state with DB persistence
- **Replay support**: Tool message transformation for session replay

#### Tool Choice API
- **Configuration type**: tool_choice field with auto, none, required, and specific modes
- **JSON serialization**: Proper serialization of all tool_choice variants
- **Loop limit behavior**: Automatic tool_choice=none when iteration limit reached
- **E2E test suite**: Comprehensive tests for all tool_choice modes

#### Command History
- **File persistence**: JSONL load/save/append operations in ~/.ikigai directory
- **Navigation**: Up/Down arrow key integration for history traversal
- **Deduplication**: Consecutive duplicate command prevention
- **Configuration**: history_size field in config with sensible defaults

#### Tab Completion (Complete)
- **Completion layer**: Dropdown display for completion candidates
- **Fuzzy matching**: fzy algorithm integration with talloc-aware wrapper
- **Prefix enforcement**: Strict prefix matching for command completion
- **Argument matching**: Context-aware completion based on current input
- **Navigation**: Arrow/Tab/Escape key interaction for selection
- **State machine**: Tab cycling through candidates with wrap-around
- **Accept behavior**: Tab accepts and dismisses, cursor positioned at end

#### JSONL Logger
- **File output**: Structured logging to ~/.ikigai/logs directory
- **Timestamps**: ISO 8601 timestamp formatting
- **Log rotation**: Atomic file rotation with configurable limits
- **Thread safety**: Mutex-protected logging for concurrent access
- **Log levels**: Debug, info, warning, error with filtering
- **HTTP logging**: Request/response logging for OpenAI API calls

#### Mouse Scroll Support
- **SGR protocol**: Parse SGR mouse escape sequences
- **Terminal integration**: Enable SGR mouse reporting mode
- **Scroll actions**: IK_INPUT_SCROLL_UP/DOWN action types
- **Handler**: Mouse scroll event handling in REPL
- **Alternate scroll mode**: Arrow keys scroll when viewing scrollback

#### UI Improvements
- **Unicode box drawing**: Replaced ASCII separators with Unicode characters
- **Input layer newline**: Fixed trailing newline handling in input layer

#### ANSI Color System
- **Escape skip**: Function to skip over ANSI CSI escape sequences
- **Width calculation**: ANSI-aware width for scrollback, input, and cursor
- **Color constants**: ANSI color codes and SGR sequence builders
- **SGR stripping**: Remove SGR sequences from pasted input
- **Message styling**: Apply colors to different message kinds

### Changed

#### Async Tool Execution
- **Thread infrastructure**: Background thread pool for tool execution
- **Non-blocking**: Tools execute without blocking the event loop
- **Debug output**: Request/response metadata with >> prefix convention

#### Terminal Improvements
- **Cursor restoration**: Proper cursor visibility on exit and panic
- **Terminal reset**: Test utility for consistent terminal state
- **HTTP/2 filtering**: Filter debug noise and redact secrets from logs

#### Code Organization
- **Layer modules**: Split layer_wrappers.c into per-layer files (completion, input, scrollback, separator, spinner)
- **Action modules**: Split repl_actions.c into focused modules (completion, history, llm, viewport)
- **Wrapper headers**: Split wrapper.h into domain-specific headers (curl, json, posix, postgres, pthread, stdlib, talloc)

### Development

#### Testing & Quality Gates
- **Test coverage**: Maintained 100% (6,374 lines, 384 functions, 2,144 branches)
- **Sanitizer fixes**: Thread safety improvements for ASan/UBSan/TSan compliance
- **Valgrind/Helgrind**: All 286 tests pass under memory and thread checkers
- **File size enforcement**: 16KB limit with automated splitting

#### Documentation
- **Task specifications**: TDD task files for all tool execution user stories
- **Architecture docs**: Canonical message format and extended thinking documentation
- **Agent skills**: Modernized skill system with composable personas

### Technical Metrics
- **Changes**: 703 files modified, +87,581/-9,083 lines
- **Commits**: 174 commits over development cycle
- **Test coverage**: 100% lines (6,374), functions (384), and branches (2,144)
- **Code quality**: All lint, format, and sanitizer checks pass

## [rel-03] - 2025-11-28

### Added

#### Database Integration (Complete)
- **PostgreSQL backend**: Full libpq integration for persistent conversation storage
- **Session management**: Create, restore, and manage sessions across application restarts
- **Message persistence**: Store and retrieve user/assistant messages with complete metadata
- **Replay system**: Core algorithm for conversation playback with mark/rewind support
- **Mark/Rewind implementation**: Checkpoint conversation state and restore to any mark
- **Migration system**: Automated database schema versioning with migration files
- **Error handling**: Comprehensive database error handling with res_t pattern

#### Event-Based Rendering
- **Unified render path**: Consistent rendering for both live streaming and replay modes
- **Event system**: Clean separation between data events and display logic
- **Replay display**: Seamless playback of historical conversations with proper formatting

#### Agent Task System
- **Hierarchical tasks**: Multi-level task execution with sub-agent optimization
- **Personas**: Composable skill sets (coverage, developer, task-runner, task-strategist, security, meta)
- **Manifest system**: Automated skill and persona discovery with .claude symlinks
- **Task state tracking**: Persistent task status with verification workflow
- **Agent scripts**: Deno-based tooling with standardized JSON output format

### Changed

#### Code Quality & Testing
- **Test coverage**: Maintained 100% coverage (4,061 lines, 257 functions, 1,466 branches)
- **Database testing**: Comprehensive integration tests with per-file database isolation
- **MOCKABLE wrappers**: Enhanced mocking infrastructure for database function testing
- **Test utilities**: Added configurable PostgreSQL connection helpers (PGHOST support)
- **Error injection**: Complete coverage of database error paths

#### Bug Fixes
- **Bug 9**: Fixed database error dangling pointer crash in error handling
- **Bug 8**: Fixed mark stack rebuild on session restoration for checkpoint recovery
- **Bug 7**: Fixed /clear command to properly display system message after clearing
- **Defensive checks**: Added NULL validation for db_connection_string in configuration

#### Code Organization
- **Agent system**: Reorganized .agents/prompts to .agents/skills for clarity
- **Documentation**: Restructured docs to reflect v0.3.0 database integration
- **Naming**: Updated to release numbering (rel-XX) from semantic versioning

### Development

#### Testing & Quality Gates
- **Docker Compose**: Local distro-check with PostgreSQL service for isolated testing
- **GitHub CI**: PostgreSQL service container integration for automated testing
- **PGHOST support**: Environment-based database host configuration
- **Build system**: Enhanced Makefile targets for docker-compose workflows

#### Documentation
- **Design documentation**: Comprehensive product vision and multi-agent architecture
- **Task system docs**: Complete documentation of hierarchical task execution
- **Protocol comparison**: Anthropic vs OpenAI streaming protocol documentation
- **Database ADRs**: Architecture decisions for PostgreSQL integration patterns

### Technical Metrics
- **Changes**: 246 files modified, +33,029/-6,446 lines
- **Commits**: 71 commits over development cycle
- **Test coverage**: 100% lines, functions, and branches
- **Code quality**: All lint, format, and sanitizer checks pass

## [rel-02] - 2025-11-22

### Added

#### OpenAI API Integration (Complete)
- **Streaming client**: Full OpenAI API integration with Server-Sent Events (SSE) parsing
- **Async I/O**: libcurl multi-handle for non-blocking HTTP requests integrated with event loop
- **GPT-5 compatibility**: Updated to support latest OpenAI API format
- **HTTP handler**: Modular request/response handling with completion callbacks
- **Error handling**: Comprehensive HTTP error handling with res_t pattern
- **State management**: REPL state machine for request/response lifecycle

#### REPL Command System
- **Command registry**: Extensible infrastructure for slash commands
- **Built-in commands**:
  - `/help` - Display available commands and usage
  - `/clear` - Clear conversation history and reset state
  - `/model` - Runtime LLM model switching
  - `/system` - Configure system message at runtime
  - `/mark` and `/rewind` - Conversation checkpoint and restore (architecture defined)

#### Configuration & Infrastructure
- **Config system**: JSON-based runtime configuration (Phase 1.1)
- **Layer abstraction**: Clean interface for terminal operations (Phase 1.2)
- **Debug pipe**: Development tool for inspecting internal state

### Changed

#### Code Quality & Testing
- **File size limits**: Refactored large files to comply with 16KB limit for maintainability
- **Test coverage**: Maintained 100% coverage (3,178 lines, 219 functions, 1,018 branches)
- **Test suite**: 129 test files with comprehensive unit and integration coverage
- **Mock infrastructure**: Link seams pattern with weak symbol wrappers (zero overhead in release builds)
- **Error injection**: Added tests for previously untestable error paths
- **LCOV refinement**: Reduced invalid exclusions through better error injection testing

#### Naming & Conventions
- **Pointer semantics**: Changed `_ref` suffix to `_ptr` for raw pointers
- **Return values**: Simplified OOM-only creator functions to return pointers directly
- **Completion callbacks**: Standardized to return `res_t` for consistent error handling

#### Code Organization
- **Module restructuring**: Reorganized input_buffer into subdirectory with full symbol prefixes
- **Wrapper functions**: Renamed to clarify external library seams
- **File splits**: Divided oversized test files for better maintainability

### Development

#### Testing & Quality Gates
- **Thread safety**: Added Helgrind testing for race condition detection
- **Code formatting**: Integrated uncrustify with automated style enforcement
- **Coverage refinement**: Improved coverage metrics accuracy through error injection
- **CI improvements**: Better visibility and reliability in GitHub Actions

#### Documentation
- **Vision docs**: Comprehensive multi-agent and mark/rewind architecture
- **ADRs**: Link seams mocking strategy and API design decisions
- **Return values**: Documented patterns and tracked technical debt
- **Organization**: Streamlined documentation structure and removed obsolete ADRs

### Technical Metrics
- **Changes**: 275 files modified, +28,676/-7,331 lines
- **Commits**: 71 commits over development cycle
- **Test coverage**: 100% lines, functions, and branches
- **Code quality**: All lint, format, and sanitizer checks pass

## [rel-01] - 2025-11-16

### Added

#### REPL Terminal Foundation (Complete)
- **Direct terminal rendering**: Single write per frame, 52× syscall reduction
- **UTF-8 support**: Full emoji, CJK, and combining character support
- **Multi-line input**: Cursor navigation and line editing
- **Readline shortcuts**: Ctrl+A/E/K/U/W for text navigation and manipulation
- **Text wrapping**: Automatic word wrapping with clean terminal restoration
- **Scrollback buffer**: O(1) arithmetic reflow (0.003-0.009 ms for 1000 lines), 726× faster than target
- **Viewport scrolling**: Page Up/Down navigation with auto-scroll on submit
- **Pretty-print infrastructure**: Format module with pp_helpers for debugging

#### Core Infrastructure
- **Error handling system**: Result types, assertions, FATAL() macros with comprehensive testing
- **Memory management**: talloc-based ownership with clear patterns and documentation
- **Generic arrays**: Type-safe wrapper pattern with base implementation
- **Build system**: Comprehensive warnings, sanitizers (ASan, UBSan, TSan), Valgrind, and coverage support
- **Testing framework**: 100% test coverage requirement (1,807 lines, 131 functions, 600 branches)

#### JSON Library Migration
- **yyjson integration**: Migrated from jansson to yyjson
- **Custom allocator**: talloc integration eliminates reference counting complexity
- **Performance**: 3× faster parsing for streaming LLM responses
- **Memory safety**: Better integration with project memory management patterns

### Changed
- Removed server/protocol code in favor of direct terminal client approach
- Restructured architecture for v1.0 desktop client vision
- Standardized on K&R code style with 120-character line width
- Adopted explicit sized integer types (<inttypes.h>) throughout codebase

### Documentation
- Comprehensive REPL documentation in docs/repl/
- Architecture decision records (ADRs) for key design choices
- Memory management, error handling, and naming convention guides
- Build system documentation with multi-distro support
- Security analysis of input parsing and terminal control

### Development
- Test-driven development with strict Red/Green/Verify cycle
- 100% coverage requirement for lines, functions, and branches
- Quality gates: fmt, check, lint, coverage, check-dynamic
- Parallel test execution support (up to 32 concurrent tests)

[rel-05]: https://github.com/mgreenly/ikigai/releases/tag/rel-05
[rel-04]: https://github.com/mgreenly/ikigai/releases/tag/rel-04
[rel-03]: https://github.com/mgreenly/ikigai/releases/tag/rel-03
[rel-02]: https://github.com/mgreenly/ikigai/releases/tag/rel-02
[rel-01]: https://github.com/mgreenly/ikigai/releases/tag/rel-01
