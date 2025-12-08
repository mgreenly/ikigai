# Bash Command Fails

## Description

User asks to run a command that fails. The tool returns the error output and exit code. The model explains the failure to the user.

## Transcript

```text
> Compile the project with gcc main.c

The compilation failed. GCC reported an undefined reference to `process_input`. It looks like you're missing a function definition or need to link another source file.
```

## Walkthrough

1. User types "Compile the project with gcc main.c" and presses Enter

2. Client builds chat completion request (see [Request A](#request-a))

3. Model responds with bash tool call (see [Response A](#response-a))

4. Tool call displayed in scrollback

5. Client executes bash with command `gcc main.c`

6. Command fails; tool returns stderr and exit code (see [Tool Result A](#tool-result-a))

7. Error result displayed in scrollback

8. Client builds follow-up request with error result (see [Request B](#request-b))

9. Model analyzes the error and explains the issue (see [Response B](#response-b))

10. Response streams to scrollback

11. All messages persisted to database

---

## Reference

### Request A

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Compile the project with gcc main.c"}
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
          "arguments": "{\"command\": \"gcc main.c\"}"
        }
      }]
    },
    "finish_reason": "tool_calls"
  }]
}
```

### Tool Result A

```json
{"success": true, "data": {"output": "/usr/bin/ld: /tmp/ccABC123.o: in function `main':\nmain.c:(.text+0x15): undefined reference to `process_input'\ncollect2: error: ld returned 1 exit status", "exit_code": 1}}
```

### Request B

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Compile the project with gcc main.c"},
    {"role": "assistant", "tool_calls": [{"id": "call_bash1", "type": "function", "function": {"name": "bash", "arguments": "{\"command\": \"gcc main.c\"}"}}]},
    {"role": "tool", "tool_call_id": "call_bash1", "content": "{\"success\": true, \"data\": {\"output\": \"/usr/bin/ld: /tmp/ccABC123.o: in function `main':\\nmain.c:(.text+0x15): undefined reference to `process_input'\\ncollect2: error: ld returned 1 exit status\", \"exit_code\": 1}}"}
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
      "content": "The compilation failed. GCC reported an undefined reference to `process_input`. It looks like you're missing a function definition or need to link another source file."
    },
    "finish_reason": "stop"
  }]
}
```
