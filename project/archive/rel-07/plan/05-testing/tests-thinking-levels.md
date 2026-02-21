# Task: Cross-Provider Thinking Level Validation Tests

**Model:** sonnet/thinking
**Depends on:** tests-anthropic-basic.md, tests-openai-basic.md, tests-google-basic.md, contract-anthropic.md, contract-google.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)
- [ ] All provider basic tests implemented and passing
- [ ] Provider contract validation tests implemented and passing

## Pre-Read

**Skills:**
- `/load errors` - Result types with OK()/ERR() patterns
- `/load memory` - talloc-based memory management

**Source:**
- `src/providers/provider.h` - Thinking level enumeration
- `src/providers/anthropic/thinking.h` - Anthropic thinking budget calculation
- `src/providers/openai/reasoning.h` - OpenAI reasoning effort mapping
- `src/providers/google/thinking.h` - Google thinking config mapping
- `tests/unit/providers/` - Existing provider test patterns

**Plan:**
- `scratch/plan/05-testing/strategy.md` - Testing strategy, thinking level coverage
- `scratch/plan/03-provider-types.md` - Provider-specific thinking transformations
- `scratch/plan/02-data-formats/request-response.md` - Internal thinking representation

## Objective

Create comprehensive cross-provider integration tests that validate thinking level abstraction works consistently across all providers. Tests verify that the same thinking level enum (MIN/LOW/MED/HIGH) correctly maps to provider-specific parameters (Anthropic token budgets, OpenAI effort levels, Google thinkingConfig), thinking content is extracted correctly, thinking tokens are counted accurately, and thinking levels can be changed mid-session.

## Rationale

The thinking abstraction is a critical feature that must work consistently across all providers. Users expect IK_THINKING_MED to produce similar behavior whether using Anthropic Claude, OpenAI o-series, or Google Gemini. These tests validate:

1. **Budget mapping correctness** - Each provider calculates budgets correctly for each level
2. **Request serialization** - Thinking config serialized to correct provider format
3. **Response extraction** - Thinking content extracted from different response formats
4. **Token accounting** - Thinking tokens counted separately from output tokens
5. **Level transitions** - Changing thinking levels mid-session works correctly
6. **Edge cases** - Thinking disabled, max budgets, invalid configs handled properly

## Interface

### Test Functions to Implement

| Function | Purpose |
|----------|---------|
| `test_thinking_budget_anthropic` | Verify Anthropic budget calculation for all 4 levels |
| `test_thinking_budget_openai` | Verify OpenAI effort mapping for all 4 levels |
| `test_thinking_budget_google` | Verify Google thinkingBudget calculation for all 4 levels |
| `test_thinking_serialization_anthropic` | Verify thinking config in Anthropic request JSON |
| `test_thinking_serialization_openai` | Verify reasoning effort in OpenAI request JSON |
| `test_thinking_serialization_google` | Verify thinkingConfig in Google request JSON |
| `test_thinking_extraction_anthropic` | Verify thinking content extracted from thinking blocks |
| `test_thinking_extraction_openai` | Verify reasoning extracted from reasoning_content field |
| `test_thinking_extraction_google` | Verify thoughts extracted from thoughts field |
| `test_thinking_tokens_anthropic` | Verify thinking_tokens counted correctly (Anthropic) |
| `test_thinking_tokens_openai` | Verify reasoning_tokens counted correctly (OpenAI) |
| `test_thinking_tokens_google` | Verify thoughtsTokenCount counted correctly (Google) |
| `test_thinking_level_transition` | Verify changing levels mid-session works |
| `test_thinking_none_skips_config` | Verify IK_THINKING_MIN skips thinking config |
| `test_thinking_max_budget` | Verify HIGH level uses maximum budget |
| `test_thinking_unsupported_model` | Verify models without thinking skip config |
| `test_thinking_streaming_content` | Verify thinking content in streaming responses |
| `test_thinking_summary_flag` | Verify include_summary flag serialized correctly |

