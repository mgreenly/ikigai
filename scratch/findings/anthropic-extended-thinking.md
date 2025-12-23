# Anthropic Extended Thinking Limits and Behavior

**Research Date:** 2025-12-14
**Models Investigated:** claude-opus-4-5, claude-sonnet-4-5, claude-3.7-sonnet, claude-haiku-4-5

## Summary

Anthropic's Extended Thinking feature supports token budgets from a minimum of 1,024 tokens up to a maximum that varies by model and configuration. Claude 3.7 Sonnet supports up to 128K thinking tokens (beta), while Claude 4 models support up to 64K tokens as standard practice. The thinking budget is a target rather than a strict limit, and actual usage may vary. Thinking content can be streamed, and may occasionally be redacted by safety systems. The feature integrates with tool use but has specific requirements around preserving thinking blocks in multi-turn conversations.

## Budget Limits

### Universal Constraints

- **Minimum budget:** 1,024 tokens (Verified - official docs)
- **Budget constraint:** `budget_tokens` must be less than `max_tokens` (Verified)
- **Context window interaction:** `prompt_tokens + max_tokens` must not exceed context window (200K tokens) - enforced as strict validation error in Claude 3.7 and 4 models (Verified)
- **Budget is a target:** Actual token usage may vary based on task complexity (Verified)

### Per-Model Limits

| Model | Min Budget | Max Budget | Context Window | Output Limit | Notes |
|-------|------------|------------|----------------|--------------|-------|
| claude-opus-4-5-20251101 | 1,024 | 64,000 (std) / 128,000 (beta) | 200K | 64K (std) / 128K (beta) | Supports effort parameter, preserves thinking blocks by default |
| claude-sonnet-4.5-20250929 | 1,024 | 64,000 (std) | 200K | 64K (std) | Used 64K reasoning tokens on AIME benchmarks |
| claude-3-7-sonnet-20250219 | 1,024 | 128,000 (with beta) | 200K | 128K (beta) | Requires `output-128k-2025-02-19` beta header for 128K, returns full thinking (not summarized) |
| claude-haiku-4-5-20251001 | 1,024 | 64,000 (std) | 200K | 64K (std) | Hybrid model with instant + extended thinking |
| claude-opus-4.1-20250805 | 1,024 | 64,000 (std) | 200K | 64K (std) | - |
| claude-opus-4-20250514 | 1,024 | 64,000 (std) | 200K | 64K (std) | - |

**Confidence:** Verified from official Anthropic documentation and announcements.

### 128K Extended Output (Beta)

- Available for Claude 3.7 Sonnet and select Claude 4 models
- Requires beta header: `anthropic-beta: output-128k-2025-02-19`
- Enables thinking budgets up to 128,000 tokens
- General availability up to 64K, 64K-128K range is beta (Verified)

### Recommended Starting Points

- Start with minimum (1,024 tokens) and increase incrementally (Verified)
- Try at least 4,000 tokens for comprehensive reasoning (Verified - official recommendation)
- Use 16K+ tokens for complex tasks (Verified)
- For budgets above 32K, use batch processing to avoid timeouts (Verified)

## Behavior Details

### Enabling/Disabling

**Request Format:**
```json
{
  "thinking": {
    "type": "enabled",        // or "disabled"
    "budget_tokens": 10000    // 1024 minimum when enabled
  }
}
```

**Disabling:**
- Set `"type": "disabled"` or omit the thinking parameter entirely (Verified)
- When disabled, no thinking blocks are generated

**Response Format:**
```json
{
  "content": [
    {
      "type": "thinking",
      "thinking": "Step-by-step reasoning...",
      "signature": "encrypted_verification_data..."
    },
    {
      "type": "text",
      "text": "Final answer..."
    }
  ]
}
```

**Confidence:** Verified from official API documentation.

### Summarized vs Full Thinking

| Model Version | Thinking Output | Billing |
|---------------|----------------|---------|
| Claude 3.7 Sonnet | Full thinking content returned | Billed for actual tokens generated |
| Claude 4 models (pre-Opus 4.5) | Summarized thinking content | Billed for FULL thinking tokens, not summary |
| Claude Opus 4.5+ | Summarized thinking content | Billed for FULL thinking tokens, not summary |

