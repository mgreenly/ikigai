# Sliding Context Window — Design Document

**Feature**: rel-13 Dynamic Sliding Context Window
**Status**: In development
**Related**: `project/README.md` (roadmap), goals #234 and #235

---

## Overview

Ikigai conversations run indefinitely — across hours, days, or weeks. The full history lives in PostgreSQL and survives restarts. But LLM providers have finite context windows, and sending the entire history on every request is wasteful and eventually impossible.

The sliding context window solves this by maintaining a token budget for the message history sent to the LLM. After each turn, old turns are pruned from the front of the in-memory window until the estimated token count falls within budget. The database retains the full history — pruning is in-memory only and is fully reversible.

A horizontal rule appears in the scrollback buffer to mark the visible cutoff, giving the user a clear signal about what the LLM currently sees.

---

## Phasing

### Phase 1 — rel-13 (this release)

- Implement sliding window with **bytes-based token estimation** (~4 bytes/token)
- Pruning triggers at turn completion (agent transitions to IDLE)
- Default budget: 100K tokens
- Configuration via environment variable and config struct

**Summaries deferred**: The original rel-13 plan included automatic summaries of pruned history. Summaries are deferred to a future release. rel-13 delivers pruning only.

### Phase 2 — Future

- Swap in real token counting via provider pre-flight APIs:
  - Anthropic: `messages.countTokens` (free)
  - OpenAI: token counting endpoint
  - Google: `countTokens` (free, 3000 RPM)
- No changes to calling code — the token counting module's dispatch pattern absorbs the new implementations

---

## Turn Definition

A **turn** is the unit of pruning. It is defined as:

- Starts with a user message
- Ends when the agent transitions to `IK_AGENT_STATE_IDLE` (defined in `apps/ikigai/agent.h`)

A single turn may contain multiple LLM round-trips: the user sends a message, the LLM responds with tool calls, tools execute, results go back to the LLM, and so on until the LLM produces a final response and control returns to the user.

**Pruning always removes whole turns.** Removing individual messages within a turn would leave the LLM with orphaned tool calls, partial responses, or mismatched tool result pairs — all of which confuse the model. Whole-turn pruning is conservative and safe.

There is no `ik_turn_t` struct. Turn boundaries are identified at runtime by detecting user message → IDLE transitions.

---

## Token Counting Module

### Design

A standalone module that takes:
- `provider` (string): e.g., `"anthropic"`, `"openai"`, `"google"`
- `model` (string): e.g., `"claude-sonnet-4-6"`, `"gpt-5-mini"`
- A byte buffer: the serialized request body (the same bytes sent to the provider API)

Returns: estimated token count.

### Dispatch Pattern

The module switches on provider/model and falls through to a bytes-based default:

```
count_tokens(provider, model, buf, len):
  if provider == "anthropic":
    return anthropic_estimate(model, buf, len)
  if provider == "openai":
    return openai_estimate(model, buf, len)
  if provider == "google":
    return google_estimate(model, buf, len)
  return bytes_estimate(buf, len)  // ~4 bytes/token fallback
```

This structure means adding a real tokenizer (Phase 2) requires only adding an implementation branch — no changes to the pruning logic or any calling code.

### What Gets Counted

The module counts the **serialized request body** — the same JSON sent to the provider API. This naturally includes everything that the provider counts as input tokens:

- System prompt / system instruction
- Tool definitions (schemas, descriptions, parameters)
- All messages: user, assistant, tool_call, tool_result
- Role labels and formatting overhead

Counting the serialized request ensures the estimate matches what the provider actually receives.

### Bytes-to-Tokens Ratio

`~4 bytes/token` is consistent across all three providers. The ratio slightly overcounts content but undercounts formatting overhead — these effects roughly cancel out.

The estimate is **conservative**: it prunes slightly early rather than slightly late, which is the correct direction for a budget ceiling. Pruning too early wastes a small amount of context; pruning too late risks exceeding the provider's limit.

---

## Provider-Specific Research

### Anthropic

- **Tokenizer**: Proprietary BPE, ~65K vocabulary
- **Overhead**: Injects a hidden 313–346 token system prompt when tools are provided (undocumented but observed)
- **Phase 2 API**: `messages.countTokens` — free, exact pre-flight counts

### OpenAI

- **Tokenizer**: tiktoken with `o200k_base` encoding for newer models (200K vocabulary)
- **Overhead**: ChatML format adds 3 tokens per message + 3 tokens for reply priming + 9 tokens when tools are defined. Tool schemas are internally converted to TypeScript type declarations before tokenizing.
- **Phase 2 API**: Token counting endpoint available

### Google

- **Tokenizer**: SentencePiece BPE, ~256K vocabulary
- **Overhead**: Formatting overhead not explicitly documented
- **Phase 2 API**: `countTokens` — free, 3000 RPM limit

### Caching