### Helper Functions

| Function | Purpose |
|----------|---------|
| `create_thinking_request(level, model)` | Create test request with thinking level |
| `verify_thinking_budget(provider, level, expected_budget)` | Verify budget calculation |
| `verify_thinking_in_request_json(json, provider, level)` | Verify thinking serialized in request |
| `verify_thinking_content_extracted(response)` | Verify thinking content present |
| `verify_thinking_token_count(usage, expected_count)` | Verify token counting |
| `load_thinking_fixture(provider, level)` | Load VCR fixture for thinking test |

## Behaviors

### Budget Calculation Per Provider

**Anthropic Claude Sonnet 4.5:**
- Model max: 64,000 tokens
- Minimum: 1,024 tokens (enforced by API)
- IK_THINKING_MIN → 1,024 tokens
- IK_THINKING_LOW → 22,016 tokens (1/3 of range from min to max)
- IK_THINKING_MED → 43,008 tokens (2/3 of range from min to max)
- IK_THINKING_HIGH → 64,000 tokens

**OpenAI GPT-5 series (gpt-5-nano, gpt-5-mini, gpt-5):**
- No token budget, uses effort enum
- IK_THINKING_MIN → "effort": "none"
- IK_THINKING_LOW → "effort": "low"
- IK_THINKING_MED → "effort": "medium"
- IK_THINKING_HIGH → "effort": "high"

**Google Gemini 3.0 Flash/Pro:**
- Uses thinkingLevel instead of thinkingBudget
- IK_THINKING_MIN → "LOW" (cannot disable)
- IK_THINKING_LOW → "LOW"
- IK_THINKING_MED → "HIGH"
- IK_THINKING_HIGH → "HIGH"

### Request Serialization Format

**Anthropic:**
```json
{
  "thinking": {
    "type": "enabled",
    "budget_tokens": 43008
  }
}
```

**OpenAI (Responses API):**
```json
{
  "reasoning": {
    "effort": "medium",
    "summary": "auto"
  }
}
```

**Google (Gemini API - 3.0 series):**
```json
{
  "generationConfig": {
    "thinkingConfig": {
      "thinkingLevel": "HIGH",
      "includeThoughts": true
    }
  }
}
```

### Response Extraction Format

**Anthropic - Thinking Blocks:**
```json
{
  "content": [
    {
      "type": "thinking",
      "text": "Let me analyze this step by step..."
    },
    {
      "type": "text",
      "text": "The answer is 42."
    }
  ]
}
```

**OpenAI - Reasoning Content:**
```json
{
  "output": [
    {
      "type": "message",
      "content": [
        {
          "type": "reasoning_content",
          "reasoning_content": "Analyzing the problem..."
        },
        {
          "type": "output_text",
          "text": "The answer is 42."
        }
      ]
    }
  ]
}
```

**Google - Thoughts Field:**
```json
{
  "candidates": [
    {
      "content": {
        "parts": [
          {"text": "The answer is 42."}
        ],
        "thoughts": "First I considered... then I realized..."
      }
    }
  ]
}
```

### Token Counting

**Anthropic:**
- `usage.thinking_tokens` - Direct field in response
- `usage.input_tokens` - Prompt tokens
- `usage.output_tokens` - Generated text tokens (excluding thinking)

**OpenAI:**
- `usage.completion_tokens_details.reasoning_tokens` - Reasoning tokens
- `usage.completion_tokens` - Total completion tokens (includes reasoning)
- `output_tokens = completion_tokens - reasoning_tokens`
- `thinking_tokens = reasoning_tokens`

**Google:**
- `usageMetadata.thoughtsTokenCount` - Thinking tokens
- `usageMetadata.candidatesTokenCount` - Total output tokens (includes thoughts)
- `output_tokens = candidatesTokenCount - thoughtsTokenCount`
- `thinking_tokens = thoughtsTokenCount`

### Thinking Level Transitions

Tests must verify that changing thinking level mid-session works correctly:

