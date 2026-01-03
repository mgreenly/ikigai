# User Stories

User journeys for the external tool architecture.

## Stories

| Story | Description |
|-------|-------------|
| [startup-experience](startup-experience.md) | User types while tools load in background |
| [add-custom-tool](add-custom-tool.md) | User adds a custom tool and uses it |
| [list-tools](list-tools.md) | User lists available tools with `/tool` |
| [inspect-tool](inspect-tool.md) | User inspects tool schema with `/tool NAME` |
| [tool-missing-credentials](tool-missing-credentials.md) | Tool guides user through credential setup |
| [tool-failure](tool-failure.md) | Tool crashes or times out |

## Test Tool

Stories use a minimal `cat` wrapper tool for testing:

```bash
#!/bin/sh
if [ "$1" = "--schema" ]; then
    cat <<'EOF'
{
  "name": "cat",
  "description": "Read file contents",
  "parameters": {
    "path": {
      "type": "string",
      "description": "Path to file",
      "required": true
    }
  },
  "returns": {
    "type": "object",
    "properties": {
      "content": {"type": "string", "description": "File contents"},
      "error": {"type": "string", "description": "Error message if failed"}
    }
  }
}
EOF
    exit 0
fi

path=$(jq -r '.path // empty')

if [ -z "$path" ]; then
    printf '{"error": "missing required parameter: path"}\n'
    exit 0
fi

if [ ! -f "$path" ]; then
    printf '{"error": "file not found: %s"}\n' "$path"
    exit 0
fi

content=$(cat "$path" | jq -Rs .)
printf '{"content": %s}\n' "$content"
```

This exercises: parameter parsing, error handling, success response.
