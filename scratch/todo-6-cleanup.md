# TODO: Cleanup (Phase 2) Alignment

Plan doc: `scratch/plan/README.md` - Phase 2 Removal section
Status: 4 items to resolve

---

## 6.1 Missing: Old Fixtures Cleanup

**Priority:** High (affects correctness)

- **Issue:** `tests/fixtures/openai/` (7 files) not scheduled for deletion
- **Plan ref:** plan/README.md - Phase 2 removal
- **Action:** Create `cleanup-openai-fixtures.md` or add to `cleanup-openai-tests.md`

**Files to delete:**
```
tests/fixtures/openai/
  error_401_unauthorized.json
  error_429_rate_limit.json
  error_500_server.json
  stream_done.txt
  stream_hello_world.txt
  stream_multiline.txt
  stream_with_usage.txt
```

**Resolution options:**
- [ ] Add fixtures deletion to `cleanup-openai-tests.md`
- [ ] Or create separate `cleanup-openai-fixtures.md`

---

## 6.2 Missing: Integration Tests Cleanup

**Priority:** High (affects correctness)

- **Issue:** `tests/integration/openai/` not explicitly listed for deletion
- **Plan ref:** plan/README.md - Phase 2 removal
- **Action:** Add to `cleanup-openai-tests.md`

**Files to delete:**
```
tests/integration/openai/
  http_handler_tool_calls_test.c
```

**Task to update:**
- [ ] `cleanup-openai-tests.md` - add integration test directory

---

## 6.3 Incomplete: File List in cleanup-openai-source.md

**Priority:** Medium (completeness)

- **Issue:** Task lists 9 files but `src/openai/` contains 19 items including headers
- **Action:** Update task with complete file inventory

**Current task lists (9 files):**
- client.c, client.h
- client_multi.c, client_multi.h
- client_multi_callbacks.c, client_multi_request.c
- client_msg.c, client_serialize.c
- tool_choice.c, tool_choice.h

**Additional files in src/openai/ (need verification):**
- http_handler.c, http_handler.h (migrated to providers/common?)
- sse_parser.c, sse_parser.h (migrated to providers/common?)
- client_multi_callbacks.h
- client_multi_internal.h
- Other headers?

**Task to update:**
- [ ] `cleanup-openai-source.md` - complete file inventory
- [ ] Clarify which files migrated vs deleted

---

## 6.4 Clarify: Adapter Shim in Phase 2 Plan

**Priority:** Low (documentation)

- **Issue:** `cleanup-openai-adapter.md` exists but shim not mentioned in Phase 2 checklist
- **Plan ref:** plan/README.md - Phase 2 prerequisites
- **Action:** Add shim removal to Phase 2 checklist in plan

**Current Phase 2 checklist missing:**
- Delete adapter shim (src/providers/openai/adapter_shim.{c,h})

**Resolution:**
- [ ] Update plan/README.md Phase 2 section to include shim deletion
- [ ] Or document shim is implicitly part of "cleanup old code"

---

## Phase 2 Prerequisites Verification

Before executing any cleanup tasks, verify all prerequisites from plan/README.md:

- [ ] `make test` passes with new provider abstraction
- [ ] All VCR tests for OpenAI provider pass
- [ ] All VCR tests for Anthropic provider pass
- [ ] All VCR tests for Google provider pass
- [ ] `/model claude-sonnet-4-5/med` works
- [ ] `/model gpt-4o` works
- [ ] `/model gemini-2.5-pro/med` works
- [ ] `/fork --model` creates child agent with correct provider
- [ ] `grep -r "openai/client" src/` returns only `src/providers/openai/` paths
- [ ] No production code imports from `src/openai/` (old location)
- [ ] Makefile updated to build from `src/providers/` only

---

## Completion Checklist

- [ ] 6.1 resolved
- [ ] 6.2 resolved
- [ ] 6.3 resolved
- [ ] 6.4 resolved
