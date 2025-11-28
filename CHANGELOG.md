# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2025-11-16

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

## [Unreleased]

### Next: Core v1.0 Features
1. **LLM Integration**: OpenAI API client with streaming responses
2. **Database Integration**: PostgreSQL for persistent conversation history and RAG
3. **Multi-LLM Support**: Anthropic, OpenAI, Google, X.AI provider interfaces
4. **Local Tool Execution**: File operations, shell commands, code analysis
5. **Enhanced Terminal UI**: Syntax highlighting, editor integration, rich formatting

[0.1.0]: https://github.com/mgreenly/ikigai/releases/tag/v0.1.0
