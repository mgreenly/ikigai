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

1. ikigai discovers tools at startup (system and user directories)
2. Each tool describes itself via `--schema` flag
3. LLM sees all tools with typed parameters
4. Tool execution: JSON in, JSON out

## Scope

**In this release:**
- Remove internal tool system completely
- External tool infrastructure (registry, discovery, execution)
- Migrate 5 built-in tools to external: bash, file_read, file_write, glob, grep
- Add new file_edit tool (search-and-replace editing)
- Tool discovery from two locations (system + user)
- Tool execution with JSON protocol
- Response wrapper (distinguishes tool errors from ikigai errors)
- `/tool` and `/tool NAME` commands
- `/refresh` command to reload tools

**Not in this release:**
- Additional tools (web search, etc.) - future releases
- Tool sets (filtering which tools LLM sees)
- Schema versioning
- Skills system (context + tools)
