# Task: Delete Legacy OpenAI Files (Part 1: File Deletions)

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. Complete context provided below.

**Model:** sonnet/thinking
**Depends on:** remove-legacy-conversation.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

All needed context is provided in this file. Do not research, explore, or spawn sub-agents.

## Objective

Delete legacy `src/openai/` directory (19 files), shim layer files, and legacy test files. Update Makefile to remove deleted sources. This prepares the codebase for direct JSON serialization in Part 2.

## Pre-Read

**Skills:**
- `/load makefile` - Build system and source file management
- `/load source-code` - File map to understand dependencies
- `/load git` - Git operations and commit practices

**Files to Review:**
- `Makefile` - Source file lists and build rules
- `scratch/legacy-openai-removal-checklist.md` - Complete inventory of files to delete

## Error Handling Policy

**Memory Allocation Failures:**
- All talloc allocations: PANIC with LCOV_EXCL_BR_LINE
- Rationale: OOM is unrecoverable, panic is appropriate

**Validation Failures:**
- Return ERR allocated on parent context (not on object being freed)
- Example: `return ERR(parent_ctx, INVALID_ARG, "message")`

**During Dual-Mode (Tasks 1-4):**
- Old API calls succeed: continue normally
- New API calls fail: log error, continue (old API is authoritative)
- Pattern: `if (is_err(&res)) { ik_log_error("Failed: %s", res.err->msg); }`

**After Migration (Tasks 5-8):**
- New API calls fail: propagate error immediately
- Pattern: `if (is_err(&res)) { return res; }`

**Assertions:**
- NULL pointer checks: `assert(ptr != NULL)` with LCOV_EXCL_BR_LINE
- Only for programmer errors, never for runtime conditions

## Implementation

### 1. Delete src/openai/ Directory

**Files to Delete (19 total):**

**Client Core:**
- `src/openai/client.c`
- `src/openai/client.h`
- `src/openai/client_msg.c`
- `src/openai/client_serialize.c`

**Multi-handle Manager:**
- `src/openai/client_multi.c`
- `src/openai/client_multi.h`
- `src/openai/client_multi_internal.h`
- `src/openai/client_multi_request.c`
- `src/openai/client_multi_callbacks.c`
- `src/openai/client_multi_callbacks.h`

**HTTP/Streaming:**
- `src/openai/http_handler.c`
- `src/openai/http_handler.h`
- `src/openai/http_handler_internal.h`
- `src/openai/sse_parser.c`
- `src/openai/sse_parser.h`

**Tool Choice:**
- `src/openai/tool_choice.c`
- `src/openai/tool_choice.h`

**Build Artifacts:**
- `src/openai/client_multi_request.o`
- `src/openai/client.o`

**Command:**
```bash
rm -rf src/openai/
```

### 2. Delete Shim Layer in src/providers/openai/

**Files to Delete:**
- `src/providers/openai/shim.c`
- `src/providers/openai/shim.h`

**Command:**
```bash
rm src/providers/openai/shim.c
rm src/providers/openai/shim.h
```

### 3. Update Makefile - Remove Legacy Sources

**Location:** Find `CLIENT_SOURCES` variable

**Remove these lines:**
```makefile
src/openai/client.c \
src/openai/client_msg.c \
src/openai/client_serialize.c \
src/openai/client_multi.c \
src/openai/client_multi_request.c \
src/openai/client_multi_callbacks.c \
src/openai/http_handler.c \
src/openai/sse_parser.c \
src/openai/tool_choice.c \
src/providers/openai/shim.c \
```

**Also remove from any test-specific source lists if present.**

### 4. Update src/providers/openai/openai.c

**Remove shim includes:**
```c
#include "providers/openai/shim.h"  // DELETE THIS LINE
```

**Remove any calls to shim functions:**
- `ik_openai_shim_build_conversation()`
- `ik_openai_shim_map_finish_reason()`

These functions should no longer be needed since we're working directly with `ik_message_t` arrays.

### 5. Delete Legacy Test Files

**If these exist, delete them:**
- `tests/unit/openai/client_test.c`
- `tests/unit/openai/client_msg_test.c`
- `tests/unit/openai/http_handler_test.c`
- `tests/unit/openai/sse_parser_test.c`
- Any other tests in `tests/unit/openai/` directory

**Command:**
```bash
rm -rf tests/unit/openai/
```

**Update Makefile:**
Remove test targets for deleted tests.

## Build Verification

After deletions:

```bash
make clean
make all
```

**Expected:**
- Build may have warnings about unused shim in request_chat.c/request_responses.c
- These will be fixed in Part 2
- No errors about missing src/openai/ files
- No linker errors

## Postconditions

- [ ] `src/openai/` directory deleted (19 files removed)
- [ ] `src/providers/openai/shim.c` and `shim.h` deleted
- [ ] Makefile updated (legacy sources removed)
- [ ] `src/providers/openai/openai.c` has no shim includes
- [ ] `tests/unit/openai/` deleted if it exists
- [ ] `make clean && make all` succeeds (may have warnings)
- [ ] No compiler errors (warnings OK)

## Success Criteria

After this task:
1. Legacy `src/openai/` directory completely removed
2. Shim layer files deleted
3. Makefile updated
4. Build completes (request_chat.c/request_responses.c will be fixed in Part 2)
5. Codebase is 21 files smaller
6. Ready for Part 2 to refactor request serialization
