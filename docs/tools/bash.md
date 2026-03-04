# bash

## NAME

bash - execute a shell command and return output

## SYNOPSIS

```json
{"name": "bash", "arguments": {"command": "..."}}
```

## DESCRIPTION

`bash` executes a shell command in a subprocess and returns the combined stdout and stderr output. Commands run in the working directory of the ikigai process. Each invocation starts a fresh shell — no state (environment changes, directory changes, shell variables) persists between calls.

Use this tool for one-off shell operations: running scripts, querying system state, invoking build tools, or performing file operations not covered by the dedicated file tools.

## PARAMETERS

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `command` | string | yes | Shell command to execute. Passed directly to `/bin/bash -c`. |

## RETURNS

The combined stdout and stderr output of the command as a string. If the command exits with a non-zero status, the output still reflects what was written — no special error format is added.

## EXAMPLES

Run a build command:

```json
{
  "name": "bash",
  "arguments": {
    "command": "make -C apps/ikigai 2>&1"
  }
}
```

Query process state:

```json
{
  "name": "bash",
  "arguments": {
    "command": "ps aux | grep ikigai"
  }
}
```

Run a test script:

```json
{
  "name": "bash",
  "arguments": {
    "command": "check-unit --file=tests/unit/repl_test.c"
  }
}
```

## NOTES

Each call launches a new shell. Directory changes (`cd`) and variable assignments do not carry over to subsequent calls.

Prefer `file_read`, `file_write`, `file_edit`, `glob`, and `grep` for file operations — they are purpose-built and return structured output. Reserve `bash` for operations those tools cannot handle.

Long-running commands block until completion. There is no timeout mechanism — keep commands short and focused.

## SEE ALSO

[file_read](file_read.md), [file_write](file_write.md), [file_edit](file_edit.md), [glob](glob.md), [grep](grep.md), [Tools](../tools.md)
