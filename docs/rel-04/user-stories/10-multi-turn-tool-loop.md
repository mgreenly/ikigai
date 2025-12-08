# Multi-Turn Tool Loop

## Description

User asks the model to find and fix a bug. The model uses multiple tools across several turns: grep to find the issue, file_read to examine context, and file_write to apply the fix. Demonstrates autonomous multi-step problem solving.

## Transcript

```text
> Find where we handle null pointers in parser.c and add a missing check

I found the issue. In parser.c, the `parse_token` function was missing a null check before dereferencing `input`. I've added the check at line 23. The function now safely returns NULL if given a null input.
```

## Walkthrough

1. User types request and presses Enter

2. Client builds chat completion request (see [Request A](#request-a))

3. Model responds with grep tool call to find null handling (see [Response A](#response-a))

4. Client executes grep, returns matches (see [Tool Result A](#tool-result-a))

5. Client builds follow-up request (see [Request B](#request-b))

6. Model responds with file_read to examine the function (see [Response B](#response-b))

7. Client executes file_read, returns file contents (see [Tool Result B](#tool-result-b))

8. Client builds follow-up request (see [Request C](#request-c))

9. Model responds with file_write to add the null check (see [Response C](#response-c))

10. Client executes file_write, returns success (see [Tool Result C](#tool-result-c))

11. Client builds follow-up request (see [Request D](#request-d))

12. Model summarizes what was done (see [Response D](#response-d))

13. Response streams to scrollback

14. All messages persisted to database

---

## Reference

### Request A

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Find where we handle null pointers in parser.c and add a missing check"}
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
          "arguments": "{\"pattern\": \"NULL\", \"path\": \"parser.c\"}"
        }
      }]
    },
    "finish_reason": "tool_calls"
  }]
}
```

### Tool Result A

```json
{"success": true, "data": {"output": "parser.c:45: if (token == NULL) return ERR_INVALID;\nparser.c:67: if (result == NULL) return ERR_ALLOC;", "count": 2}}
```

### Request B

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Find where we handle null pointers in parser.c and add a missing check"},
    {"role": "assistant", "tool_calls": [{"id": "call_grep1", "type": "function", "function": {"name": "grep", "arguments": "{\"pattern\": \"NULL\", \"path\": \"parser.c\"}"}}]},
    {"role": "tool", "tool_call_id": "call_grep1", "content": "{\"success\": true, \"data\": {\"output\": \"parser.c:45: if (token == NULL) return ERR_INVALID;\\nparser.c:67: if (result == NULL) return ERR_ALLOC;\", \"count\": 2}}"}
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
      "tool_calls": [{
        "index": 0,
        "id": "call_read1",
        "type": "function",
        "function": {
          "name": "file_read",
          "arguments": "{\"path\": \"parser.c\"}"
        }
      }]
    },
    "finish_reason": "tool_calls"
  }]
}
```

### Tool Result B

```json
{"success": true, "data": {"output": "#include \"parser.h\"\n\nToken *parse_token(const char *input) {\n    int len = strlen(input);  // BUG: no null check\n    Token *t = malloc(sizeof(Token));\n    if (t == NULL) return NULL;\n    // ... rest of function\n}"}}
```

### Request C

```json
{
  "model": "gpt-5-mini",
  "messages": [
    "...previous messages...",
    {"role": "assistant", "tool_calls": [{"id": "call_read1", "type": "function", "function": {"name": "file_read", "arguments": "{\"path\": \"parser.c\"}"}}]},
    {"role": "tool", "tool_call_id": "call_read1", "content": "{\"success\": true, \"data\": {\"output\": \"#include \\\"parser.h\\\"\\n\\nToken *parse_token(const char *input) {\\n    int len = strlen(input);  // BUG: no null check\\n    Token *t = malloc(sizeof(Token));\\n    if (t == NULL) return NULL;\\n    // ... rest of function\\n}\"}}"}
  ],
  "tools": ["...standard tools array..."],
  "tool_choice": "auto",
  "stream": true
}
```

### Response C

```json
{
  "choices": [{
    "delta": {
      "role": "assistant",
      "tool_calls": [{
        "index": 0,
        "id": "call_write1",
        "type": "function",
        "function": {
          "name": "file_write",
          "arguments": "{\"path\": \"parser.c\", \"content\": \"#include \\\"parser.h\\\"\\n\\nToken *parse_token(const char *input) {\\n    if (input == NULL) return NULL;\\n    int len = strlen(input);\\n    Token *t = malloc(sizeof(Token));\\n    if (t == NULL) return NULL;\\n    // ... rest of function\\n}\"}"
        }
      }]
    },
    "finish_reason": "tool_calls"
  }]
}
```

### Tool Result C

```json
{"success": true, "data": {"output": "Wrote 215 bytes to parser.c", "bytes": 215}}
```

### Request D

```json
{
  "model": "gpt-5-mini",
  "messages": [
    "...previous messages...",
    {"role": "assistant", "tool_calls": [{"id": "call_write1", "type": "function", "function": {"name": "file_write", "arguments": "..."}}]},
    {"role": "tool", "tool_call_id": "call_write1", "content": "{\"success\": true, \"data\": {\"output\": \"Wrote 215 bytes to parser.c\", \"bytes\": 215}}"}
  ],
  "tools": ["...standard tools array..."],
  "tool_choice": "auto",
  "stream": true
}
```

### Response D

```json
{
  "choices": [{
    "delta": {
      "role": "assistant",
      "content": "I found the issue. In parser.c, the `parse_token` function was missing a null check before dereferencing `input`. I've added the check at line 23. The function now safely returns NULL if given a null input."
    },
    "finish_reason": "stop"
  }]
}
```
