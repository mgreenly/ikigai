# Task: Delete Legacy OpenAI Test Files

**Model:** sonnet/none
**Depends on:** cleanup-openai-source.md, tests-openai-basic.md, tests-openai-streaming.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load makefile` - Test target structure

**Source:**
- `tests/unit/openai/` - Legacy tests to delete
- `tests/unit/providers/openai/` - New tests (must exist and pass)

## Objective

Delete legacy test files in `tests/unit/openai/` after verifying replacement tests exist and pass in `tests/unit/providers/openai/`.

## Test File Mapping

Verify each new test exists before deleting old:

| Old Test | New Test | Coverage |
|----------|----------|----------|
| `tests/unit/openai/client_http_test.c` | `tests/unit/providers/openai/test_openai_client.c` | Basic client operations |
| `tests/unit/openai/client_http_sse_basic_test.c` | `tests/unit/providers/openai/test_openai_streaming.c` | SSE parsing |
| `tests/unit/openai/client_http_sse_chunk_test.c` | `tests/unit/providers/openai/test_openai_streaming.c` | Chunked events |
| `tests/unit/openai/client_http_sse_edge_test.c` | `tests/unit/providers/openai/test_openai_streaming.c` | Edge cases |
| `tests/unit/openai/client_multi_test.c` | `tests/unit/providers/openai/test_openai_adapter.c` | Multi-handle |
| `tests/unit/openai/client_multi_callbacks_test.c` | `tests/unit/providers/openai/test_openai_adapter.c` | Callbacks |
| `tests/unit/openai/client_serialize_test.c` | `tests/unit/providers/openai/test_openai_request.c` | Serialization |
| `tests/unit/openai/tool_choice_test.c` | `tests/unit/providers/common/test_tool_choice.c` | Tool choice |

## Behaviors

**Step 1: Verify new tests exist**
```bash
# All these files must exist
ls tests/unit/providers/openai/test_openai_client.c
ls tests/unit/providers/openai/test_openai_streaming.c
ls tests/unit/providers/openai/test_openai_adapter.c
ls tests/unit/providers/openai/test_openai_request.c
ls tests/unit/providers/common/test_tool_choice.c
```

**Step 2: Verify new tests pass**
```bash
make build/tests/unit/providers/openai/test_openai_client && ./build/tests/unit/providers/openai/test_openai_client
make build/tests/unit/providers/openai/test_openai_streaming && ./build/tests/unit/providers/openai/test_openai_streaming
make build/tests/unit/providers/openai/test_openai_adapter && ./build/tests/unit/providers/openai/test_openai_adapter
make build/tests/unit/providers/openai/test_openai_request && ./build/tests/unit/providers/openai/test_openai_request
```

**Step 3: Delete legacy test files**
```bash
rm -rf tests/unit/openai/
```

**Step 4: Delete legacy test fixtures**
```bash
rm -rf tests/fixtures/openai/
```

**Step 5: Delete legacy integration tests**
```bash
rm -rf tests/integration/openai/
```

**Step 6: Update Makefile**

Remove test targets for:
```
tests/unit/openai/client_http_test
tests/unit/openai/client_http_sse_basic_test
tests/unit/openai/client_http_sse_chunk_test
tests/unit/openai/client_http_sse_edge_test
tests/unit/openai/client_multi_test
tests/unit/openai/client_multi_callbacks_test
tests/unit/openai/client_serialize_test
tests/unit/openai/tool_choice_test
```

**Step 7: Verify full test suite**
```bash
make check
```

## Postconditions

- [ ] All new test files exist (5 files verified)
- [ ] All new tests pass individually
- [ ] `tests/unit/openai/` directory deleted
- [ ] `tests/fixtures/openai/` directory deleted
- [ ] `tests/integration/openai/` directory deleted
- [ ] Makefile updated to remove old test targets
- [ ] `make check` passes with no missing tests
- [ ] Changes committed to git with message: `task: cleanup-openai-tests.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).