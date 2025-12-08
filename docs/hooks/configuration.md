# Hook Configuration

Hooks are configured in `.claude/settings.json` at the project root.

## Configuration Structure

```json
{
  "hooks": {
    "<HookType>": [
      {
        "matcher": "<pattern>",
        "hooks": [
          {
            "type": "command",
            "command": "<path/to/script>",
            "timeout": <milliseconds>
          }
        ]
      }
    ]
  }
}
```

## Configuration Fields

### Hook Type

Top-level key identifying when the hook runs:
- `SessionStart` - Triggered when a session begins
- `UserPromptSubmit` - Triggered when user submits a prompt
- `Stop` - Triggered when Claude finishes responding

### Matcher

Pattern to filter when hooks run:
- `"*"` - Runs for all events (wildcard)
- Custom patterns - Match specific conditions (format TBD)

Multiple matcher blocks can be defined for the same hook type.

### Hook Definition

Each hook in the `hooks` array has:

**type** (required): Currently only `"command"` is supported
- Executes a shell command

**command** (required): Path to the executable
- Can be relative to project root or absolute
- Must be executable (`chmod +x`)
- Receives JSON input via stdin

**timeout** (optional): Maximum execution time in milliseconds
- Hook is killed if it exceeds this duration
- Defaults to reasonable limits if not specified
- Prevents hung hooks from blocking

## Example Configuration

```json
{
  "hooks": {
    "SessionStart": [
      {
        "matcher": "*",
        "hooks": [
          {
            "type": "command",
            "command": "agents/logs/hooks/session_start.sh",
            "timeout": 30000
          }
        ]
      }
    ],
    "UserPromptSubmit": [
      {
        "matcher": "*",
        "hooks": [
          {
            "type": "command",
            "command": "agents/logs/hooks/user_prompt.sh",
            "timeout": 10000
          }
        ]
      }
    ],
    "Stop": [
      {
        "matcher": "*",
        "hooks": [
          {
            "type": "command",
            "command": "agents/logs/hooks/stop.sh",
            "timeout": 30000
          }
        ]
      }
    ]
  }
}
```

## Multiple Hooks

You can define multiple hooks for the same event:

```json
{
  "hooks": {
    "UserPromptSubmit": [
      {
        "matcher": "*",
        "hooks": [
          {
            "type": "command",
            "command": "hooks/log.sh",
            "timeout": 5000
          },
          {
            "type": "command",
            "command": "hooks/analytics.sh",
            "timeout": 10000
          }
        ]
      }
    ]
  }
}
```

Hooks execute in the order defined.

## Best Practices

1. **Use Relative Paths**: Relative to project root for portability
2. **Set Reasonable Timeouts**: 5-30 seconds typically sufficient
3. **Make Scripts Executable**: `chmod +x` before committing
4. **Handle Errors Gracefully**: Hooks should exit 0 on success
5. **Keep Hooks Fast**: Long-running hooks slow the UX
6. **Test Hooks Independently**: Run with sample JSON before deploying

## Reloading Configuration

Changes to `.claude/settings.json` require restarting Claude Code:
- Settings are loaded once at session start
- No hot-reloading during active sessions
- Exit and restart to apply changes
