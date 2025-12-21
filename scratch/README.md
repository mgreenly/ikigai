# Release 07: Multi-Provider AI API Abstraction

## High-Level Goal

Abstract support for multiple AI providers and models, enabling ikigai to work seamlessly with any LLM API while maintaining a consistent internal interface.

Instead of being tightly coupled to a single provider (OpenAI), create a flexible architecture that supports:
- **Commercial APIs**: OpenAI, Anthropic, Google Gemini, xAI Grok, Meta Llama
- **Unified Gateways**: OpenRouter (500+ models from 60+ providers)
- **Future providers**: Easy to add new providers without major refactoring

## Scope

**In-scope providers (Core 3):**
- OpenAI
- Anthropic
- Google

**Implementation order:**
1. **Abstraction + OpenAI** - Build provider interface, refactor existing `src/openai/` to implement it, prove nothing breaks
2. **Anthropic** - Validate abstraction handles different API shape (system prompt, thinking budget, streaming format)
3. **Google** - Third provider confirms abstraction is solid (different thinking model)

**Deferred to future release:**
- xAI (Grok)
- Meta (Llama)
- OpenRouter

## Key Design Questions

### 1. Configuration

**How do users specify providers and models?**
- What does provider configuration look like in the config file?
- How do we store API keys for multiple providers?
- How do we set default providers/models?
- Can users configure fallback chains? (e.g., try Claude, fall back to GPT-5)
- Do we support per-user or per-session provider selection?

### 2. Model Assignment

**How do we route different agents to different models?**
- Can each agent specify its preferred model/provider?
- How do we handle agent-level vs system-level defaults?
- Do we support rules? (e.g., "use cheap model for summaries, expensive for code")
- How do we track which model/provider was used for each conversation turn?

### 3. Abstraction Layer Design

**What does the internal API representation look like?**
- What's the minimal common interface all providers must implement?
- How do we represent requests and responses internally?
- Do we normalize to a single format (e.g., OpenAI-compatible) or define our own?
- How do we handle provider-specific features that don't exist in others?

**What functions must each provider implement?**
- `chat_completion(messages, options)` - Basic chat
- `stream_completion(messages, options)` - Streaming responses
- `count_tokens(text, model)` - Token counting
- `list_models()` - Available models
- `validate_config()` - Check API keys and settings
- What else?

### 4. Token Management

**DECIDED: Response-based counting for rel-07**

Pre-send estimation is deferred. For this release:
- **Estimation interface:** Stub function returns 0 (hook for future `libikigai-tokenizer`)
- **Post-response counting:** Extract `usage` from API response (input/output/thinking tokens)
- **Accumulation:** Track running total per conversation
- **Display:** Show exact token count after each exchange (no `~` prefix needed without estimation)
- **Storage:** Token counts stored per-message in `data` JSONB (see Event History)

**Deferred to future release:**
- Local tokenizer library (`libikigai-tokenizer`) for pre-send estimates
- `~NUMBER` display during composition
- Context window warnings before hitting limits
- Cost tracking and aggregation

See `project/tokens/` for tokenizer library research.

### 5. Event History and Context

**How does provider/model selection interact with conversation history?**
- Must we store provider + model for every message in event history?
- Why: Token counts, costs, and context windows vary by model
- What happens when user switches providers mid-conversation?
  - Reset context? Warning? Allow continuation?
- How do we handle conversation export/import across providers?
- Do we need provider-specific metadata in events?

### 6. Extended Thinking Abstraction

**How do we create a unified interface for reasoning/thinking modes?**

Different providers have different approaches:
- **Anthropic**: `thinking.type` + `thinking.budget_tokens` (enabled/disabled with token budget)
- **OpenAI**: Reasoning models (o3, o3-mini) with `reasoning.effort` (low/medium/high) in Responses API
- **Google**: `thinkingLevel` (LOW/HIGH) or `thinkingBudget` (token count or -1 for dynamic)
- **xAI**: No dedicated thinking mode yet
- **Meta**: No dedicated thinking mode (future reasoning model planned)

**Questions:**
- Do we map all to a common parameter? (e.g., `thinking: {enabled: bool, effort: low|medium|high}`)
- How do we expose provider-specific options when needed?
- How do we handle providers that don't support thinking?
- Do we expose thinking token costs separately?

**DECIDED: `/model NAME/THINKING` Syntax**

Users can switch models and thinking levels with a single command:
```
/model claude-sonnet-4.5/med
/model o3-mini/high
/model gemini-2.5-pro/low
/model gpt-4o/none
```

**Abstract Thinking Levels:**
- `none` - Thinking disabled (0 budget or minimum possible)
- `low` - 1/3 of maximum budget
- `med` - 2/3 of maximum budget
- `high` - 3/3 of maximum budget

**Mapping Strategy:**

The system translates abstract levels to provider-specific parameters using model metadata:

```
Calculation:
  none → min_budget (or 0 if supported)
  low  → min_budget + (1/3 × (max_budget - min_budget))
  med  → min_budget + (2/3 × (max_budget - min_budget))
  high → max_budget
```

