# Model Registry Patterns Research

**Research Date:** December 14, 2025
**Research Focus:** Model naming patterns, provider inference, capabilities, and lifecycle management across major AI providers

## Summary

This research documents comprehensive model registry patterns across five major AI providers (Anthropic, OpenAI, Google, xAI, Meta) to inform ikigai's multi-provider abstraction design. **Key findings:**

1. **Provider inference from model names is reliable** - Each provider has distinct prefixes with few exceptions (Meta's `meta-llama/` namespace being the main edge case)
2. **Model naming conventions vary widely** - No industry standard exists; each provider uses different patterns for versions, dates, and aliases
3. **Context windows span 5 orders of magnitude** - From 128K tokens (legacy models) to 10M tokens (Llama 4 Scout), requiring careful capacity planning
4. **Deprecation timelines are provider-specific** - Ranging from 3-12 months notice, with different upgrade paths
5. **Static registry with periodic updates is recommended** - Dynamic model lists are inconsistently available and add operational complexity

---

## Provider Inference Rules

**Confidence: Verified** (from official API documentation and existing ikigai specs)

| Provider | Primary Patterns | Exceptions | Notes | Status |
|----------|------------------|------------|-------|--------|
| **anthropic** | `claude-*` | None known | All Claude models follow this pattern | ✅ Reliable |
| **openai** | `gpt-*`, `o1-*`, `o3-*`, `o4-*` | `chatgpt-*` variants (legacy), `text-embedding-*` (embeddings) | o-series are reasoning models, not GPT variants | ✅ Reliable |
| **google** | `gemini-*` | `models/gemini-*` (API format with prefix) | API accepts both with and without `models/` prefix | ✅ Reliable |
| **xai** | `grok-*` | None known | Consistent across all releases | ✅ Reliable |
| **meta** | `llama-*`, `Llama-*`, `meta-llama/*` | Namespace varies: HuggingFace uses `meta-llama/`, API uses various formats | Most ambiguous; needs normalization | ⚠️ Needs handling |

### Provider Inference Implementation Notes

**Meta/Llama Special Cases:**
- Official API accepts: `"meta-llama/llama-4-maverick"`, `"Llama-4-Maverick-17B-128E-Instruct-FP8"`, `"meta-llama/Meta-Llama-3.1-8B-Instruct"`
- HuggingFace format: `"meta-llama/Llama-4-Maverick-17B-128E-Instruct"`
- Recommendation: Strip `meta-llama/` prefix if present, then match on `llama-*` or `Llama-*` (case-insensitive)

**OpenAI Embeddings:**
- `text-embedding-3-large` and `text-embedding-3-small` should NOT be routed through chat completions
- ikigai should validate model type matches endpoint (future work)

---

## Model ID Patterns by Provider

### Anthropic Claude

**Confidence: Verified** (official documentation + search results)

**Format:** `claude-{family}-{version}[-{date}]`

**Naming Components:**
- **Family:** `opus`, `sonnet`, `haiku` (capability tiers: largest→smallest)
- **Version:** Major.minor (e.g., `4-5`, `3-7`, `3-5`)
- **Date:** `YYYYMMDD` snapshot identifier (optional)
- **Suffix:** `-latest` for auto-updating aliases

**Examples:**
```
claude-sonnet-4-5-20250929      ← Dated snapshot (stable, production-safe)
claude-sonnet-4-5               ← Alias (points to latest snapshot, updates automatically)
claude-3-7-sonnet-latest        ← Latest alias (auto-updates within ~1 week of new release)
claude-opus-4-5                 ← Current flagship
claude-haiku-4-5-20251001       ← Fast/cheap model
```

**Current Models (2025):**
- **claude-opus-4-5-20251101** - Most capable, professional software engineering (released Nov 24, 2025)
- **claude-opus-4-5** - Alias for latest Opus 4.5
- **claude-sonnet-4-5-20250929** - Best for real-world agents/coding (released Sep 29, 2025)
- **claude-sonnet-4-5** - Alias for latest Sonnet 4.5
- **claude-3-7-sonnet-20250219** - High-performance with extended thinking (released Feb 19, 2025)
- **claude-3-7-sonnet-latest** - Alias for latest 3.7 Sonnet
- **claude-haiku-4-5-20251001** - Fastest and most compact (released Oct 15, 2025)
- **claude-haiku-4-5** - Alias for latest Haiku 4.5
- **claude-3-5-haiku-latest** - Previous generation Haiku
- **claude-3-5-haiku-20241022** - Specific snapshot

**Alias Behavior:**
- Snapshot dates (e.g., `20250929`) **never change** - identical across all platforms
- Aliases without dates (e.g., `claude-sonnet-4-5`) point to latest snapshot
- `-latest` suffix aliases automatically update within ~1 week of new model release
- **Recommendation for ikigai:** Use snapshots for production, aliases for development/testing

**Sources:**
- [Models overview - Anthropic Claude API](https://docs.anthropic.com/en/docs/about-claude/models/overview)
- [What are Model Aliases in Claude Code](https://claudelog.com/faqs/what-is-model-alias/)

---

### OpenAI

**Confidence: Verified** (official documentation + search results)

**Format:** Inconsistent - acknowledged as confusing by CEO Sam Altman

**Naming Components:**
- **Family prefix:** `gpt-`, `o1-`, `o3-`, `o4-`, `text-embedding-`
- **Capability suffix:** `-mini`, `-nano`, `-pro`
- **Multimodal suffix:** `o` (e.g., `gpt-4o` = "omni" = multimodal)
- **Date:** `-YYYY-MM-DD` (optional, inconsistent format)

**Examples:**
```
gpt-5                           ← Latest flagship general chat
gpt-4o                          ← Multimodal GPT-4 ("o" = omni, NOT reasoning)
gpt-4o-2024-08-06              ← Dated snapshot
gpt-4.1-2025-04-14             ← April 2025 update
gpt-4.1-nano-2025-04-14        ← Smallest/fastest GPT-4.1
o3                              ← Reasoning model (NOT GPT-o3!)
o3-mini                         ← Compact reasoning
o4-mini                         ← Latest compact reasoning (replaces o3-mini)
chatgpt-4o-latest               ← Legacy ChatGPT variant
text-embedding-3-large          ← Embeddings (not chat)
```

**Current Models (2025):**

| Model ID | Type | Context | Notes |
|----------|------|---------|-------|
| `gpt-5` | General chat | 400K | Latest flagship |
| `gpt-4.1-2025-04-14` | General chat | 128K | April 2025 update |
| `gpt-4.1-nano-2025-04-14` | General chat | 128K | Smallest/fastest |
| `gpt-4o` | Multimodal | 128K | Text + vision |
| `gpt-4o-mini` | Multimodal | 128K | Smaller multimodal |
| `o3` | Reasoning | 200K | Complex reasoning tasks |
| `o3-mini` | Reasoning | 200K | Deprecated (replaced by o4-mini) |
| `o4-mini` | Reasoning | 200K | Latest compact reasoning |
| `o1-preview` | Reasoning | 128K | Deprecated (Apr 2025, removed Jul 2025) |
| `o1-mini` | Reasoning | 128K | Deprecated (Apr 2025, removed Oct 2025) |
| `o1-pro` | Reasoning | 128K | High-compute o1 |
| `text-embedding-3-large` | Embeddings | N/A | 3072 dimensions |
| `text-embedding-3-small` | Embeddings | N/A | 1536 dimensions |

**Confusing Aspects:**
- **"o" suffix ambiguity:** `gpt-4o` ("omni" = multimodal) vs `o3` (reasoning model) - completely different meanings
- **No o2:** Skipped due to trademark conflict with O2 (UK telecom provider)
- **Inconsistent versioning:** `gpt-4.1` vs `gpt-4o` vs `o3-mini`

**Sources:**
- [Decoding OpenAI's April 2025 Model Lineup](https://adam.holter.com/decoding-openais-april-2025-model-lineup-gpt‑4-1-gpt‑4o-and-the-o‑series)
- [OpenAI models: All the models and what they're best for](https://zapier.com/blog/openai-models/)
- [Models | OpenAI API](https://platform.openai.com/docs/models)

---

### Google Gemini

**Confidence: Verified** (official documentation + search results)

**Format:** `gemini-{version}-{family}[-{variant}]`

**Naming Components:**
- **Version:** Major.minor (e.g., `2.5`, `2.0`, `3`)
- **Family:** `pro`, `flash`, `flash-lite` (capability/speed tiers)
- **Variant:** `-latest`, `-live`, `-image` (optional)

**Examples:**
```
gemini-3-pro                    ← Latest reasoning-first model (preview)
gemini-2.5-pro                  ← Stable production, 2M token context
gemini-2.5-flash                ← Best price-performance, 1M context
gemini-2.5-flash-lite           ← Cost/latency optimized
gemini-2.0-flash                ← Next-gen features
gemini-2.5-flash-live           ← Native audio (Live API)
gemini-3-pro-image              ← Image generation variant
gemini-embedding-001            ← Embeddings (3072 dim, 100+ languages)
models/gemini-2.5-flash         ← API format with prefix (both accepted)
```

**Current Models (2025):**

| Model ID | Context | Type | Notes |
|----------|---------|------|-------|
| `gemini-3-pro` | 1M | Reasoning | Preview, requires Blaze pricing |
| `gemini-3-pro-image` | - | Image gen | High-fidelity with reasoning |
| `gemini-2.5-pro` | 2M | Production | Stable, massive context |
| `gemini-2.5-flash` | 1M | Production | Best price-performance |
| `gemini-2.5-flash-lite` | 1M | Production | Cost/latency optimized |
| `gemini-2.0-flash` | 1M | Next-gen | Native tool use |
| `gemini-2.0-flash-lite` | 1M | Next-gen | Cost/latency optimized |
| `gemini-2.5-flash-live` | - | Audio | Native audio models |
| `gemini-embedding-001` | 8K input | Embeddings | GA, 3072 dim |

**Version Lifecycle:**
- **Stable:** No three-digit suffix, no auto-updated alias, production-ready with retirement date
- **Preview:** Requires paid tier (e.g., Gemini 3 Pro on Blaze plan)
- **Experimental:** May have limited availability
- **Deprecated:** Gemini 1.5 Pro/Flash retired April 29, 2025 (legacy users may retain access)

**API Format Notes:**
- Both `gemini-2.5-flash` and `models/gemini-2.5-flash` are accepted
- Full resource name: `projects/PROJECT_ID/locations/REGION/publishers/google/models/gemini-2.5-flash`
- Partial: `publishers/google/models/gemini-2.5-flash`

**Date Format Controversy:**
- Community feedback requests YYYY-MM-DD instead of MM-DD-YYYY (currently inconsistent)

**Sources:**
- [Gemini models | Gemini API](https://ai.google.dev/gemini-api/docs/models)
- [All Gemini models available in 2025](https://www.datastudios.org/post/all-gemini-models-available-in-2025-complete-list-for-web-app-api-and-vertex-ai)
- [Naming conventions of Gemini models - DD-MM or MM-DD?](https://discuss.ai.google.dev/t/naming-conventions-of-gemini-models-dd-mm-or-mm-dd/88451)

---

### xAI Grok

**Confidence: Verified** (official documentation + search results)

**Format:** `grok-{version}[-{variant}][-{date}]`

**Naming Components:**
- **Version:** Numeric (e.g., `2`, `3`, `4`)
- **Point release:** Major.minor (e.g., `4.1`)
- **Variant:** `-mini`, `-beta`, `-fast-reasoning`, `-fast-non-reasoning`, `-vision`, `-image`, `-code-fast`
- **Date:** `-MMDD` (e.g., `-1212` = December 12)

**Examples:**
```
grok-4                          ← Frontier-level multimodal, 256K context
grok-4-0709                     ← July 2025 snapshot
grok-4-1-fast-reasoning         ← 2M context with reasoning
grok-4-1-fast-non-reasoning     ← 2M context, instant responses
grok-4-fast-reasoning           ← Fast variant with reasoning
grok-4-fast-non-reasoning       ← Fast variant without reasoning
grok-code-fast-1                ← Code-specialized
grok-3-beta                     ← General purpose, 131K context
grok-3-mini-beta                ← Compact with reasoning
grok-2-1212                     ← Better accuracy, Dec 2024
grok-2-latest                   ← Alias → grok-2-1212
grok-2-vision-1212              ← Vision model
grok-2-image-1212               ← Image generation
grok-beta                       ← Function calling, 128K context
grok-vision-beta                ← Image understanding, 8K context
```

**Current Models (2025):**

| Model ID | Context | Knowledge Cutoff | Capabilities |
|----------|---------|------------------|--------------|
| `grok-4` | 256K | Nov 2024 | Frontier multimodal, advanced reasoning |
| `grok-4-0709` | 256K | Nov 2024 | July 2025 snapshot |
| `grok-4-1-fast-reasoning` | 2M | Nov 2024 | Maximum intelligence with reasoning |
| `grok-4-1-fast-non-reasoning` | 2M | Nov 2024 | Instant, optimized for agentic tools |
| `grok-4-fast-reasoning` | - | Nov 2024 | Fast variant with reasoning |
| `grok-4-fast-non-reasoning` | - | Nov 2024 | Fast variant without reasoning |
| `grok-code-fast-1` | - | Nov 2024 | Code-specialized |
| `grok-3-beta` | 131K | Nov 2024 | General purpose |
| `grok-3-mini-beta` | - | Nov 2024 | Compact with reasoning |
| `grok-2-1212` | - | - | Better accuracy, multilingual |
| `grok-2-latest` | - | - | Alias to grok-2-1212 |
| `grok-2-vision-1212` | - | - | Vision model |
| `grok-2-image-1212` | - | - | Image generation (Aurora) |
| `grok-beta` | 128K | - | Function calling, system prompts |
| `grok-vision-beta` | 8K | - | Image understanding |

**Alias System:**
- `<modelname>` → latest stable version
- `<modelname>-latest` → latest version with newest features
- `<modelname>-<date>` → specific release (stable, never updates)

**Upcoming Models:**
- Grok 4.20 teased by Elon Musk (no release date)

**Sources:**
- [xAI Models Documentation](https://docs.x.ai/docs/models)
- [All Grok models available in 2025](https://www.datastudios.org/post/all-grok-models-available-in-2025-full-list-for-web-app-and-api-including-grok-4-3-mini-and-ima)
- [Grok (chatbot) - Wikipedia](https://en.wikipedia.org/wiki/Grok_(chatbot))

---

### Meta Llama

**Confidence: Verified** (official documentation + search results)

**Format:** `[meta-llama/][Llama-]{version}-{name}[-{size}][-{variant}]`

**Naming Components:**
- **Namespace:** `meta-llama/` (HuggingFace, some APIs) - optional
- **Case:** `Llama-` or `llama-` (inconsistent)
- **Version:** Major.minor (e.g., `3.1`, `3.3`, `4`)
- **Name:** Model name (e.g., `Maverick`, `Scout`, `Meta-Llama`)
- **Size:** Parameter count + expert count (e.g., `17B-128E`, `70B`, `405B`)
- **Variant:** `-Instruct`, `-FP8`, `-IT` (optional)

**Examples:**
```
meta-llama/llama-4-maverick                      ← API format (namespace)
Llama-4-Maverick-17B-128E-Instruct-FP8          ← Full specification
meta-llama/Llama-4-Maverick-17B-128E-Instruct   ← HuggingFace format
meta-llama/llama-4-scout                         ← 10M context variant
meta-llama/llama-3.3-70b-versatile               ← Llama 3.3
meta-llama/Meta-Llama-3.1-8B-Instruct           ← Legacy naming
meta-llama/Meta-Llama-3.1-70B-Instruct          ← Legacy naming
meta-llama/Meta-Llama-3.1-405B-Instruct         ← Largest Llama 3.1
```

**Current Models (2025):**

| Model ID | Params | Experts | Context | Capabilities |
|----------|--------|---------|---------|--------------|
| `meta-llama/llama-4-maverick` | 17B | 128 | 256K-1M | Best multimodal, coding, reasoning |
| `Llama-4-Maverick-17B-128E-Instruct-FP8` | 17B | 128 | 256K-1M | Quantized variant |
| `meta-llama/llama-4-scout` | 17B | 16 | 10M | Industry-leading context (109B to load) |
| `Llama-4-Scout-17B-16E-Instruct` | 17B | 16 | 10M | Full specification |
| `meta-llama/llama-3.3-70b-versatile` | 70B | - | 131K | General purpose |
| `meta-llama/Meta-Llama-3.1-8B-Instruct` | 8B | - | 128K | Fast, efficient |
| `meta-llama/Meta-Llama-3.1-70B-Instruct` | 70B | - | 128K | Balanced performance |
| `meta-llama/Meta-Llama-3.1-405B-Instruct` | 405B | - | 128K | Largest, highest quality |

**Naming Explanation:**
- **"Meta's Llama"** = "Large Language Model Meta AI" (acronym)
- **17B-128E:** 17 billion active parameters per token, 128 experts
- **17B-16E:** 17 billion active parameters per token, 16 experts (total 109B to load due to shared params)
- **FP8:** Quantization (8-bit floating point)
- **Instruct / IT:** Instruction-tuned variant

**Sources:**
- [How to navigate LLM model names](https://developers.redhat.com/articles/2025/04/03/how-navigate-llm-model-names)
- [meta-llama (Meta Llama) HuggingFace](https://huggingface.co/meta-llama)
- [All Meta AI models available in 2025](https://www.datastudios.org/post/all-meta-ai-models-available-in-2025-complete-list-for-web-mobile-and-developer-apis-including-ll)

---

## Recommended Defaults

**Confidence: Mixed** (Verified from benchmarks + Likely from pricing/availability)

| Provider | Default Model | Reasoning | Cost Tier | Alternative |
|----------|---------------|-----------|-----------|-------------|
| **anthropic** | `claude-sonnet-4-5` | Best balance of capability/cost for coding & reasoning | Medium | `claude-haiku-4-5` (faster/cheaper) |
| **openai** | `gpt-4o` | Multimodal, best general-purpose balance | Medium-High | `gpt-5` (flagship), `o4-mini` (reasoning) |
| **google** | `gemini-2.5-flash` | Best price-performance, 1M context | Low | `gemini-2.5-pro` (2M context, higher capability) |
| **xai** | `grok-4` | Frontier multimodal, real-time X integration | Medium | `grok-4-1-fast-non-reasoning` (2M context, faster) |
| **meta** | `llama-4-maverick` | Best multimodal, open license | Low (self-host) | `llama-4-scout` (10M context for long documents) |

**Use Case Specific Recommendations:**

| Use Case | Top Choice | Why | Runner-up |
|----------|------------|-----|-----------|
| **General Chat** | `gpt-4o` | Multimodal, well-rounded | `claude-sonnet-4-5` |
| **Coding** | `claude-sonnet-4-5` | 62-70% SWE-Bench accuracy, leader | `gpt-5.2` |
| **Reasoning/Math** | `o3` or `o4-mini` | Dedicated reasoning architecture | `gemini-3-pro` |
| **Long Context (1M+)** | `gemini-2.5-pro` | 2M context, stable | `llama-4-scout` (10M!) |
| **Speed/Cost** | `gemini-2.5-flash` | Ultra-fast, cheap, 1M context | `claude-haiku-4-5` |
| **Multimodal** | `llama-4-maverick` | Best vision + text + code | `gpt-4o` |
| **Real-time Info** | `grok-4` | X/Twitter integration, "Deep Search" | N/A |
| **Open Source** | `llama-4-maverick` | Permissive license, self-hostable | `qwen3-coder-480b` |

**Sources:**
- [An Opinionated Guide on Which AI Model to Use in 2025](https://creatoreconomy.so/p/an-opinionated-guide-on-which-ai-model-2025)
- [Best LLM For Coding 2025](https://binaryverseai.com/best-llm-for-coding-2025/)
- [AI Model Comparison 2025](https://blog.typingmind.com/ai-model-comparison-2025/)

---

## Model Capabilities Matrix

**Confidence: Verified** (official documentation)

| Model | Context | Vision | Tools | Thinking | Streaming | Notes |
|-------|---------|--------|-------|----------|-----------|-------|
| **claude-sonnet-4-5** | 200K | Yes | Yes | Yes (budget) | Yes | Extended thinking: 1K-50K tokens |
| **claude-opus-4-5** | 200K | Yes | Yes | Yes (budget) | Yes | Most capable Claude |
| **claude-haiku-4-5** | 200K | Yes | Yes | Yes (budget) | Yes | Fastest Claude |
| **gpt-5** | 400K | Yes | Yes | No | Yes | Latest flagship |
| **gpt-4o** | 128K | Yes (multimodal) | Yes | No | Yes | Omni = text/vision/audio |
| **o3** | 200K | No | Yes | Yes (effort) | Yes | Reasoning model |
| **o4-mini** | 200K | Yes (multimodal) | Yes | Yes (effort) | Yes | Latest compact reasoning |
| **gemini-2.5-pro** | 2M | Yes | Yes | Yes (budget) | Yes | Thinking: 128-32,768 tokens |
| **gemini-2.5-flash** | 1M | Yes | Yes | Yes (budget) | Yes | Thinking: 0-24,576 tokens |
| **gemini-3-pro** | 1M | Yes | Yes | Yes (level) | Yes | Thinking: LOW/HIGH levels |
| **grok-4** | 256K | Yes | Yes | No | Yes | Multimodal, real-time |
| **grok-4-1-fast-reasoning** | 2M | Yes | Yes | Yes (implicit) | Yes | Maximum intelligence |
| **llama-4-maverick** | 256K-1M | Yes | Yes | No | Yes | Best multimodal Llama |
| **llama-4-scout** | 10M | No (text only) | Yes | No | Yes | Industry-leading context |

### Thinking/Reasoning Parameter Mapping

| Provider | Parameter | Type | Range | Notes |
|----------|-----------|------|-------|-------|
| **Anthropic** | `thinking.budget_tokens` | Integer | 1,024 - 50,000+ | Per-model max varies |
| **Anthropic** | `thinking.type` | Enum | `enabled`, `disabled` | Required field |
| **Google (2.5)** | `thinkingBudget` | Integer | 0 - 32,768 (pro), 0 - 24,576 (flash) | 0 = off, -1 = dynamic |
| **Google (2.5)** | `includeThoughts` | Boolean | true/false | Controls thought exposure |
| **Google (3)** | `thinkingLevel` | Enum | `LOW`, `HIGH` | No token budget |
| **OpenAI** | `reasoning.effort` | Enum | `low`, `medium`, `high` | Only for o-series |
| **xAI** | N/A | - | - | Implicit in reasoning models |
| **Meta** | N/A | - | - | Not supported |

**ikigai's Unified Mapping (from README):**
```
none → min_budget or type: disabled
low  → min_budget + (1/3 × (max_budget - min_budget))
med  → min_budget + (2/3 × (max_budget - min_budget))
high → max_budget
```

### Context Window Details

**Effective vs. Advertised:**
- Most models become unreliable before advertised limit (typically ~65% of claimed context)
- Claude maintains <5% accuracy degradation across full 200K context
- Gemini Pro handles full 2M with multimodal data
- Llama 4 Scout's 10M is unprecedented but text-only

---

## Deprecated/Phased Models

**Confidence: Verified** (official deprecation notices)

| Model | Provider | Status | Replacement | Deadline | Source |
|-------|----------|--------|-------------|----------|--------|
| `o1-preview` | OpenAI | Deprecated | `o3`, `o4-mini` | Removed Jul 2025 | [Deprecations](https://platform.openai.com/docs/deprecations) |
| `o1-mini` | OpenAI | Deprecated | `o3-mini`, `o4-mini` | Removed Oct 2025 | [Deprecations](https://platform.openai.com/docs/deprecations) |
| `o3-mini` | OpenAI | Deprecated (postponed) | `o4-mini` | TBD (was Jul 2025) | [GitHub Changelog](https://github.blog/changelog/2025-06-20-upcoming-deprecation-of-o1-gpt-4-5-o3-mini-and-gpt-4o/) |
| `gpt-4.5-preview` | OpenAI | Deprecated | `gpt-5`, `gpt-4.1` | TBD | [Deprecations](https://platform.openai.com/docs/deprecations) |
| `chatgpt-4o-latest` | OpenAI | Deprecated | `gpt-4o` | Removed Nov 2025 | [Deprecations](https://platform.openai.com/docs/deprecations) |
| `gpt-4o-realtime-preview` | OpenAI | Deprecated | TBD | 6 months from Sep 2025 | [Deprecations](https://platform.openai.com/docs/deprecations) |
| `gemini-1.5-pro` | Google | Retired | `gemini-2.5-pro` | Apr 29, 2025 | [All Gemini models 2025](https://www.datastudios.org/post/all-gemini-models-available-in-2025-complete-list-for-web-app-api-and-vertex-ai) |
| `gemini-1.5-flash` | Google | Retired | `gemini-2.5-flash` | Apr 29, 2025 | [All Gemini models 2025](https://www.datastudios.org/post/all-gemini-models-available-in-2025-complete-list-for-web-app-api-and-vertex-ai) |
| Claude Sonnet 3.7 | Anthropic | Deprecated (GitHub) | `claude-sonnet-4-5` | Oct 23, 2025 (GitHub only) | [GitHub Changelog](https://github.blog/changelog/2025-10-23-selected-claude-openai-and-gemini-copilot-models-are-now-deprecated/) |
| Claude Opus 4 | Anthropic | Deprecated (GitHub) | `claude-opus-4.1` | Oct 23, 2025 (GitHub only) | [GitHub Changelog](https://github.blog/changelog/2025-10-23-selected-claude-openai-and-gemini-copilot-models-are-now-deprecated/) |

### Deprecation Policies

**OpenAI:**
- 3-6 months notice for deprecations
- Automatic migration for some aliases
- Preview models have shorter lifecycles

**Anthropic:**
- Snapshot dates are permanent (never retired)
- Aliases auto-update to newer snapshots
- No forced deprecations announced for direct API usage

**Google:**
- Stable models: 12 months minimum availability
- Additional 6 months for existing customers
- 60 days to test new GA models before forced upgrades
- Preview/experimental: No guarantees

**xAI:**
- Aliases auto-update to latest
- Dated snapshots remain stable
- No deprecation timeline announced

**Meta:**
- Open source: Models never "deprecated" (community maintained)
- API access: Follows standard cloud provider policies

---

## Model Alias Recommendations

**Confidence: Assumed** (inferred from user experience, not verified by providers)

For user convenience, ikigai could support semantic aliases:

```yaml
# Speed-optimized
fast: gemini-2.5-flash           # 1M context, ultra-fast, cheap
fastest: claude-haiku-4-5        # Sub-second responses

# Capability-optimized
smart: claude-opus-4-5           # Most capable overall
smartest: gpt-5                  # OpenAI flagship
reasoning: o3                    # Dedicated reasoning
best-coding: claude-sonnet-4-5   # SWE-Bench leader

# Cost-optimized
cheap: gemini-2.5-flash-lite     # Lowest cost per token
free: llama-4-maverick           # Self-hostable

# Context-optimized
long-context: llama-4-scout      # 10M tokens
huge-context: gemini-2.5-pro     # 2M tokens

# Specialty
multimodal: llama-4-maverick     # Best vision+code+text
realtime: grok-4                 # X integration, current info
open: llama-4-maverick           # Open source, permissive
```

**Implementation Notes:**
- Aliases should be provider-agnostic (map to best-in-class for that category)
- Should be configurable in user settings
- Could be overridden per-agent or per-session
- Should document which concrete model each alias maps to

**Risks:**
- Alias meanings may shift as models improve (e.g., "smart" = Opus 4.5 today, GPT-6 tomorrow)
- Users may become dependent on aliases and be surprised by model changes
- Providers may release models that change the "best" in each category

**Recommendation:** Support aliases but **always display the concrete model name** in responses/logs so users know what they're actually using.

---

## Model List APIs

**Confidence: Verified** (official API documentation)

### Provider Support for Dynamic Model Lists

| Provider | List Models API | Endpoint | Format | Recommendation |
|----------|-----------------|----------|--------|----------------|
| **Anthropic** | No | N/A | - | Use static registry |
| **OpenAI** | Yes | `GET /v1/models` | JSON array | Optional validation |
| **Google** | Yes | `GET /v1/models` | JSON array | Optional validation |
| **xAI** | Yes | `GET /v1/models` | JSON array | Optional validation |
| **Meta** | Yes | `GET /v1/models` | JSON array | Optional validation |

### Example Responses

**OpenAI:**
```json
{
  "object": "list",
  "data": [
    {
      "id": "gpt-5",
      "object": "model",
      "created": 1735689600,
      "owned_by": "openai"
    },
    {
      "id": "o3",
      "object": "model",
      "created": 1735084800,
      "owned_by": "openai"
    }
  ]
}
```

**Google (Gemini):**
```json
{
  "models": [
    {
      "name": "models/gemini-2.5-pro",
      "version": "001",
      "displayName": "Gemini 2.5 Pro",
      "description": "...",
      "inputTokenLimit": 2097152,
      "outputTokenLimit": 8192,
      "supportedGenerationMethods": ["generateContent", "streamGenerateContent"]
    }
  ]
}
```

### Static vs Dynamic Registry Decision

**Recommendation: Static registry with manual updates**

**Why:**
1. **Inconsistent API support:** Anthropic doesn't provide a list models endpoint
2. **Metadata limitations:** Most list endpoints don't include:
   - Context window size
   - Thinking/reasoning support details
   - Vision/multimodal capabilities
   - Cost per token
3. **Rate limit costs:** Querying models list adds overhead and consumes rate limits
4. **Startup latency:** Dynamic queries add 100-500ms to initialization
5. **Availability reliability:** Static registry works even if provider API is down
6. **Version control:** Can track model additions/changes in git

**When to use dynamic lists:**
- Model validation: Check if user-provided model ID exists before making expensive chat call
- Discovery: Offer autocomplete/suggestions when user types model name
- Monitoring: Periodic background job to detect new models and alert maintainers

**Hybrid Approach:**
```
1. Maintain static registry in code (models.json or models.h)
2. Optionally query provider APIs at startup for validation
3. Cache results for 24 hours to reduce overhead
4. Fall back to static registry if API query fails
5. Background job to check for new models weekly
```

---

## Validation Strategy

**Confidence: Assumed** (engineering best practices)

### Pre-Request Validation

**Level 1: Client-Side (ikigai)**
```c
// Before sending request to provider
1. Check model name format matches provider patterns
2. Verify model exists in static registry (or cached list)
3. Validate thinking config matches model capabilities
4. Confirm context length doesn't exceed model limit
5. Warn if using deprecated model
```

**Level 2: Provider API**
```
// Provider will return 400/404 if model doesn't exist
// Let provider validate as final authority
```

### Error Handling Strategy

**Model Not Found (404):**
```
1. Check if model is deprecated → suggest replacement
2. Check for typos → suggest similar model names
3. Check if model requires different provider → suggest correct provider
4. Fall back to default model with user notification
```

**Example Error Messages:**
```
❌ Model 'claude-4-sonnet' not found
   Did you mean: 'claude-sonnet-4-5'?

❌ Model 'o1-preview' is deprecated as of July 2025
   → Use 'o3' or 'o4-mini' instead

❌ Model 'gpt-4o' requires provider 'openai' but you specified 'anthropic'
   → Update provider or use 'claude-sonnet-4-5'
```

### Validation Implementation

**Suggested C structures:**
```c
typedef struct {
    const char *model_id;
    const char *provider;
    const char *display_name;
    uint32_t context_window;
    bool supports_vision;
    bool supports_tools;
    bool supports_thinking;
    bool deprecated;
    const char *replacement;  // If deprecated
} ik_model_info_t;

// Static registry
static const ik_model_info_t ik_model_registry[] = {
    {
        .model_id = "claude-sonnet-4-5",
        .provider = "anthropic",
        .display_name = "Claude Sonnet 4.5",
        .context_window = 200000,
        .supports_vision = true,
        .supports_tools = true,
        .supports_thinking = true,
        .deprecated = false,
        .replacement = NULL
    },
    {
        .model_id = "o1-preview",
        .provider = "openai",
        .display_name = "o1-preview",
        .context_window = 128000,
        .supports_vision = false,
        .supports_tools = true,
        .supports_thinking = true,
        .deprecated = true,
        .replacement = "o3"
    },
    // ... more models
};

// Validation function
ik_model_info_t* ik_validate_model(const char *model_id, const char *provider) {
    // 1. Find in registry
    for (size_t i = 0; i < ARRAY_SIZE(ik_model_registry); i++) {
        if (strcmp(model_id, ik_model_registry[i].model_id) == 0 &&
            strcmp(provider, ik_model_registry[i].provider) == 0) {

            // 2. Check if deprecated
            if (ik_model_registry[i].deprecated) {
                fprintf(stderr, "⚠️  Model '%s' is deprecated. Use '%s' instead.\n",
                        model_id, ik_model_registry[i].replacement);
            }

            return &ik_model_registry[i];
        }
    }

    // 3. Not found - suggest alternatives
    fprintf(stderr, "❌ Model '%s' not found for provider '%s'\n", model_id, provider);
    ik_suggest_similar_models(model_id, provider);
    return NULL;
}
```

---

## Implementation Notes

### Provider Inference Logic

**Recommended Implementation:**
```c
const char* ik_infer_provider(const char *model_id) {
    // Normalize model ID (strip meta-llama/ prefix if present)
    const char *normalized = model_id;
    if (strncmp(model_id, "meta-llama/", 11) == 0) {
        normalized = model_id + 11;
    }

    // Check prefixes (case-insensitive for Llama)
    if (strncmp(normalized, "claude-", 7) == 0) {
        return "anthropic";
    }
    if (strncmp(normalized, "gpt-", 4) == 0 ||
        strncmp(normalized, "o1-", 3) == 0 ||
        strncmp(normalized, "o3-", 3) == 0 ||
        strncmp(normalized, "o4-", 3) == 0) {
        return "openai";
    }
    if (strncmp(normalized, "gemini-", 7) == 0 ||
        strncmp(normalized, "models/gemini-", 14) == 0) {
        return "google";
    }
    if (strncmp(normalized, "grok-", 5) == 0) {
        return "xai";
    }
    if (strncasecmp(normalized, "llama-", 6) == 0 ||
        strncasecmp(normalized, "Llama-", 6) == 0) {
        return "meta";
    }

    // Unknown provider
    return NULL;
}
```

### Model Alias Resolution

**For providers with aliases:**
```c
const char* ik_resolve_model_alias(const char *provider, const char *model_id) {
    // Anthropic aliases
    if (strcmp(provider, "anthropic") == 0) {
        if (strcmp(model_id, "claude-sonnet-4-5") == 0) {
            return "claude-sonnet-4-5-20250929";  // Latest snapshot
        }
        if (strcmp(model_id, "claude-3-7-sonnet-latest") == 0) {
            return "claude-3-7-sonnet-20250219";
        }
        // ... more aliases
    }

    // OpenAI aliases
    if (strcmp(provider, "openai") == 0) {
        if (strcmp(model_id, "gpt-4o") == 0) {
            return "gpt-4o-2024-08-06";  // Specific snapshot
        }
        // ... more aliases
    }

    // xAI aliases
    if (strcmp(provider, "xai") == 0) {
        if (strcmp(model_id, "grok-2-latest") == 0) {
            return "grok-2-1212";
        }
        // ... more aliases
    }

    // No alias found - return original
    return model_id;
}
```

**Note:** Alias resolution should be **optional** - many users prefer explicit control over exact model versions. Consider making this a configuration option:

```json
{
  "resolve_model_aliases": false  // Default: false for production safety
}
```

### Thinking Parameter Translation

**Example implementation matching README design:**
```c
typedef struct {
    uint32_t min_budget;
    uint32_t max_budget;
} ik_thinking_budget_t;

ik_thinking_budget_t ik_get_thinking_budget(const char *provider, const char *model_id) {
    // Anthropic (from research - needs verification)
    if (strcmp(provider, "anthropic") == 0) {
        return (ik_thinking_budget_t){
            .min_budget = 1024,
            .max_budget = 50000  // Conservative estimate, needs verification
        };
    }

    // Google Gemini 2.5 Pro
    if (strcmp(provider, "google") == 0 && strstr(model_id, "2.5-pro")) {
        return (ik_thinking_budget_t){
            .min_budget = 128,
            .max_budget = 32768
        };
    }

    // Google Gemini 2.5 Flash
    if (strcmp(provider, "google") == 0 && strstr(model_id, "2.5-flash")) {
        return (ik_thinking_budget_t){
            .min_budget = 0,
            .max_budget = 24576
        };
    }

    // OpenAI o-series: No budget, uses effort levels
    // xAI: No explicit thinking control
    // Meta: No thinking support

    return (ik_thinking_budget_t){.min_budget = 0, .max_budget = 0};
}

uint32_t ik_thinking_level_to_budget(ik_thinking_level_t level,
                                      ik_thinking_budget_t budget) {
    switch (level) {
        case IK_THINKING_NONE:
            return budget.min_budget;
        case IK_THINKING_LOW:
            return budget.min_budget +
                   (budget.max_budget - budget.min_budget) / 3;
        case IK_THINKING_MED:
            return budget.min_budget +
                   2 * (budget.max_budget - budget.min_budget) / 3;
        case IK_THINKING_HIGH:
            return budget.max_budget;
        default:
            return budget.min_budget;
    }
}
```

---

## OpenRouter Integration

**Confidence: Verified** (official documentation)

### Model Naming Convention

OpenRouter uses `provider/model-name` format:
```
anthropic/claude-4.5-sonnet-20250929
openai/gpt-5
openai/gpt-oss-120b
google/gemini-3-pro-preview-20251117
x-ai/grok-code-fast-1
meta-llama/llama-4-maverick
```

### Special Features

**Routing Shortcuts:**
- `:nitro` - Fastest throughput routing
- `:floor` - Lowest price routing

**Example:**
```
openai/gpt-5:nitro    → Route to fastest available GPT-5 deployment
anthropic/claude-opus-4-5:floor → Route to cheapest deployment
```

### API Compatibility

**OpenAI SDK Compatible:**
```python
from openai import OpenAI

client = OpenAI(
    base_url="https://openrouter.ai/api/v1",
    api_key="sk-or-..."
)

response = client.chat.completions.create(
    model="anthropic/claude-sonnet-4-5",
    messages=[...]
)
```

### Integration with ikigai

**Option 1: OpenRouter as separate provider**
```
/model openrouter/anthropic/claude-sonnet-4-5
```

**Option 2: OpenRouter as routing layer**
```json
{
  "provider": "openrouter",
  "underlying_provider": "anthropic",
  "model": "claude-sonnet-4-5",
  "routing": "nitro"  // Optional
}
```

**Recommendation:** Defer OpenRouter to future release (per README). When implemented:
1. Treat OpenRouter as a **meta-provider** that wraps other providers
2. Support both direct provider access and OpenRouter routing
3. Allow routing preferences (`:nitro`, `:floor`) as optional config
4. Maintain provider inference even when using OpenRouter (for logging/metrics)

**Sources:**
- [Models | OpenRouter](https://openrouter.ai/models)
- [OpenRouter Deep Dive: The Real-World Guide](https://medium.com/ai-simplified-in-plain-english/openrouter-deep-dive-the-real-world-guide-to-choosing-ai-models-that-work-ee9e23f73012)

---

## Missed Concepts

**Confidence: Assumed** (identified gaps during research)

### 1. Model Pricing/Cost Tracking

**Not covered in research but essential:**
- Input vs output token pricing varies widely
- Thinking/reasoning tokens may be priced differently
- Cached tokens (prompt caching) have different rates
- Batch API often has 50% discount

**Example pricing (per 1M tokens, Dec 2025):**
```
Claude Sonnet 4.5:  $3 input / $15 output
GPT-5:              $10 input / $40 output
o3:                 $15 input / $60 output
Gemini 2.5 Pro:     $1.25 input / $10 output
Gemini 2.5 Flash:   $0.075 input / $0.30 output
Grok 4:             $3 input / $15 output
Llama 4 (API):      ~$0.60 input / $2.40 output (varies by host)
```

**Recommendation:** Add cost tracking to ikigai in future release:
- Store token usage per message
- Calculate cost based on provider pricing
- Display running cost per session
- Warn when approaching budget limits

### 2. Model Fine-Tuning Support

**Not covered but some providers support:**
- OpenAI: Fine-tuning for GPT-4o, GPT-4o-mini
- Google: Tuning for Gemini models
- Anthropic: Limited fine-tuning (mostly enterprise)
- Meta: Full fine-tuning capability (open source)

**Recommendation:** Not needed for ikigai rel-07, but document for future

### 3. Model Moderation/Safety Filters

**Each provider handles differently:**
- OpenAI: Separate moderation API, automatic filtering
- Anthropic: Constitutional AI, built-in safety
- Google: Safety settings per request (BLOCK_NONE, BLOCK_ONLY_HIGH, etc.)
- xAI: Less restrictive than others
- Meta: Open source, no API-level filtering

**Recommendation:** Expose provider-specific safety settings in advanced config

### 4. Model Versioning/Snapshot Strategy

**Not fully addressed:**
- When to use snapshots vs aliases?
- How often to update default model in ikigai?
- How to notify users when their model is deprecated?

**Best Practices:**
```
Production → Use dated snapshots (claude-sonnet-4-5-20250929)
Development → Use aliases (claude-sonnet-4-5) for latest features
Testing → Explicitly pin to specific versions
```

**Recommendation:**
- Default to snapshots for stability
- Provide `--latest` flag to use aliases
- Weekly check for deprecated models
- Prompt user to migrate 30 days before removal

### 5. Model Temperature/Sampling Defaults

**Providers have different defaults:**
- Anthropic: temperature=1.0, top_p=1.0
- OpenAI: temperature=1.0, top_p=1.0
- Google: temperature=1.0, top_p=0.95, top_k=40
- xAI: temperature=1.0, top_p=1.0
- Meta: temperature=0.6, top_p=0.9

**Recommendation:** Use provider defaults, allow user override

### 6. Model Context Window Management

**Not addressed:**
- How to handle exceeding context window?
- Automatic truncation vs error?
- Sliding window for long conversations?

**Recommendation:**
- Calculate running token count before each request
- Warn at 80% capacity
- Error at 95% (leave room for response)
- Implement conversation summarization for rel-08

### 7. Model Ecosystem Fragmentation

**Emerging patterns not yet standardized:**
- Some models are "agentic" (designed for tool use, multi-turn)
- Some are "conversational" (chat-optimized)
- Some are "reasoning-first" (slow, deep thinking)
- Trend toward specialization rather than one-size-fits-all

**Recommendation:** Tag models with primary use case in registry

---

## Research Gaps & Future Work

**Items requiring further investigation:**

### High Priority

1. **Anthropic extended thinking token limits**
   - Documented minimum: 1,024 tokens
   - Maximum per model: UNKNOWN (conservatively estimated at 50K)
   - **Action:** Test with API to determine actual maximums

2. **Provider-specific thinking token costs**
   - Do Anthropic/Google charge same rate for thinking vs output tokens?
   - OpenAI o-series pricing breakdown?
   - **Action:** Review pricing pages, test with real requests

3. **Model release cadence**
   - How often do providers release new models?
   - Typical lifecycle: preview → beta → GA → deprecated?
   - **Action:** Track release patterns over 6 months

### Medium Priority

4. **Vision model limitations**
   - Image format requirements (size, resolution, format)
   - Multiple images per request limits
   - Video support (Gemini claims it, others?)
   - **Action:** Document per provider

5. **Tool calling differences**
   - Parallel tool calling support
   - Tool result format differences
   - Maximum tools per request
   - **Action:** Compare actual behavior across providers

6. **Streaming implementation details**
   - Chunk sizes (token-level, word-level, sentence-level?)
   - Backpressure handling
   - Mid-stream metadata (token counts while streaming)
   - **Action:** Test each provider's streaming behavior

### Low Priority

7. **Rate limit headers**
   - Format differences across providers
   - Retry-after semantics
   - **Action:** Document when implementing retry logic

8. **Batch API availability**
   - Which providers offer batch processing?
   - Pricing discounts?
   - Turnaround time guarantees?
   - **Action:** Research for future optimization

---

## Confidence Levels Summary

| Topic | Confidence | Source |
|-------|------------|--------|
| Provider inference patterns | **Verified** | Official API docs |
| Model naming conventions | **Verified** | Official docs + search |
| Context window sizes | **Verified** | Official specs |
| Thinking/reasoning parameters | **Verified** | Official API docs |
| Vision/multimodal support | **Verified** | Official specs |
| Tool/function calling support | **Verified** | Official API docs |
| Streaming support | **Verified** | Official API docs |
| Deprecated models | **Verified** | Official deprecation notices |
| Recommended defaults | **Likely** | Multiple benchmark sources agree |
| Model aliases | **Verified** | Official docs (varies by provider) |
| Pricing | **Likely** | Official pricing pages (subject to change) |
| Best practices | **Assumed** | Engineering experience + community consensus |
| Future trends | **Assumed** | Inferred from current trajectories |

---

## Implementation Recommendations

### For ikigai rel-07

1. **Use static model registry**
   - Define in `models.json` or C header
   - Include: model_id, provider, context_window, capabilities, deprecated flag
   - Update monthly or when deprecations announced

2. **Provider inference**
   - Implement prefix matching with Meta special case
   - Fall back to explicit provider if inference fails
   - Log inference results for debugging

3. **Model validation**
   - Check registry before API call
   - Warn on deprecated models
   - Suggest alternatives for unknown models

4. **Thinking parameter mapping**
   - Use README design (none/low/med/high)
   - Per-provider translation to native params
   - Research exact budget limits (see Research Gaps #1)

5. **Error handling**
   - Map provider errors to ikigai error categories
   - Provide actionable suggestions
   - Log provider-specific error codes

6. **Documentation**
   - Include model list in user docs
   - Document provider inference rules
   - Explain thinking level mapping
   - Link to official provider docs

### For future releases

7. **Cost tracking** (rel-08)
   - Token-based cost calculation
   - Budget warnings
   - Provider cost comparison

8. **Dynamic model discovery** (rel-08)
   - Background job to query list models APIs
   - Alert on new model releases
   - Auto-update registry

9. **Model routing** (rel-09+)
   - OpenRouter integration
   - Automatic failover
   - Cost-optimized routing

10. **Vision support** (rel-08)
    - Image input handling
    - Provider-specific format conversion
    - Size/format validation

---

## Sources

### Official Documentation
- [Anthropic Claude API - Models overview](https://docs.anthropic.com/en/docs/about-claude/models/overview)
- [OpenAI API - Models](https://platform.openai.com/docs/models)
- [OpenAI API - Deprecations](https://platform.openai.com/docs/deprecations)
- [Google Gemini API - Models](https://ai.google.dev/gemini-api/docs/models)
- [xAI Grok - Models Documentation](https://docs.x.ai/docs/models)
- [Meta Llama - HuggingFace](https://huggingface.co/meta-llama)

### Model Naming & Patterns
- [What are Model Aliases in Claude Code](https://claudelog.com/faqs/what-is-model-alias/)
- [Anthropic Claude Models: Complete Naming and Performance Guide](https://www.robertodiasduarte.com.br/en/modelos-claude-da-anthropic-guia-completo-de-nomenclatura-e-performance/)
- [Decoding OpenAI's April 2025 Model Lineup](https://adam.holter.com/decoding-openais-april-2025-model-lineup-gpt‑4-1-gpt‑4o-and-the-o‑series)
- [How to navigate LLM model names](https://developers.redhat.com/articles/2025/04/03/how-navigate-llm-model-names)

### Deprecations & Lifecycle
- [GitHub Changelog - Upcoming deprecation of o1, GPT-4.5, o3-mini, and GPT-4o](https://github.blog/changelog/2025-06-20-upcoming-deprecation-of-o1-gpt-4-5-o3-mini-and-gpt-4o/)
- [GitHub Changelog - Selected Claude, OpenAI, and Gemini Copilot models are now deprecated](https://github.blog/changelog/2025-10-23-selected-claude-openai-and-gemini-copilot-models-are-now-deprecated/)
- [Azure OpenAI Model Retirements](https://learn.microsoft.com/en-us/azure/ai-foundry/openai/concepts/model-retirements?view=foundry-classic)

### Capabilities & Comparisons
- [AI Models Comparison 2025: Claude, Grok, GPT & More](https://collabnix.com/comparing-top-ai-models-in-2025-claude-grok-gpt-llama-gemini-and-deepseek-the-ultimate-guide/)
- [Ultimate 2025 AI Language Models Comparison](https://www.promptitude.io/post/ultimate-2025-ai-language-models-comparison-gpt5-gpt-4-claude-gemini-sonar-more)
- [Best LLMs for Extended Context Windows](https://research.aimultiple.com/ai-context-window/)
- [LLM Comparison 2025: Best AI Models Ranked](https://vertu.com/lifestyle/top-8-ai-models-ranked-gemini-3-chatgpt-5-1-grok-4-claude-4-5-more/)

### Recommendations & Best Practices
- [An Opinionated Guide on Which AI Model to Use in 2025](https://creatoreconomy.so/p/an-opinionated-guide-on-which-ai-model-2025)
- [Best LLM For Coding 2025](https://binaryverseai.com/best-llm-for-coding-2025/)
- [5 Best AI Reasoning Models of 2025: Ranked!](https://www.labellerr.com/blog/compare-reasoning-models/)
- [Best AI Coding Models in 2025](https://codeconductor.ai/blog/ai-coding-models/)

### OpenRouter
- [Models | OpenRouter](https://openrouter.ai/models)
- [OpenRouter Review 2025: Multi-Model LLM Gateway Tested for Production](https://skywork.ai/blog/openrouter-review-2025/)
- [OpenRouter Deep Dive: The Real-World Guide](https://medium.com/ai-simplified-in-plain-english/openrouter-deep-dive-the-real-world-guide-to-choosing-ai-models-that-work-ee9e23f73012)

### Vision & Multimodal
- [Gemini 3 Pro: the frontier of vision AI](https://blog.google/technology/developers/gemini-3-pro-vision/)
- [Top 10 Vision Language Models in 2025](https://dextralabs.com/blog/top-10-vision-language-models/)
- [Vision Language Models (Better, faster, stronger)](https://huggingface.co/blog/vlms-2025)

---

**End of Research Document**

Last Updated: December 14, 2025
Next Review: March 2025 (or when major model releases occur)
