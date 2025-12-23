# TODO: Testing Strategy Alignment

Plan docs: `scratch/plan/05-testing/`
Status: 6 items to resolve (including 1 doc sync item)

---

## 5.1 Missing: Contract Validation Tests (Anthropic/Google)

**Priority:** Medium (improves quality)

- **Issue:** Only `openai-equivalence-validation.md` exists, no Anthropic/Google contract tests
- **Plan ref:** 05-testing/strategy.md - contract_validations/ directory
- **Action:** Create `contract-anthropic.md` and `contract-google.md` tasks

**Tasks to create:**
- [ ] `contract-anthropic.md` - validate mocks match real Anthropic API
- [ ] `contract-google.md` - validate mocks match real Google API

**Pattern from openai-equivalence-validation.md:**
- Record live API responses
- Compare mock responses to live responses
- Validate schema compatibility

---

## 5.2 Missing: Thinking Level Integration Tests

**Priority:** Medium (improves quality)

- **Issue:** Plan details cross-provider thinking validation but no task
- **Plan ref:** 05-testing/strategy.md - lines 258-273
- **Action:** Create `tests-thinking-levels.md` task

**Test scenarios to cover:**
- [ ] Each thinking level (none/low/med/high) maps correctly per provider
- [ ] Anthropic: thinking budget calculation
- [ ] OpenAI: reasoning effort mapping
- [ ] Google: thinking level vs budget (Gemini 2.5 vs 3)
- [ ] Cross-provider consistency

---

## 5.3 Missing: Performance Benchmarking Task

**Priority:** Low (nice to have)

- **Issue:** Plan specifies <100 microseconds serialization target but no task
- **Plan ref:** 05-testing/strategy.md - lines 289-293
- **Action:** Create `tests-performance-benchmarking.md` task

**Benchmark requirements:**
- Request serialization: 10 messages, 5 tools
- Use `clock_gettime(CLOCK_MONOTONIC)`
- 1000 iterations
- Target: < 100 microseconds per request

---

## 5.4 Missing: Coverage Enforcement Task

**Priority:** Low (nice to have)

- **Issue:** Plan specifies 100% coverage target but no measurement/enforcement task
- **Plan ref:** 05-testing/strategy.md - lines 295-312
- **Action:** Create `tests-coverage-enforcement.md` task

**Coverage requirements:**
- 100% target for provider adapters
- Critical paths: request serialization, response parsing, error mapping, thinking level, tool calls, streaming
- Tool: `make coverage`

**Note:** May already be covered by existing Makefile targets - verify before creating task.

---

## 5.5 Underspecified: Error Testing Coverage

**Priority:** Medium (improves quality)

- **Issue:** Tasks mention error tests but don't specify HTTP status code matrix
- **Plan ref:** 05-testing/strategy.md - lines 248-257
- **Action:** Expand error testing sections in tests-*-basic.md tasks

**HTTP status codes to test:**
| Code | Category | Tested? |
|------|----------|---------|
| 401 | ERR_AUTH | ? |
| 403 | ERR_AUTH | ? |
| 429 | ERR_RATE_LIMIT | ? |
| 500 | ERR_SERVER | ? |
| 503 | ERR_SERVER | ? |
| 504 | ERR_TIMEOUT (Google) | ? |

**Tasks to update:**
- [ ] `tests-anthropic-basic.md` - add error matrix
- [ ] `tests-openai-basic.md` - add error matrix
- [ ] `tests-google-basic.md` - add error matrix

---

## 5.6 Doc Sync: VCR Format Evolution

**Priority:** Low (documentation)

- **Issue:** Strategy shows simple JSON fixtures but implementation uses JSONL
- **Plan ref:** 05-testing/strategy.md - Fixture loading section
- **Action:** Update strategy to reflect JSONL implementation

**Current plan example:**
```c
const char *response_json = load_fixture("anthropic/response_basic.json");
```

**Actual implementation (vcr-core.md):**
- JSONL format with `_request`, `_response`, `_body`, `_chunk` lines
- `vcr_next_chunk()` API

**Resolution:**
- [ ] Update 05-testing/strategy.md fixture loading section

---

## Completion Checklist

- [ ] 5.1 resolved
- [ ] 5.2 resolved
- [ ] 5.3 resolved
- [ ] 5.4 resolved
- [ ] 5.5 resolved
- [ ] 5.6 resolved