**Examples:**

Anthropic (assuming max_budget = 30,000):
```
none → type: "disabled"
low  → type: "enabled", budget_tokens: 10,000
med  → type: "enabled", budget_tokens: 20,000
high → type: "enabled", budget_tokens: 30,000
```

Google gemini-2.5-flash (max_budget = 24,576):
```
none → thinkingBudget: 0
low  → thinkingBudget: 8,192
med  → thinkingBudget: 16,384
high → thinkingBudget: 24,576
```

Google gemini-2.5-pro (max_budget = 32,768, min_budget = 128):
```
none → thinkingBudget: 128
low  → thinkingBudget: 10,922
med  → thinkingBudget: 21,845
high → thinkingBudget: 32,768
```

Google gemini-3-pro (thinkingLevel only):
```
none → thinkingLevel: "LOW"
low  → thinkingLevel: "LOW"
med  → thinkingLevel: "HIGH"
high → thinkingLevel: "HIGH"
```

OpenAI o3-mini:
```
none → (no reasoning config)
low  → reasoning.effort: "low"
med  → reasoning.effort: "medium"
high → reasoning.effort: "high"
```

**User Feedback:**

After switching, the system displays the concrete mapping:
```
> /model claude-sonnet-4.5/med

✓ Switched to Anthropic claude-sonnet-4.5-20250929
  Thinking: enabled (20,000 token budget - medium)
```

```
> /model gemini-2.5-flash/low

✓ Switched to Google gemini-2.5-flash
  Thinking: 8,192 token budget (low)
```

```
> /model gpt-4o/high

✓ Switched to OpenAI gpt-4o
  ⚠ Thinking not supported by this model (ignored)
```

**Provider Module Requirements:**

Each provider implementation must provide model metadata to enable thinking level mapping:

```typescript
interface ModelMetadata {
  model_id: string;
  display_name: string;
  context_window: number;

  thinking: {
    supported: boolean;

    // For budget-based providers (Anthropic, Google 2.5)
    budget?: {
      min: number;      // Minimum token budget
      max: number;      // Maximum token budget
      type: "tokens";
    }

    // For level-based providers (Google 3, OpenAI)
    levels?: {
      available: string[];  // e.g., ["low", "medium", "high"]
      type: "effort" | "level";
    }
  };

  // Other capabilities
  supports_tools: boolean;
  supports_vision: boolean;
  supports_streaming: boolean;
}
```

**Examples:**

```typescript
// Anthropic
{
  model_id: "claude-sonnet-4.5-20250929",
  display_name: "Claude Sonnet 4.5",
  context_window: 200000,
  thinking: {
    supported: true,
    budget: {
      min: 1024,
      max: 50000,  // To be determined through research/testing
      type: "tokens"
    }
  }
}

// Google Gemini 2.5 Flash
{
  model_id: "gemini-2.5-flash",
  display_name: "Gemini 2.5 Flash",
  context_window: 1000000,
  thinking: {
    supported: true,
    budget: {
      min: 0,
      max: 24576,
      type: "tokens"
    }
  }
}

// Google Gemini 3 Pro
{
  model_id: "gemini-3-pro",
  display_name: "Gemini 3 Pro",
  context_window: 1000000,
  thinking: {
    supported: true,
    levels: {
      available: ["LOW", "HIGH"],
      type: "level"
    }
  }
}

// OpenAI o3-mini
{
  model_id: "o3-mini",
  display_name: "o3-mini",
  context_window: 128000,
  thinking: {
    supported: true,
    levels: {
      available: ["low", "medium", "high"],
      type: "effort"
    }
  }
}

// OpenAI GPT-4o (no thinking)
{
  model_id: "gpt-4o",
  display_name: "GPT-4o",
  context_window: 128000,
  thinking: {
    supported: false
  }
}
```

**Fork Command Integration:**

The `/fork` command must support model/thinking specification to enable creating child agents with different providers.

**Syntax:**
```bash
/fork                                    # Inherits parent's model/thinking
/fork "prompt"                          # Inherits model/thinking + assigns task
/fork --model NAME/THINKING             # Override model/thinking
/fork --model NAME/THINKING "prompt"   # Override model/thinking + assigns task
```

**Default Behavior (Inheritance):**

When forking without `--model`, the child inherits the parent's provider, model, and thinking level:

```
────────────────── agent [abc123...] ───────────────
Provider: Anthropic (claude-sonnet-4.5/med)

> /fork "Research OAuth 2.0"

────────────────── agent [xyz789...] ───────────────
Provider: Anthropic (claude-sonnet-4.5/med)  ← Inherited
Parent: abc123...
Forked from: abc123... at message 8

> Research OAuth 2.0
[Agent starts working with same model as parent]
```

**Override Behavior:**

Users can explicitly specify a different model/thinking level for the child:

```
────────────────── agent [abc123...] ───────────────
Provider: Anthropic (claude-sonnet-4.5/med)

> /fork --model o3-mini/high "Solve this complex algorithm problem"

────────────────── agent [xyz789...] ───────────────
Provider: OpenAI (o3-mini)
Reasoning: high effort
Parent: abc123... (Anthropic claude-sonnet-4.5/med)

> Solve this complex algorithm problem
[Agent starts with o3-mini's high-effort reasoning]
```

**Database Schema Extension:**

The `agents` table must store model configuration:

```sql
ALTER TABLE agents ADD COLUMN provider TEXT;         -- 'anthropic', 'openai', 'google'
ALTER TABLE agents ADD COLUMN model TEXT;            -- 'claude-sonnet-4.5-20250929', 'o3-mini', etc.
ALTER TABLE agents ADD COLUMN thinking_level TEXT;   -- 'none', 'low', 'med', 'high'
```

Example rows:
```
uuid        parent_uuid  provider   model                      thinking_level
abc123...   NULL         anthropic  claude-sonnet-4.5-20250929 med
xyz789...   abc123...    openai     o3-mini                    high
```

**Context Inheritance Across Providers:**

When a child uses a different provider than its parent:
1. Parent's conversation history is translated to provider-neutral format
2. Child receives history in its provider's expected format
3. Both agents maintain their own provider-specific configurations
4. Token counts and costs are tracked separately per agent

**Use Cases:**

- **Cheap exploration, expensive execution:**
  ```bash
  # Parent uses fast/cheap model for discussion
  > /fork --model o3/high "Now implement the solution with deep reasoning"
  ```

- **Specialized models for subtasks:**
  ```bash
  # Parent doing general work with Claude
  > /fork --model gemini-2.5-pro/high "Use Google's grounding for this research task"
  ```

- **Testing across providers:**
  ```bash
  # Compare responses from different models
  > /fork --model claude-sonnet-4.5/high "Solve this problem"
  > /fork --model o3-mini/high "Solve this problem"
  > /fork --model gemini-3-pro/high "Solve this problem"
  ```

**Research Tasks:**

The following limits must be researched and documented for each provider:

1. **Anthropic Extended Thinking Limits**
   - Verify minimum: 1,024 tokens (documented)
   - Determine maximum budget per model:
     - claude-sonnet-4.5-20250929: ? tokens
     - claude-opus-4.5-20251101: ? tokens
     - claude-3-7-sonnet-20250219: ? tokens
     - Other models supporting extended thinking
   - Method: Documentation review + API testing

2. **Google Gemini Thinking Limits**
   - Already documented in specs:
     - gemini-2.5-pro: 128 - 32,768 tokens
     - gemini-2.5-flash: 0 - 24,576 tokens
     - gemini-2.5-flash-lite: 512 - 24,576 tokens
     - gemini-3-pro: LOW/HIGH levels only
   - Verify if other Gemini models have thinking support

3. **OpenAI Reasoning Models**
   - Document which models support reasoning:
     - o3: supports reasoning.effort?
     - o3-mini: supports reasoning.effort (confirmed)
     - o1: supports reasoning?
   - Verify if there are token budget limits for reasoning

4. **Provider-Specific Thinking Costs**
   - Research pricing differences for thinking tokens:
     - Anthropic: Same as output tokens or different?
     - Google: Same as output tokens or different?
     - OpenAI: Reasoning tokens priced separately?

### 7. System Prompts Abstraction

**DECIDED: Separate field with per-provider translation**

**Internal Representation:**
- System prompt is a **separate field** (not in messages array)
- Array of text blocks (supports future caching breakpoints)
- One logical system prompt per request (multiple blocks allowed)
- Empty/null system prompt is valid

**Provider Translation:**

| Provider | Translation Strategy |
|----------|---------------------|
| Anthropic | Direct mapping to `system` parameter (array format) |
| OpenAI | Concatenate blocks into single `role: "system"` message at position 0 |
| Google | Map to `systemInstruction.parts[]` array |

**Deferred:**
- Caching hints (`cache_control`) - Skip for rel-07

### 8. Tool/Function Calling Abstraction

**DECIDED: Superset internal format with per-provider translation**

**Internal Tool Definition:**
```c
struct ik_tool_def {
    char *name;
    char *description;
    json_t *parameters;      // JSON Schema
    bool strict;             // OpenAI strict mode (default: true)
};
```

**Internal Tool Choice (superset of all providers):**
```c
typedef enum {
    IK_TOOL_CHOICE_AUTO,      // Model decides
    IK_TOOL_CHOICE_NONE,      // No tools allowed
    IK_TOOL_CHOICE_REQUIRED,  // Must use at least one tool
    IK_TOOL_CHOICE_SPECIFIC   // Must use named tool
} ik_tool_choice_t;

struct ik_tool_config {
    ik_tool_choice_t choice;
    char *specific_tool;      // Only used when choice == SPECIFIC
    bool parallel_allowed;    // Default: true
};
```

**Internal Tool Call (response):**
- `id` - Generated by provider (we generate UUID for Google)
- `name` - Function name
- `arguments` - Parsed JSON object (not string)

