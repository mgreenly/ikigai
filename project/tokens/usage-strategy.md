# Token Usage Strategy

## Overview

This document describes how ikigai tracks and displays token usage, combining local estimates with exact counts from API responses.

## Core Principle

**Response usage is ground truth.** Every LLM provider returns exact token counts in their response. We use local estimates for immediate feedback, then replace with exact values when available.

## Display Pattern

| State | Display | Source |
|-------|---------|--------|
| Composing message | `~10.2K` | Local estimate (tokenizer library) |
| Waiting for response | `~10.2K` | Local estimate |
| Response received | `11.0K` | Exact from response.usage |
| Provider switch | `0` | Reset (new tokenizer) |
| First response after switch | `8.5K` | Exact from new provider |

The `~` prefix clearly signals "this is an estimate" to users.

## Why This Approach

### Response-Based (Exact)

Every provider returns usage data:

```json
// OpenAI / xAI
"usage": {
  "prompt_tokens": 1234,
  "completion_tokens": 567,
  "total_tokens": 1801
}

// Anthropic
"usage": {
  "input_tokens": 1234,
  "output_tokens": 567
}

// Google Gemini
"usageMetadata": {
  "promptTokenCount": 1234,
  "candidatesTokenCount": 567,
  "totalTokenCount": 1801
}

// Meta Llama (self-hosted, varies by runtime)
// Check your inference server's response format
```

Benefits:
- 100% accurate for the provider's billing
- Includes system prompts, tools, special tokens
- No local computation needed
- Always matches what provider charges

### Local Estimates (Approximate)

Using the tokenizer library for pre-response estimates:

Benefits:
- Immediate feedback while typing
- Works offline
- Enables pre-flight checks ("will this fit?")
- Warns user before hitting context limits

Limitations:
- ~80-95% accurate depending on provider
- Anthropic estimates use cl100k_base fallback (~80-90% accurate)
- Doesn't account for provider-specific formatting

## Implementation Flow

```
┌─────────────────────────────────────────────────────────────┐
│ User composing message                                      │
│                                                             │
│   Local tokenizer: estimate current context                 │
│   Display: "~10,234 tokens"                                 │
│                                                             │
│   If estimate > 90% of context window:                      │
│     Show warning: "Approaching context limit"               │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ User sends message                                          │
│                                                             │
│   Display: "~10,234 tokens" (still estimate)                │
│   State: waiting for response                               │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ Response received                                           │
│                                                             │
│   Extract: response.usage.prompt_tokens (or equivalent)     │
│   Extract: response.usage.completion_tokens                 │
│   Calculate: total = prompt + completion                    │
│                                                             │
│   Display: "11,012 tokens" (exact, no ~)                    │
│   Store: exact count for this conversation                  │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ Next message                                                │
│                                                             │
│   Base: last exact count (11,012)                           │
│   Add: estimate for new user message                        │
│   Display: "~11,500 tokens"                                 │
└─────────────────────────────────────────────────────────────┘
```

## Provider Switch Handling

When user switches providers mid-conversation:

1. Reset token count to `0`
2. Display `0 tokens` (or `~X` if estimating new context)
3. After first response: show exact count from new provider

This is correct because:
- Different providers tokenize differently
- Previous count is meaningless for new provider
- Fresh start gives accurate tracking

## Context Window Warnings

Using estimates for proactive warnings:

```
Context: 128K tokens (e.g., GPT-4)

At ~100K tokens: "~78% of context used"
At ~115K tokens: "~90% of context used - consider starting new conversation"
At ~125K tokens: "Approaching limit - response may be truncated"
```

Even with ~10% estimation error, these warnings are useful:
- 90% warning might fire at actual 82-99% - still useful
- Better to warn early than hit limit unexpectedly

## Data Model

```
Conversation {
  provider: string           // "openai", "anthropic", etc.
  model: string              // "gpt-4o", "claude-sonnet-4", etc.

  // Updated after each response
  last_exact_tokens: int     // From response.usage

  // Calculated on demand
  estimated_tokens: int      // last_exact + estimate(pending_content)
  is_estimate: bool          // True if pending content exists
}
```

## Edge Cases

### First Message (No History)

- Estimate based on system prompt + user message
- Display: `~1,234 tokens`
- After response: exact count

### Streaming Responses

- Update token count when stream completes
- Some providers send usage in final chunk
- Others require separate API call (rare)

### Context Overflow Error

If provider returns context exceeded error:
- Display error to user
- Suggest: truncate history, start new conversation, or switch to larger context model

### Provider Without Usage Data

If a provider doesn't return usage (uncommon):
- Fall back to estimate permanently
- Always show `~` prefix
- Log warning for debugging

## Summary

| What | How | Accuracy |
|------|-----|----------|
| Pre-send estimate | Local tokenizer | ~80-95% |
| Post-response count | response.usage | 100% |
| Context warnings | Local estimate + margin | Good enough |
| Cost tracking | response.usage only | 100% |

The hybrid approach gives users immediate, useful feedback while ensuring billing and final counts are always accurate.

## Related Documentation

- [tokenizer-library-design.md](tokenizer-library-design.md) - Library architecture
- [anthropic-tokens.md](anthropic-tokens.md) - Anthropic specifics (API-only exact counts)
