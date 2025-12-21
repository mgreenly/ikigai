# Task: Update /model Command

**Model:** sonnet/thinking
**Depends on:** agent-provider-fields.md, configuration.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load source-code` - Map of source files by functional area

**Source:**
- `src/commands_basic.c` - Command handlers including /model
- `src/completion.c` - Tab completion provider

**Plan:**
- `scratch/plan/README.md` - Q6 Extended Thinking section
- `scratch/plan/thinking-abstraction.md` - User feedback and model capabilities

## Objective

Update `/model` command to support `MODEL/THINKING` syntax, infer provider from model prefix, update agent state with provider/model/thinking_level, and warn users when requesting thinking on models that don't support it. Implement model capability lookup to identify which models support thinking and their maximum budgets.

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t cmd_model_parse(const char *input, char **model, char **thinking)` | Parse MODEL/THINKING syntax, returns OK/ERR |
| `res_t ik_model_supports_thinking(const char *model, bool *supports)` | Check if model supports extended thinking |
| `res_t ik_model_get_thinking_budget(const char *model, int32_t *budget)` | Get max thinking tokens for model, 0 if unsupported |
| `res_t ik_infer_provider(const char *model, const char **provider)` | Infer provider from model name prefix |

Structs to define:

| Struct | Members | Purpose |
|--------|---------|---------|
| `ik_model_capability_t` | prefix (str), provider (str), supports_thinking (bool), max_thinking_tokens (int32_t) | Model capability lookup table entry |

Data structures to create:

| Structure | Purpose |
|-----------|---------|
| `MODEL_CAPABILITIES[]` | Static array of model capabilities for lookup |

## Behaviors

**Command Parsing:**
- Parse `/model MODEL/THINKING` syntax
- Extract model name before `/`
- Extract thinking level after `/`
- Default to current thinking level if `/THINKING` omitted
- Return ERR_INVALID_ARG for malformed input

**Provider Inference:**
- Match model prefix to provider:
  - `claude-*` -> "anthropic"
  - `gpt-*` or `o1*` or `o3*` -> "openai"
  - `gemini-*` -> "google"
- Use longest matching prefix
- Return ERR_NOT_FOUND for unknown prefixes

**Model Capability Lookup:**
- Search MODEL_CAPABILITIES array by prefix matching
- Return thinking support status
- Return max thinking token budget
- Handle unknown models gracefully (assume no thinking)

**Agent State Update:**
- Update agent->provider
- Update agent->model
- Update agent->thinking_level
- Persist to database

**User Feedback:**
- Display provider name and model
- Display thinking level with concrete value:
  - none -> "disabled"
  - low -> "low (14,336 tokens)" for Anthropic
  - medium -> "medium (43,008 tokens)" for Anthropic
  - high -> "high (86,016 tokens)" for Anthropic
  - extended -> "extended (129,024 tokens)" for Anthropic
- Warn if model doesn't support thinking but user requested it
- Adjust feedback based on provider capabilities

**Completion Updates:**
- List models from all providers in tab completion
- Include thinking level suffixes (/none, /low, /med, /high, /extended)
- Organize by provider

## Test Scenarios

**Parsing:**
- `/model claude-sonnet-4-5/med` -> model="claude-sonnet-4-5", thinking="med"
- `/model gpt-4o` -> model="gpt-4o", thinking=current
- `/model gemini-2.5/high` -> model="gemini-2.5", thinking="high"
- `/model invalid/` -> ERR_INVALID_ARG

**Provider Inference:**
- "claude-sonnet-4-5" -> "anthropic"
- "gpt-4o" -> "openai"
- "o1-preview" -> "openai"
- "gemini-2.5-flash" -> "google"
- "unknown-model" -> ERR_NOT_FOUND

**Capability Lookup:**
- "claude-sonnet-4" -> supports_thinking=true, budget=128000
- "gpt-4o" -> supports_thinking=false, budget=0
- "o1" -> supports_thinking=true, budget=0 (no budget control)
- "gemini-2.5" -> supports_thinking=true, budget=24576
- "gemini-1.5" -> supports_thinking=false, budget=0

**Agent State:**
- Before: provider="anthropic", model="claude-3-5-sonnet", thinking="none"
- Execute: `/model gpt-4o/high`
- After: provider="openai", model="gpt-4o", thinking="high"
- Database updated correctly

**User Feedback:**
- `/model claude-sonnet-4-5/med` displays:
  ```
  Switched to Anthropic claude-sonnet-4-5
    Thinking: medium (43,008 tokens)
  ```
- `/model gpt-4o/none` displays:
  ```
  Switched to OpenAI gpt-4o
    Thinking: disabled
  ```
- `/model gpt-4o/high` displays:
  ```
  Switched to OpenAI gpt-4o
    Thinking: high
    Warning: gpt-4o does not support extended thinking
  ```

**Completion:**
- Tab completion after `/model ` shows:
  ```
  claude-sonnet-4-5    gpt-4o    gemini-2.5-flash
  claude-opus-4        o1        gemini-2.0-flash
  ```
- Tab completion after `/model claude-sonnet-4-5/` shows:
  ```
  none    low    med    high    extended
  ```

## Postconditions

- [ ] `/model` parses MODEL/THINKING syntax correctly
- [ ] Provider inferred from model prefix
- [ ] Agent state updated with provider/model/thinking_level
- [ ] `ik_model_supports_thinking()` returns correct values
- [ ] `ik_model_get_thinking_budget()` returns correct budgets
- [ ] Warning displayed when requesting thinking on non-thinking model
- [ ] Completion shows models from all providers
- [ ] Completion shows thinking level suffixes
- [ ] Database persists updated values
- [ ] All tests compile without warnings
- [ ] All tests pass
- [ ] `make check` passes