**Important:** For Claude 4 models, the visible token count in response does NOT match billed output tokens. You pay for the full internal reasoning, not the summary. (Verified)

### Streaming Behavior

**SSE Event Types:**
- `message_start` - Message begins
- `content_block_start` - Content block (thinking or text) begins
- `content_block_delta` - Content chunk arrives
  - `thinking_delta` - Thinking content chunk
  - `text_delta` - Text content chunk
  - `signature_delta` - Signature verification data
- `content_block_stop` - Content block ends
- `message_delta` - Metadata update
- `message_stop` - Message complete

**Streaming Characteristics:**
- Thinking is delivered BEFORE text content (Verified)
- Thinking blocks may arrive in larger chunks with delays (expected behavior)
- Streaming is REQUIRED when `max_tokens > 21,333` (Verified)
- Signature is added via `signature_delta` just before `content_block_stop` (Verified)

**Example Event:**
```
event: content_block_delta
data: {"type": "content_block_delta", "index": 0, "delta": {"type": "thinking_delta", "thinking": "Let me solve this..."}}
```

**Confidence:** Verified from official streaming documentation.

### Redacted Thinking

**When it occurs:**
- Safety systems flag thinking content (Verified)
- Occurs occasionally, not predictably
- Expected behavior, not an error

**Response format:**
```json
{
  "type": "redacted_thinking",
  "data": "encrypted_thinking_content..."
}
```

**Behavior:**
- Redacted thinking is decrypted when passed back to API (Verified)
- Allows Claude to continue reasoning without losing context
- Still billed as output tokens (Verified)
- Must be preserved and passed back unmodified in multi-turn conversations (Verified)

**Testing:**
Use magic string: `ANTHROPIC_MAGIC_STRING_TRIGGER_REDACTED_THINKING_46C9A13E193C177646C7398A98432ECCCE4C1253D5E2D82641AC0E52CC2876CB` (Verified)

**Confidence:** Verified from official documentation.

### Interaction with Tools

**Tool Choice Constraints:**
- Only supports `tool_choice: {"type": "auto"}` (default) or `{"type": "none"}` (Verified)
- Does NOT support `{"type": "any"}` or forcing specific tools (Verified)
- Incompatible with forced tool use

**Preserving Thinking Blocks:**
- MUST pass thinking blocks back with tool results (Verified)
- Include complete, unmodified thinking blocks from last assistant turn (Verified)
- Failing to do so causes API error (Verified)
- Can omit thinking blocks from earlier turns (but recommended to pass all)

**Error Message (if thinking blocks not preserved):**
```
Expected `thinking` or `redacted_thinking`, but found `tool_use`.
When `thinking` is enabled, a final `assistant` message must start
with a thinking block (preceding the lastmost set of `tool_use` and
`tool_result` blocks).
```

**Toggling Thinking During Tool Use:**
- Cannot toggle thinking mid-assistant-turn (Verified)
- Tool use loops are part of a single assistant turn (Verified)
- Must complete turn before changing thinking mode

**Interleaved Thinking (Claude 4 models only):**
- Beta header: `interleaved-thinking-2025-05-14` (Verified)
- Allows thinking BETWEEN tool calls (Verified)
- `budget_tokens` can exceed `max_tokens` (total across all thinking blocks) (Verified)
- Enables more sophisticated multi-step reasoning with tools

**Confidence:** Verified from official documentation.

### Context Window Behavior

**Thinking Block Removal:**
- **Claude 3.7 and Claude 4 (pre-Opus 4.5):** Thinking blocks from previous turns are automatically removed from context (Verified)
- **Claude Opus 4.5+:** Thinking blocks from previous turns are PRESERVED by default (Verified)
  - Enables cache optimization
  - Reduces token usage in multi-turn conversations
  - No negative intelligence impact

**Context Calculation (without tools):**
```
context_window = (current_input_tokens - previous_thinking_tokens) +
                 (thinking_tokens + encrypted_thinking + text_output_tokens)
```

