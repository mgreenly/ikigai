# LLM Conversation Example

This shows how the external tool protocol appears to the LLM in an actual conversation.

## System Prompt (Tool Presentation)

```markdown
You have access to the following tools:

### bash
Execute bash commands with output capture and error handling.

**Parameters:**
- `command` (string, required): The bash command to execute
- `timeout` (integer, optional, default: 30): Command timeout in seconds
- `working_dir` (string, optional, default: "."): Working directory for execution

**Returns:**
- `stdout` (string): Standard output from command
- `stderr` (string): Standard error from command
- `exit_code` (integer): Process exit code (0 = success)
- `truncated` (boolean): Whether output was truncated

**Output limits:**
- stdout: 10KB max
- stderr: 4KB max
- Truncated output includes warning message

**Usage:**
When you need to execute shell commands, list files, search code, or perform
system operations. Check exit_code to determine success/failure.
```

## Conversation Flow

### User Message
```
User: What files are in the src/ directory?
```

### LLM Response with Tool Call

**LLM thinks:** Need to list files, use bash tool

```json
{
  "reasoning": "I'll list the contents of src/ directory to see what files are there",
  "tool_calls": [
    {
      "tool": "bash",
      "params": {
        "command": "ls -la src/"
      }
    }
  ]
}
```

### Agent Execution (Internal)

Agent receives tool call:
1. Looks up `bash` in tool registry
2. Validates params against schema (command present, valid type)
3. Executes: `.ikigai/tools/bash '{"command": "ls -la src/"}'`
4. Receives structured JSON output

### Tool Result (What LLM Sees)

```json
{
  "tool": "bash",
  "result": {
    "stdout": "total 48\ndrwxr-xr-x  8 user  staff   256 Dec 23 10:15 .\ndrwxr-xr-x 15 user  staff   480 Dec 23 09:30 ..\n-rw-r--r--  1 user  staff  1234 Dec 23 10:15 main.c\n-rw-r--r--  1 user  staff   567 Dec 23 10:10 db.c\n-rw-r--r--  1 user  staff   890 Dec 23 10:12 repl.c\n-rw-r--r--  1 user  staff   445 Dec 23 10:08 util.c\n",
    "stderr": "",
    "exit_code": 0,
    "truncated": false
  }
}
```

### LLM Synthesizes Answer

```
The src/ directory contains 4 C source files:

- main.c (1,234 bytes) - likely the entry point
- db.c (567 bytes) - database functionality
- repl.c (890 bytes) - REPL/terminal interface
- util.c (445 bytes) - utility functions

Total of 3,136 bytes of source code.
```

## Error Handling Example

### User Message
```
User: Show me the contents of config.json
```

### LLM Tool Call

```json
{
  "tool": "bash",
  "params": {
    "command": "cat config.json"
  }
}
```

### Tool Result (File Doesn't Exist)

```json
{
  "tool": "bash",
  "result": {
    "stdout": "",
    "stderr": "cat: config.json: No such file or directory\n",
    "exit_code": 1,
    "truncated": false
  }
}
```

### LLM Handles Error

```
The file config.json doesn't exist in the current directory.

Would you like me to:
1. Search for it in subdirectories?
2. Create a new config.json file?
3. Check if there's a different configuration file?
```

## Multi-Step Example

### User Message
```
User: Find all TODO comments in C files and count them
```

### LLM Plan & Execute

**Step 1: Search for TODOs**
```json
{
  "tool": "bash",
  "params": {
    "command": "grep -r 'TODO' src/ --include='*.c' || true"
  }
}
```

**Result:**
```json
{
  "stdout": "src/main.c:// TODO: Add error handling\nsrc/db.c:// TODO: Implement connection pooling\nsrc/db.c:// TODO: Add retry logic\n",
  "stderr": "",
  "exit_code": 0,
  "truncated": false
}
```

**Step 2: Count them**
```json
{
  "tool": "bash",
  "params": {
    "command": "grep -r 'TODO' src/ --include='*.c' | wc -l"
  }
}
```

**Result:**
```json
{
  "stdout": "3\n",
  "stderr": "",
  "exit_code": 0,
  "truncated": false
}
```

**LLM Response:**
```
Found 3 TODO comments in your C files:

1. src/main.c:
   - Add error handling

2. src/db.c:
   - Implement connection pooling
   - Add retry logic

You might want to prioritize the error handling and retry logic for robustness.
```

## Output Truncation Example

### LLM Tool Call
```json
{
  "tool": "bash",
  "params": {
    "command": "find /usr -name '*.so' 2>/dev/null"
  }
}
```

### Tool Result (Too Much Output)
```json
{
  "stdout": "/usr/lib/x86_64-linux-gnu/libz.so.1\n/usr/lib/x86_64-linux-gnu/libcrypto.so.1.1\n... [9,900 more bytes] ...\n\n[OUTPUT TRUNCATED - exceeded 10KB limit]",
  "stderr": "",
  "exit_code": 0,
  "truncated": true
}
```

### LLM Adapts Strategy
```
The output was truncated (exceeded 10KB limit). Let me refine the search:
```

**New approach:**
```json
{
  "tool": "bash",
  "params": {
    "command": "find /usr -name '*.so' 2>/dev/null | wc -l"
  }
}
```

**Result:**
```json
{
  "stdout": "1847\n",
  "stderr": "",
  "exit_code": 0,
  "truncated": false
}
```

**LLM:**
```
Found 1,847 shared library files. Would you like me to filter by a specific pattern
or directory to get more manageable results?
```

## Key Observations

### From LLM Perspective

**Structured I/O:**
- Tool parameters are typed and validated
- Results are predictable JSON structure
- No string parsing or format guessing needed

**Error Signals:**
- `exit_code != 0` indicates failure
- `stderr` contains error messages
- `truncated == true` means adjust approach

**Same as Native Tools:**
- LLM treats bash tool exactly like built-in tools
- No special handling needed
- Consistent mental model

### Token Efficiency

**Tool call:** ~50 tokens
```json
{"tool": "bash", "params": {"command": "ls -la src/"}}
```

**Structured result:** ~100 tokens
```json
{"stdout": "...", "stderr": "", "exit_code": 0, "truncated": false}
```

**Compare to teaching bash in system prompt:**
- No need to teach stdout/stderr concepts
- No need to teach exit codes
- No need to teach output parsing
- Just: "Here's a tool, here's what it returns"

Much more token-efficient than teaching LLM to write and parse bash.

## System Prompt Simplification

### Without External Tool Protocol
```markdown
You can execute bash commands. Here's how:

1. Write bash commands carefully, they execute directly
2. Parse stdout to extract information
3. Check stderr for errors
4. Remember that exit code 0 means success
5. Handle pipes, redirects, and quoting correctly
6. Be careful with special characters
7. Remember output limits and truncation
...
[500+ tokens of bash tutoring]
```

### With External Tool Protocol
```markdown
You have a bash tool. It accepts:
- command: string to execute
- timeout: seconds (optional)
- working_dir: path (optional)

Returns:
- stdout, stderr, exit_code, truncated

Check exit_code for success/failure.
```

**~100 tokens instead of 500+**, and more reliable.
