# Simple Greeting Without Tool Use

## Description

User sends a simple greeting message. The model responds conversationally without invoking any tools, demonstrating that tools are available but not used when unnecessary.

## Transcript

```text
> Hello

Hello! How can I help you today?
```

## Walkthrough

1. User types "Hello" and presses Enter

2. Client builds chat completion request with user message and tools array (see [Request A](#request-a))

3. Request sent to OpenAI API

4. Model determines no tools are needed for a simple greeting

5. Response streams back with content only, no `tool_calls` (see [Response A](#response-a))

6. Response streams to scrollback

7. Assistant message persisted to database

8. Client returns to idle state, ready for next input

---

## Reference

### Request A

```json
{
  "model": "gpt-5-mini",
  "messages": [
    {"role": "user", "content": "Hello"}
  ],
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "glob",
        "description": "Find files matching a glob pattern",
        "parameters": {
          "type": "object",
          "properties": {
            "pattern": {"type": "string", "description": "Glob pattern (e.g., 'src/**/*.c')"},
            "path": {"type": "string", "description": "Base directory (default: cwd)"}
          },
          "required": ["pattern"]
        }
      }
    },
    {
      "type": "function",
      "function": {
        "name": "file_read",
        "description": "Read contents of a file",
        "parameters": {
          "type": "object",
          "properties": {
            "path": {"type": "string", "description": "Path to file"}
          },
          "required": ["path"]
        }
      }
    },
    {
      "type": "function",
      "function": {
        "name": "grep",
        "description": "Search file contents for a pattern",
        "parameters": {
          "type": "object",
          "properties": {
            "pattern": {"type": "string", "description": "Search pattern (regex)"},
            "path": {"type": "string", "description": "File or directory to search"},
            "glob": {"type": "string", "description": "File pattern filter (e.g., '*.c')"}
          },
          "required": ["pattern"]
        }
      }
    },
    {
      "type": "function",
      "function": {
        "name": "file_write",
        "description": "Write content to a file",
        "parameters": {
          "type": "object",
          "properties": {
            "path": {"type": "string", "description": "Path to file"},
            "content": {"type": "string", "description": "Content to write"}
          },
          "required": ["path", "content"]
        }
      }
    },
    {
      "type": "function",
      "function": {
        "name": "bash",
        "description": "Execute a shell command",
        "parameters": {
          "type": "object",
          "properties": {
            "command": {"type": "string", "description": "Command to execute"}
          },
          "required": ["command"]
        }
      }
    }
  ],
  "tool_choice": "auto",
  "stream": true
}
```

### Response A

```json
{
  "choices": [{
    "delta": {"role": "assistant", "content": "Hello! How can I help you today?"},
    "finish_reason": "stop"
  }]
}
```
