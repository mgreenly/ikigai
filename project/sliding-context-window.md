# Dynamic Sliding Context Window

## Overview

Session-based LLMs have finite context windows (typically 200k tokens). As conversations grow, history eventually fills the available context. Tools like Claude Code handle this reactively — compacting memory only when the limit is hit.

The Dynamic Sliding Context Window takes a proactive approach: set a fixed budget for conversation history and continuously clip the oldest turns to stay within it. Plan for steady-state operation from the beginning rather than treating context exhaustion as an emergency.

## Design Principles

- **Budget from the start** — never hit the wall, never scramble
- **Stay well under model max** — models degrade at high token usage; target ~150k total even when 200k is available
- **User controls everything** — history size, summary size, and system prompt (via loaded skills) are all user-managed
- **Database is permanent** — clipping affects what's sent to the model, never what's stored
- **Model-independent** — works across all providers without depending on provider-specific tokenization

## Context Budget Model

The user manages three independent budgets:

| Budget | Controlled by | Description |
|--------|--------------|-------------|
| System prompt | User (loaded skills/pinned files) | User self-manages by choosing what to load |
| Live history | User (configurable cap) | Recent conversation turns sent to the model |
| Summary | User (configurable cap) | Compressed history of clipped turns |

There is no enforced total budget. The user understands that total = system + history + summary and keeps it comfortably under the model's maximum. Ikigai enforces the history and summary caps; the user manages system prompt size through their own choices.

## Token Counting

### The Problem

Ikigai supports multiple AI providers (Anthropic, OpenAI, Google). Each uses a different tokenizer. Users can switch models mid-conversation, invalidating any provider-specific token counts.

### The Solution

A **token counting module** with a pluggable backend:

- **Interface**: given text, return a token count and whether it's exact or estimated
- **Estimator backend** (day one): characters / 4 — model-independent, always available
- **Exact backends** (future): real tokenizers for models with known algorithms (e.g. tiktoken for OpenAI)

The module looks up the algorithm for the current model. If one exists, it returns an exact count. Otherwise, it falls back to the character estimator.

### Display

Users always see token counts. Exact values display as `77k`. Estimated values display as `~77k`. The implementation detail is hidden except for that tilde.

Budgets are configured in tokens (the user's mental model). The counting layer handles conversion internally.

## Turns

A **turn** is the atomic unit of the sliding window. It is one complete exchange:

- User prompt
- Tool calls and tool results (zero or more)
- Assistant response

Turns drop out of context as a single chunk. This gives humans an understandable unit for reasoning about what the model can see.

Individual messages within a turn are never split — either the entire turn is in context or it isn't.

## Phase 1: Sliding Window (no summary)

Phase 1 implements history clipping without summarization. Clipped turns simply leave context with no replacement.

### Components

1. **Token counting module** — character-based estimator as the only backend
2. **`DEFAULT_HISTORY_TOKEN_LIMIT`** in `config_default.h` — set to 100,000
3. **Turn grouping** — group individual messages (user, tool_call, tool_result, assistant) into logical turns
4. **Per-turn token tracking** — estimate each turn's token size when it completes
5. **Window boundary** — an index or message ID on the agent marking where live context starts
6. **Clipping logic** — after each completed turn, if total live history exceeds the cap, advance the boundary past the oldest turn(s) until under budget
7. **Request builder** — `ik_request_build_from_conversation()` starts from the window boundary instead of message 0
8. **Context divider** — a horizontal line in the chat UI separating clipped history from live context

### Clipping Flow

```
Turn completes (assistant response received and stored)
    |
    v
Estimate token count of new turn
    |
    v
Sum all live turns (boundary to current)
    |
    v
Over history cap? --no--> Done
    |
    yes
    v
Advance boundary past oldest live turn
    |
    v
Repeat until under cap
```

### In-Memory Model

`agent->messages` stays complete — all messages from the entire conversation, never mutated by clipping. A **window boundary** tracks where live context begins. The request builder copies only messages from the boundary forward.

This means:
- The database keeps everything (permanent record)
- Memory keeps everything (needed for UI display)
- Only the API request is filtered by the window

### UI: Context Divider

A horizontal divider in the chat scrollback separates clipped history from live context. Everything above the divider is "the model can't see this." Everything below is live.

The divider moves downward as old turns get clipped. If the user is scrolled back reading old messages, they see it move. No animation or notification — it's furniture, not an event.

### Integration Point

The primary code change is in `ik_request_build_from_conversation()` (`providers/request_tools.c`). Currently it copies all messages:

```c
for (i = 0; i < agent->message_count; i++) {
    ik_request_add_message_direct(req, agent->messages[i]);
}
```

With the sliding window, it starts from the boundary:

```c
for (i = agent->context_window_start; i < agent->message_count; i++) {
    ik_request_add_message_direct(req, agent->messages[i]);
}
```

## Phase 2: Summary (future)

Turns that fall out of the live history window feed into a summary. The summary occupies its own budget and provides the model with compressed context about earlier conversation. Design details deferred.

## Configuration

Phase 1: a single compile-time default in `config_default.h`. No runtime configuration.

Future (rel-15 per-agent config): per-agent history and summary caps, adjustable at runtime.
