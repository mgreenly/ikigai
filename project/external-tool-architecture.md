# External Tool Architecture

## Goal

Provide a maximally useful small set of building blocks that enable powerful agent capabilities while remaining simple, maintainable, and extensible.

**Core principle**: All tools are external tools. Maximize extensibility through JSON protocol and auto-discovery.

**Implementation status**: The external tool system (rel-08) is fully implemented with tool discovery, registry, execution protocol, and 6 core tools.

## Philosophy

**Why Minimal?** Reduce maintenance burden, leverage model bash strengths, ensure universal compatibility, prioritize user extensibility.

**Why Bash-Centric?** Models excel at bash commands, Unix output parsing, pipelines, and standard error formats. Building on these strengths beats teaching custom protocols.

## Tool Architecture

All tools in ikigai are external tools. There are no internal tools.

External executables in 3 discoverable directories (see Discovery System section):

- File operations (read, write, search, edit)
- Version control (git operations)
- Database queries and migrations
- Code analysis and generation
- Project-specific workflows
- Third-party integrations (including MCP wrapper tools)

**Implementation (rel-08)**: 6 core tools in `libexec/ikigai/`:

| Tool | Purpose | Input Parameters | Output Fields |
|------|---------|------------------|---------------|
| `bash-tool` | Execute shell commands | `command` (string) | `output` (string), `exit_code` (int) |
| `file-read-tool` | Read file contents | `file_path` (string), `offset` (int, optional), `limit` (int, optional) | `content` (string), `lines_read` (int) |
| `file-write-tool` | Write/overwrite files | `file_path` (string), `content` (string) | `bytes_written` (int) |
| `file-edit-tool` | Search/replace in files | `file_path` (string), `old_text` (string), `new_text` (string) | `replacements` (int) |
| `glob-tool` | Pattern-based file search | `pattern` (string), `path` (string, optional) | `files` (array of strings) |
| `grep-tool` | Content search | `pattern` (string), `path` (string, optional), `ignore_case` (bool, optional) | `matches` (array of match objects) |

All tools are built from source (C implementations) and installed to `libexec/ikigai/` during `make install`.

**Tool Protocol:**
- Naming: Executables ending in `-tool` (e.g., `bash-tool`, `file-read-tool`)
- Schema: `tool-name --schema` outputs JSON schema to stdout
- Execution: JSON arguments via stdin, JSON results via stdout
- Timeouts: 1 second for schema, 30 seconds for execution
- Output limits: 8KB for schema, 64KB for execution
- Error handling: Non-zero exit = failure, stderr ignored

**Schema Format:**
```json
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

**Execution Protocol:**
```bash
# Input: JSON on stdin
echo '{"command":"echo hello"}' | bash-tool

# Output: JSON on stdout
{"output":"hello","exit_code":0}
```

## Extension Model: Skills + External Tools

External tools combined with skills form ikigai's complete extension system, replacing traditional MCP integration.

### Skills (Knowledge)

Markdown files teaching domain concepts:

```markdown
# Database

Schema documentation, query patterns, migration strategies.

## Available Tools

- `database-query` - Execute SQL queries with result formatting
- `database-migrate` - Run migrations safely
- `database-backup` - Create timestamped backups
```

**Purpose:**
- Document domain knowledge (schemas, patterns, best practices)
- Introduce relevant external tools for that domain
- Provide usage examples and guidance
- Explain when/why to use specific tools

### External Tools (Capabilities)

Scripts that do the actual work:

```
.ikigai/scripts/database/
├── README.md              # Discovery documentation
├── query.ts               # Execute queries
├── migrate.ts             # Run migrations
└── backup.ts              # Create backups
```

**Purpose:**
- Provide executable capabilities for the domain
- Standard interface (JSON in/out, error codes)
- Self-contained with clear contracts

### The Combination

```
User loads database skill
  ↓
Skill teaches: schema, patterns, query strategies
  ↓
Skill mentions: database-query, migrate, backup tools
  ↓
