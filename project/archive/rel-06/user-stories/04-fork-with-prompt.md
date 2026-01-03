# Fork With Prompt

## Description

User runs `/fork "prompt"` to create a child agent with a task. The child receives the prompt as its first new message and begins working immediately.

## Transcript

```text
───────────────────────── agent [abc123...] ─────────────────────────
> /fork "Research OAuth 2.0 best practices"

───────────────────────── agent [xyz789...] ─────────────────────────
Forked from abc123...

Research OAuth 2.0 best practices

I'll research OAuth 2.0 best practices for you...

[Agent begins working on the task]
```

## Walkthrough

1. User types `/fork "Research OAuth 2.0 best practices"`
2. REPL parses command, extracts prompt string
3. Handler waits for sync barrier
4. Handler creates child agent (same as basic fork)
5. Handler copies parent history to child
6. Handler appends prompt as user message to child's history
7. Handler switches to child agent
8. Handler triggers LLM call with the prompt
9. Child agent begins responding to the prompt
10. Parent continues to exist, can be switched back to

## Reference

```json
{
  "command": "/fork",
  "arguments": {
    "prompt": "optional quoted string"
  },
  "with_prompt": {
    "appends_user_message": true,
    "triggers_llm_call": true,
    "child_starts_working": true
  }
}
```
