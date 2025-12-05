# Fix: Rename ik_openai_msg_t to ik_msg_t

## Agent
model: haiku

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `naming.md` - Naming conventions
- `style.md` - Code style conventions
- `tdd.md` - Test-driven development approach

## Context

From `docs/TODO.md`:
> The internal canonical database/memory message format is named `ik_openai_msg_t`
> The `ik_openai_msg_t` needs to be renamed to `ik_internal_msg_t`

**Issue**: `ik_openai_msg_t` suggests it's OpenAI-specific, but it's actually the **provider-agnostic canonical message format** used for:
1. Database storage (superset of all message types)
2. In-memory representation
3. Rendering
4. All LLM providers (not just OpenAI)

**Confusion**: The name violates the provider abstraction. Future Anthropic/Google/X.AI support will use the same canonical format.

## High-Level Goal

**Rename `ik_openai_msg_t` to `ik_msg_t` throughout the codebase.**

This reflects that it's the core message type, not tied to any specific provider.

## Files to Explore

### Find all occurrences:
```bash
grep -r "ik_openai_msg_t" src/ tests/ --include="*.c" --include="*.h"
```

### Likely locations:
- `src/openai/client_msg.h` - Type definition
- `src/openai/client_msg.c` - Factory functions
- `src/openai/client.h` - API declarations
- `src/openai/client.c` - Usage in conversion functions
- All test files using the canonical message format

## Task

Perform a global rename refactoring:

1. **Update type definition**:
   - Find `typedef struct ik_openai_msg ik_openai_msg_t;`
   - Rename struct to `struct ik_msg`
   - Rename typedef to `ik_msg_t`

2. **Update factory functions**:
   - `ik_openai_msg_create()` → `ik_msg_create()`
   - `ik_openai_msg_create_tool_call()` → `ik_msg_create_tool_call()`
   - `ik_openai_msg_create_tool_result()` → `ik_msg_create_tool_result()`
   - Any other `ik_openai_msg_*` functions → `ik_msg_*`

3. **Update all variable declarations**:
   - `ik_openai_msg_t *msg` → `ik_msg_t *msg`
   - Function parameters
   - Struct fields
   - Return types

4. **Update all test files**:
   - Find all uses in `tests/unit/` and `tests/integration/`
   - Update type references
   - Verify tests still pass

5. **Verify compilation**:
   ```bash
   make clean
   make BUILD=debug
   make check
   ```

## Success Criteria

- All instances of `ik_openai_msg_t` renamed to `ik_msg_t`
- All related functions renamed from `ik_openai_msg_*` to `ik_msg_*`
- `grep -r "ik_openai_msg_t" src/ tests/` returns zero results
- `make check` passes (100% tests)
- `make lint` passes
- No compilation warnings

## Notes

- This is a pure refactoring - no behavior changes
- The type definition likely stays in `src/openai/client_msg.h` for now (can be moved to a provider-agnostic location in a future task)
- Use your editor's search-and-replace or a script for consistency
- Commit message: "Rename ik_openai_msg_t to ik_msg_t for provider independence"