**Internal Tool Result (for next request):**
- `tool_call_id` - Matches the id from tool call
- `content` - Result as string (JSON or plain text)
- `is_error` - Whether this is an error result

**Provider Translation:**

| Provider | Definition | Response | Result Submission |
|----------|-----------|----------|-------------------|
| Anthropic | `parameters` → `input_schema` | Extract from `tool_use` blocks | `tool_result` block, role `user` |
| OpenAI | Wrap in `{"type": "function", "function": {...}}` | Parse `arguments` from JSON string | `role: "tool"` message |
| Google | Wrap in `{"functionDeclarations": [...]}` | Extract from `functionCall`, generate ID | `role: "function"` + `functionResponse` |

**rel-07 Implementation:**
- Internal format supports all modes (superset)
- Adapters only implement `AUTO` for tool choice
- Other modes return "not yet implemented" error
- `parallel_allowed` passed to OpenAI, ignored for others
- `strict` passed to OpenAI, ignored for others
- No schema validation (pass through to provider)

### 9. Streaming Abstraction

**DECIDED: Normalized event types with immediate pass-through**

**Internal Stream Event Types (superset):**
```c
typedef enum {
    IK_STREAM_START,           // Stream beginning, includes model info
    IK_STREAM_TEXT_DELTA,      // Text content fragment
    IK_STREAM_THINKING_DELTA,  // Thinking content fragment
    IK_STREAM_TOOL_CALL_START, // Tool call beginning (id, name)
    IK_STREAM_TOOL_CALL_DELTA, // Tool call arguments fragment
    IK_STREAM_TOOL_CALL_DONE,  // Tool call complete
    IK_STREAM_DONE,            // Stream complete (includes usage, finish_reason)
    IK_STREAM_ERROR            // Error occurred
} ik_stream_event_t;

struct ik_stream_event {
    ik_stream_event_t type;
    int block_index;           // For parallel content blocks
    union {
        struct { char *model; } start;
        struct { char *text; } text_delta;
        struct { char *text; } thinking_delta;
        struct { char *id; char *name; } tool_call_start;
        struct { char *id; char *json_fragment; } tool_call_delta;
        struct { char *id; } tool_call_done;
        struct { ik_finish_reason_t reason; ik_usage_t usage; } done;
        struct { ik_error_t error; } error;
    } data;
};
```

**Key Decisions:**
1. **Normalize to common events** - Don't expose raw provider formats
2. **Pass through immediately** - No buffering (except to parse complete JSON chunks)
3. **Block indexing** - Track which content block each delta belongs to
4. **Tool call lifecycle** - `START` → `DELTA`* → `DONE` pattern
5. **Usage in DONE** - Single completion event with all final metadata

**Provider Translation:**

| Event | OpenAI | Anthropic | Google |
|-------|--------|-----------|--------|
| `START` | First chunk | `message_start` | First chunk |
| `TEXT_DELTA` | `delta.content` | `text_delta` | `parts[0].text` |
| `THINKING_DELTA` | `reasoning_summary_text.delta` | `thinking_delta` | `thought: true` part |
| `TOOL_CALL_START` | First `tool_calls` chunk | `content_block_start` | N/A (emit on complete) |
| `TOOL_CALL_DELTA` | `tool_calls[].function.arguments` | `input_json_delta` | N/A |
| `TOOL_CALL_DONE` | Empty delta with complete args | `content_block_stop` | Emit with START |
| `DONE` | `finish_reason` + `[DONE]` | `message_stop` | `finishReason: STOP` |
| `ERROR` | Error event | `error` event | Error in response |

**Note:** Google doesn't stream tool calls incrementally - we emit `TOOL_CALL_START` + `TOOL_CALL_DONE` together when the complete call arrives.

### 10. Error Handling

**DECIDED: Normalized error categories with retry hints**

**Internal Error Categories (superset):**
```c
typedef enum {
    IK_ERR_AUTH,            // Invalid/missing API key (401, 403)
    IK_ERR_RATE_LIMIT,      // Too many requests (429)
    IK_ERR_INVALID_REQUEST, // Malformed request (400)
    IK_ERR_CONTEXT_LENGTH,  // Input too long (400 with specific code)
    IK_ERR_CONTENT_FILTER,  // Content policy violation
    IK_ERR_BILLING,         // Quota/payment issue (402)
    IK_ERR_NOT_FOUND,       // Resource not found (404)
    IK_ERR_SERVER,          // Internal server error (500)
    IK_ERR_OVERLOADED,      // Service overloaded (503, 529)
    IK_ERR_TIMEOUT,         // Request timeout (502, 504)
    IK_ERR_UNKNOWN          // Unmapped error
} ik_error_category_t;

struct ik_error {
    ik_error_category_t category;
    int http_status;
    char *message;           // Human-readable
    char *provider_code;     // Original provider error code/type
    int retry_after_ms;      // -1 if not retryable, 0 if immediate, >0 for delay
    bool retryable;          // Hint for caller
};
```

