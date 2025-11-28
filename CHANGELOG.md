# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

[rel-03]: https://github.com/mgreenly/ikigai/releases/tag/rel-03
[rel-02]: https://github.com/mgreenly/ikigai/releases/tag/rel-02
[rel-01]: https://github.com/mgreenly/ikigai/releases/tag/rel-01
