# Implementation Plan

Technical specifications for the external tool architecture. Tasks coordinate naming and function prototypes through these documents.

## Documents

| Document | Purpose |
|----------|---------|
| [architecture.md](architecture.md) | Structs, functions, data flow, migration phases |
| [removal-specification.md](removal-specification.md) | Complete specification for removing internal tool system (Phase 1) |
| [integration-specification.md](integration-specification.md) | Exact struct/function changes for new code integration |
| [tool-discovery-execution.md](tool-discovery-execution.md) | Discovery protocol, execution flow, build system, /refresh command |
| [tool-specifications.md](tool-specifications.md) | Complete schemas, behavior, and error handling for 6 external tools |
| [error-codes.md](error-codes.md) | Standard error codes for ikigai and tools |
| [architecture-current.md](architecture-current.md) | Reference: existing internal tool system (what we're replacing) |

## Reading Order

1. **architecture.md** - Start here for what we're building
2. **tool-discovery-execution.md** - Deep dive on discovery and execution specifics
3. **error-codes.md** - Reference for error handling

Note: `architecture-current.md` is reference only - describes the existing system for context when needed.

## Migration Phases

**Human verification required between each phase.** See architecture.md for details.

1. **Phase 1:** First tool (bash) - standalone executable
2. **Phase 2:** Remaining tools (one at a time) - standalone executables
3. **Phase 3:** Remove internal tools
4. **Phase 4:** Sync infrastructure (async internals, blocking API)
5. **Phase 5:** Commands (/tool, /refresh)
6. **Phase 6:** Expose async API to event loop

## Key Types

```c
ik_tool_registry_t          // Tool registry (dynamic, populated at runtime)
ik_tool_registry_entry_t    // Single tool: name, path, schema
ik_tool_discovery_state_t   // Discovery state (internal until Phase 6)
tool_scan_state_t           // Discovery state enum
```

## Key Functions

```c
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

Two places in existing code need modification:

1. **LLM request building** (`src/openai/client.c`) - Replace `ik_tool_build_all()` with `ik_tool_registry_build_all()`

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