**Retry Strategy:**

| Category | Retryable | Strategy |
|----------|-----------|----------|
| `RATE_LIMIT` | Yes | Use `retry_after_ms` or exponential backoff |
| `OVERLOADED` | Yes | Exponential backoff (start 1s) |
| `TIMEOUT` | Yes | Immediate retry (max 2 attempts) |
| `SERVER` | Yes | Exponential backoff (max 3 attempts) |
| `AUTH` | No | User must fix credentials |
| `BILLING` | No | User must fix account |
| `INVALID_REQUEST` | No | Caller must fix request |
| `CONTEXT_LENGTH` | No | Caller must reduce input |
| `CONTENT_FILTER` | No | Content issue |
| `NOT_FOUND` | No | Resource doesn't exist |

**Provider Error Mapping:**

| Category | OpenAI | Anthropic | Google |
|----------|--------|-----------|--------|
| `AUTH` | 401 `authentication_error` | 401, 403 | 403 `PERMISSION_DENIED` |
| `RATE_LIMIT` | 429 `rate_limit_error` | 429 | 429 `RESOURCE_EXHAUSTED` |
| `INVALID_REQUEST` | 400 `invalid_request_error` | 400 | 400 `INVALID_ARGUMENT` |
| `CONTEXT_LENGTH` | 400 + `context_length_exceeded` | 400 + message match | 400 + message match |
| `CONTENT_FILTER` | `content_filter` finish | `refusal` stop | `SAFETY` finish |
| `BILLING` | `insufficient_quota` | 402 | `RESOURCE_EXHAUSTED` + quota |
| `NOT_FOUND` | 404 | 404 | 404 |
| `SERVER` | 500 | 500 | 500 |
| `OVERLOADED` | 503 | 529 | 503 |
| `TIMEOUT` | N/A | 502 | 504 |

**rel-07 Implementation:**
- Map all provider errors to internal categories
- Populate `retry_after_ms` from headers when available
- Set `retryable` hint based on category
- No automatic retry in abstraction layer (caller decides)

### 11. RAG and Retrieval

**Do any providers offer native RAG support?**
- **OpenAI**: File search in Assistants API, not in Chat Completions
- **Anthropic**: No native RAG, partner with Voyage AI for embeddings
- **Google**: Grounding with Google Search, retrieval metadata
- **xAI**: Unknown
- **Meta**: No native RAG in API
- **OpenRouter**: Passes through provider capabilities

**Questions:**
- Do we build our own RAG layer on top of all providers?
- How do we integrate embeddings (different for each provider)?
- Do we use a unified vector store approach?
- Is this rel-07 or future work?

### 12. Model Selection and Discovery

**How do users discover and select models?**
- Do we maintain a registry of supported models per provider?
- How do we update when providers release new models?
- Do we query provider APIs for available models at runtime?
- How do we expose model capabilities (context window, multimodal, etc.)?
- Do we support model aliases? (e.g., "fast", "smart", "cheap")

### 13. Cost and Billing

**How do we track and report costs?**
- Each provider has different pricing (input/output tokens, per-request, etc.)
- Do we integrate pricing data?
- How do we estimate costs before sending requests?
- Do we warn users about expensive operations?
- How do we report total spend across all providers?

### 14. Multimodal Support

**How do we handle images, audio, video?**
- Different providers support different modalities
- Different input formats (URL vs base64)
- How do we abstract this?
- Do we validate inputs before sending to provider?

### 15. OpenRouter Integration

**Should OpenRouter be a first-class provider or special case?**
- OpenRouter provides access to 500+ models including commercial ones
- It has unique features: fallbacks, routing variants (`:nitro`, `:floor`), BYOK
- Do we treat it as one provider or expose underlying provider models?
- How do we handle OpenRouter-specific features?

### 16. Provider-Specific Features

**How do we expose features that don't exist across all providers?**
- Anthropic's prompt caching
- Google's grounding and search integration
- OpenAI's Responses API state management
- OpenRouter's routing preferences

**Options:**
- Ignore them (lowest common denominator)
- Expose through provider-specific config
- Abstract where possible, pass through where not


---

## Abstraction Layer Design Decisions

**Principle:** Internal format is a true superset of features we intend to support. Store everything in the most granular form. Concrete provider adapters combine/reformat as needed for each provider's API.

**Direction:**
- **Outbound (request):** Our rich format → adapter strips/combines/reformats → provider's format
- **Inbound (response):** Provider's format → adapter extracts/labels → our rich format (as much as possible)

### Request-Side Features

