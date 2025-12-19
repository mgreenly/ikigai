# Memory Documents

**Status:** Early design discussion, not finalized.

## Overview

Memory documents are files stored in the database instead of the filesystem. They use an `ikigai:///` URI scheme (parallel to `file:///`) that the system intercepts and routes to database storage.

## Why

Agents often need to create documents (research notes, design decisions, patterns) that:
- Shouldn't clutter the git workspace
- Should persist across sessions
- Should be accessible to all agents

Memory documents provide a place for this without polluting the project.

## URI Scheme

The `ikigai:///` scheme mirrors the familiar `file:///` convention:

| Scheme | Storage | Example |
|--------|---------|---------|
| `file:///` | Local filesystem | `file:///home/user/project/README.md` |
| `ikigai:///` | Database | `ikigai:///oauth-research.md` |

Tools detect the URI scheme and route accordingly.

## Scope

- Global namespace across all projects and sessions
- Organize via path structure: `ikigai:///project-name/topic.md`
- No enforced hierarchy - agents decide organization

**Future:** When the database becomes project-aware (knows current git project/worktree), memory documents may support project-scoped filtering.

## Examples

```
ikigai:///oauth-research.md           # General research
ikigai:///ikigai/error-patterns.md    # Project-specific patterns
ikigai:///ikigai/api/design.md        # Nested organization
ikigai:///shared/coding-standards.md  # Cross-project standards
```

## User Access

Users interact with memory documents via slash commands and an external editor.

### Slash Commands

| Command | Purpose |
|---------|---------|
| `/memory edit <path>` | Open in $EDITOR, save back to DB on close |
| `/memory list [prefix]` | List documents (optional path filter) |
| `/memory delete <path>` | Remove a document |

### Edit Flow

1. Fetch document content from database
2. Write to temp file
3. Open $EDITOR
4. On close, compare against original
5. If changed, write back to database
6. Clean up temp file

Creating a new document: `/memory edit <new-path>` opens an empty editor; saving creates the document.

## Prompt Expansion

Users can reference files and memory documents inline while composing prompts. References are markers that expand to content on submit.

### Markers

| Marker | Expands to |
|--------|------------|
| `@path/to/file` | Filesystem file content |
| `@@path/to/doc` | Memory document content |

### Example

```
Implement auth following @@auth-patterns.md and update @src/auth.c
```

On submit, both markers expand: `@@auth-patterns.md` fetches from database, `@src/auth.c` reads from filesystem. The expanded content is sent to the LLM.

### Fuzzy Finder Triggers

| Trigger | Behavior |
|---------|----------|
| `@` + any char (not `@`) | Opens file fuzzy finder |
| `@@` + any char | Opens memory document fuzzy finder |
| `Ctrl+F` | Opens file fuzzy finder (browse mode, empty query) |
| `Ctrl+M` | Opens memory document fuzzy finder (browse mode, empty query) |

**Flow:**
1. Type `@sr` → fuzzy finder shows files matching "sr"
2. Select a result → `@src/auth.c` inserted at cursor
3. Continue typing
4. On submit → markers expand to content

**Cancel behavior:** Escape dismisses the dropdown (standard autocomplete behavior). If only `@` or `@@` typed (no dropdown yet), delete manually.

### File List (Filesystem)

A background task maintains the list of searchable files:

- Filtered by configurable include patterns
- Defaults cover common text file types (`.c`, `.h`, `.md`, `.json`, `.py`, `.js`, `.ts`, `.yaml`, `.sh`, `.sql`, `Makefile`, `Dockerfile`, etc.)
- Pattern configuration location TBD (global vs per-project config not yet defined)

### Memory List (Database)

- Query returns all memory documents
- No filtering (global namespace for now)
- Path structure provides implicit organization

## Agent Usage

Agents use memory documents like regular files:

```
# Read
/read ikigai:///oauth-patterns.md

# Write (agent creates via normal file write)
Write the research findings to ikigai:///oauth-research.md
```

No special syntax beyond the URI scheme.

## Related

- [opus/prompt-processing.md](opus/prompt-processing.md) - `/read` pre-processing
- [opus/agents.md](opus/agents.md) - Agent types and context
