# Minimal Tool Architecture

## Goal

Provide a maximally useful small set of building blocks that enable powerful agent capabilities while remaining simple, maintainable, and extensible.

**Core principle**: Minimize internal tools, maximize extensibility through external tools.

## Philosophy

### Why Minimal?

1. **Reduced maintenance burden** - Fewer internal tools means less code to maintain, test, and document
2. **Leverage model strengths** - LLMs excel at bash and Unix tool output interpretation
3. **Universal compatibility** - Standard Unix tools work everywhere, require no special handling
4. **User extensibility first** - External tools are the norm, not the exception

### Why Bash-Centric?

Models already excel at:
- Constructing bash commands with proper escaping
- Parsing Unix tool output (grep, find, jq, git)
- Composing pipelines and command chains
- Error interpretation from standard formats

Building on these strengths is simpler than teaching custom tool protocols.

## Tool Groups

### Internal Tools (Privileged Operations)

Small, fixed set requiring privileged access or internal state:

- **`bash`** - Execute shell commands with output limits
- **`bash_interactive`** - Long-running interactive shells (optional)
- **`web_search`** - Search the web (DuckDuckGo default, configurable providers)
- **`slash_command`** - Execute ikigai commands (/clear, /mark, /forget, etc.)

**Criteria for internal tools:**
- Requires privileged system access (command execution, web access)
- Needs access to ikigai's internal state (conversation context, config)
- Cannot be safely delegated to external scripts
- Would be identical across all users (no customization needed)

### External Tools (Everything Else)

Scripts in `.ikigai/scripts/` that models execute via bash:

- File operations (read, write, search, edit)
- Version control (git operations)
- Database queries and migrations
- Code analysis and generation
- Project-specific workflows
- Third-party integrations (including MCP wrapper tools)

**Characteristics:**
- Standard interface: bash command in, JSON out
- Self-documenting via README.md convention
- No registration or manifest required
- User-customizable and project-specific
- Discovered progressively through skills

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

## Progressive Discovery

Tools are discovered progressively, not front-loaded, through a layered system.

### Layer 1: Default System Prompt

Foundation layer - always available:

**Bash fundamentals:**
- File operations: `cat`, `head`, `tail`, `wc`
- Search: `grep`, `find`, `rg` (ripgrep)
- JSON: `jq` patterns for parsing and manipulation
- Git: `git status`, `git diff`, `git log`, `git add`, `git commit`
- Build: `make`, `cmake`, language-specific tools

**These replace internal tools** that other agents provide (file_read, file_write, search, git_commit).

**Teaches patterns like:**
```bash
# Read file section
cat file.c | sed -n '50,100p'

# Search with context
grep -A 5 -B 5 'pattern' file.c

# JSON extraction
curl api.example.com | jq '.results[] | .name'

# Git operations
git add src/ && git commit -m "message"
```

### Layer 2: Skills Introduce Domain Tools

Expansion layer - loaded on demand:

**User loads skill:**
```
/load database
```

**database.md skill provides:**
- Schema documentation
- Query patterns
- Performance considerations
- Mentions: `database-query`, `database-migrate`, `database-backup` tools

**Model discovers:**
- Domain knowledge from skill content
- Domain-specific tools mentioned in skill
- Tools self-documented via README.md in scripts directory