| # | Feature | Decision |
|---|---------|----------|
| 1 | **Messages** | Array of typed content blocks (TEXT, IMAGE, TOOL_CALL, TOOL_RESULT, THINKING). Roles: USER, ASSISTANT, TOOL. Metadata per message: model, timestamp, token_count. |
| 2 | **System Prompt** | Separate field (not in messages array). Array of content blocks (supports caching breakpoints). Adapters merge into messages for providers that require it. |
| 3 | **Model Selection** | Pass through exactly as provided. `provider` + `model` strings. No alias resolution, no internal registry (future work). |
| 4 | **Thinking Config** | Abstract levels: `none`, `low`, `med`, `high`. Adapters map to provider-specific mechanisms (budget, effort, level). Keep existing README decisions. |
| 5 | **Tool Definitions** | `name`, `description`, `parameters` (JSON Schema). Array of definitions on request. |
| 6 | **Tool Choice** | `auto` only for this version. Future work: none, required, specific tool forcing. |
| 7 | **Response Format** | Skip for this version. Rely on prompt engineering or tool-with-schema pattern. |
| 8 | **Temperature/Sampling** | Skip for this version. Use provider/model defaults. |
| 9 | **Max Output Tokens** | Include with sensible default (e.g., 4096). Required for Anthropic. |
| 10 | **Stop Sequences** | Skip for this version. Tool calling covers primary use cases. |
| 11 | **Multimodal Content** | Skip for this version (text only). Future: base64 + media_type internal format. |
| 12 | **Caching Hints** | Skip for this version. Future: Anthropic-style cache_control markers. |
| 13 | **Grounding/Search** | Skip for this version. Can be approximated with custom search tool. |

### Response-Side Features

| # | Feature | Decision |
|---|---------|----------|
| 14 | **Text Content** | `CONTENT_TEXT` blocks. Multiple blocks possible. |
| 15 | **Thinking Content** | `CONTENT_THINKING` blocks. Per-provider research needed for exposure/structure. |
| 16 | **Tool Calls** | `CONTENT_TOOL_CALL` blocks with `id`, `name`, `arguments` (JSON). |
| 17 | **Finish Reason** | Enum: `STOP`, `LENGTH`, `TOOL_USE`, `CONTENT_FILTER`, `ERROR`, `UNKNOWN`. Adapters map provider values. |
| 18 | **Token Usage** | Fields: `input_tokens`, `output_tokens`, `thinking_tokens`, `cached_tokens`, `total_tokens`. Fields may be 0/-1 if provider doesn't report. |
| 19 | **Model Identification** | String passthrough from provider response. No transformation. |
| 20 | **Provider Metadata** | Opaque JSON passthrough for anything not explicitly modeled. Useful for debugging/extensibility. |

### Operational Features

| # | Feature | Decision |
|---|---------|----------|
| 21 | **Streaming** | Normalized event types: `START`, `TEXT_DELTA`, `THINKING_DELTA`, `TOOL_CALL_START`, `TOOL_CALL_DELTA`, `DONE`, `ERROR`. Adapters translate provider formats. |
| 22 | **Error Handling** | Categories: `AUTH`, `RATE_LIMIT`, `INVALID_REQUEST`, `CONTEXT_LENGTH`, `CONTENT_FILTER`, `SERVER`, `TIMEOUT`, `UNKNOWN`. Include `http_status`, `message`, `provider_code`, `retry_after_ms`. |
| 23 | **Token Counting** | **Pre-send:** Stub interface returns 0 (future: `libikigai-tokenizer` library). **Post-response:** Extract and accumulate `usage` from API response (input/output/thinking tokens). Display running total. Response usage is ground truth. |
| 24 | **Capability Query** | Skip for this version. Pass through and handle errors. Future: registry or API discovery. |

### Per-Provider Research Checklist

When implementing each provider adapter, investigate these information loss categories:

**Response Content:**
1. **Thinking exposure** - Is reasoning/thinking content returned or hidden? How is it labeled/structured?
2. **Content block boundaries** - Are text/tool_calls/thinking clearly delimited or merged?
3. **Block ordering** - Is order guaranteed? (thinking before text, etc.)

**Token Accounting:**
4. **Token breakdown** - Separate input/output/thinking counts? Or just total? Or nothing?
5. **Cached token reporting** - Does provider report cache hits and tokens saved?

**Model Information:**
6. **Actual model used** - Does response identify exact model/version, or just echo request?

**Completion Status:**
7. **Finish reason granularity** - Can we distinguish: natural stop, max tokens, tool use, content filter, error?

**Streaming:**
8. **Chunk granularity** - Token-by-token? Word? Sentence? Paragraph?
9. **Mid-stream metadata** - Usage stats during stream or only at end?
10. **Thinking in stream** - Is thinking streamed, delivered first, or omitted?

**Operational:**
11. **Request ID** - Is there a provider request ID for debugging/support?
12. **Rate limit headers** - Retry-after, remaining quota, etc.?
13. **Error detail** - Structured error codes or just messages?

**System Prompt:**
14. **Cache control support** - Does provider support caching hints?
15. **Multiple blocks** - Can provider handle segmented system prompt, or must we concatenate?
16. **Positioning constraints** - Must system prompt be first, or flexible?

**Tool Calling:**
17. **ID format** - Provider-generated or our responsibility?
18. **Parallel calls** - Can multiple tool calls appear in one response?
19. **Interleaving** - Can tool calls be interleaved with text?