All three providers have prompt caching. Caching does **not** change what's counted as input tokens — it only affects the billing rate. For budget purposes, caching is irrelevant.

| Provider | Cache Discount |
|----------|---------------|
| Anthropic | 90% on cache hits |
| OpenAI | 50%, automatic |
| Google | 75% |

---

## Pruning Mechanics

Pruning runs after every turn completes. The algorithm:

1. **Trigger**: Agent transitions to `IK_AGENT_STATE_IDLE`
2. **Serialize**: Build the hypothetical next request (system prompt + tools + all messages currently in window)
3. **Count**: Pass serialized bytes to the token counting module
4. **Check**: If estimated tokens ≤ budget, done
5. **Prune**: Remove the oldest whole turn from the in-memory window
6. **Repeat**: Go to step 2
7. **Safety floor**: Always keep the most recent turn, even if it alone exceeds the budget. Never leave the window empty.
8. **Update HR**: Move the horizontal rule position in the scrollback buffer

Pruning is **in-memory only**. The `agent->messages[]` array is updated; the database is not touched. The full history remains queryable for replay, summaries (future), and audit.

---

## Horizontal Rule

The horizontal rule (HR) is a visual marker in the scrollback buffer that shows the user where the LLM's context begins.

**Properties**:
- Purely visual — not persisted to the database
- Not sent to the LLM
- Not an event in the `messages` table
- The cutoff position is deterministic from budget + messages, so the HR can be reconstructed without storage
- Follows project convention: empty line before and after

**Placement**: The HR appears after the last LLM response in the turn that triggered pruning. As older turns are pruned in subsequent turns, the HR moves forward.

**Implementation**: Inserted into the `ik_scrollback_t` buffer (`apps/ikigai/scrollback.h`) as a regular line using `ik_scrollback_append_line()`. The scrollback has no special HR concept — it's just a separator line rendered visually.

---

## Configuration

### New Field

A new field is added to `ik_config_t` (`apps/ikigai/config.h`):

```c
typedef struct {
    // ... existing fields ...
    int64_t sliding_context_tokens;  // Token budget for sliding window (0 = disabled)
} ik_config_t;
```

### Default Value

Defined in `apps/ikigai/config_defaults.h`:

```c
#define IK_DEFAULT_SLIDING_CONTEXT_TOKENS 100000
```

100K tokens is a reasonable default that fits comfortably within all three providers' context windows while leaving room for output.

### Environment Override

```
IKIGAI_SLIDING_CONTEXT_TOKENS=200000
```

### Sub-Agent Inheritance

Sub-agents inherit the sliding context window size from their parent at fork time. The inheritance happens in `ik_agent_copy_conversation()` (`apps/ikigai/agent.h`), which deep-copies the parent's message array. Config inheritance is handled separately at fork time in `apps/ikigai/commands_fork.c`.

After forking, each sub-agent's sliding window operates independently on its own message array. Sub-agent conversations are independent after the fork point.

### Future: Per-Agent Configuration

rel-15 (Per-Agent Configuration) will allow runtime adjustment of the sliding context window budget per agent via `/config set SLIDING_CONTEXT_TOKENS=200000`. The current implementation uses a global config value inherited at fork time, which is sufficient for rel-13.

---

## Architecture Decisions

### Prune Whole Turns, Not Individual Messages

Removing a single message from within a turn leaves the LLM with incomplete context: a tool call without its result, an assistant response without the user message that prompted it, or a tool result without the corresponding call. All of these confuse the model and can cause errors. Whole-turn pruning eliminates this class of problem.

### Budget is a Ceiling, Not a Target

The actual context size fluctuates below the budget. After pruning a turn, the window may be well under budget. This is expected and correct. The budget is a maximum, not a goal.

### Count the Serialized Request, Not Raw Messages

Token counting operates on the bytes that will actually be sent to the provider — the serialized JSON request. This is the only way to accurately capture:
- Provider-specific formatting overhead
- Tool schema serialization
- System prompt encoding differences

Counting raw message strings would miss all of this.

### The Token Counting Module Takes Provider and Model

The module accepts both `provider` and `model` strings. `provider` selects the general encoding approach; `model` allows for model-specific variations within a provider (e.g., different vocabulary sizes). For Phase 1, both parameters are used only for future dispatch — the bytes-based fallback ignores them.

### The HR is Not Persisted

The cutoff position is deterministic: given the budget and the current message array, any reader can compute where pruning would have occurred. Storing the HR position would create a second source of truth that could drift. The HR is reconstructed on each render.

### Pruning is In-Memory Only

The database is the source of truth for conversation history. Pruning modifies only the in-memory `agent->messages[]` array. This preserves:
- **Replay**: `ik_agent_replay_history()` (`apps/ikigai/db/agent_replay.h`) can reconstruct any agent's full history from the database on restart
- **Future summaries**: Pruned messages remain available for summary generation
- **Audit**: Complete conversation history is always available

