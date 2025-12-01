# Tool Loop Limit Reached

## Description

User asks for a task that causes the model to make repeated tool calls. After reaching the configured limit (3 in this example), the client stops the loop and informs both the model and user.

## Transcript

```text
> Keep searching for errors in every file

I was searching through files but reached the tool call limit (3 calls). I found errors in main.c and config.c. To continue searching, you can ask me to resume or increase the limit.
```

## Walkthrough

1. User types request and presses Enter

2. Client builds chat completion request (see [Request A](#request-a))

3. Model responds with grep tool call (see [Response A](#response-a))

4. Client executes grep (turn 1 of 3), returns results

5. Client builds follow-up request

6. Model responds with another grep tool call (see [Response B](#response-b))

7. Client executes grep (turn 2 of 3), returns results

8. Client builds follow-up request

9. Model responds with another grep tool call (see [Response C](#response-c))

10. Client executes grep (turn 3 of 3), returns results

11. Client detects limit reached, adds limit message to tool result

12. Client builds final request with limit notification (see [Request D](#request-d))

13. Model acknowledges limit and summarizes progress (see [Response D](#response-d))

14. Response streams to scrollback

15. All messages persisted to database

---

## Reference

### Request A

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Keep searching for errors in every file"}
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
        "id": "call_grep1",
        "type": "function",
        "function": {
          "name": "grep",
          "arguments": "{\"pattern\": \"error\", \"path\": \"src/main.c\"}"
        }
      }]
    },
    "finish_reason": "tool_calls"
  }]
}
```

### Tool Result A

```json
{"output": "src/main.c:12: log_error(\"Failed to initialize\");", "count": 1}
```

### Response B

```json
{
  "choices": [{
    "delta": {
      "role": "assistant",
      "tool_calls": [{
        "index": 0,
        "id": "call_grep2",
        "type": "function",
        "function": {
          "name": "grep",
          "arguments": "{\"pattern\": \"error\", \"path\": \"src/config.c\"}"
        }
      }]
    },
    "finish_reason": "tool_calls"
  }]
}
```

### Tool Result B

```json
{"output": "src/config.c:45: return CONFIG_ERROR;", "count": 1}
```

### Response C

```json
{
  "choices": [{
    "delta": {
      "role": "assistant",
      "tool_calls": [{
        "index": 0,
        "id": "call_grep3",
        "type": "function",
        "function": {
          "name": "grep",
          "arguments": "{\"pattern\": \"error\", \"path\": \"src/parser.c\"}"
        }
      }]
    },
    "finish_reason": "tool_calls"
  }]
}
```

### Tool Result C

```json
{"output": "src/parser.c:78: parse_error(line, col);", "count": 1, "limit_reached": true, "limit_message": "Tool call limit reached (3). Stopping tool loop."}
```

### Request D

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Keep searching for errors in every file"},
    {"role": "assistant", "tool_calls": [{"id": "call_grep1", "type": "function", "function": {"name": "grep", "arguments": "{\"pattern\": \"error\", \"path\": \"src/main.c\"}"}}]},
    {"role": "tool", "tool_call_id": "call_grep1", "content": "{\"output\": \"src/main.c:12: log_error(\\\"Failed to initialize\\\");\", \"count\": 1}"},
    {"role": "assistant", "tool_calls": [{"id": "call_grep2", "type": "function", "function": {"name": "grep", "arguments": "{\"pattern\": \"error\", \"path\": \"src/config.c\"}"}}]},
    {"role": "tool", "tool_call_id": "call_grep2", "content": "{\"output\": \"src/config.c:45: return CONFIG_ERROR;\", \"count\": 1}"},
    {"role": "assistant", "tool_calls": [{"id": "call_grep3", "type": "function", "function": {"name": "grep", "arguments": "{\"pattern\": \"error\", \"path\": \"src/parser.c\"}"}}]},
    {"role": "tool", "tool_call_id": "call_grep3", "content": "{\"output\": \"src/parser.c:78: parse_error(line, col);\", \"count\": 1, \"limit_reached\": true, \"limit_message\": \"Tool call limit reached (3). Stopping tool loop.\"}"}
  ],
  "tools": ["...standard tools array..."],
  "tool_choice": "none",
  "stream": true
}
```

### Response D

```json
{
  "choices": [{
    "delta": {
      "role": "assistant",
      "content": "I was searching through files but reached the tool call limit (3 calls). I found errors in main.c and config.c. To continue searching, you can ask me to resume or increase the limit."
    },
    "finish_reason": "stop"
  }]
}
```