Model has: Knowledge + Capabilities
```

This beats MCP's function-signature-only approach by providing rich context alongside capabilities.

## Discovery System

**Implementation (rel-08)**: Tools are discovered automatically from 3 directories with override precedence.

### Three-Directory Discovery

Implemented in `src/tool_discovery.c`:

1. **System directory**: `PREFIX/libexec/ikigai/` - Core tools shipped with ikigai
2. **User directory**: `~/.ikigai/tools/` - User-installed tools (override system)
3. **Project directory**: `$PWD/.ikigai/tools/` - Project-specific tools (override user and system)

**Override precedence**: Project > User > System (later directories override earlier ones)

**Discovery algorithm**:
1. Scan all three directories for executable files ending in `-tool`
2. Call each tool with `--schema` flag (1 second timeout)
3. Parse JSON schema output (max 8KB)
4. Extract tool name from filename: `bash-tool` → `bash`, `file-read-tool` → `file_read`
5. Add to registry (later discoveries override earlier ones with same name)
6. Build complete tool list for API transmission

**Implementation components**:
- `tool_discovery.c` - Directory scanning and schema fetching
- `tool_registry.c` - In-memory registry with override support
- `tool_external.c` - Tool execution with stdin/stdout IPC
- `tool_wrapper.c` - Success/failure JSON wrapping

**Commands**:
- `/tool` - List all discovered tools
- `/tool <name>` - Inspect specific tool schema and path
- `/refresh` - Reload tool registry from all 3 directories

### Progressive Discovery (Theoretical)

The original design concept for progressive discovery through skills remains relevant for future enhancements:

**Layer 1**: Default System Prompt - Teaching bash fundamentals (file ops, search, JSON, git)

**Layer 2**: Skills Introduce Domain Tools - Domain-specific knowledge + tool discovery via `/load` commands

**Layer 3**: Project-Specific Tools - Custom tools in `.ikigai/tools/` for specialized workflows

**Current status**: All tools are discovered upfront at startup and sent to the API. Progressive discovery through skills is planned for future releases.

## Why This Architecture Wins

### 1. User Extensibility (Biggest Selling Point)

**Implementation**: Adding a custom tool requires:

1. Write executable script ending in `-tool` (any language)
2. Implement `--schema` flag returning JSON schema
3. Read JSON from stdin, write JSON to stdout
4. Place in `~/.ikigai/tools/` (user) or `.ikigai/tools/` (project)
5. Run `/refresh` to reload registry

**Example**: Creating a custom `weather-tool`:

```bash
#!/usr/bin/env python3
# ~/.ikigai/tools/weather-tool

import sys, json

if '--schema' in sys.argv:
    schema = {
        "name": "weather",
        "description": "Get current weather for a city",
        "parameters": {
            "type": "object",
            "properties": {
                "city": {"type": "string", "description": "City name"}
            },
            "required": ["city"]
        }
    }
    print(json.dumps(schema))
    sys.exit(0)

# Read arguments from stdin
args = json.load(sys.stdin)
city = args['city']