1. Create request with IK_THINKING_LOW
2. Send request, verify LOW budget used
3. Change to IK_THINKING_HIGH
4. Send another request, verify HIGH budget used
5. Verify both requests used same provider/model
6. Verify conversation history preserved across level changes

### Edge Cases

**IK_THINKING_MIN:**
- No thinking config sent in request
- Thinking content still extracted if model produces it
- Thinking tokens still counted if present

**Maximum Budget:**
- IK_THINKING_HIGH uses provider's absolute maximum
- Anthropic: 64,000 tokens (Sonnet/Opus) or 32,000 tokens (Haiku)
- Google: "HIGH" level (Gemini 3.0 series) or 24,576 tokens (Gemini 2.5 Flash Lite)
- OpenAI: "high" effort (no token budget)

**Unsupported Models:**
- All models in scope support thinking
- GPT-5 series: full reasoning effort support
- Claude series: full thinking budget support
- Gemini 3.0/2.5: full thinking support

**Invalid Configurations:**
- Negative thinking level: returns IK_ERR_CAT_INVALID_ARG
- Unknown thinking level: returns IK_ERR_CAT_INVALID_ARG
- Thinking on non-thinking model: silently skipped

## Directory Structure

```
tests/integration/
├── test_thinking_levels_e2e.c        - Main thinking integration tests
├── test_thinking_budgets.c           - Budget calculation tests per provider
├── test_thinking_serialization.c     - Request serialization tests
├── test_thinking_extraction.c        - Response parsing tests
├── test_thinking_streaming.c         - Streaming thinking content tests

tests/fixtures/vcr/
├── anthropic/
│   ├── thinking_none.jsonl
│   ├── thinking_low.jsonl
│   ├── thinking_med.jsonl
│   ├── thinking_high.jsonl
│   └── thinking_streaming.jsonl
├── openai/
│   ├── reasoning_low.jsonl
│   ├── reasoning_med.jsonl
│   ├── reasoning_high.jsonl
│   └── reasoning_streaming.jsonl
└── google/
    ├── thinking_low.jsonl
    ├── thinking_med.jsonl
    ├── thinking_high.jsonl
    └── thinking_streaming.jsonl
```

## Test Scenarios

### Budget Calculation Tests (3 tests)

**Anthropic Budget Calculation:**
1. For IK_THINKING_MIN: verify budget = 1024
2. For IK_THINKING_LOW: verify budget = 22,016
3. For IK_THINKING_MED: verify budget = 43,008
4. For IK_THINKING_HIGH: verify budget = 64,000
5. Assert: all budgets within valid range [1024, 64000]

**OpenAI Effort Mapping:**
1. For IK_THINKING_MIN: verify no reasoning parameter
2. For IK_THINKING_LOW: verify effort = "low"
3. For IK_THINKING_MED: verify effort = "medium"
4. For IK_THINKING_HIGH: verify effort = "high"
5. Assert: effort string matches expected value exactly

**Google Level Mapping (Gemini 3.0):**
1. For IK_THINKING_MIN: verify thinkingLevel = "LOW"
2. For IK_THINKING_LOW: verify thinkingLevel = "LOW"
3. For IK_THINKING_MED: verify thinkingLevel = "HIGH"
4. For IK_THINKING_HIGH: verify thinkingLevel = "HIGH"
5. Assert: level string matches expected value exactly

### Serialization Tests (3 tests)

**Anthropic Request Serialization:**
1. Create request with IK_THINKING_MED, model claude-sonnet-4-5
2. Serialize request to JSON
3. Parse JSON, extract thinking.budget_tokens
4. Verify budget_tokens ≈ 43,008
5. Verify thinking.type = "enabled"
6. Assert: JSON valid and parseable

**OpenAI Request Serialization:**
1. Create request with IK_THINKING_MED, model gpt-5-mini
2. Serialize request to JSON
3. Parse JSON, extract reasoning.effort
4. Verify effort = "medium"
5. Verify reasoning.summary present (default: "auto")
6. Assert: JSON valid and parseable

