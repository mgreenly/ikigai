# Task: Logger Dependency Injection

## Target

Refactor Issue #3: Convert global logger state to dependency injection pattern

## Context

The current logger implementation uses global state:

**`src/logger.c` current state:**
```c
// Global mutex for thread-safe logging
static pthread_mutex_t ik_log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Global log file handle
static FILE *ik_log_file = NULL;

// Global log level
static ik_log_level_t g_log_level = IK_LOG_DEBUG;
```

**DI Violations:**
1. Global static variables instead of context struct
2. Cannot have multiple logger instances
3. Hidden dependencies - functions don't declare they need logger
4. Difficult to test - no way to inject mock logger
5. Violates "No global state" principle from DI skill

**Current Usage Pattern:**
```c
// Anywhere in codebase:
ik_log_debug("message");        // Uses global logger
ik_log_info("info");
ik_log_warn_json(doc);          // Takes ownership of doc
```

**Goal:** Convert to explicit context injection:
```c
// In initialization:
ik_logger_t *logger = ik_logger_create(ctx, config);

// Pass logger to functions that need it:
ik_some_function(env, ...);  // env contains logger

// Or explicit logger parameter:
ik_log_debug(logger, "message");
```

## Pre-read

### Skills
- default
- database
- errors
- git
- log
- makefile
- naming
- quality
- scm
- source-code
- style
- tdd
- align

### Documentation
- docs/README.md
- docs/memory.md
- .agents/skills/di.md (DI principles)
- .agents/skills/ddd.md (explicit context pattern)

### Source Files (Logger)
- src/logger.h (public API)
- src/logger.c (implementation with globals)

### Source Files (Usage - representative sample)
- src/client.c (main, creates shared context)
- src/shared.c (shared context initialization)
- src/repl.c (REPL main loop)
- src/openai/client.c (LLM client)
- src/db/connection.c (database)
- src/history.c (history management)

### Related Context Types
- src/shared.h (ik_shared_ctx_t - potential logger holder)
- src/agent.h (ik_agent_ctx_t - could access via shared)

### Test Files
- tests/unit/test_logger.c

## Pre-conditions

1. Working tree is clean (`git status --porcelain` returns empty)
2. All tests pass (`make check`)
3. Global logger state exists in `src/logger.c`

## Task

Convert the logger from global state to dependency injection:

1. Create `ik_logger_t` context struct
2. Add logger to `ik_shared_ctx_t` (composition root pattern)
3. Update logger API to accept context
4. Update all call sites to pass logger from context
5. Remove global state from `logger.c`

## API Design

### New Logger Context

```c
// logger.h

typedef enum {
    IK_LOG_DEBUG = 0,
    IK_LOG_INFO = 1,
    IK_LOG_WARN = 2,
    IK_LOG_ERROR = 3,
    IK_LOG_FATAL = 4
} ik_log_level_t;

typedef struct ik_logger ik_logger_t;

// Create logger instance
// path: log file path (NULL for stderr)
// level: minimum log level
ik_logger_t *ik_logger_create(TALLOC_CTX *ctx, const char *path, ik_log_level_t level);

// Logging functions (take logger as first param)
void ik_log_debug(ik_logger_t *log, const char *fmt, ...);
void ik_log_info(ik_logger_t *log, const char *fmt, ...);
void ik_log_warn(ik_logger_t *log, const char *fmt, ...);
void ik_log_error(ik_logger_t *log, const char *fmt, ...);
void ik_log_fatal(ik_logger_t *log, const char *fmt, ...);  // Calls exit(1)

// JSON logging (takes ownership of doc)
void ik_log_debug_json(ik_logger_t *log, yyjson_mut_doc *doc);
void ik_log_info_json(ik_logger_t *log, yyjson_mut_doc *doc);
void ik_log_warn_json(ik_logger_t *log, yyjson_mut_doc *doc);
void ik_log_error_json(ik_logger_t *log, yyjson_mut_doc *doc);

// Create JSON doc for structured logging
yyjson_mut_doc *ik_log_create(void);  // Unchanged - no logger needed
```

### Shared Context Integration

```c
// shared.h
typedef struct ik_shared_ctx {
    ik_cfg_t *cfg;
    ik_term_ctx_t *term;
    ik_db_ctx_t *db;
    ik_history_t *history;
    ik_logger_t *logger;  // NEW: logger instance
    // ...
} ik_shared_ctx_t;
```

