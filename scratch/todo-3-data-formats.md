# TODO: Data Formats Alignment

Plan docs: `scratch/plan/02-data-formats/`
Status: 3 items to resolve

---

## 3.1 Missing: Provider Data Persistence

**Priority:** Medium (improves quality)

- **Issue:** Plan mentions `provider_data` field for opaque metadata (Google thought signatures) but no task covers serialization/deserialization
- **Plan ref:** 02-data-formats/request-response.md - provider_data field
- **Action:** Add provider_data handling to response parsing tasks

**Tasks to update:**
- [ ] `google-response.md` - extract and store thought signatures in provider_data
- [ ] `google-request.md` - resubmit provider_data thought signatures
- [ ] Possibly `anthropic-response.md` if thinking verification data needed

**Note:** This is related to 2.1 (Google Thought Signatures) - may be resolved together.

---

## 3.2 Missing: Rate Limit Header Parsing (Centralized)

**Priority:** Low (design decision)

- **Issue:** Plan details rate limit headers but parsing is scattered per-provider, not centralized
- **Plan ref:** 02-data-formats/error-handling.md - Rate limit headers
- **Action:** Consider adding shared utility or accept decentralized approach

**Headers per provider:**
- Anthropic: `anthropic-ratelimit-*`, `retry-after`
- OpenAI: `x-ratelimit-*`, `x-ratelimit-reset-*`
- Google: `retryDelay` in error response body

**Resolution options:**
- [ ] Create shared `ik_parse_rate_limit_headers()` utility
- [ ] Or accept decentralized (each provider parses own headers) - simpler

---

## 3.3 Clarify: ik_content_block_t Definition Location

**Priority:** Low (verification)

- **Issue:** Union type referenced by request-builders.md but not explicitly in task list
- **Action:** Verify defined in `provider-types.md` task

**Verification:**
- [ ] Read `provider-types.md` task
- [ ] Confirm `ik_content_block_t` union is defined there
- [ ] If missing, add to task

---

## Completion Checklist

- [ ] 3.1 resolved
- [ ] 3.2 resolved
- [ ] 3.3 resolved
