# Fix: Rename ->role to ->kind

## Agent
model: sonnet

## Skills to Load

Read these `.agents/skills/` files:
- `default.md` - Project context and structure
- `naming.md` - Naming conventions

## Context

**Prerequisite:** The previous fix (`msg-type-unification.md`) replaced all `ik_openai_msg_t` with `ik_msg_t`.

Now the code references `->role` but `ik_msg_t` uses `->kind` as its discriminator field.

The `ik_msg_t` struct (from `src/msg.h`):
```c
typedef struct {
    char *kind;       /* Message kind discriminator */
    char *content;    /* Message text content or human-readable summary */
    char *data_json;  /* Structured data for tool messages */
} ik_msg_t;
```

## High-Level Goal

**Replace all `->role` accesses with `->kind`.**

## Pre-conditions

- Previous fix completed (no `ik_openai_msg_t` in codebase)
- Code compiles but tests fail due to `->role` not existing

## Task

### Step 1: Find all occurrences

```bash
grep -rn "->role" src/ tests/ --include="*.c" --include="*.h"
```

### Step 2: Update src/openai/client_msg.c

Change all:
```c
msg->role = talloc_strdup(msg, "...");
```
To:
```c
msg->kind = talloc_strdup(msg, "...");
```

### Step 3: Update src/openai/client.c

In the serialization logic, `->role` is used to check message type and to set the wire format role. Update:

```c
// Change checks like:
if (strcmp(msg->role, "tool_call") == 0)
// To:
if (strcmp(msg->kind, "tool_call") == 0)
```

**IMPORTANT:** When serializing to OpenAI wire format, the JSON field is still `"role"`:
```c
yyjson_mut_obj_add_str(doc, msg_obj, "role", msg->kind)
```
This is correct - we're putting the `kind` value into the wire format's `role` field.

### Step 4: Update remaining files

For each file with `->role`:
- `src/marks.c`
- `src/repl_tool.c`
- `src/repl_actions.c`
- `src/repl_event_handlers.c`
- All test files

Change `->role` to `->kind`.

### Step 5: Verify

```bash
# Should return 0 results for struct field access
# (string literals like "role" in JSON are OK)
grep -rn "\->role" src/ tests/ --include="*.c" --include="*.h"

# Must compile clean
make clean && make BUILD=debug

# All tests must pass
make check

# Lint must pass
make lint
```

## Post-conditions

- Zero occurrences of `->role` (as struct field access) in src/ and tests/
- `make check` passes (100% tests)
- `make lint` passes
- `make BUILD=debug` compiles without warnings

## Notes

- String literals `"role"` in JSON serialization are CORRECT - that's the wire format field name
- Only change STRUCT FIELD ACCESS (`->role` or `.role`)
- The discriminator values remain the same: "user", "assistant", "system", "tool_call", "tool_result"