**Google Request Serialization:**
1. Create request with IK_THINKING_MED, model gemini-3.0-flash
2. Serialize request to JSON
3. Parse JSON, extract generationConfig.thinkingConfig
4. Verify thinkingLevel = "HIGH"
5. Verify includeThoughts = true
6. Assert: JSON valid and parseable

### Content Extraction Tests (3 tests)

**Anthropic Thinking Extraction:**
1. Load VCR fixture: thinking_med.jsonl
2. Create provider, send request with IK_THINKING_MED
3. Parse response
4. Verify content array has thinking block (type="thinking")
5. Verify thinking text non-empty
6. Verify text block follows thinking block
7. Assert: content blocks in correct order

**OpenAI Reasoning Extraction:**
1. Load VCR fixture: reasoning_med.jsonl
2. Create provider, send request with IK_THINKING_MED
3. Parse response
4. Verify output contains reasoning_content item
5. Verify reasoning_content field non-empty
6. Verify output_text item present
7. Assert: reasoning extracted successfully

**Google Thoughts Extraction:**
1. Load VCR fixture: thinking_med.jsonl
2. Create provider, send request with IK_THINKING_MED
3. Parse response
4. Verify candidates[0].content.thoughts field present
5. Verify thoughts string non-empty
6. Verify parts array has text
7. Assert: thoughts extracted from separate field

### Token Counting Tests (3 tests)

**Anthropic Thinking Tokens:**
1. Load VCR fixture with known token counts
2. Parse response
3. Verify usage.thinking_tokens > 0
4. Verify usage.output_tokens excludes thinking tokens
5. Verify total_tokens = input_tokens + output_tokens + thinking_tokens
6. Assert: token counts add up correctly

**OpenAI Reasoning Tokens:**
1. Load VCR fixture with reasoning
2. Parse response
3. Verify usage.completion_tokens_details.reasoning_tokens > 0
4. Verify output_tokens = completion_tokens - reasoning_tokens
5. Verify thinking_tokens = reasoning_tokens
6. Assert: token calculation correct

**Google Thoughts Tokens:**
1. Load VCR fixture with thoughts
2. Parse response
3. Verify usageMetadata.thoughtsTokenCount > 0
4. Verify output_tokens = candidatesTokenCount - thoughtsTokenCount
5. Verify thinking_tokens = thoughtsTokenCount
6. Assert: token calculation correct

### Level Transition Test (1 test)

**Mid-Session Thinking Level Change:**
1. Create agent with IK_THINKING_LOW
2. Send message, capture request JSON
3. Verify LOW budget used in request
4. Change thinking level to IK_THINKING_HIGH
5. Send another message, capture request JSON
6. Verify HIGH budget used in second request
7. Verify same provider and model used
8. Verify conversation history includes both messages
9. Assert: level transition successful

### Edge Case Tests (5 tests)

**MIN Level Skips Config:**
1. Create request with IK_THINKING_MIN
2. Serialize to JSON
3. Verify no thinking/reasoning/thinkingConfig parameter
4. Send request, parse response
5. If response has thinking content, verify still extracted
6. Assert: MIN level works correctly

**HIGH Level Uses Maximum:**
1. For each provider, create request with IK_THINKING_HIGH
2. Verify Anthropic uses 64,000 tokens (Sonnet/Opus)
3. Verify OpenAI uses "high" effort
4. Verify Google uses "HIGH" level
5. Assert: maximum budgets/levels correct

**All Models Support Thinking:**
1. Verify gpt-5-nano, gpt-5-mini, gpt-5 all accept effort param
2. Verify claude-haiku-4-5, claude-sonnet-4-5, claude-opus-4-5 all accept budget
3. Verify gemini-2.5-flash-lite, gemini-3.0-flash, gemini-3.0-pro all accept thinking config
4. Assert: all models in scope support thinking

