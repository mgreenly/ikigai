# Bash Command

## Description

User asks to run a shell command. The model uses the bash tool to execute it and reports the output.

## Transcript

```text
> What's the current git branch?

You're on the `main` branch.
```

## Walkthrough

1. User types "What's the current git branch?" and presses Enter

2. Client builds chat completion request (see [Request A](#request-a))

3. Model responds with bash tool call (see [Response A](#response-a))

4. Tool call displayed in scrollback

5. Client executes bash with command `git branch --show-current`

6. Tool returns command output (see [Tool Result A](#tool-result-a))

7. Tool result displayed in scrollback

8. Client builds follow-up request with tool result (see [Request B](#request-b))

9. Model reports the branch name (see [Response B](#response-b))

10. Response streams to scrollback

11. All messages persisted to database

---

## Reference

### Request A

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "What's the current git branch?"}
  ],
  "tools": ["...standard tools array..."],
  "tool_choice": "auto",
  "stream": true
}
```

### Response A

```json
{
  "choices": [{
    "delta": {
      "role": "assistant",
      "tool_calls": [{
        "index": 0,
        "id": "call_bash1",
        "type": "function",
        "function": {
          "name": "bash",
          "arguments": "{\"command\": \"git branch --show-current\"}"
        }
      }]
    },
    "finish_reason": "tool_calls"
  }]
}
```

### Tool Result A

```json
{"success": true, "data": {"output": "main", "exit_code": 0}}
```

### Request B

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "What's the current git branch?"},
    {"role": "assistant", "tool_calls": [{"id": "call_bash1", "type": "function", "function": {"name": "bash", "arguments": "{\"command\": \"git branch --show-current\"}"}}]},
    {"role": "tool", "tool_call_id": "call_bash1", "content": "{\"success\": true, \"data\": {\"output\": \"main\", \"exit_code\": 0}}"}
  ],
  "tools": ["...standard tools array..."],
  "tool_choice": "auto",
  "stream": true
}
```

### Response B

```json
{
  "choices": [{
    "delta": {
      "role": "assistant",
      "content": "You're on the `main` branch."
    },
    "finish_reason": "stop"
  }]
}
```
