# Task: Update /model Command

**Phase:** 3 - Commands
**Depends on:** 07-agent-provider-fields

## Objective

Update `/model` command to support `MODEL/THINKING` syntax and provider inference.

## Deliverables

1. Update `src/commands_basic.c` `/model` handler:
   - Parse `MODEL/THINKING` syntax
   - Extract model name and thinking level
   - Infer provider from model prefix
   - Update agent's provider/model/thinking_level

2. Add feedback display:
   - Show provider name and model
   - Show thinking level with concrete value
   - Warn if model doesn't support thinking

3. Update completion provider:
   - Update `src/completion.c` `provide_model_args()`
   - List models from all providers
   - Include thinking level suffixes

## Reference

- `scratch/README.md` - Q6 Extended Thinking section
- `scratch/plan/thinking-abstraction.md` - User Feedback section

## Examples

```
/model claude-sonnet-4-5/med
✓ Switched to Anthropic claude-sonnet-4-5
  Thinking: medium (43,008 tokens)

/model gpt-4o/none
✓ Switched to OpenAI gpt-4o
  Thinking: disabled
```

## Verification

- `/model` parses MODEL/THINKING syntax
- Provider inferred correctly
- Agent state updated
- Completion shows all models