**Context Calculation (with tools):**
```
context_window = (current_input_tokens + previous_thinking_tokens + tool_use_tokens) +
                 (thinking_tokens + encrypted_thinking + text_output_tokens)
```

**Important:** Thinking blocks MUST be preserved when using tools, which affects context usage. (Verified)

**Confidence:** Verified from official documentation.

### Prompt Caching Interaction

**Cache Invalidation:**
- Changing thinking parameters (enabled/disabled, budget) invalidates message cache (Verified)
- System prompts and tools remain cached despite thinking changes (Verified)
- Interleaved thinking amplifies cache invalidation (multiple thinking blocks)

**Thinking Block Caching:**
- Thinking blocks are cached when passed back with tool results (Verified)
- Cached thinking blocks count as input tokens when read from cache (Verified)
- Creates tradeoff: no context window cost but input token billing from cache

**Recommended Cache Duration:**
- Use 1-hour cache for extended thinking tasks (longer than default 5 minutes) (Verified)
- Important for multi-step workflows and long reasoning sessions

**Confidence:** Verified from official documentation.

### Effort Parameter (Claude Opus 4.5 only)

**Beta Header:** `effort-2025-11-24` (Verified)

**Levels:**
- `high` - Maximum capability (default, equivalent to omitting parameter)
- `medium` - Balanced token savings (76% fewer output tokens than high on some benchmarks)
- `low` - Maximum efficiency (significant token savings)

**What it affects:**
- All tokens: text responses, tool calls, thinking (if enabled) (Verified)
- Can be used WITH or WITHOUT thinking enabled (Verified)
- Lower effort = fewer tool calls, terser responses

**Relationship with Thinking:**
- Effort controls HOW tokens are spent (Verified)
- Thinking budget controls MAX thinking tokens (Verified)
- Independent but complementary controls

**Confidence:** Verified from official documentation.

### Token Counting

**Input Tokens:**
- User messages, system prompt, tool definitions
- Previous thinking blocks (when using tools)
- Cached thinking blocks (when read from cache)

**Output Tokens (billed):**
- Full thinking tokens generated (even if summarized in response)
- Text response tokens
- Tool call tokens

**Output Tokens (visible):**
- For Claude 3.7: Full thinking content
- For Claude 4: Summarized thinking content (MUCH shorter than billed)

**Important:** Billed output tokens != visible output tokens for Claude 4 models. (Verified)

**Confidence:** Verified from official documentation.

### Incompatibilities

Extended thinking is NOT compatible with:
- `temperature` modifications (Verified)
- `top_k` modifications (Verified)
- Pre-filled responses (Verified)
- Forced tool use (`tool_choice: {"type": "any"}` or specific tool) (Verified)

When thinking is enabled:
- `top_p` can be set between 0.95 and 1.0 (Verified)
- Streaming is required when `max_tokens > 21,333` (Verified)

**Confidence:** Verified from official documentation.

## Recommended Budgets for ikigai

Based on research findings, here are recommended mappings for `/model NAME/THINKING`:

### Budget Calculation Formula

```
Calculation:
  none → type: "disabled"
  low  → min_budget + (1/3 × (max_budget - min_budget))
  med  → min_budget + (2/3 × (max_budget - min_budget))
  high → max_budget
```

### Per-Model Recommendations

**Claude Opus 4.5 (max: 64,000 tokens):**
```
none → type: "disabled"
low  → type: "enabled", budget_tokens: 22,000
med  → type: "enabled", budget_tokens: 43,000
high → type: "enabled", budget_tokens: 64,000
```

**Claude Sonnet 4.5 (max: 64,000 tokens):**
```
none → type: "disabled"
low  → type: "enabled", budget_tokens: 22,000
med  → type: "enabled", budget_tokens: 43,000
high → type: "enabled", budget_tokens: 64,000
```

**Claude 3.7 Sonnet (max: 128,000 tokens with beta header):**
```
none → type: "disabled"
low  → type: "enabled", budget_tokens: 43,000
med  → type: "enabled", budget_tokens: 85,000
high → type: "enabled", budget_tokens: 128,000
```
Note: Requires `anthropic-beta: output-128k-2025-02-19` header for 128K limit.

