# External Tool Examples

## Tool Execution Examples (Implemented)

**Bash tool:**
```bash
$ echo '{"command":"echo hello"}' | libexec/ikigai/bash-tool
{"output":"hello","exit_code":0}

$ echo '{"command":"false"}' | libexec/ikigai/bash-tool
{"output":"","exit_code":1}
```

**File read tool:**
```bash
$ echo '{"file_path":"src/main.c","offset":50,"limit":10}' | libexec/ikigai/file-read-tool
{"content":"...(10 lines of code)...","lines_read":10}
```

**Grep tool:**
```bash
$ echo '{"pattern":"init_database","path":"src/"}' | libexec/ikigai/grep-tool
{"matches":[{"file":"src/database.c","line":42,"content":"void init_database()..."}]}
```

## Tool Discovery Example

```bash
$ ikigai
> /tool
Available tools:
  bash (libexec/ikigai/bash-tool)
  file_read (libexec/ikigai/file-read-tool)
  file_write (libexec/ikigai/file-write-tool)
  file_edit (libexec/ikigai/file-edit-tool)
  glob (libexec/ikigai/glob-tool)
  grep (libexec/ikigai/grep-tool)

> /tool bash
Tool: bash
Path: libexec/ikigai/bash-tool
Schema:
{
  "name": "bash",
  "description": "Execute a shell command and return output",
  "parameters": {
    "type": "object",
    "properties": {
      "command": {
        "type": "string",
        "description": "Shell command to execute"
      }
    },
    "required": ["command"]
  }
}
```

## User Extension Example

Add custom weather tool to user directory:

```bash
$ cat > ~/.ikigai/tools/weather-tool << 'EOF'
#!/usr/bin/env python3
import sys, json

if '--schema' in sys.argv:
    print(json.dumps({
        "name": "weather",
        "description": "Get current weather",
        "parameters": {
            "type": "object",
            "properties": {
                "city": {"type": "string", "description": "City name"}
            },
            "required": ["city"]
        }
    }))
    sys.exit(0)

args = json.load(sys.stdin)
result = {"temperature": 72, "condition": "sunny", "city": args['city']}
print(json.dumps(result))
EOF

$ chmod +x ~/.ikigai/tools/weather-tool
$ ikigai
> /refresh
Tool registry refreshed: 7 tools loaded

> /tool weather
Tool: weather
Path: ~/.ikigai/tools/weather-tool
Schema: {...}
```

Model can now use the weather tool in conversations without any code changes or restarts.

## Project Override Example

Override system `bash-tool` with project-specific version:

```bash
$ mkdir -p .ikigai/tools
$ cp libexec/ikigai/bash-tool .ikigai/tools/bash-tool
# Modify .ikigai/tools/bash-tool to add project-specific behavior
$ ikigai
> /refresh
Tool registry refreshed: 7 tools loaded
# bash tool now uses project version
```
