# Task: Google Provider Integration

**UNATTENDED EXECUTION:** This task executes automatically without human oversight. All context is provided.

**Model:** sonnet/thinking
**Depends on:** provider-anthropic.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

This task integrates the tool registry with the Google provider. Google serialization should read tools from `req->tools[]` and transform to Gemini format.

## Pre-Read

**Skills:**
- `/load errors` - res_t patterns
- `/load style` - Code style

**Plan:**
- `cdd/plan/architecture.md` - Section "Schema Transformation" for Google format
- `cdd/plan/integration-specification.md` - Section "Google Provider"

**Source:**
- `src/providers/google/request.c` - Current serialization
- `src/providers/request_tools.c` - Where tools are populated

## Libraries

Use only existing libraries. No new dependencies.

## Preconditions

- [ ] Working copy is clean (verify with `jj diff --summary`)
- [ ] Anthropic integration complete

## Objective

Verify and update Google provider to correctly serialize tools from registry:

1. **Verify** `req->tools[]` is populated from registry
2. **Verify** Google serializer reads from `req->tools[]`
3. **Update** schema transformation if needed for Gemini format

## Google Schema Format

Google uses `functionDeclarations` array:

```json
{
  "tools": [{
    "functionDeclarations": [
      {
        "name": "bash",
        "description": "Execute a shell command",
        "parameters": {
          "type": "object",
          "properties": {...},
          "required": [...]
        }
      }
    ]
  }]
}
```

**Key differences from canonical JSON Schema:**
- Wrapped in `tools[].functionDeclarations[]`
- Uses `parameters` key (same as canonical)
- **MUST remove** `additionalProperties` field (Gemini doesn't support it)
- Keeps `required` array as-is

## Expected Behavior

When making LLM request with Gemini model:
1. `ik_request_build_from_conversation()` populates `req->tools[]` from registry
2. `ik_google_serialize_request()` reads `req->tools[]`
3. For each tool, transforms to Gemini format in `functionDeclarations`
4. `additionalProperties` field is removed from schema
5. Final request JSON includes tools array

## Files to Review/Modify

- `src/providers/google/request.c` - Serialization logic
- `src/providers/google/request_helpers.c` - Helper functions

## Completion

After completing work, commit all changes:

```bash
jj commit -m "$(cat <<'EOF'
task(provider-google.md): [success|partial|failed] - Google provider integration

[Details about verification/changes]
EOF
)"
```

Report status:
- Success: `/task-done provider-google.md`
- Partial/Failed: `/task-fail provider-google.md`

## Postconditions

- [ ] Google requests include tools from registry
- [ ] Schema uses `functionDeclarations` format
- [ ] `additionalProperties` removed from schema
- [ ] `make check` passes
- [ ] All changes committed
- [ ] Working copy is clean
