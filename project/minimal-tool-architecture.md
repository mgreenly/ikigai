# Minimal Tool Architecture

## Goal

Provide a maximally useful small set of building blocks that enable powerful agent capabilities while remaining simple, maintainable, and extensible.

**Core principle**: Minimize internal tools, maximize extensibility through external tools.

## Philosophy

**Why Minimal?** Reduce maintenance burden, leverage model bash strengths, ensure universal compatibility, prioritize user extensibility.

**Why Bash-Centric?** Models excel at bash commands, Unix output parsing, pipelines, and standard error formats. Building on these strengths beats teaching custom protocols.

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

Foundation layer teaching bash fundamentals: file ops (`cat`, `head`, `tail`, `wc`), search (`grep`, `find`, `rg`), JSON (`jq`), git, build tools. Replaces internal tools other agents provide (file_read, file_write, search, git_commit).

### Layer 2: Skills Introduce Domain Tools

Expansion layer loaded on demand. Example: `/load database` provides schema docs, query patterns, and mentions domain tools (database-query, database-migrate). Model discovers knowledge + capabilities progressively, not all upfront.

### Layer 3: Project-Specific Tools

Project layer: custom tools in `.ikigai/scripts/` for build, deployment, testing, analysis. Discovered via README.md convention and project skills.

## Why This Architecture Wins

### 1. User Extensibility (Biggest Selling Point)

Adding a tool: write script (outputs JSON), document in README, optionally create skill. Done. No protocol, registration, manifest, config files, or restart needed. User extensions are first-class.

### 2. Model Strengths

Leverage existing LLM excellence at bash commands, Unix output parsing, pipelines, and error interpretation rather than teaching new protocols.

### 3. Maintenance

Internal tools (C, integrated, full coverage, affect all users) vs external tools (any language, isolated, self-contained, user-modifiable). Fewer internal tools = lower burden.

### 4. Portability

Bash and Unix tools are universal. No special platform support needed.

### 5. Composition

External tools compose naturally via bash: `database-query "..." | jq -r '.[]' | xargs backup-user`. No custom composition layer.

## MCP Integration

MCP servers integrate as external tools via wrapper scripts. Model sees standard tool interface (bash command, JSON output), never knows MCP is involved. MCP is one implementation strategy for external tools, not the primary extension model.

## Internal Tool Criteria

Tools qualify as internal if they require: (1) privileged access (command execution, web search, app state), (2) ikigai runtime state access, (3) universal behavior (no customization), (4) security/safety validation. Otherwise, make it external.

## System Prompt Role

The default system prompt teaches bash fundamentals, replacing what would be internal tools.

### Fundamental Patterns

**File:** `cat file.c`, `sed -n '50,100p' file.c`, `wc -l file.c`
**Search:** `find src/ -name '*.c'`, `grep -r 'pattern' src/`, `rg 'pattern' src/`
**JSON:** `curl api.example.com | jq '.results'`, `jq -r '.name' data.json`
**Git:** `git status`, `git diff`, `git add src/ && git commit -m "message"`

System prompt teaches idiomatic usage, composition patterns, and error handling. Evolves based on usage - no code changes needed.

## Comparison to Alternatives

### vs Alternatives

**vs MCP:** Simpler (bash + JSON vs protocol spec), leverages model strengths, no lock-in.

**vs Claude Code:** Minimal internal tools vs many built-ins. Bash replaces read/write/search/edit/glob/grep tools. Less code to maintain, easier user extension.

**vs Other Agents:** Most provide extensive internal tool suites. Ikigai uses bash + Unix tools instead. Tradeoff: More complex commands for simpler architecture and better extensibility.

## Migration Strategy

**Phase 1:** Core tools (bash, web_search, slash_command) + system prompt teaching bash fundamentals.
**Phase 2:** External tool framework (`.ikigai/scripts/`, README convention, JSON contract, examples).
**Phase 3:** Skills integration (markdown format, tool references, progressive discovery).
**Phase 4:** Refinement (evolve prompt, refine conventions, improve discovery).

## Output Limits & Safety

**Output limit:** ~10KB default prevents context flooding, loops, binary reads. Returns error with truncated output and hint.

**Timeout:** 30 seconds prevents blocking, loops, hangs. Models use background execution for long ops.

**Sandboxing:** Project root working dir, current user (not root), full file system access, no network restrictions. Safety from user awareness, model training, output limits - not restrictions. Commands run directly with stderr/stdout capture.

## Performance

**External tool overhead:** Single-digit milliseconds. Tested bash_tool execution (fork/exec + command + output capture) completes in ~3ms. This validates the external tool architecture - the overhead of spawning external processes is negligible compared to API latency and model inference time.

## Concrete Examples

### Before/After Examples

**File read:** `{"tool": "file_read", "path": "src/main.c", "start_line": 50}` → `sed -n '50,100p' src/main.c`
**Search:** `{"tool": "grep", "pattern": "init_database", "path": "src/"}` → `grep -r 'init_database' src/`
**Git commit:** `{"tool": "git_commit", "files": [...], "message": "..."}` → `git add src/*.c && git commit -m "..."`

### User Extension Example

Add custom complexity analysis tool:
1. Write script: `.ikigai/scripts/analysis/complexity.ts` (outputs JSON)
2. Document: Add README.md with usage
3. Optional: Create skill referencing the tool

Model discovers via README exploration or skill loading. No registration or restart needed.

### Discovery Workflow Example

Session starts with bash basics. User runs `/load database`. Model learns schema, patterns, and discovers database-query, database-migrate tools. Can now read code (bash), understand schema (skill), run queries and migrations (tools).

## Future Evolution

**System prompt** evolves based on usage patterns - no code changes needed.
**Tool conventions** refine through experience (I/O formats, error codes, docs).
**Alternative shells** possible (fish, zsh, powershell) via system prompt adaptation.
**Visual output** future: charts, diffs, tables via terminal protocols.

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
