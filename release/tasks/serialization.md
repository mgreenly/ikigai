# Task: Serialize Thinking Signatures

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All needed context is provided.

**Model:** sonnet
**Depends on:** types.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load errors` - For error handling patterns
- `/load style` - For code style conventions

**Source:**
- `src/providers/anthropic/request_serialize.c` - Serialization (lines 20-71)
- `tests/unit/providers/anthropic/request_serialize_success_test.c` - Test patterns
- `tests/unit/providers/anthropic/request_serialize_coverage_1_test.c` - Coverage patterns

**Plan:**
- `release/plan/thinking-signatures.md` - Section 7

## Libraries

- `yyjson` - Already used for JSON serialization (use wrapper functions)

Do not introduce new dependencies.

## Preconditions

- [ ] Git workspace is clean
- [ ] `types.md` task completed (signature field and redacted_thinking type exist)

## Objective

Update `ik_anthropic_serialize_content_block` to:
1. Include `signature` field when serializing thinking blocks
2. Handle `IK_CONTENT_REDACTED_THINKING` blocks

## Interface

**Function:** `ik_anthropic_serialize_content_block`

### Thinking Block Serialization

Current (lines 36-39):
```c
case IK_CONTENT_THINKING:
    if (!yyjson_mut_obj_add_str_(doc, obj, "type", "thinking")) return false;
    if (!yyjson_mut_obj_add_str_(doc, obj, "thinking", block->data.thinking.text)) return false;
    break;
```

New:
```c
case IK_CONTENT_THINKING:
    if (!yyjson_mut_obj_add_str_(doc, obj, "type", "thinking")) return false;
    if (!yyjson_mut_obj_add_str_(doc, obj, "thinking", block->data.thinking.text)) return false;
    if (block->data.thinking.signature != NULL) {
        if (!yyjson_mut_obj_add_str_(doc, obj, "signature", block->data.thinking.signature)) return false;
    }
    break;
```

### Redacted Thinking Block Serialization

Add new case before `default`:
```c
case IK_CONTENT_REDACTED_THINKING:
    if (!yyjson_mut_obj_add_str_(doc, obj, "type", "redacted_thinking")) return false;
    if (!yyjson_mut_obj_add_str_(doc, obj, "data", block->data.redacted_thinking.data)) return false;
    break;
```

## Behaviors

- Signature is only serialized if non-NULL (allows backwards compat)
- Redacted thinking uses `"type": "redacted_thinking"` and `"data"` field
- Order of JSON fields doesn't matter to Anthropic API
- Return false on any serialization failure (caller handles error)

## Test Scenarios

Add tests in `request_serialize_success_test.c` or new file:

1. `test_serialize_thinking_with_signature` - Verify JSON includes signature
2. `test_serialize_thinking_null_signature` - Verify no signature field when NULL
3. `test_serialize_redacted_thinking` - Verify correct JSON format

Expected JSON for thinking:
```json
{"type": "thinking", "thinking": "text", "signature": "sig"}
```

Expected JSON for redacted_thinking:
```json
{"type": "redacted_thinking", "data": "encrypted"}
```

## Completion

```bash
git add -A
git commit -m "$(cat <<'EOF'
task(serialization.md): success - serialize thinking signatures

Added signature field to thinking block serialization.
Added redacted_thinking block serialization.
EOF
)"
```

Report status: `/task-done serialization.md`

## Postconditions

- [ ] Compiles without warnings
- [ ] All tests pass (`make check`)
- [ ] All changes committed
- [ ] Git workspace is clean
