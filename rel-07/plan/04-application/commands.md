# Command Behavior

## Overview

This document specifies the behavior of commands that interact with the multi-provider system: `/model` and `/fork`.

## /model Command

### Syntax

```
/model MODEL[/THINKING]
```

**Components:**
- `MODEL` - Model identifier (e.g., `claude-sonnet-4-5`, `gpt-5-mini`, `gemini-3.0-flash`)
- `THINKING` - Optional thinking level: `none`, `low`, `med`, `high`

**Examples:**
```
/model claude-sonnet-4-5        # Use default thinking for this model
/model claude-sonnet-4-5/med    # Explicit medium thinking
/model gpt-5-mini/none          # Disable thinking
/model gemini-3.0-flash/high    # Maximum thinking level
```

### Provider Inference

Provider is inferred from model prefix:

| Prefix | Provider |
|--------|----------|
| `claude-` | anthropic |
| `gpt-` | openai |
| `gemini-` | google |

**Inference Logic:**

The provider inference function examines the model string prefix using string comparison. It checks for the following patterns in order:
- Models starting with "claude-" map to anthropic provider
- Models starting with "gpt-" map to openai provider
- Models starting with "gemini-" map to google provider
- If no prefix matches, the model is unknown and should return an error

### Parsing Logic

The command argument is parsed to extract the model name and optional thinking level:

1. Search for a slash separator in the argument string
2. If no slash is found:
   - The entire argument is the model name
   - Thinking level is unspecified (will use provider default)
3. If a slash is found:
   - Extract the model name from before the slash
   - Extract the thinking level string from after the slash
   - Validate the thinking level against allowed values: "none", "low", "med", "high"
   - If invalid, return an error with the invalid value and list of valid options

### Agent State Update

When `/model` is executed, the following steps occur:

1. Parse the command argument to extract model name and thinking level
2. Infer the provider from the model name using prefix matching
3. If provider cannot be inferred, return an error indicating unknown model
4. Update the agent's provider, model, and thinking level fields
5. Invalidate any cached provider instance (will be recreated on next use)
6. Persist the updated agent state to the database

### Provider Lifecycle on Model Switch

When the model changes, provider memory is managed as follows:

1. **Ownership:** Agent owns provider via talloc (`agent->provider` is child of agent context)
2. **Cleanup:** Before updating model fields, free existing provider:
   ```c
   if (agent->provider) {
       talloc_free(agent->provider);
       agent->provider = NULL;
   }
   ```
3. **Lazy recreation:** New provider created by `ik_provider_get_or_create()` on next LLM request
4. **In-flight safety:** Model switch is rejected if requests are active (see "Model Switch During Active Request")

This ensures no memory leaks and no dangling pointers during provider transitions.

### User Feedback