## TDD Cycle

### Red Phase

1. Update `tests/unit/test_logger.c`:
   - Add tests for `ik_logger_create()`
   - Add tests for logging with context
   - Add tests for NULL logger handling (should assert)

2. Create stub API changes in `logger.h`.

3. Run `make check` - tests should fail.

### Green Phase

1. **Update `src/logger.h`:**
   - Add `ik_logger_t` opaque type
   - Add `ik_logger_create()` function
   - Update all logging macros/functions to take `ik_logger_t *` first param

2. **Update `src/logger.c`:**
   - Define `struct ik_logger`:
     ```c
     struct ik_logger {
         FILE *file;
         ik_log_level_t level;
         pthread_mutex_t mutex;
     };
     ```
   - Implement `ik_logger_create()`:
     - Allocate struct with talloc
     - Open log file
     - Initialize mutex
     - Set destructor for cleanup
   - Update all logging functions to use context instead of globals
   - Remove global variables

3. **Update `src/shared.c`:**
   - Add `ik_logger_t *logger` to `ik_shared_ctx_t`
   - Create logger in `ik_shared_ctx_create()`
   - Pass logger to subsystems that need it

4. Run `make check` - new tests should pass, but call sites will fail.

### Refactor Phase

This is the largest phase - updating all call sites.

**Strategy:** Use a macro for gradual migration:
```c
// Temporary compatibility - remove after all call sites updated
#define IK_LOG_COMPAT(level, ...) \
    ik_log_##level(g_compat_logger, __VA_ARGS__)
```

**Call Site Categories:**

1. **Files with access to shared context:**
   - `src/repl.c` - has `repl->shared->logger`
   - `src/agent.c` - has `agent->shared->logger`
   - `src/repl_*.c` - has `repl->shared->logger`

2. **Files with access to agent context:**
   - `src/openai/*.c` - needs logger passed through

3. **Files initialized from shared:**
   - `src/db/*.c` - add logger to `ik_db_ctx_t`
   - `src/history.c` - add logger parameter

4. **Utility files:**
   - `src/config.c` - add logger parameter
   - `src/scrollback.c` - add logger parameter or access via parent

**Update order:**
1. `src/shared.c` - create logger, add to shared
2. `src/client.c` - pass logger through
3. `src/repl*.c` - use `repl->shared->logger`
4. `src/openai/*.c` - pass logger through client
5. `src/db/*.c` - add to db context
6. `src/history.c` - pass logger parameter
7. Remaining files...

5. Run `make check` after each file update.

6. Run `make lint` - verify no new warnings.

7. Run `make coverage` - verify coverage maintained.

## Post-conditions

1. `ik_logger_t` context struct exists
2. No global logger state in `logger.c`
3. Logger passed explicitly to all functions that log
4. `ik_shared_ctx_t` contains logger instance
5. All tests pass (`make check`)
6. Lint passes (`make lint`)
7. Coverage maintained at 100% (`make coverage`)
8. Working tree is clean (changes committed)

## Commit Strategy

This is a large change - use incremental commits:

### Commit 1: Add ik_logger_t API
```
feat: add ik_logger_t context struct

- New ik_logger_create() function
- Logger functions now take ik_logger_t* first param
- Old global API still works (temporary compatibility)
- Full test coverage for new API
```

### Commit 2: Integrate with shared context
```
feat: add logger to ik_shared_ctx_t

- Create logger in ik_shared_ctx_create()
- Logger accessible via shared->logger
```

### Commit 3-N: Update call sites (one commit per file/module)
```
refactor: use injected logger in repl.c

- Replace global logger calls with repl->shared->logger
```

### Final Commit: Remove compatibility layer
```
refactor: remove global logger compatibility

- Remove static globals from logger.c
- All logging now uses explicit context
- Completes DI migration for logger
```

## Risk Assessment

**Risk: High**
- Many files affected (potentially 30+)
- Threading concerns with mutex in context
- Need to maintain logging during migration
- Compatibility layer helps manage risk

## Estimated Complexity

**High** - Large-scale refactoring across entire codebase

## Fallback Plan

If full DI is too disruptive:
1. Keep global logger for now
2. Add optional context parameter (NULL uses global)
3. Migrate incrementally over time

## Notes

- The panic handler (`g_term_ctx_for_panic`) is a justified exception for async-signal safety
- Logger DI is important but not as critical as panic handling
- Consider whether logging in signal handlers needs special handling