---

## Existing Message History Architecture

Understanding the existing architecture is prerequisite to implementing pruning.

### Storage

Messages are stored in the PostgreSQL `messages` table as events with a `kind` discriminator. Two levels of message representation exist:

**`ik_msg_t`** (`apps/ikigai/msg.h`): Database/serialization level
```c
typedef struct {
    int64_t id;       // DB row ID (0 if not from DB)
    char *kind;       // Message kind discriminator
    char *content;    // Message text content or human-readable summary
    char *data_json;  // Structured data for tool messages
    bool interrupted; // True if message was interrupted/cancelled
} ik_msg_t;
```

**`ik_message_t`** (`apps/ikigai/providers/provider.h`): Provider/request level
```c
struct ik_message {
    ik_role_t role;
    ik_content_block_t *content_blocks;
    size_t content_count;
    char *provider_metadata;
    bool interrupted;
};
```

### Message Kinds

**Conversation kinds** (sent to the LLM, `ik_msg_is_conversation_kind()` returns `true`):

| Kind | Description |
|------|-------------|
| `system` | System prompt message |
| `user` | User input |
| `assistant` | LLM response |
| `tool_call` | Tool invocation request |
| `tool_result` | Tool execution result |
| `tool` | Generic tool message |

**Metadata kinds** (not sent to the LLM, `ik_msg_is_conversation_kind()` returns `false`):

| Kind | Description |
|------|-------------|
| `clear` | Clear event (context wipe) |
| `mark` | Named checkpoint |
| `rewind` | Rewind navigation event |
| `agent_killed` | Agent termination event |
| `command` | Slash command event |
| `fork` | Fork event |

### In-Memory Conversation

Each agent maintains:
```c
ik_message_t **messages;  // Array of message pointers (apps/ikigai/agent.h)
size_t message_count;
size_t message_capacity;
```

Request building (`ik_request_build_from_conversation()`) iterates this array, filtering to conversation kinds only.

### Delta-Based Storage

Child agents store only post-fork messages. On startup, `ik_agent_replay_history()` (`apps/ikigai/db/agent_replay.h`) implements a "walk backwards, play forwards" algorithm:

1. Start at the leaf agent
2. Walk up the ancestor chain
3. For each agent, find the most recent `clear` event
4. Collect message ranges, then reverse to get chronological order

This allows the full conversation context to be reconstructed from the ancestor chain without denormalizing into each child.

### Turns Are Implicit

There is no `ik_turn_t` struct or turn table. Turn boundaries are identified dynamically by the transition sequence:
- Turn start: user message received (agent state transitions from `IK_AGENT_STATE_IDLE` to `IK_AGENT_STATE_WAITING_FOR_LLM`)
- Turn end: agent transitions back to `IK_AGENT_STATE_IDLE`

The pruning implementation must track these boundaries to identify whole-turn boundaries in `agent->messages[]`.

---

## Implementation Touchpoints

| File | Role |
|------|------|
| `apps/ikigai/agent.h` | Agent state machine, `messages[]` array, `ik_agent_copy_conversation()` |
| `apps/ikigai/config.h` | Add `sliding_context_tokens` field |
| `apps/ikigai/config_defaults.h` | Add `IK_DEFAULT_SLIDING_CONTEXT_TOKENS` |
| `apps/ikigai/msg.h` | `ik_msg_is_conversation_kind()`, `ik_msg_t` |
| `apps/ikigai/scrollback.h` | `ik_scrollback_append_line()` for HR insertion |
| `apps/ikigai/db/agent_replay.h` | Replay algorithm (read-only for this feature) |
| `apps/ikigai/providers/provider.h` | `ik_request` struct, `ik_message_t` |
| `apps/ikigai/repl_actions_llm.c` | Conversation loop — pruning triggers here |
| `apps/ikigai/repl_callbacks.c` | Completion callbacks — IDLE transition fires here |
| `apps/ikigai/commands_fork.c` | Fork and config inheritance |

New files to create:
- `apps/ikigai/token_count.h` / `apps/ikigai/token_count.c` — Token counting module
- `apps/ikigai/sliding_window.h` / `apps/ikigai/sliding_window.c` — Pruning logic
- `tests/unit/token_count_test.c` — Unit tests for token counting module
- `tests/unit/sliding_window_test.c` — Unit tests for pruning logic

---

## Related Documents

- `project/README.md` — Roadmap, rel-13 entry
- `project/context-management-v2.md` — Context commands (`/clear`, `/mark`, `/fork`)
- `project/tokens/usage-strategy.md` — Token display strategy, hybrid exact/estimate approach
- `project/tokens/anthropic-tokens.md` — Anthropic tokenizer specifics
- `project/tokens/tokenizer-library-design.md` — Tokenizer library architecture
- `CLAUDE.md` — Project overview describing the sliding window vision
