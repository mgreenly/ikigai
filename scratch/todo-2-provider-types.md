# TODO: Provider Types Alignment

Plan doc: `scratch/plan/03-provider-types.md`
Status: 4 items to resolve (including 1 doc sync item)

---

## 2.1 Missing: Google Thought Signature Handling

**Priority:** High (affects correctness)

- **Issue:** Plan mentions thought signature resubmission for Gemini 3 but no explicit task
- **Plan ref:** 03-provider-types.md - Section 6.32
- **Action:** Add thought signature handling to `google-core.md` or create dedicated task

**Investigation needed:**
- [ ] Read plan section on thought signatures
- [ ] Check if `google-core.md` or `google-request.md` covers this
- [ ] If missing, add to existing task or create `google-thought-signatures.md`

---

## 2.2 Verify: OpenAI Streaming Tasks Complete

**Priority:** Medium (verification)

- **Issue:** Agent noted openai-streaming-*.md tasks exist but implementation details unclear
- **Action:** Review `openai-streaming-chat.md` and `openai-streaming-responses.md` for completeness

**Verification checklist:**
- [ ] Read `openai-streaming-chat.md` - verify SSE event mapping
- [ ] Read `openai-streaming-responses.md` - verify SSE event mapping
- [ ] Check both implement `ik_stream_event_t` emission
- [ ] Check both handle tool call streaming

---

## 2.3 Verify: Google Streaming Task Complete

**Priority:** Medium (verification)

- **Issue:** `google-streaming.md` exists but needs validation
- **Action:** Review task for SSE event mapping completeness

**Verification checklist:**
- [ ] Read `google-streaming.md`
- [ ] Verify SSE event types mapped to `ik_stream_event_t`
- [ ] Verify tool ID generation during streaming
- [ ] Verify thinking content handling

---

## 2.4 Doc Sync: Plan Diagram Shows Sync Flow

**Priority:** Low (documentation)

- **Issue:** Plan Section 3 shows sync transformation flow but tasks correctly implement async
- **Plan ref:** 03-provider-types.md - Section 3 diagram
- **Action:** Update plan diagram to show async pattern

**Resolution:**
- [ ] Update diagram in 03-provider-types.md to show async vtable pattern
- [ ] Or add note that diagram is conceptual, implementation is async

---

## Completion Checklist

- [ ] 2.1 resolved
- [ ] 2.2 resolved
- [ ] 2.3 resolved
- [ ] 2.4 resolved
