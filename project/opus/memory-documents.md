# Memory Documents

**Status:** Early design discussion, not finalized.

## Overview

Memory documents are files stored in the database instead of the filesystem. They use a virtual `.memory/` path prefix that the system intercepts and routes to database storage.

## Why

Agents often need to create documents (research notes, design decisions, patterns) that:
- Shouldn't clutter the git workspace
- Should persist across sessions
- Should be accessible to all agents

Memory documents provide a place for this without polluting the project.

## Usage

Memory documents work like regular files:

```
# Read
/read .memory/oauth-patterns.md

# Write (agent creates via normal file write)
Write the research findings to .memory/oauth-research.md

# Reference in prompts
/read .memory/ikigai/auth-design.md
Implement authentication following the patterns above.
```

No special syntax. The `.memory/` prefix is the only indicator.

## Scope

- Global namespace across all projects and sessions
- Organize via path structure: `.memory/project-name/topic.md`
- No enforced hierarchy - agents decide organization

## Examples

```
.memory/oauth-research.md           # General research
.memory/ikigai/error-patterns.md    # Project-specific patterns
.memory/ikigai/api/design.md        # Nested organization
.memory/shared/coding-standards.md  # Cross-project standards
```

## Behavior

| Path | Storage |
|------|---------|
| `.memory/*` | Database |
| Everything else | Filesystem |

Read and write tools check the path prefix and route accordingly.

## Related

- [prompt-processing.md](prompt-processing.md) - `/read` pre-processing
- [agents.md](agents.md) - Agent types and context