# Fetch weather (simplified)
result = {"temperature": 72, "condition": "sunny", "city": city}
print(json.dumps(result))
```

No protocol libraries, registration, manifest files, or restart needed. User extensions are first-class.

### 2. Model Strengths

Leverage existing LLM excellence at bash commands, Unix output parsing, pipelines, and error interpretation rather than teaching new protocols.

### 3. Maintenance

All tools are external (any language, isolated, self-contained, user-modifiable). No internal tools means lower maintenance burden for ikigai core.

### 4. Portability

Bash and Unix tools are universal. No special platform support needed.

### 5. Composition

External tools compose naturally via bash: `database-query "..." | jq -r '.[]' | xargs backup-user`. No custom composition layer.

## MCP Integration

MCP servers integrate as external tools via wrapper scripts. Model sees standard tool interface (bash command, JSON output), never knows MCP is involved. MCP is one implementation strategy for external tools, not the primary extension model.

## System Prompt Role

The default system prompt teaches bash fundamentals for working with external tools.

### Fundamental Patterns

**File:** `cat file.c`, `sed -n '50,100p' file.c`, `wc -l file.c`
**Search:** `find src/ -name '*.c'`, `grep -r 'pattern' src/`, `rg 'pattern' src/`
**JSON:** `curl api.example.com | jq '.results'`, `jq -r '.name' data.json`
**Git:** `git status`, `git diff`, `git add src/ && git commit -m "message"`

System prompt teaches idiomatic usage, composition patterns, and error handling. Evolves based on usage - no code changes needed.

## Comparison to Alternatives

### vs Alternatives

**vs MCP:** Simpler tool protocol (bash + JSON vs protocol spec), leverages model strengths, no lock-in. ikigai's external tool protocol is lighter-weight - just `--schema` flag and JSON I/O - while MCP requires server processes and protocol implementations.

**vs Claude Code:** Different philosophy. Claude Code has many built-in tools (Read, Write, Edit, Grep, Glob, etc.). ikigai implements these as external tools, making them user-extensible. Both approaches work; ikigai optimizes for user customization at the cost of slightly more complex implementation.

**Note on current state**: In rel-08, ikigai has 6 external tools (bash, file_read, file_write, file_edit, glob, grep) similar to Claude Code's built-in set. The architecture difference is that these are external executables that can be overridden by users, not compiled-in C code. The theoretical "bash-centric" approach (replacing all tools with bash commands) is planned for future releases.

**vs Other Agents:** Most agents provide extensive internal tool suites. ikigai's external tool architecture enables users to add/modify/override tools without touching ikigai's source code. Tradeoff: More implementation complexity (process spawning, IPC, JSON protocols) for better user extensibility.

## Migration Strategy

**Phase 1 (COMPLETED - rel-08):** External tool framework
- Tool discovery from 3 directories (system/user/project)
- Tool protocol (--schema, JSON stdin/stdout, timeouts)
- Tool registry with override precedence
- Tool execution via process spawning and IPC
- Commands: /tool, /refresh
- 6 core tools: bash, file_read, file_write, file_edit, glob, grep

**Phase 2 (PLANNED):** Additional external tools
- web_fetch - Fetch URL content
- web_search - Web search integration
- Orchestration tools (fork, kill, send) - once ikigai has API for external tools

**Phase 3 (PLANNED):** Skills integration
- Markdown format for domain knowledge
- Tool references in skills
- Progressive discovery (load tools on demand, not upfront)
- Skill-based tool documentation

**Phase 4 (PLANNED):** Refinement
- System prompt evolution based on usage patterns
- Tool convention refinement (error codes, output formats)
- Alternative shell support (fish, zsh, powershell)
- Visual output (charts, diffs, tables)

## Output Limits & Safety

**Implementation (rel-08)**: Safety through timeouts and output limits, not sandboxing.

**Output limits:**
- Schema discovery: 8KB max (buffer size in `tool_discovery.c`)
- Tool execution: 64KB max (buffer size in `tool_external.c`)
- Prevents context flooding, infinite loops, binary data reads

**Timeouts:**
- Schema discovery: 1 second (`tool_discovery.c` line 69)
- Tool execution: 30 seconds (`tool_external.c` line 83)
- Prevents blocking, infinite loops, hangs
- Implemented via `alarm()` syscall

**Process spawning:**
- Uses `fork()` + `execl()` for isolation
- Stdin/stdout pipes for JSON communication
- Stderr redirected to `/dev/null` (errors via exit codes)
- Parent waits for child completion (`waitpid()`)
- Non-zero exit = tool failure

**Sandboxing**: None currently. Tools run as current user with full filesystem access. Safety comes from:
- User awareness (tools are user-provided or system-provided)
- Model training (avoid dangerous commands)
- Output limits and timeouts
- Explicit tool protocol (not arbitrary command execution)

## Performance

**External tool overhead:** Single-digit milliseconds. Tested bash_tool execution (fork/exec + command + output capture) completes in ~3ms. This validates the external tool architecture - the overhead of spawning external processes is negligible compared to API latency and model inference time.

## Concrete Examples

### Tool Execution Examples (Implemented)

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

### Tool Discovery Example

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

### User Extension Example

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

### Project Override Example

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

## Future Evolution

**System prompt** evolves based on usage patterns - no code changes needed.
**Tool conventions** refine through experience (I/O formats, error codes, docs).
**Alternative shells** possible (fish, zsh, powershell) via system prompt adaptation.
**Visual output** future: charts, diffs, tables via terminal protocols.

## Summary

**Current implementation (rel-08):**
- 6 external tools in `libexec/ikigai/`: bash, file_read, file_write, file_edit, glob, grep
- Tool discovery from 3 directories with override precedence (system/user/project)
- Tool protocol: `--schema` flag, JSON stdin/stdout, 30-second timeout, 64KB output limit
- Components: tool_discovery.c, tool_registry.c, tool_external.c, tool_wrapper.c
- Commands: /tool (list/inspect), /refresh (reload registry)

**Planned features:**
- Additional external tools (web_fetch, web_search, orchestration commands)
- Skills integration with tool references
- Progressive discovery (load tools on demand, not upfront)
- Ikigai API for external tools to access orchestration features

**Architecture goals:**
- All tools are external - zero internal tools
- Leverage model strengths (bash, Unix tools, JSON parsing)
- User extensibility - external tools are trivial to add, first-class support
- Simple, universal building blocks with minimal maintenance burden

**Biggest win:** Any user can add a custom tool in any language by writing a script with `--schema` flag and JSON I/O. No protocol libraries, registration, manifests, or restarts needed. Project-specific overrides enable per-project customization without affecting other projects.