**Claude Haiku 4.5 (max: 64,000 tokens):**
```
none → type: "disabled"
low  → type: "enabled", budget_tokens: 22,000
med  → type: "enabled", budget_tokens: 43,000
high → type: "enabled", budget_tokens: 64,000
```

### Conservative Alternative (avoiding timeouts)

For budgets above 32K, Anthropic recommends batch processing to avoid networking issues. If using real-time API:

**Conservative mappings (all Claude 4 models):**
```
none → type: "disabled"
low  → type: "enabled", budget_tokens: 11,000
med  → type: "enabled", budget_tokens: 22,000
high → type: "enabled", budget_tokens: 32,000
```

### Implementation Notes

1. **Max Tokens Coordination:** Ensure `max_tokens` is set high enough to accommodate thinking budget + expected text output (e.g., `max_tokens = budget_tokens + 4096`)

2. **Context Window Validation:** Before sending request, verify: `prompt_tokens + max_tokens <= 200000`

3. **Model-Specific Headers:**
   - Claude 3.7 Sonnet with 128K: Add `anthropic-beta: output-128k-2025-02-19`
   - Opus 4.5 with effort: Add `anthropic-beta: effort-2025-11-24`
   - Interleaved thinking: Add `anthropic-beta: interleaved-thinking-2025-05-14`

4. **Tool Use Requirements:**
   - Always preserve and pass back thinking blocks from last assistant turn
   - Consider using interleaved thinking for complex multi-step tool workflows

5. **Streaming Requirement:**
   - Enable streaming when `max_tokens > 21,333`
   - Handle `thinking_delta` events in stream processor

## Missed Concepts / Important Discoveries

### 1. Thinking Encryption and Signatures

All thinking content is encrypted and returned with a `signature` field for verification. This signature:
- Is opaque and should not be parsed
- Is significantly longer in Claude 4 models
- Is platform-compatible (works across Anthropic API, Bedrock, Vertex AI)
- Is required for passing thinking blocks back to API

**Implication for ikigai:** Store thinking blocks with signatures intact. Do not attempt to parse or validate signatures.

### 2. Summarized Thinking Billing Trap

Claude 4 models return summarized thinking but bill for full thinking tokens. This creates a significant UX challenge:
- Users see ~500 tokens of thinking in response
- But are billed for ~10,000 tokens of actual reasoning
- Token counts in response do NOT match billing

**Implication for ikigai:** Display warnings about thinking token costs. Consider showing estimated vs. actual token usage.

### 3. Effort Parameter (Opus 4.5 Only)

The effort parameter is a powerful new control that:
- Works independently of thinking (can use without thinking enabled)
- Affects all token usage (thinking + text + tool calls)
- Provides 48-76% token savings on medium vs. high effort
- Is currently beta and Opus 4.5 exclusive

**Implication for ikigai:** Consider adding effort control as separate dimension for Opus 4.5. Could map thinking levels to effort levels for non-thinking models.

### 4. Thinking Block Preservation in Opus 4.5

Major change: Opus 4.5 preserves thinking blocks across turns by default (earlier models remove them). This:
- Improves cache hit rates
- Reduces token usage in long conversations
- Consumes more context window space
- Is automatic (no opt-in required)

**Implication for ikigai:** Be aware of context usage differences between Opus 4.5 and earlier models.

### 5. Interleaved Thinking Unlocks Advanced Workflows

Interleaved thinking (Claude 4 with beta header) allows thinking BETWEEN tool calls:
- Enables multi-step reasoning with intermediate analysis
- Budget can exceed max_tokens (applies to total across all thinking blocks in turn)
- Dramatically improves tool-based workflows

**Implication for ikigai:** Consider enabling by default for tool-heavy agents using Claude 4 models.

### 6. Strict Context Window Enforcement

Claude 3.7+ enforces `prompt_tokens + max_tokens <= context_window` as strict validation error (older models silently adjusted). This:
- Requires active token management
- Can cause request failures if not checked
- Affects thinking more than non-thinking (larger max_tokens needed)