See [03-provider-types.md](../03-provider-types.md#user-feedback) for feedback format.

### Tab Completion

Tab completion should provide the following model suggestions:

**Anthropic models:**
- claude-haiku-4-5
- claude-sonnet-4-5 (default)
- claude-sonnet-4-5/none
- claude-sonnet-4-5/low
- claude-sonnet-4-5/med
- claude-sonnet-4-5/high
- claude-opus-4-5

**OpenAI models:**
- gpt-5-nano
- gpt-5-mini (default)
- gpt-5-mini/none
- gpt-5-mini/low
- gpt-5-mini/med
- gpt-5-mini/high
- gpt-5

**Google models:**
- gemini-2.5-flash-lite
- gemini-3.0-flash (default)
- gemini-3.0-flash/none
- gemini-3.0-flash/low
- gemini-3.0-flash/med
- gemini-3.0-flash/high
- gemini-3.0-pro

### Model Switch During Active Request

When `/model` is invoked while a streaming response is in progress:

1. **Reject the switch** - Display error: "Cannot switch model while response is streaming. Wait for completion or press Ctrl+C to cancel."

2. **Rationale:** Allowing mid-stream switching would create confusing UX:
   - Partial response from old model visible in scrollback
   - Unclear which model generated which content
   - Potential for response corruption if callbacks interleave

3. **Detection:** Check `agent->curl_still_running > 0` before processing /model command

4. **User options:**
   - Wait for current response to complete, then switch
   - Press Ctrl+C to cancel current request, then switch

**Note:** This applies only to the current agent. Other agents in the tree can have model switches applied independently.

## /fork Command

### Syntax

```
/fork [--model MODEL/THINKING] ["prompt"]
```

**Components:**
- `--model MODEL/THINKING` - Optional model override for child agent
- `"prompt"` - Optional initial task for child agent

**Examples:**
```
/fork                                         # Inherit parent's model
/fork "Investigate the bug"                   # Inherit + assign task
/fork --model gpt-5/high                      # Override model
/fork --model gpt-5/high "Solve this"         # Override + task
```

### Inheritance Rules

| Parent Setting | No Override | With Override |
|----------------|-------------|---------------|
| provider | Inherited | From model inference |
| model | Inherited | From `--model` value |
| thinking_level | Inherited | From `--model` value |

### Argument Parsing

The `/fork` command accepts optional arguments in a specific order:

1. Check if the argument starts with the `--model` flag
2. If `--model` is present:
   - Extract the model argument that follows the flag
   - Find the end of the model argument (terminated by space or quote character)
   - Parse the model argument using the same logic as `/model` command
   - Advance past the model argument and skip any whitespace
3. Check if a quoted prompt follows:
   - Look for an opening quote character
   - Find the matching closing quote
   - If no closing quote is found, return an error for unclosed quote
   - Extract the text between quotes as the prompt
4. If no arguments are present, all fields remain unset (NULL/default)

The parsed arguments include:
- model: NULL if not specified (inherit from parent)
- thinking level: only used if model is specified
- prompt: NULL if not provided

### Child Agent Creation

When a child agent is created from `/fork`, the following steps occur:

1. Allocate a new agent context structure
2. Generate a new UUID for the child agent
3. Store the parent's UUID as the parent reference
4. Determine model settings:
   - If `--model` was provided, infer provider from model and use specified thinking level
   - If no override, copy provider, model, and thinking level from parent
5. Record the fork point by capturing the parent's last message ID
6. Insert the new agent record into the database
7. If a prompt was provided, insert an initial user message for the child agent
8. Return the newly created child agent context

### Database Storage

Child agent record requires the following fields:
- uuid: newly generated unique identifier
- parent_uuid: reference to parent agent
- fork_message_id: message ID where fork occurred
- provider: inherited or overridden provider name
- model: inherited or overridden model name
- thinking_level: inherited or overridden thinking level
- status: set to 'running'

### Provider Instance Ownership

Each agent has its **own** provider instance - they are never shared:

1. **Inheritance:** Forked agent copies parent's `provider`, `model`, and `thinking_level` fields
2. **No instance sharing:** Parent's `provider` pointer is NOT copied - child starts with `provider = NULL`
3. **Lazy creation:** Child's provider instance created on first LLM request via `ik_provider_get_or_create()`
4. **Independent state:** Each agent maintains independent HTTP connection state, rate limit tracking, etc.

**Rationale:**
- Sharing provider instances would create complex synchronization issues
- Each agent may have concurrent requests that need independent curl_multi handles
- Memory cleanup is simpler with per-agent ownership (talloc hierarchy)

**Memory model:**
```
parent_agent
├── provider (ik_provider_t*)  ← parent's instance
└── ...

child_agent
├── provider = NULL            ← created lazily on first request
├── provider = "anthropic" (copied from parent)
├── model = "claude-sonnet-4-5" (copied from parent)
└── ...
```

## Error Handling

### Unknown Model

```
> /model unknown-model

Unknown model: unknown-model

Supported models:
  Anthropic: claude-haiku-4-5, claude-sonnet-4-5, claude-opus-4-5
  OpenAI:    gpt-5-nano, gpt-5-mini, gpt-5
  Google:    gemini-2.5-flash-lite, gemini-3.0-flash, gemini-3.0-pro
```

### Invalid Thinking Level

```
> /model claude-sonnet-4-5/maximum

Invalid thinking level: maximum
Valid levels: none, low, med, high
```

### Missing Credentials

```
> /model claude-sonnet-4-5/med
Hello!

No credentials for anthropic. Set ANTHROPIC_API_KEY or add to credentials.json

Get your API key at: https://console.anthropic.com/settings/keys
```

### No Model Configured Error

When user sends a message but no model has been configured for the agent:

**Detection:** `agent->model == NULL`

**Error Message:**
```
No model configured. Use /model to select one.

Examples:
  /model claude-sonnet-4-5      (Anthropic)
  /model gpt-5-mini             (OpenAI)
  /model gemini-3.0-flash       (Google)

Run /model with Tab for available options.
```

**Behavior:**
1. Display error in scrollback (not a crash/panic)
2. Return to input prompt - user can then run /model
3. Do NOT send anything to any provider
4. Do NOT save message to database

**When this occurs:**
- Fresh agent with no prior /model command
- After database truncation (migration 005)
- If config.json has no default_provider set

## References

- [03-provider-types.md](../03-provider-types.md) - Thinking level mapping and user feedback
- [configuration.md](configuration.md) - Credentials loading
- [database-schema.md](database-schema.md) - Agent table schema
