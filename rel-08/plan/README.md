# Implementation Plan

Technical specifications for the external tool architecture. Tasks coordinate naming and function prototypes through these documents.

## Documents

| Document | Purpose |
|----------|---------|
| [paths-module.md](paths-module.md) | **FIRST:** Install directory detection and path resolution infrastructure |
| [paths-test-migration.md](paths-test-migration.md) | Test migration strategy for paths module (160+ test updates) |
| [architecture.md](architecture.md) | Structs, functions, data flow, migration phases |
| [memory-management.md](memory-management.md) | Ownership rules, talloc patterns, thread memory model |
| [removal-specification.md](removal-specification.md) | Complete specification for removing internal tool system (Phase 1) |
| [integration-specification.md](integration-specification.md) | Exact struct/function changes for new code integration |
| [tool-discovery-execution.md](tool-discovery-execution.md) | Discovery protocol, execution flow, build system, /refresh command |
| [tool-specifications.md](tool-specifications.md) | Complete schemas, behavior, and error handling for 6 external tools |
| [test-specification.md](test-specification.md) | **TDD guidance:** Test files, scenarios, and patterns for each task |
| [error-codes.md](error-codes.md) | Standard error codes for ikigai and tools |
| [architecture-current.md](architecture-current.md) | Reference: existing internal tool system (what we're replacing) |

## Reading Order

1. **paths-module.md** - **START HERE:** Foundation infrastructure for install detection
2. **paths-test-migration.md** - Test migration strategy (read with paths-module.md)
3. **architecture.md** - What we're building (tool system)
4. **memory-management.md** - Ownership rules and talloc patterns
5. **tool-discovery-execution.md** - Deep dive on discovery and execution specifics
6. **test-specification.md** - TDD guidance for each task
7. **error-codes.md** - Reference for error handling

Note: `architecture-current.md` is reference only - describes the existing system for context when needed.

## Migration Phases

**Human verification required between each phase.** See architecture.md for details.

0. **Phase 0:** Path resolution infrastructure (FIRST)
   - Install type detection (development/user/system)
   - Directory path computation and caching
   - Dependency injection throughout codebase
   - **Note:** Will affect many existing tests
1. **Phase 1:** First tool (bash) - standalone executable
2. **Phase 2:** Remaining tools (one at a time) - standalone executables
   - 2a: file_read
   - 2b: file_write
   - 2c: file_edit
   - 2d: glob
   - 2e: grep
3. **Phase 3:** Remove internal tools
4. **Phase 4:** Sync infrastructure (async internals, blocking API)
5. **Phase 5:** Commands (/tool, /refresh)
6. **Phase 6:** Expose async API to event loop

## Key Types

```c
// Phase 0: Path resolution
ik_paths_t                  // Install paths (opaque, created at startup)

// Phases 1-6: Tool system
ik_tool_registry_t          // Tool registry (dynamic, populated at runtime)
ik_tool_registry_entry_t    // Single tool: name, path, schema
ik_tool_discovery_state_t   // Discovery state (internal until Phase 6)
tool_scan_state_t           // Discovery state enum
```

## Key Functions

```c
// Phase 0: Path resolution
ik_paths_init()                     // Initialize paths (call at startup)
ik_paths_get_config_dir()           // Get config directory
ik_paths_get_data_dir()             // Get data directory
ik_paths_get_tools_system_dir()     // Get system tools directory
ik_paths_get_tools_user_dir()       // Get user tools directory
ik_paths_get_tools_project_dir()    // Get project tools directory

// Phases 1-6: Tool system
// Registry
ik_tool_registry_create()
ik_tool_registry_lookup()
ik_tool_registry_build_all()

// Discovery - Phases 2-5: only blocking API is public
ik_tool_discovery_run()  // Blocking: spawns all, waits for completion

// Discovery - Phase 6: async primitives exposed
ik_tool_discovery_start()           // Spawn all, return immediately
ik_tool_discovery_add_fds()         // Add to select() fdset
ik_tool_discovery_process_fds()     // Handle ready fds
ik_tool_discovery_is_complete()     // Check completion
ik_tool_discovery_finalize()        // Cleanup

// Execution
ik_tool_external_exec()

// Response wrapper
ik_tool_wrap_success()
ik_tool_wrap_failure()
```

## Integration Points

**Phase 0 (Paths):**

1. **Main initialization** (`src/client.c`) - Create `ik_paths_t` early, pass to REPL init
2. **REPL context** (`src/repl.h`) - Add `ik_paths_t *paths` field
3. **Tool discovery** - Receive directory paths from `ik_paths_t` instance
4. **Many tests** - Provide mock/test paths instances

**Phases 1-6 (Tool system):**

1. **LLM request building** (`src/providers/openai/request_chat.c`) - Replace `ik_tool_build_all()` with `ik_tool_registry_build_all()`

2. **Tool execution** (`src/repl_tool.c`) - Replace `ik_tool_dispatch()` with registry lookup + `ik_tool_external_exec()`

## Libraries

Use only these libraries. Do not introduce new dependencies.

| Library | Use For | Location |
|---------|---------|----------|
| yyjson | JSON parsing and building | `src/vendor/yyjson/` (vendored) |
| talloc | Memory management | System library |
| POSIX | fork/exec/pipe, select, signals | System |

**For this release:**
- JSON: Use `yyjson` with `src/json_allocator.c` for talloc integration
- Process spawning: Use POSIX `fork()`/`exec()`/`pipe()`
- Async I/O: Use `select()` (already used in REPL event loop)

**Do not introduce:**
- New JSON libraries
- Process pool libraries
- Async frameworks