**Invalid Thinking Level:**
1. Create request with invalid thinking level (-1)
2. Attempt to serialize
3. Verify returns IK_ERR_CAT_INVALID_ARG
4. Assert: invalid level rejected

**Streaming Thinking Content:**
1. For each provider, start_stream() with IK_THINKING_MED
2. Drive event loop
3. Collect stream events
4. Verify thinking content delivered via stream events
5. Anthropic: content_block_delta with thinking type
6. OpenAI: delta with reasoning_content
7. Google: chunks with thoughts field
8. Assert: streaming thinking works across all providers

### Summary Flag Test (1 test)

**Include Summary Flag:**
1. Create request with include_summary=true
2. Serialize for each provider
3. Anthropic: verify thinking.include_summary (if supported)
4. OpenAI: verify reasoning.summary = "auto"
5. Google: verify includeThoughts = true
6. Assert: summary flags serialized correctly

## Postconditions

**Test Coverage:**
- [ ] 5 test files created covering all thinking scenarios
- [ ] 18+ tests total across all files
- [ ] All 4 thinking levels tested (MIN/LOW/MED/HIGH)
- [ ] All 3 providers tested (Anthropic, OpenAI, Google)
- [ ] Budget calculation verified for each provider
- [ ] Request serialization verified for each provider
- [ ] Response extraction verified for each provider
- [ ] Token counting verified for each provider
- [ ] Level transitions tested
- [ ] Edge cases covered (min, max, unsupported, invalid)

**Fixture Coverage:**
- [ ] 15+ VCR fixtures created (5 per provider)
- [ ] Fixtures cover MIN/LOW/MED/HIGH for each provider
- [ ] Streaming thinking fixtures for each provider
- [ ] All fixtures recorded with VCR_RECORD=1
- [ ] No API keys in fixtures (verify: grep -r api_key tests/fixtures/)

**Integration:**
- [ ] Tests integrated into Makefile
- [ ] `make test-thinking` target runs all thinking tests
- [ ] Tests included in `make check`
- [ ] Tests pass with VCR playback (no live API calls)

**Quality:**
- [ ] All tests compile without warnings
- [ ] All tests pass
- [ ] 100% coverage on thinking-related code paths
- [ ] No memory leaks (verify: make valgrind)

**Git Requirements:**
- [ ] Changes committed to git with message: `task: tests-thinking-levels.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Verification

```bash
# Build thinking tests
make build/tests/integration/test_thinking_levels_e2e
make build/tests/integration/test_thinking_budgets
make build/tests/integration/test_thinking_serialization
make build/tests/integration/test_thinking_extraction
make build/tests/integration/test_thinking_streaming

# Run all thinking tests
make test-thinking

# Verify all pass
echo $?  # Should be 0

# Run with valgrind to check for leaks
make valgrind-thinking

# Verify fixtures exist
ls tests/fixtures/vcr/anthropic/thinking_*.jsonl
ls tests/fixtures/vcr/openai/reasoning_*.jsonl
ls tests/fixtures/vcr/google/thinking_*.jsonl

# Verify no credentials leaked
grep -r "sk-ant-" tests/fixtures/ || echo "OK - no Anthropic keys"
grep -r "sk-proj-" tests/fixtures/ || echo "OK - no OpenAI keys"
grep -r "AIza" tests/fixtures/ || echo "OK - no Google keys"
```

## Cross-Provider Consistency Requirements

These tests enforce that thinking works consistently across providers:

1. **Same level, similar behavior:** IK_THINKING_MED should produce roughly similar thinking depth across all providers
2. **Budget proportions:** LOW/MED/HIGH map to 1/3, 2/3, and max of each provider's range
3. **Content extraction:** Thinking content always extracted regardless of provider format
4. **Token accounting:** Thinking tokens always counted separately from output tokens
5. **Level changes:** All providers support changing thinking level mid-session
6. **MIN handling:** All providers handle thinking disabled correctly

## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).
