# ikigai Vision

## Overview

ikigai's vision is to be the **power user's AI coding agent** with git-aware multi-agent parallelization. Two core features differentiate ikigai from other coding agents:

1. **Multi-Agent Conversations** - Parallel conversations in isolated worktrees
2. **Mark/Rewind** - Checkpoint/rollback for iterative exploration

## Design Philosophy

**Git-Native Workflow:**
- Each agent can work in its own git worktree on its own branch
- Root agent stays on main branch in primary directory
- Helper agents work on feature branches in `.worktrees/`
- Memory documents persist in PostgreSQL (outside git)

**Power User First:**
- Keyboard-driven navigation
- Slash commands for explicit actions
- No hand-holding, maximum control
- Terminal-native (no web UI, no Electron)

**Parallelization Without Complexity:**
- Simple conversation switching (like tmux panes)
- No complex state management
- Each agent has isolated context
- Shared knowledge through memory documents

## Quick Navigation

### Core Vision Documents

- [commands.md](commands.md) - Complete command reference
- [keybindings.md](keybindings.md) - Keyboard navigation and shortcuts
- [multi-agent.md](multi-agent.md) - Multi-agent conversation design
- [mark-rewind.md](mark-rewind.md) - Checkpoint/rollback system
- [git-integration.md](git-integration.md) - Git worktree workflows
- [why-ikigai.md](why-ikigai.md) - The vision and sales pitch

### Workflow Examples

- [workflows.md](workflows.md) - Real-world usage patterns and examples
- [why-ikigai.md](why-ikigai.md) - Vision, philosophy, and comparisons

## Core Concepts

### Agents

- **Root Agent**: Always exists, typically on main branch in primary worktree
- **Helper Agents**: Created as needed, can work in separate worktrees on feature branches
- Each agent has complete conversation history and context
- Agents share access to memory document store

### Memory Documents

- Persistent knowledge base (PostgreSQL)
- Accessible by all agents
- Survive agent destruction
- Referenced with `#mem-<id>` or `#mem-<alias>`

### Worktrees

- Git worktrees provide physical isolation for parallel work
- Optional: agents can work without worktrees (research, analysis)
- Automatic cleanup when agents close
- Natural mapping: agent → branch → worktree

### Mark/Rewind

- In-session checkpoints for exploration
- Non-destructive (all messages preserved in DB)
- Named or anonymous marks
- LIFO stack for nested exploration

## Status Line

Shows current context at a glance:

```
[Agent: main] [Branch: main] [Worktree: ~/projects/ikigai/main]
[Agent: oauth-impl | +2] [Branch: feature-oauth] [Worktree: ~/projects/ikigai/main/.worktrees/oauth-impl]
```

Indicators:
- Current agent name
- Active helper count (e.g., `+2` means 2 other agents exist)
- Current git branch
- Current working directory / worktree path
- Mark count (when marks exist)

## Command Categories

### Agent Management
- `/agent new [name]` - Create helper agent
- `/agent list` - Show all agents
- `/agent <name>` - Switch to agent
- `/agent close` - Close current helper
- `/agent rename <name>` - Rename current agent

### Conversation Control
- `/mark [label]` - Create checkpoint
- `/rewind [label]` - Rollback to checkpoint
- `/clear` - Clear session context

### Memory Documents
- `/memory list` - List all documents
- `/memory show <id|alias>` - Display document
- `/memory merge <id|alias>` - Pull into context
- `/memory delete <id|alias>` - Remove document

### Git Integration
- `/agent new <name> --worktree` - Create agent with worktree
- `/agent merge` - Merge current branch to main
- `/branch` - Show branch status

## Key Bindings

### Agent Navigation
- `Ctrl-\` - Agent switcher (fuzzy find)
- `Ctrl-\ Ctrl-\` - Toggle to last agent
- `Ctrl-\ m` - Jump to root agent (main)

### Mark/Rewind
- `Ctrl-m` - Quick mark (anonymous)
- `Ctrl-r` - Quick rewind (to last mark)

### Scrollback
- `Page Up/Down` - Scroll display
- `Ctrl-Home/End` - Jump to top/bottom
- `Ctrl-l` - Redraw screen

See [keybindings.md](keybindings.md) for complete reference.

## Design Principles

### 1. Explicit Over Implicit

Commands are explicit actions:
- `/agent new research` - Create agent
- `/mark before-refactor` - Create checkpoint
- No automatic branching or hidden state

### 2. Git-Aware By Default

ikigai understands git and makes it first-class:
- Show current branch in status line
- Agents can own worktrees
- Memory docs reference commits/branches
- Integration with git operations

### 3. No Context Pollution

Each agent has isolated context:
- Own conversation history
- Own working directory (when using worktrees)
- No shared state except memory documents
- `/clear` on one agent doesn't affect others

### 4. Persistent Knowledge

Memory documents are the knowledge layer:
- Survive agent destruction
- Searchable and referenceable
- Can include code snippets, research, patterns
- Shared across all agents

### 5. Power User Efficiency

Fast, keyboard-driven workflows:
- Fuzzy matching for agents/commands
- Quick shortcuts for common actions
- No unnecessary confirmations
- Terminal-native speed

## Implementation Status

**v1.0 (Current Focus):**
- ✅ Terminal REPL foundation
- 🚧 OpenAI streaming integration
- 🚧 Single agent conversation
- 🚧 Mark/rewind implementation
- ⏳ Database persistence

**v2.0 (Future):**
- ⏳ Multi-agent support
- ⏳ Git worktree integration
- ⏳ Memory document store
- ⏳ Advanced navigation (key bindings)
- ⏳ Multi-LLM support

## Related Documentation

- [../architecture.md](../architecture.md) - Overall architecture
- [../v1-conversation-management.md](../v1-conversation-management.md) - Message lifecycle
- [../v1-database-design.md](../v1-database-design.md) - Database schema
- [../../multi-agent-ux.md](../../multi-agent-ux.md) - Original multi-agent concept