**Implication for ikigai:** Implement pre-flight validation before sending requests with thinking enabled.

### 7. Cache Invalidation on Thinking Changes

Changing thinking parameters invalidates message cache but NOT system prompt cache. This creates opportunities for optimization:
- Keep system prompts stable across thinking level changes
- Cache large context in system prompt, not messages
- Use 1-hour cache for thinking sessions

**Implication for ikigai:** Design caching strategy around thinking parameter stability.

### 8. Streaming "Chunky" Behavior is Expected

Thinking content streams in larger chunks with delays between events (not token-by-token like text). Anthropic says this is expected and optimal behavior.

**Implication for ikigai:** Don't treat uneven streaming as a bug. May need special UI handling for thinking vs. text streaming.

## Open Questions

1. **Default Budget When Not Specified:** What happens if thinking is enabled without budget_tokens? (Likely: API error requiring budget_tokens)

2. **Exact Budget for "none" Level:** Should `none` map to `type: "disabled"` or `type: "enabled"` with `budget_tokens: 1024`? (Recommendation: disabled)

3. **Batch Processing Threshold:** At what exact token budget does batch processing become necessary? (Anthropic says "above 32K" but not exact threshold)

4. **Interleaved Thinking Token Counting:** How are tokens counted when budget exceeds max_tokens with interleaved thinking? (Needs testing)

5. **Thinking with Vision:** Any special considerations for thinking with image inputs? (Not documented)

6. **Thinking Token Pricing:** Are thinking tokens priced the same as regular output tokens? (Likely: yes, but not explicitly stated)

## Sources

### Official Anthropic Documentation
- [Building with extended thinking - Claude Docs](https://platform.claude.com/docs/en/build-with-claude/extended-thinking) (accessed 2025-12-14)
- [Effort - Claude Docs](https://platform.claude.com/docs/en/build-with-claude/effort) (accessed 2025-12-14)
- [Introducing Claude Opus 4.5](https://www.anthropic.com/news/claude-opus-4-5) (accessed 2025-12-14)
- [Introducing Claude Sonnet 4.5](https://www.anthropic.com/news/claude-sonnet-4-5) (accessed 2025-12-14)
- [Claude 3.7 Sonnet announcement](https://www.anthropic.com/news/claude-3-7-sonnet) (accessed 2025-12-14)

### AWS Documentation
- [Extended thinking - Amazon Bedrock](https://docs.aws.amazon.com/bedrock/latest/userguide/claude-messages-extended-thinking.html) (accessed 2025-12-14)
- [Claude 3.7 Sonnet - Amazon Bedrock](https://docs.aws.amazon.com/bedrock/latest/userguide/model-parameters-anthropic-claude-37.html) (accessed 2025-12-14)

### Community Resources
- [Claude 3.7 Sonnet, extended thinking and long output - Simon Willison](https://simonwillison.net/2025/Feb/25/llm-anthropic-014/) (accessed 2025-12-14)
- [Promptfoo Anthropic Provider Documentation](https://www.promptfoo.dev/docs/providers/anthropic/) (accessed 2025-12-14)

### Known Issues
- [Redacted thinking blocks dropped in streaming - LiteLLM Issue #10328](https://github.com/BerriAI/litellm/issues/10328) (accessed 2025-12-14)
- [Anthropic requires thinking blocks for tool support - Continue Issue #4470](https://github.com/continuedev/continue/issues/4470) (accessed 2025-12-14)

---

**Research Confidence:** High - All key findings verified against official Anthropic documentation and announcements. Budget limits, streaming behavior, tool use requirements, and caching interactions are well-documented. Some edge cases and exact thresholds remain untested but documented behavior is clear.

**Next Steps for ikigai:**
1. Implement thinking level mapping using recommended budgets
2. Add pre-flight validation for context window constraints
3. Design thinking block preservation logic for tool use
4. Consider adding effort parameter support for Opus 4.5
5. Test interleaved thinking with complex tool workflows
6. Implement proper streaming event handling for thinking_delta
7. Add user warnings about summarized vs. billed token counts