## Sub-agent Strategy (CRITICAL)

**Files affected:** 30+ files across the codebase

This task MUST use sub-agents to parallelize the call site updates. The refactor phase involves updating many files with a similar pattern.

### Phase 1: Core Infrastructure (Sequential - Main Agent)

Complete these steps before spawning sub-agents:

1. Update `src/logger.h` and `src/logger.c` with new API
2. Update `tests/unit/test_logger.c` with new tests
3. Update `src/shared.h` and `src/shared.c` to hold logger
4. Add temporary compatibility macros
5. Verify: `make check` passes

### Phase 2: Call Site Migration (Parallel - Sub-agents)

Group files by module and spawn sub-agents for each group:

**Group 1: REPL Module (7 files)**
```
src/repl.c
src/repl_init.c
src/repl_actions.c
src/repl_actions_llm.c
src/repl_actions_viewport.c
src/repl_actions_history.c
src/repl_actions_completion.c
```

**Group 2: OpenAI Module (6 files)**
```
src/openai/client.c
src/openai/client_msg.c
src/openai/client_multi.c
src/openai/client_multi_request.c
src/openai/client_multi_callbacks.c
src/openai/http_handler.c
```

**Group 3: Database Module (5 files)**
```
src/db/connection.c
src/db/migration.c
src/db/session.c
src/db/message.c
src/db/replay.c
```

**Group 4: Tool Module (6 files)**
```
src/tool_dispatcher.c
src/tool_bash.c
src/tool_glob.c
src/tool_grep.c
src/tool_file_read.c
src/tool_file_write.c
```

**Group 5: UI/Rendering Module (6 files)**
```
src/scrollback.c
src/layer_scrollback.c
src/layer_input.c
src/layer_separator.c
src/layer_spinner.c
src/render.c
```

**Group 6: Remaining Files (5+ files)**
```
src/agent.c
src/history.c
src/config.c
src/commands.c
src/input.c
(any others discovered)
```

### Sub-agent Prompt Template

```
Migrate logger calls in [MODULE] files to use injected logger.

Pre-read:
- .agents/skills/scm.md (commit discipline)
- src/logger.h (new API with ik_logger_t*)
- src/shared.h (ik_shared_ctx_t contains logger)
- [List of files in this group]

Context:
- Logger is now accessed via: repl->shared->logger (in REPL files)
- Or via: agent->shared->logger (in agent-related files)
- Or passed as parameter (in utility functions)

Task for each file:
1. Add #include "shared.h" if needed
2. Find the appropriate logger access path for this file
3. Replace all ik_log_XXX(...) calls with ik_log_XXX(logger, ...)
4. Replace all ik_log_XXX_json(...) calls with ik_log_XXX_json(logger, ...)
5. Run: make check (must pass)
6. Commit each file separately

Files to update:
[LIST FILES]

Return: {"ok": true} or {"ok": false, "reason": "...", "files_completed": [...]}
```

### Coordination Strategy

1. **Main agent completes Phase 1** (infrastructure + compatibility layer)
2. **Spawn 6 sub-agents in parallel** for Phase 2 groups
3. **Each sub-agent:**
   - Works on its assigned file group
   - Commits after each file
   - Reports success/failure per file
4. **Main agent collects results:**
   - Verify all groups succeeded
   - Run `make lint && make coverage`
   - If all pass, remove compatibility macros (Phase 3)

### Phase 3: Cleanup (Sequential - Main Agent)

After all sub-agents complete:

1. Remove compatibility macros from `logger.h`
2. Remove any remaining global state from `logger.c`
3. Run final verification: `make clean && make check && make lint && make coverage`
4. Final commit: "refactor: remove global logger compatibility layer"

### Error Handling

If a sub-agent fails:
- Check which files succeeded (from `files_completed`)
- Resume with remaining files
- Or escalate to higher model for complex cases

### Estimated Sub-agent Breakdown

| Group | Files | Model | Thinking |
|-------|-------|-------|----------|
| REPL Module | 7 | sonnet | thinking |
| OpenAI Module | 6 | sonnet | thinking |
| Database Module | 5 | sonnet | thinking |
| Tool Module | 6 | sonnet | thinking |
| UI/Rendering | 6 | sonnet | thinking |
| Remaining | 5+ | sonnet | thinking |

**Total parallelization:** 6 sub-agents can complete 35+ file updates concurrently
