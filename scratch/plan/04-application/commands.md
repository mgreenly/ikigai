# Command Behavior

## Overview

This document specifies the behavior of commands that interact with the multi-provider system: `/model` and `/fork`.

## /model Command

### Syntax

```
/model MODEL[/THINKING]
```

**Components:**
- `MODEL` - Model identifier (e.g., `claude-sonnet-4-5`, `gpt-4o`, `gemini-2.5-pro`)
- `THINKING` - Optional thinking level: `none`, `low`, `med`, `high`

**Examples:**
```
/model claude-sonnet-4-5        # Use default thinking for this model
/model claude-sonnet-4-5/med    # Explicit medium thinking
/model gpt-4o/none              # Disable thinking
/model gemini-2.5-pro/high      # Maximum thinking budget
```

### Provider Inference

Provider is inferred from model prefix:

| Prefix | Provider |
|--------|----------|
| `claude-` | anthropic |
| `gpt-`, `o1`, `o3`, `o4` | openai |
| `gemini-` | google |
| `grok-` | xai |
| `llama-` | meta |

**Inference Logic:**

The provider inference function examines the model string prefix using string comparison. It checks for the following patterns in order:
- Models starting with "claude-" map to anthropic provider
- Models starting with "gpt-" or "o1", "o3", "o4" map to openai provider
- Models starting with "gemini-" map to google provider
- Models starting with "grok-" map to xai provider
- Models starting with "llama-" map to meta provider
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

### User Feedback

See [thinking-abstraction.md](thinking-abstraction.md#user-feedback) for feedback format.

### Tab Completion

Tab completion should provide the following model suggestions:

**Anthropic models:**
- claude-sonnet-4-5
- claude-sonnet-4-5/none
- claude-sonnet-4-5/low
- claude-sonnet-4-5/med
- claude-sonnet-4-5/high
- claude-opus-4-5
- claude-haiku-4-5

**OpenAI models:**
- gpt-4o
- gpt-4o/none
- o3
- o3/low
- o3/med
- o3/high
- o3-mini

**Google models:**
- gemini-2.5-pro
- gemini-2.5-pro/none
- gemini-2.5-pro/low
- gemini-2.5-pro/med
- gemini-2.5-pro/high
- gemini-2.5-flash

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
/fork                                      # Inherit parent's model
/fork "Investigate the bug"                # Inherit + assign task
/fork --model o3-mini/high                 # Override model
/fork --model o3-mini/high "Solve this"    # Override + task
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

## Error Handling

### Unknown Model

```
> /model unknown-model

Unknown model: unknown-model

Supported models:
  Anthropic: claude-sonnet-4-5, claude-opus-4-5, claude-haiku-4-5
  OpenAI:    gpt-4o, o3, o3-mini, o4-mini
  Google:    gemini-2.5-pro, gemini-2.5-flash
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

## References

- [03-provider-types.md](../03-provider-types.md) - Thinking level mapping and user feedback
- [configuration.md](configuration.md) - Credentials loading
- [database-schema.md](database-schema.md) - Agent table schema
