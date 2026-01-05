# Tool Failure

Tool crashes or times out during execution.

## Description

When a tool fails at the ikigai level (crash, timeout, invalid output), ikigai returns `tool_success: false` with an error. This is distinct from tool-level errors (like missing credentials) where `tool_success: true`.

## Transcript - Timeout

```text
> run a very slow operation

I'll run that for you.

The tool timed out after 30 seconds. Here's what I captured before it stopped:

  stdout: "Processing item 1... Processing item 2..."
  stderr: (empty)

It looks like it was stuck processing items. Would you like me to try with
a smaller batch, or try a different approach?
```

## Transcript - Crash

```text
> do something that crashes the tool

The tool crashed with exit code 139. Here's the output:

  stdout: (empty)
  stderr: "Segmentation fault (core dumped)"

Exit code 139 indicates a segmentation fault (SIGSEGV). This is a bug in
the tool itself. Would you like me to try a different approach?
```

## Walkthrough - Timeout

1. User asks LLM to do something
2. LLM calls tool
3. ikigai spawns tool process
4. Tool runs longer than timeout (30s default)
5. ikigai kills process
6. ikigai returns timeout error (see [Timeout Response](#timeout-response))
7. LLM sees `tool_success: false`, handles gracefully

## Walkthrough - Crash

1. User asks LLM to do something
2. LLM calls tool
3. ikigai spawns tool process
4. Tool exits with non-zero code
5. ikigai returns crash error (see [Crash Response](#crash-response))
6. LLM sees `tool_success: false`, handles gracefully

## Reference

### Timeout Response

```json
{
  "tool_success": false,
  "error": "Tool 'slow-tool' timed out after 30s",
  "error_code": "TOOL_TIMEOUT",
  "exit_code": null,
  "stdout": "partial output before timeout...\n",
  "stderr": ""
}
```

### Crash Response

```json
{
  "tool_success": false,
  "error": "Tool 'buggy-tool' crashed with exit code 139",
  "error_code": "TOOL_CRASHED",
  "exit_code": 139,
  "stdout": "",
  "stderr": "Segmentation fault (core dumped)\n"
}
```

### Invalid Output Response

```json
{
  "tool_success": false,
  "error": "Tool 'broken-tool' returned invalid JSON",
  "error_code": "INVALID_OUTPUT",
  "exit_code": 0,
  "stdout": "not valid json{{{",
  "stderr": ""
}
```

## Notes

- `tool_success: false` means ikigai couldn't get a valid response
- LLM cannot parse `result` field - it's absent
- `exit_code`, `stdout`, `stderr` included for debugging context
- `exit_code` is null when process was killed (timeout)
- These are ikigai-level errors, not tool-level errors
- LLM should offer alternatives or ask user what to do next

## Comparison

| Scenario | tool_success | Where error lives |
|----------|--------------|-------------------|
| Tool works, operation fails | true | `result.error` |
| Tool missing credentials | true | `result.error` |
| Tool crashes | false | `error` |
| Tool times out | false | `error` |
| Tool returns bad JSON | false | `error` |