**Progressive, not overwhelming:**
- Session starts with bash basics (system prompt)
- Expands as needed (load skills)
- No massive upfront tool inventory
- Context-aware discovery (only load what's relevant)

### Layer 3: Project-Specific Tools

Project layer - discovered through project conventions:

Projects may have `.ikigai/scripts/` directory with custom tools:
- Build orchestration
- Deployment workflows
- Testing pipelines
- Custom analysis

Discovered same way as system tools - README.md convention, mentioned in project-specific skills.

## Why This Architecture Wins

### 1. User Extensibility (Biggest Selling Point)

**Adding a tool is trivial:**
```bash
# 1. Write script
echo '#!/usr/bin/env deno run' > .ikigai/scripts/custom/analyze.ts
# Script outputs JSON to stdout

# 2. Document in README
cat >> .ikigai/scripts/custom/README.md <<EOF
## analyze.ts
Analyzes code complexity.
Usage: deno run analyze.ts <file>
EOF

# 3. Optionally create skill
cat > .agents/skills/custom-analysis.md <<EOF
# Custom Analysis
Mentions: analyze tool
EOF

# Done - model can now use it
```

**No:**
- Protocol implementation
- Registration/manifest
- Configuration files
- Restart required

**Everything about ikigai is tuned for external tools** - discovery, documentation conventions, execution patterns. User extensions are first-class, not bolted-on.

### 2. Model Strengths

LLMs are already excellent at:
- Bash command construction
- Unix tool output parsing
- Pipeline composition
- Standard error interpretation

We leverage existing capabilities rather than teaching new protocols.

### 3. Maintenance

**Internal tools:**
- Written in C
- Integrated into binary
- Full test coverage required
- Breaking changes affect all users

**External tools:**
- Any language (Deno/TypeScript default)
- Isolated, independent
- Self-contained tests
- Users can fork/modify without touching ikigai

Fewer internal tools = lower maintenance burden.

### 4. Portability

Bash and Unix tools are universal. External tools work anywhere ikigai runs, no special platform support needed.

### 5. Composition

External tools compose naturally via bash:
```bash
database-query "SELECT id FROM users" | jq -r '.[]' | xargs -I {} backup-user {}
```

No custom composition layer needed.

## MCP Integration

MCP servers can be integrated as external tools through wrapper scripts.

### MCP as Implementation Detail

```
.ikigai/scripts/weather/
├── README.md              # "weather <city>" - get forecast
└── weather.ts             # Wraps weather MCP server
```

**weather.ts (wrapper):**
```typescript
// Connects to MCP server
// Translates args to MCP call
// Returns JSON to stdout
```

**Model sees:**
- Standard external tool interface
- Documented in README.md
- Called via bash: `weather "San Francisco"`
- Returns JSON output

**Model never:**
- Interacts with MCP protocol
- Knows MCP is involved
- Needs MCP-specific instructions

### When MCP Wrappers Make Sense

- Existing MCP server you want to use
- Third-party MCP ecosystem tools
- Complex integration already implemented in MCP

But implementation is hidden from models and most users.

### Primary Extension Model

External tools are the primary extension model, not MCP. MCP is one possible implementation strategy for external tools, not the extension interface itself.

## Internal Tool Criteria

A tool qualifies as internal if it meets these criteria:

### 1. Privileged Access Required

Operations that cannot be delegated to external scripts:
- Executing arbitrary shell commands (bash tool itself)
- Web search (requires API keys, rate limiting, provider management)
- Internal app commands (slash commands affecting conversation state)

### 2. Access to Internal State

Operations requiring ikigai's runtime state:
- Conversation context (for /forget, /remember filtering)
- Active session information
- Configuration state

### 3. Universal Across Users

Tool behavior identical for all users:
- No customization points
- No project-specific logic
- Core functionality everyone needs

### 4. Security/Safety Critical

Operations requiring validation, sandboxing, output limits:
- Bash execution with timeout/output limits
- Web requests with safety checks

If a tool doesn't meet these criteria, it should be external.

## System Prompt Role

The default system prompt teaches bash fundamentals, replacing what would be internal tools.

### Fundamental Patterns

**File operations:**
```bash
# Read file
cat file.c

# Read section
sed -n '50,100p' file.c

# Count lines
wc -l file.c

# Write file
cat > file.c <<'EOF'
content here
EOF
```

**Search operations:**
```bash
# Find files
find src/ -name '*.c'

# Search content
grep -r 'pattern' src/

# Search with ripgrep (faster)
rg 'pattern' src/
```

**JSON operations:**
```bash
# Parse API response
curl api.example.com | jq '.results'

# Extract field
jq -r '.name' data.json

# Filter array
jq '.items[] | select(.active)' data.json
```

**Git operations:**
```bash
# Status
git status --porcelain

# Diff
git diff HEAD

# Stage and commit
git add src/ && git commit -m "message"

# History
git log --oneline -10
```

### Teaching Philosophy

- Show idiomatic usage, not just syntax
- Emphasize composition (pipes, chains)
- Cover error handling patterns
- Provide real-world examples

### Evolution Over Time

System prompt can evolve based on:
- Common model mistakes (add clarifying examples)
- New bash patterns (teach better approaches)
- User feedback (what's confusing?)

No code changes required - system prompt is data.

## Comparison to Alternatives

### vs MCP (Model Context Protocol)

**MCP approach:**
- Protocol specification
- Server implementation required
- Registration/discovery mechanism
- Function signatures exposed to model
- Custom transport layer

**Ikigai approach:**
- Bash execution (universal)
- Script with JSON output (simple)
- README.md convention (documentation-as-discovery)
- Full context via skills (knowledge + capability)
- Standard pipes/files (universal transport)

**Advantage:** Simpler for users, leverages model strengths, no protocol lock-in.

### vs Claude Code

**Claude Code approach:**
- Many built-in tools (read, write, search, edit, glob, grep, bash, etc.)
- Rich tool-specific functionality
- Tightly integrated into agent experience

**Ikigai approach:**
- Minimal internal tools (bash, web_search, slash_command)
- Bash replaces most built-in tools
- External tools for domain-specific needs

**Advantage:** Less code to maintain, easier user extension, same capabilities.

### vs Other Coding Agents

Most coding agents provide extensive internal tool suites:
- File operations (read, write, edit, search, glob)
- Git operations (commit, diff, status, log)
- Code analysis (AST, symbols, references)
- Build operations (compile, test, lint)

Ikigai provides bash and teaches the model to use standard Unix tools for these operations.

**Tradeoff:** More complex commands (bash strings vs simple tool calls) for simpler architecture and easier extensibility.

## Migration Strategy

### Current State (rel-05)

ikigai currently has tool infrastructure but limited tool implementation. This positions us well for the minimal architecture.

**Existing:**
- Tool execution framework
- Conversation integration
- Result handling

**To add:**
- Bash tool (privileged command execution)
- Output limits and safety
- System prompt with bash patterns

### Phase 1: Core Internal Tools

Implement the minimal internal tool set:

1. **bash tool** - Command execution with output limits (~10KB default)
2. **web_search tool** - DuckDuckGo default, extensible to other providers
3. **slash_command tool** - Execute ikigai commands

System prompt teaches bash fundamentals (file ops, search, git, jq).

### Phase 2: External Tool Framework

Establish conventions and discovery:

1. **Script directory** - `.ikigai/scripts/` structure
2. **README convention** - Documentation-as-discovery pattern
3. **JSON contract** - Standard input/output format
4. **Example tools** - Database, git, code analysis scripts

### Phase 3: Skills Integration

Connect skills to external tools:

1. **Skill format** - Markdown with tool references
2. **Persona loading** - Skills define tool context
3. **Progressive discovery** - Layer 1 (prompt) → Layer 2 (skills) → Layer 3 (project)

### Phase 4: Refinement

Iterate based on usage:

1. **System prompt evolution** - Add patterns, fix misunderstandings
2. **Tool conventions** - Refine based on real tools
3. **Discovery UX** - Improve how models find and use tools

## Output Limits & Safety

Bash tool includes safety mechanisms to prevent context flooding and runaway commands.

### Output Limits

**Default limit: ~10KB** (~2500 tokens)

Prevents:
- `cat large-file.log` from flooding context
- Infinite loops printing to stdout
- Accidental binary file reads

**Behavior when exceeded:**
```json
{
  "success": false,
  "error": "Output exceeded limit (10KB)",
  "code": "OUTPUT_LIMIT_EXCEEDED",
  "truncated_output": "first 10KB of output...",
  "hint": "Use head/tail/grep to reduce output"
}
```

### Timeout

**Default timeout: 30 seconds**

Prevents:
- Long-running commands blocking conversation
- Accidental infinite loops
- Network operations hanging indefinitely

Models learn to use background execution for long operations.

### Sandboxing

Execution context:
- Working directory: Project root
- User: Current user (not root)
- No network restrictions (model can use curl, wget, etc.)
- File system: Full access (model can read/write/delete)

**Rationale:** Terminal agents need real access to be useful. Safety comes from:
- User awareness (they're running a coding agent)
- Model training (avoid destructive operations)
- Output limits (prevent resource exhaustion)

### Command Restrictions

**Not restricted:**
- File operations
- Git operations
- Build commands
- Network access

**Execution safety:**
- No shell access for commands to escape sandbox
- Commands run directly, not through shell interpretation layers
- Standard stderr/stdout capture

## Concrete Examples

### Before/After: File Operations

**Other agents (internal tool):**
```json
{
  "tool": "file_read",
  "path": "src/main.c",
  "start_line": 50,
  "end_line": 100
}
```

**Ikigai (bash):**
```bash
sed -n '50,100p' src/main.c
```

### Before/After: Search

**Other agents (internal tool):**
```json
{
  "tool": "grep",
  "pattern": "init_database",
  "path": "src/",
  "recursive": true
}
```

**Ikigai (bash):**
```bash
grep -r 'init_database' src/
```

### Before/After: Git Commit

**Other agents (internal tool):**
```json
{
  "tool": "git_commit",
  "files": ["src/main.c", "src/db.c"],
  "message": "Add database initialization"
}
```

**Ikigai (bash):**
```bash
git add src/main.c src/db.c && git commit -m "Add database initialization"
```

### User Extension Example

**User wants custom code complexity analysis:**

```bash
# 1. Create script
cat > .ikigai/scripts/analysis/complexity.ts <<'EOF'
#!/usr/bin/env deno run --allow-read

const file = Deno.args[0];
const code = await Deno.readTextFile(file);

// Analyze complexity
const result = {
  file: file,
  lines: code.split('\n').length,
  complexity: calculateComplexity(code)
};

console.log(JSON.stringify(result, null, 2));
EOF

# 2. Document
cat > .ikigai/scripts/analysis/README.md <<'EOF'
## complexity.ts

Analyzes code complexity metrics.

Usage: `deno run --allow-read complexity.ts <file>`

Returns JSON with file metrics.
EOF

# 3. Create skill (optional)
cat > .agents/skills/complexity-analysis.md <<'EOF'
# Code Complexity Analysis

Use the `complexity.ts` tool to analyze code complexity before refactoring.

Tool: `.ikigai/scripts/analysis/complexity.ts`
EOF

# Done - model can now discover and use it
```

**Model discovers tool through:**
1. Reading README.md in scripts directory (if exploring)
2. Loading complexity-analysis skill (if explicitly loaded)
3. Asking model to check for analysis tools (model explores directory)

### Discovery Workflow Example

**Session start:**
```
User: "Help me refactor the database code"

Model knows: bash basics (from system prompt)
  - Can read files, search, git operations
  - Can explore directory structure
```

**User loads skill:**
```
User: "/load database"

Model learns:
  - Schema structure (from skill)
  - Query patterns (from skill)
  - Tools: database-query, database-migrate (mentioned in skill)

Model explores: .ikigai/scripts/database/README.md
  - Discovers tool usage and contracts
```

**Model can now:**
- Read database code (bash: cat, grep)
- Understand schema context (skill knowledge)
- Run queries to test changes (database-query tool)
- Execute migrations safely (database-migrate tool)

## Future Evolution

### System Prompt Learning

System prompt evolves based on:
- Common model errors (add clarifying examples)
- Frequently asked questions (add guidance)
- New patterns discovered (teach better approaches)

No code changes required - prompt is configuration.

### Tool Conventions

External tool conventions will refine through usage:
- Input/output formats
- Error code standards
- Documentation patterns
- Discovery mechanisms

### Alternative Shells

While bash is default, system could support:
- `fish` shell (if detected)
- `zsh` with plugins
- `powershell` (Windows environments)

Model adapts to available shell via system prompt.

### Visual Tools

Future: Tools that produce visual output:
- Charts/graphs (rendered in terminal via sixel/kitty protocols)
- Diffs (syntax highlighted)
- Tables (formatted for readability)

Still external tools, richer output formats.

## Summary

**Minimal internal tools:**
- bash (command execution)
- web_search (web access)
- slash_command (app commands)

**Everything else:** External tools via bash

**Extension model:** Skills (knowledge) + External Tools (capabilities)

**Discovery:** Progressive layers (prompt → skills → project)

**Biggest win:** User extensibility - external tools are the norm, trivial to add, first-class support.

This architecture minimizes maintenance burden while maximizing user power through simple, universal building blocks.