**Errors:**
20. **Error type mappings** - Exact mapping of provider error codes to our categories
21. **Context length detection** - How to identify context overflow from error response?


---

## Configuration Design Decisions

**File Structure:**
- `config.json` - Settings, defaults, preferences (shareable for debugging)
- `credentials.json` - API keys only (private, stored alongside config.json)

**Rationale:** Splitting secrets from config allows users to share config.json when debugging issues without exposing credentials.

### Provider Naming

Provider implementations are named after the **company**, not the product:

| Company | Provider Name | Products |
|---------|---------------|----------|
| Anthropic | `anthropic` | Claude |
| OpenAI | `openai` | GPT, o-series |
| Google | `google` | Gemini |
| xAI | `xai` | Grok |
| Meta | `meta` | Llama |

Config sections, module names, and credentials keys all use these names consistently.

### Credential Precedence

```
Environment Variable → credentials.json
```

Environment variables use each provider's standard/recommended name (see research checklist).

### config.json Structure

```json
{
  "providers": {
    "anthropic": { "default_model": "claude-sonnet-4" },
    "openai": { "default_model": "gpt-4o" },
    "google": { "default_model": "gemini-2.5-pro" },
    "xai": { "default_model": "grok-2" },
    "meta": { "default_model": "llama-4-maverick" }
  },
  "default_provider": "anthropic",
  "max_output_tokens": 4096,
  "listen_address": "127.0.0.1",
  "listen_port": 1984,
  "max_tool_turns": 50,
  "max_output_size": 1048576,
  "history_size": 10000
}
```

### credentials.json Structure

```json
{
  "anthropic": { "api_key": "sk-ant-..." },
  "openai": { "api_key": "sk-..." },
  "google": { "api_key": "..." },
  "xai": { "api_key": "..." },
  "meta": { "api_key": "..." }
}
```

File permissions should be `600` (owner read/write only).

### Behavior Decisions

| Aspect | Decision |
|--------|----------|
| **Missing credentials** | Lazy error on first use (not startup) |
| **Fallback chains** | Not supported (skip for this version) |
| **Default thinking level** | Per-provider default, not configurable in file |
| **Thinking override** | Set interactively via `/model MODEL/THINKING` |
| **OpenRouter** | Deferred to separate discussion |

### Per-Provider Research Checklist

When implementing each provider, research:

1. **Standard env var name** - What does the provider recommend? (e.g., `OPENAI_API_KEY`, `ANTHROPIC_API_KEY`)
2. **API key format** - Prefix pattern for validation hints (e.g., `sk-ant-` for Anthropic)
3. **Default model** - Sensible default for new users

---

## Event History Design Decisions

**Principle:** Store minimum required metadata per message. Provider/model info on every assistant message enables cost tracking, replay, and debugging without session-level redundancy.

### Existing Schema

```sql
messages (
  id BIGSERIAL PRIMARY KEY,
  session_id BIGINT REFERENCES sessions(id),
  kind TEXT,        -- 'clear', 'system', 'user', 'assistant', 'mark', 'rewind'
  content TEXT,
  data JSONB,       -- Event-specific metadata
  created_at TIMESTAMPTZ
)
```

### Message Storage by Kind

**User messages:**
- `content` = User's input text
- `data` = NULL (no metadata needed)

**System messages:**
- `content` = System prompt text
- `data` = NULL (no metadata needed)

**Assistant messages:**
- `content` = Text response only (no thinking)
- `data` = Provider/model metadata + thinking content:

```json
{
  "provider": "anthropic",
  "model": "claude-sonnet-4-20250514",
  "thinking_level": "med",
  "thinking": "Let me analyze this problem...",
  "input_tokens": 1234,
  "output_tokens": 567,
  "thinking_tokens": 890
}
```

### Field Descriptions

| Field | Type | Description |
|-------|------|-------------|
| `provider` | string | Provider name (`anthropic`, `openai`, etc.) |
| `model` | string | Exact model identifier from response |
| `thinking_level` | string | Abstract level used (`none`, `low`, `med`, `high`) |
| `thinking` | string | Thinking/reasoning content (if provider exposed it) |
| `input_tokens` | int | Tokens in request (from response usage) |
| `output_tokens` | int | Tokens in response (from response usage) |
| `thinking_tokens` | int | Thinking tokens (if reported separately, else 0) |

### Behavior Decisions

| Aspect | Decision |
|--------|----------|
| **Provider switching** | Allowed freely mid-conversation |
| **History to new provider** | Sent as-is (no transformation) |
| **Session-level provider** | Not stored (redundant with per-message) |
| **Thinking storage** | Separate from content in `data.thinking` |

### Rationale

- **Thinking separate from content:** Allows UI to show/hide thinking, replay without thinking if desired
- **Per-message provider/model:** Enables accurate cost tracking when switching models mid-conversation
- **No session-level storage:** Can derive from first assistant message if needed; avoids sync issues

---

## Model Assignment Design Decisions

**Principle:** Each agent has its own provider/model/thinking settings. Provider is inferred from model name for brevity.

