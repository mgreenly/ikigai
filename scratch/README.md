# Release 07: Multi-Provider AI API Abstraction

## High-Level Goal

Abstract support for multiple AI providers and models, enabling ikigai to work seamlessly with any LLM API while maintaining a consistent internal interface. Instead of being tightly coupled to a single provider (OpenAI), create a flexible architecture that supports commercial APIs (OpenAI, Anthropic, Google) with easy extensibility for future providers.

## Scope

**Core 3 Providers (In-Scope):**
- OpenAI (GPT-4o, o3-mini, o3)
- Anthropic (Claude Sonnet 4.5, Opus 4.5, 3.7)
- Google (Gemini 2.5 Pro, Flash, Gemini 3 Pro)

**Implementation Order:**
1. **Abstraction + OpenAI** - Build provider interface, refactor existing `src/openai/` to implement it, prove nothing breaks
2. **Anthropic** - Validate abstraction handles different API shape (system prompt, thinking budget, streaming format)
3. **Google** - Third provider confirms abstraction is solid (different thinking model)

**Deferred to Future Releases:**
- xAI (Grok)
- Meta (Llama)
- OpenRouter (unified gateway to 500+ models)

## User-Facing Features

### Model Selection

Switch models and thinking levels with a single command:

```
/model claude-sonnet-4-5/med
/model o3-mini/high
/model gemini-2.5-pro/low
/model gpt-4o/none
```

**Thinking Levels:**
- `none` - Thinking disabled or minimum budget
- `low` - 1/3 of maximum thinking budget
- `med` - 2/3 of maximum thinking budget
- `high` - Full thinking budget

The system automatically translates these abstract levels to provider-specific parameters (token budgets, effort levels, etc.) and displays the concrete mapping:

```
> /model claude-sonnet-4-5/med

✓ Switched to Anthropic claude-sonnet-4-5-20250929
  Thinking: enabled (43,008 token budget - medium)
```

### Fork with Model Override

Create child agents with different providers/models:

```
/fork                                  # Inherits parent's model/thinking
/fork "prompt"                         # Inherits model/thinking + assigns task
/fork --model NAME/THINKING            # Override model/thinking
/fork --model NAME/THINKING "prompt"   # Override model/thinking + assigns task
```

**Use Cases:**
- **Cheap exploration, expensive execution:**
  ```
  # Parent uses fast/cheap model for discussion
  > /fork --model o3/high "Now implement the solution with deep reasoning"
  ```

- **Specialized models for subtasks:**
  ```
  # Parent doing general work with Claude
  > /fork --model gemini-2.5-pro/high "Use Google for this research task"
  ```

- **Testing across providers:**
  ```
  > /fork --model claude-sonnet-4-5/high "Solve this problem"
  > /fork --model o3-mini/high "Solve this problem"
  > /fork --model gemini-3-pro/high "Solve this problem"
  ```

### Configuration

**Two Files:**
- `config.json` - Settings, defaults, preferences (shareable for debugging)
- `credentials.json` - API keys only (private, 600 permissions)

**Example config.json:**
```json
{
  "providers": {
    "anthropic": { "default_model": "claude-sonnet-4" },
    "openai": { "default_model": "gpt-4o" },
    "google": { "default_model": "gemini-2.5-pro" }
  },
  "default_provider": "anthropic",
  "max_output_tokens": 4096
}
```

**Example credentials.json:**
```json
{
  "anthropic": { "api_key": "sk-ant-..." },
  "openai": { "api_key": "sk-..." },
  "google": { "api_key": "..." }
}
```

**Credential Precedence:** Environment variable → credentials.json

**Standard Environment Variables:**
- `ANTHROPIC_API_KEY`
- `OPENAI_API_KEY`
- `GOOGLE_API_KEY`

### Token Tracking

**Post-Response Counting:**
- Extract exact token counts from API responses (input/output/thinking)
- Accumulate running total per conversation
- Display after each exchange
- Store per-message in database

