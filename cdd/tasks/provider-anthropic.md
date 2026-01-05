# Task: Anthropic Provider Integration

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All context is provided.

**Model:** sonnet/thinking
**Depends on:** discovery-infrastructure.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

This task integrates the tool registry with the Anthropic provider. Anthropic serialization should read tools from `req->tools[]` (populated by request builder) and transform to Anthropic format.

## Pre-Read

**Skills:**
- `/load errors` - res_t patterns
- `/load style` - Code style

**Plan:**
- `cdd/plan/architecture.md` - Section "Schema Transformation" for Anthropic format
- `cdd/plan/integration-specification.md` - Section "Anthropic Provider"

**Source:**
- `src/providers/anthropic/request.c` - Current serialization
- `src/providers/request_tools.c` - Where tools are populated into req->tools[]

## Libraries

Use only existing libraries. No new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)
- [ ] Discovery infrastructure complete
- [ ] Registry populated with 6 tools

## Objective

Verify and update Anthropic provider to correctly serialize tools from registry:

1. **Verify** `req->tools[]` is populated from registry (done in request_tools.c)
2. **Verify** Anthropic serializer reads from `req->tools[]`
3. **Update** schema transformation if needed for Anthropic format

## Anthropic Schema Format

Anthropic uses `input_schema` key for tool parameters:

```json
{
  "name": "bash",
  "description": "Execute a shell command",
  "input_schema": {
    "type": "object",
    "properties": {...},
    "required": [...]
  }
}
```

**Key differences from canonical JSON Schema:**
- Uses `input_schema` instead of `parameters`
- Keeps `additionalProperties` as-is (passthrough)
- Keeps `required` array as-is

## Expected Behavior

When making LLM request with Claude model:
1. `ik_request_build_from_conversation()` populates `req->tools[]` from registry
2. `ik_anthropic_serialize_request_stream()` reads `req->tools[]`
3. For each tool, transforms to Anthropic format with `input_schema`
4. Final request JSON includes `tools` array with all 6 tools

## Test Specification

**Reference:** `cdd/plan/test-specification.md` → "Phase 4: Provider Integration" → "provider-anthropic.md"

**Test file to create:** `tests/unit/providers/anthropic/tools_serialize_test.c`

**Goals:** Verify tools from registry serialized to Anthropic format with `input_schema`.

| Test | Goal |
|------|------|
| `test_serialize_tools_empty` | No tools → no tools array in request |
| `test_serialize_tools_single` | Single tool has Anthropic format |
| `test_serialize_tools_input_schema` | Parameters wrapped in `input_schema` key |
| `test_serialize_tools_all_fields` | name, description, input_schema all present |
| `test_serialize_preserves_additionalProperties` | additionalProperties passed through |

**Mocking:** Create mock registry with known entries, verify JSON output structure.

**Pattern:** Follow `tests/unit/providers/anthropic/request_serialize_test.c`

**Integration verification:**
1. Request to Claude includes tools array
2. Each tool has `input_schema` (not `parameters`)
3. `make check` passes

## Files to Review/Modify

- `src/providers/anthropic/request.c` - Serialization logic
- `src/providers/request.h` - Function signature changes
- `src/providers/request_tools.c` - Tool population

## Completion

After completing work, commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(provider-anthropic.md): [success|partial|failed] - Anthropic provider integration

[Details about verification/changes]
EOF
)"
```

Report status:
- Success: `/task-done provider-anthropic.md`
- Partial/Failed: `/task-fail provider-anthropic.md`

## Postconditions

- [ ] Anthropic requests include tools from registry
- [ ] Schema uses `input_schema` format
- [ ] `make check` passes
- [ ] All changes committed
- [ ] Working copy is clean