### Command Syntax

```
/model MODEL/THINKING
```

Examples:
```
/model claude-sonnet-4/high
/model o3-mini/med
/model gemini-2.5-pro/low
/model gpt-4o/none
```

Provider is inferred from model name prefix. No explicit provider in command.

### Per-Agent Fields

```c
typedef struct ik_agent_ctx {
    // ... existing fields ...
    
    char *provider;                    // "anthropic", "openai", etc. (inferred)
    char *model;                       // "claude-sonnet-4-20250514"
    ik_thinking_level_t thinking_level; // NONE, LOW, MED, HIGH
} ik_agent_ctx_t;
```

### Resolution Order

```
Agent's own settings → Parent agent (if forked) → config.json defaults
```

| Scenario | Source |
|----------|--------|
| New root agent | `config.json`: default_provider + provider's default_model |
| `/model MODEL/THINKING` | Updates current agent directly |
| `/fork` (no --model) | Inherits parent's provider/model/thinking |
| `/fork --model MODEL/THINKING` | Uses specified model/thinking |

### Provider Inference

Model name prefixes map to providers:

| Prefix | Provider |
|--------|----------|
| `claude-*` | `anthropic` |
| `gpt-*`, `o1-*`, `o3-*` | `openai` |
| `gemini-*` | `google` |
| `grok-*` | `xai` |
| `llama-*` | `meta` |

### Behavior Decisions

| Aspect | Decision |
|--------|----------|
| **Routing rules** | Skip for this version (no automatic model selection) |
| **Model registry** | Skip for this version (pass through, infer provider) |
| **Per-turn tracking** | Already in Event History (per-message `data` JSONB) |

### Per-Provider Research Checklist

1. **Model name patterns** - What prefixes/patterns identify this provider's models?
2. **Default model** - Sensible default for new users
3. **Model list** - Available models for validation/completion (future)
## Research Artifacts

All provider API specifications documented in `rel-07/specs/`:
- `anthropic.md` - Claude API (extended thinking, tool use)
- `google.md` - Gemini API (thinking levels, grounding)
- `xai.md` - Grok API (flexible message ordering)
- `openai.md` - OpenAI Chat Completions + Responses API
- `meta.md` - Llama API (open weights, MoE models)
- `openrouter.md` - Unified gateway to 500+ models

Token counting research in `project/tokens/`:
- Tokenizer library design
- Provider-specific tokenization details
- Usage strategy (local estimates + exact API counts)

## Next Steps

1. **Define the abstraction interface** - What's the contract all providers implement?
2. **Design the configuration schema** - How users specify providers and models
3. **Prototype a provider adapter** - Implement one provider (OpenAI?) as reference
4. **Design event history extensions** - What metadata do we store per message?
5. **Token counting strategy** - Local vs API, caching, estimation
6. **Create user stories** - How will users interact with multi-provider support?
7. **Plan the implementation** - Break into tasks, identify dependencies


---

## Deferred Features (Not This Release)

The following features are explicitly out of scope for rel-07:

### Additional Providers

- **xAI (Grok)** - OpenAI-compatible API, straightforward to add after Core 3
- **Meta (Llama)** - Requires self-hosting or third-party inference, different deployment model

### OpenRouter Integration (Q15)

OpenRouter provides unified access to 500+ models from 60+ providers with unique features (fallbacks, routing variants, BYOK). Deferred because:
- Adds complexity to provider abstraction
- Unique routing/fallback semantics need separate design
- Can be added as another provider later

### Provider-Specific Features (Q16)

Advanced provider-specific capabilities are deferred:
- Anthropic prompt caching (`cache_control`)
- Google grounding/search integration
- OpenAI Responses API state management

These can be exposed through provider-specific config or `data` passthrough in future releases.

### RAG and Retrieval (Q11)

Retrieval Augmented Generation for permanent memory and document grounding. Deferred because:
- Substantial feature requiring separate design (embeddings, vector store, retrieval logic)
- Not dependent on multi-provider abstraction (additive feature)
- Providers lack consistent native RAG support
- Deserves its own release

### Multimodal Support (Q14)

Image, audio, and video input/output. Deferred because:
- Adds complexity to message format and provider adapters
- Not all providers support all modalities
- Text-only is sufficient for initial multi-provider release


### Other Deferred Items

| Feature | Reason |
|---------|--------|
| Fallback chains | Adds routing complexity |
| Model capability registry | Can infer/pass-through for now |
| Response format constraints | Prompt engineering sufficient |
| Cost/billing tracking | Have token counts, pricing integration later |

## Open Questions

- Do we support multiple providers active simultaneously or one at a time?
- Can a single conversation span multiple providers?
- How do we handle model-specific prompt engineering differences?
- Do we need a provider compatibility matrix?
- How do we test against multiple providers (cost, API keys)?
- Do we support offline mode for open source models?
- How do we handle provider deprecations and API version updates?

---

**This document is a living plan. It will evolve as we research, prototype, and make design decisions.**