**Example Display:**
```
> How does async I/O work?

[Response text...]

Tokens: 1,234 input + 567 output + 890 thinking = 2,691 total
```

### Provider Switching

Switch providers freely mid-conversation. History is preserved and sent to new provider in appropriate format.

```
> /model claude-sonnet-4-5/med
✓ Switched to Anthropic claude-sonnet-4-5-20250929

[Continue conversation with Claude...]

> /model o3-mini/high
✓ Switched to OpenAI o3-mini
  Reasoning: high effort

[Continue same conversation with OpenAI...]
```

## What's Deferred

**Additional Providers:**
- xAI (Grok) - OpenAI-compatible API, straightforward after Core 3
- Meta (Llama) - Requires self-hosting or third-party inference
- OpenRouter - Unified gateway (500+ models), needs separate design for routing/fallbacks

**Advanced Features:**
- Anthropic prompt caching (`cache_control`)
- Google grounding/search integration
- OpenAI Responses API state management
- Fallback chains (try provider A, fall back to B)
- Model capability registry/discovery
- Cost/billing tracking (have token counts, pricing integration later)
- Pre-send token estimation (local tokenizer library)

**Multimodal Support:**
- Image, audio, video input/output
- Deferred because: not all providers support all modalities, adds complexity

**RAG and Retrieval:**
- Retrieval Augmented Generation for permanent memory and document grounding
- Deferred because: substantial feature requiring separate design, not dependent on multi-provider work

## Open Questions

- How do we handle model-specific prompt engineering differences?
- Do we need a provider compatibility matrix?
- How do we test against multiple providers (cost, API keys)?
- Do we support offline mode for open source models?
- How do we handle provider deprecations and API version updates?

## Technical Details

For implementation details, architecture decisions, and technical specifications, see:

- **[plan/README.md](plan/README.md)** - Design overview and document index

**Architecture:**
- **[plan/01-architecture/overview.md](plan/01-architecture/overview.md)** - System architecture, vtable pattern, module organization
- **[plan/01-architecture/provider-interface.md](plan/01-architecture/provider-interface.md)** - Provider vtable interface specification

**Data Formats:**
- **[plan/02-data-formats/request-response.md](plan/02-data-formats/request-response.md)** - Internal data format (superset of all providers)
- **[plan/02-data-formats/streaming.md](plan/02-data-formats/streaming.md)** - Normalized streaming event types
- **[plan/02-data-formats/error-handling.md](plan/02-data-formats/error-handling.md)** - Error categories, mapping, retry strategies

**Provider Details:**
- **[plan/03-provider-types.md](plan/03-provider-types.md)** - Provider-specific transformation and thinking abstraction

**Application:**
- **[plan/04-application/commands.md](plan/04-application/commands.md)** - `/model` and `/fork` command specifications
- **[plan/04-application/configuration.md](plan/04-application/configuration.md)** - Configuration file format and precedence
- **[plan/04-application/database-schema.md](plan/04-application/database-schema.md)** - Schema changes for provider/model storage

**Testing:**
- **[plan/05-testing/strategy.md](plan/05-testing/strategy.md)** - Mock HTTP pattern, test organization
- **[plan/05-testing/vcr-cassettes.md](plan/05-testing/vcr-cassettes.md)** - VCR fixture format for HTTP recording/playback

## Research Artifacts

Provider API specifications documented in `findings/`:
- `anthropic-extended-thinking.md` - Claude API (extended thinking, tool use)
- `google-gemini-thinking.md` - Gemini API (thinking levels, grounding)
- `openai-reasoning-models.md` - OpenAI Chat Completions + Responses API
- `model-registry-patterns.md` - Model registry patterns
- `provider-configuration.md` - Provider configuration patterns
- `thinking-token-pricing.md` - Thinking token pricing research

Token counting research in `project/tokens/`:
- Tokenizer library design
- Provider-specific tokenization details
- Usage strategy (local estimates + exact API counts)
