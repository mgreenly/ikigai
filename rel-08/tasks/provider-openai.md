# Task: OpenAI Provider Integration

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All context is provided.

**Model:** sonnet/thinking
**Depends on:** provider-google.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

This task integrates the tool registry with the OpenAI provider. OpenAI has the most complex transformation due to strict mode requirements.

## Pre-Read

**Skills:**
- `/load errors` - res_t patterns
- `/load style` - Code style

**Plan:**
- `cdd/plan/architecture.md` - Section "Schema Transformation" for OpenAI format
- `cdd/plan/integration-specification.md` - Section "OpenAI Provider Note"

**Source:**
- `src/providers/openai/request_chat.c` - Current serialization
- `src/providers/request_tools.c` - Where tools are populated

## Libraries

Use only existing libraries. No new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)
- [ ] Google integration complete

## Objective

Verify and update OpenAI provider to correctly serialize tools from registry:

1. **Verify** `req->tools[]` is populated from registry
2. **Check** if OpenAI serializer reads from `req->tools[]` or calls `ik_tool_build_all()` directly
3. **Update** to use registry if needed
4. **Verify** strict mode transformation is correct

## OpenAI Schema Format

OpenAI uses function wrapper with strict mode:

```json
{
  "type": "function",
  "function": {
    "name": "bash",
    "description": "Execute a shell command",
    "strict": true,
    "parameters": {
      "type": "object",
      "properties": {...},
      "required": ["command"],
      "additionalProperties": false
    }
  }
}
```

**Key differences from canonical JSON Schema:**
- Wrapped in `{type: "function", function: {...}}`
- Adds `strict: true` for structured outputs
- **MUST add** `additionalProperties: false`
- **MUST add** ALL property names to `required[]` (even optional ones)

## Test Specification

**Reference:** `cdd/plan/test-specification.md` → "Phase 4: Provider Integration" → "provider-openai.md"

**Test file to create:** `tests/unit/providers/openai/tools_serialize_test.c`

**Goals:** Verify tools wrapped in function type with strict mode transformations.

| Test | Goal |
|------|------|
| `test_serialize_function_wrapper` | Each tool has `{type:"function", function:{...}}` |
| `test_serialize_strict_true` | `strict:true` added to function object |
| `test_serialize_additional_properties_false` | `additionalProperties:false` in parameters |
| `test_serialize_all_properties_required` | All property names in required array |
| `test_serialize_optional_becomes_required` | Optional props added to required for strict |

**Mocking:** Create mock registry with tools having optional properties.

**Pattern:** Follow `tests/unit/providers/openai/request_chat_test.c`

**Expected Behavior (verify in tests):**
1. `ik_request_build_from_conversation()` populates `req->tools[]` from registry
2. `ik_openai_serialize_chat_request()` reads `req->tools[]`
3. Each tool wrapped in `{type: "function", function: {...}}`
4. `strict: true` and `additionalProperties: false` added
5. All properties in `required[]` (strict mode quirk)

## OpenAI Strict Mode Quirk

OpenAI's structured outputs require ALL properties in `required[]`, even ones that were optional in the original schema. This is a quirk of their strict mode validation.

**Original (from tool):**
```json
{
  "properties": {"command": {...}, "timeout": {...}},
  "required": ["command"]
}
```

**Transformed (for OpenAI):**
```json
{
  "properties": {"command": {...}, "timeout": {...}},
  "required": ["command", "timeout"],
  "additionalProperties": false
}
```

## Files to Review/Modify

- `src/providers/openai/request_chat.c` - Main serialization
- Check if it calls `ik_tool_build_all()` directly (needs update) or reads `req->tools[]`

## Completion

After completing work, commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(provider-openai.md): [success|partial|failed] - OpenAI provider integration

[Details about verification/changes]
EOF
)"
```

Report status:
- Success: `/task-done provider-openai.md`
- Partial/Failed: `/task-fail provider-openai.md`

## Postconditions

- [ ] OpenAI requests include tools from registry
- [ ] Schema has `strict: true` and function wrapper
- [ ] `additionalProperties: false` added
- [ ] All properties in `required[]`
- [ ] `make check` passes
- [ ] All changes committed
- [ ] Working copy is clean
