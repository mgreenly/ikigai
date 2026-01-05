# External Tool Architecture

ikigai lets you add custom tools with zero token overhead.

## What's New

Any executable that follows a simple JSON protocol becomes a first-class tool.

## Why This Approach

Most agents support external tools through bash, but bash has overhead: command syntax tokens, text output parsing, no schema validation. Built-in tools avoid this, and you usually can't add new tools without modifying the agent (or going through MCP).

Ikigai lets you promote frequently-used operations to first-class tools. Write a simple script that speaks JSON, drop it in, and it becomes indistinguishable from a built-in tool. Same typed parameters, same structured output, zero additional overhead.

This doesn't replace bash. You won't wrap everything. But when something gets called often enough that the token tax matters, you can eliminate it. Package a custom tool with a skill, and that skill runs at native efficiency.

## User Benefits

**Extensibility** - Add tools in any language. Drop an executable in `~/.ikigai/tools/`, run `/refresh`, done.

**Simplicity** - Tools manage their own credentials and configuration. ikigai just runs them.

**Zero token tax** - Custom tools run at native efficiency. No bash syntax, no text parsing.

## How It Works

1. ikigai discovers tools at startup by scanning ALL THREE directories:
   - System tools: `PREFIX/libexec/ikigai/` (6 tools shipped with ikigai)
   - User tools: `~/.ikigai/tools/` (your personal custom tools, available in all projects)
   - Project tools: `$PWD/.ikigai/tools/` (project-specific tools, local to this directory)
2. Each tool describes itself via `--schema` flag
3. LLM sees all tools with typed parameters (from all three directories combined)
4. Tool execution: JSON in, JSON out
5. Override precedence: **Project > User > System** (most specific wins)
   - Project tools override user tools, user tools override system tools
   - Example: custom "bash" in project dir takes precedence over your personal "bash", which overrides system "bash"

## Scope

**In this release:**
- External tool infrastructure (registry, discovery, execution)
- 6 external tools: bash, file_read, file_write, file_edit, glob, grep
- **Three-tier discovery** (ALL THREE directories scanned, any/all/none may exist):
  - System: `PREFIX/libexec/ikigai/` - tools shipped with ikigai
  - User: `~/.ikigai/tools/` - your personal custom tools (global)
  - Project: `$PWD/.ikigai/tools/` - project-specific tools (local)
  - Override precedence: Project > User > System (most specific wins)
- Tool execution with JSON protocol (JSON Schema for tool schemas)
- Response wrapper (distinguishes tool errors from ikigai errors)
- `/tool` and `/tool NAME` commands
- `/refresh` command to reload tools from all three directories
- Remove internal tool system

## Migration Order

External tools must work before internal tools are removed. See `plan/README.md` for detailed phases.

0. **Path resolution infrastructure** - Install directory detection (FIRST STEP)
   - Detects install type (development/user/system)
   - Provides paths to all subsystems via dependency injection
   - Foundation for tool discovery (needs system/user/project tool directories)
   - See `plan/paths-module.md` for specification
   - **Note:** Will affect many existing tests (need to provide paths instance)
1. **Build external tools** - Standalone executables, tested independently
2. **Remove internal tools** - Delete old tool system
3. **Build infrastructure** - Registry, discovery, execution (blocking API)
4. **Add commands** - /tool, /refresh
5. **Async optimization** - Non-blocking startup (optional enhancement)

**Not in this release:**
- Additional tools (web search, etc.) - future releases
- Tool sets (filtering which tools LLM sees)
- Schema versioning
- Skills system (context + tools)
