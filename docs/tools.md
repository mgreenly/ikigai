# Tools

Tools are the capabilities ikigai agents use to interact with the world — reading files, running commands, searching, and coordinating with other agents. The agent's LLM calls tools by name; ikigai dispatches them and returns results.

## Tool Types

**Internal tools** are compiled into ikigai and handle agent coordination:

| Tool | Description |
|------|-------------|
| [fork](tools/fork.md) | Create a child agent with a specific task |
| [kill](tools/kill.md) | Terminate an agent and all its descendants |
| [send](tools/send.md) | Send a message to another agent |
| [wait](tools/wait.md) | Wait for messages from other agents (fan-in support) |

**External tools** are standalone executables discovered at startup:

| Tool | Description |
|------|-------------|
| [bash](tools/bash.md) | Execute a shell command and return output |
| [file_read](tools/file_read.md) | Read contents of a file, with optional pagination |
| [file_write](tools/file_write.md) | Write content to a file (creates or overwrites) |
| [file_edit](tools/file_edit.md) | Edit a file by replacing exact text matches |
| [glob](tools/glob.md) | Find files matching a glob pattern |
| [grep](tools/grep.md) | Search for a pattern in files using regular expressions |
| [web_fetch](tools/web_fetch.md) | Fetch a URL and return content as markdown |
| [web_search](tools/web_search.md) | Search the web using the Brave Search API |

## Tool Discovery

External tools are discovered at startup from three directories, scanned in order:

| Directory | Purpose |
|-----------|---------|
| `$IKIGAI_LIBEXEC_DIR` | System tools installed with ikigai |
| `~/.ikigai/tools/` | User-level tools |
| `.ikigai/tools/` | Project-local tools (current working directory) |

**Override precedence:** project > user > system. If the same tool name appears in multiple directories, the project-level version wins.

Each external tool must:
- Be an executable file named `<tool-name>-tool` (hyphens in the filename become underscores in the tool name, e.g., `file-read-tool` → `file_read`)
- Accept a `--schema` flag and print a JSON schema to stdout

At startup, ikigai spawns each discovered tool with `--schema` to read its definition. Tools that fail or time out (1 second) are silently skipped.

## Writing Custom Tools

To add a project-local tool, create an executable at `.ikigai/tools/<name>-tool`. The tool must:

1. Support `--schema`: print a JSON schema describing its name, description, and parameters
2. Accept JSON on stdin when invoked: read `{"param": "value", ...}` and print a result

```
.ikigai/tools/
└── my-tool-tool        # tool name becomes "my_tool"
```

Example schema output from `--schema`:

```json
{
  "name": "my_tool",
  "description": "Does something useful",
  "parameters": {
    "type": "object",
    "properties": {
      "input": {
        "type": "string",
        "description": "The input value"
      }
    },
    "required": ["input"]
  }
}
```

## See Also

- [Commands](commands.md) — Slash commands for the same operations (human-facing)
- [Configuration](configuration.md) — `IKIGAI_LIBEXEC_DIR` and related settings
