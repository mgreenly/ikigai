# TODO: Architecture Alignment

Plan docs: `scratch/plan/01-architecture/`
Status: 4 items to resolve

---

## 1.1 Missing: Request Builder Task

**Priority:** Critical (blocks implementation)

- **Issue:** `repl-provider-routing.md` calls `ik_request_build_from_conversation()` but no task defines this
- **Plan ref:** 01-architecture/overview.md - Request normalization
- **Action:** Verify `request-builders.md` covers this or create new task

**Investigation needed:**
- [ ] Read `scratch/tasks/request-builders.md`
- [ ] Check if `ik_request_build_from_conversation()` is defined
- [ ] If missing, add to task or create new task

---

## 1.2 Missing: Thinking Validation Enforcement

**Priority:** High (affects correctness)

- **Issue:** Plan requires providers validate thinking level in `start_request()`/`start_stream()` with `ERR_INVALID_ARG`, but no task implements validation
- **Plan ref:** 01-architecture/provider-interface.md - Thinking Config section
- **Action:** Update provider-core tasks to include validation logic

**Tasks to update:**
- [ ] `openai-core.md` - add `ik_openai_validate_thinking()`
- [ ] `anthropic-core.md` - add `ik_anthropic_validate_thinking()`
- [ ] `google-core.md` - add `ik_google_validate_thinking()`

---

## 1.3 Missing: Model Change Provider Reset

**Priority:** High (affects correctness)

- **Issue:** `agent-provider-fields.md` has no mechanism to detect model change and reset cached provider
- **Plan ref:** 01-architecture/overview.md - Provider lifecycle
- **Action:** Add provider cache invalidation to `model-command.md`

**Tasks to update:**
- [ ] `model-command.md` - add provider invalidation on model change
- [ ] `agent-provider-fields.md` - add `ik_agent_clear_provider()` or similar

---

## 1.4 Clarify: OpenAI Shim Technical Debt

**Priority:** Low (documentation only)

- **Issue:** 7 tasks implement OpenAI shim that will be deleted in Phase 2
- **Files:** openai-shim-*.md, cleanup-openai-adapter.md
- **Action:** Document this is intentional Phase 1 approach, not a plan/task mismatch

**Resolution options:**
- [ ] Add note to plan/README.md explaining shim is temporary Phase 1 bridge
- [ ] Or accept as-is (shim tasks are correctly linked to cleanup task)

---

## Completion Checklist

- [ ] 1.1 resolved
- [ ] 1.2 resolved
- [ ] 1.3 resolved
- [ ] 1.4 resolved
