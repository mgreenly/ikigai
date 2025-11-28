# Multi-Agent Conversations

**Core Innovation:** Multiple parallel AI conversations, each with isolated context and optional git worktrees.

## The Concept

Think tmux for AI conversations. Multiple agents running in parallel, each focused on different work, sharing knowledge through memory documents.

**Not multi-threaded execution:**
- One conversation active at a time
- Explicit switching between agents
- Each agent maintains full context
- Simple, predictable behavior

**Why this works:**
- AI agents work asynchronously anyway (API calls)
- Human attention is sequential
- Context switching is explicit and fast
- Physical isolation via git worktrees

## Agent Types

### Root Agent

**Always exists:** Created on startup, default name "main", can be renamed, cannot be closed, works on main branch.

**Your home base:** Long-running conversation, coordinates overall work, you switch to helpers as needed.

### Helper Agents

**Created on demand:** `/agent new [name]`, optional worktree for physical isolation, ephemeral (close when done), own conversation history.

**Use cases:** Feature development on separate branch, research without worktree, refactoring in isolation, parallel experimental work.

## Creating Agents

### Basic Agent (No Worktree)

```bash
/agent new research

[You] Research OpenAI streaming API patterns and create a memory doc
[Assistant researches, creates #mem-streaming]

# Switch back to main
Ctrl-\ m

# Reference research
[You] Implement streaming using #mem-124
```

**Best for:** Research, documentation, analysis, planning.

### Agent with Worktree

```bash
/agent new oauth-impl --worktree

# Creates branch, worktree in .worktrees/oauth-impl, switches to worktree

[You] Implement OAuth 2.0 authentication
[Assistant creates files, tests, commits]
```

**Best for:** Feature development, refactoring, experiments, any code changes.

## Switching Between Agents

### Quick Switch (Ctrl-\)

Press `Ctrl-\` to open agent switcher with fuzzy search.

### Toggle (Ctrl-\ Ctrl-\)

Double-tap to toggle between current and last agent.

### Jump to Main (Ctrl-\ m)

From any agent, jump to root.

### Command-Based Switch

```bash
/agent <name>      # Supports fuzzy matching
/agent oauth       # Matches oauth-impl
```

## Listing Agents

```bash
/agent list

Active Agents:
• main (you are here) - Branch: main
  Messages: 45 | Last activity: now

• oauth-impl - Branch: oauth-impl
  Worktree: .worktrees/oauth-impl
  Messages: 23 | Marks: 2 | Ahead: 3 commits
  Last activity: 5 minutes ago

• research - No worktree
  Messages: 8 | Created: #mem-124, #mem-125
```

## Closing Agents

### Standard Close

```bash
/agent close

# Prompts for merge, merges to main, deletes worktree, deletes branch
# Memory documents preserved
```

### Close Without Merge

```bash
/agent close --no-merge    # Discard work

/agent close --keep-branch  # Merge but keep branch
```

## Agent Naming

**Random Names:** Default uses PascalCase adjective + Muppet character (BouncyGonzo, CheerfulFozzie, SparklingKermit). Memorable, easy to type, impossible to conflict.

**Custom Names:** `/agent new oauth-impl` for descriptive feature names.

**Renaming:** `/agent rename <name>` to rename current agent.

## Context Isolation

Each agent has completely isolated context:

**Independent Conversation History:** No pollution between agents.

**Independent Working Directory:** Each worktree-backed agent has own directory. All file operations relative to agent's directory.

**Independent Scrollback:** `/clear` in one agent doesn't affect others.

**Shared Memory Documents:** Only memory docs are shared across agents.

## Workflows

### Background Research

```bash
/agent new research
[You] Research OpenAI streaming, create memory doc when done

# Switch back to main
Ctrl-\ m
[Continue working]

# Check later
Ctrl-\ research
[Assistant] Research complete! Created #mem-124

/agent close
```

### Parallel Features

```bash
# Create multiple feature agents
/agent new oauth --worktree
/agent new error-refactor --worktree
/agent new api-endpoints --worktree

# All work in parallel, switch between as needed
# Merge when each completes
/agent merge && /agent close
```

### Experimental Approaches

```bash
/mark before-experiments

/agent new approach-a --worktree
/agent new approach-b --worktree

# Compare results, keep best, discard others
/agent close --no-merge  # Discard
```

### Coordinated Work

```bash
# Main agent coordinates
/agent new research            # Research patterns
/agent new impl --worktree     # Implement using research
/agent new testing --worktree  # Write tests

# Coordinate handoffs via memory docs
```

## Agent Status Line

```bash
[Agent: main] [Branch: main] [Clean]

[Agent: oauth-impl | +2] [Branch: oauth-impl] [Modified: 3]

[Agent: research | +3] [No worktree]
```

**Indicators:** Agent name, other agent count (`| +N`), branch, git status, worktree presence.

## Memory Documents

Shared knowledge layer across all agents.

### Creating Memory Docs

```bash
[You] Document the OAuth implementation patterns
[Assistant creates comprehensive document]
Created #mem-156 (oauth/patterns)
```

### Referencing Memory Docs

```bash
# In any agent
[You] Implement authentication using #mem-156
[Assistant loads memory doc and applies patterns]
```

### Organizing with Aliases

```bash
/memory alias 124 openai/streaming
/memory alias 156 oauth/patterns

# Reference by alias
[You] Implement using #mem-oauth/patterns
```

## Best Practices

### 1. One Agent per Feature

Don't overload agents. Create separate agents for separate logical features.

### 2. Research Agents Don't Need Worktrees

Research-only agents don't need worktrees. Save them for code changes.

### 3. Close Agents When Done

Don't accumulate stale agents. Close and merge when feature is complete.

### 4. Use Memory Docs for Shared Knowledge

Share knowledge via memory docs, not copy-paste between conversations.

### 5. Name Agents Descriptively

Use clear names like `oauth-impl`, `error-refactor`, `api-research` instead of `temp` or `test`.

## Advanced Patterns

### Agent Handoff

Agent A does initial work and creates memory doc. Agent B continues from that doc.

### Nested Agents

Main coordinates, creates level-1 helpers, which can create their own sub-agents.

### Agent Templates (Future)

```bash
/agent new oauth-impl --template=feature
# Auto-configures worktree, hooks, test structure, memory doc
```

## Implementation Notes

### Agent Structure

```c
typedef struct ik_agent_t {
    char *name;
    char *branch_name;
    char *worktree_path;
    ik_message_t **messages;
    ik_mark_t **marks;
    ik_scrollback_t *scrollback;
    int64_t db_agent_id;
    struct {
        int modified_count;
        int ahead_count;
    } git_status;
} ik_agent_t;
```

### Agent Manager

```c
typedef struct ik_agent_manager_t {
    ik_agent_t *root_agent;
    ik_agent_t **helper_agents;
    ik_agent_t *current_agent;
    ik_agent_t *previous_agent;
    ik_memory_store_t *memory_store;
} ik_agent_manager_t;
```

### Switching Logic

```c
res_t switch_to_agent(ik_agent_manager_t *mgr, const char *name) {
    ik_agent_t *target = find_agent_fuzzy(mgr, name);
    mgr->previous_agent = mgr->current_agent;
    mgr->current_agent = target;
    if (target->worktree_path)
        chdir(target->worktree_path);
    update_status_line(mgr);
    render_scrollback(target->scrollback);
    return OK(target);
}
```

## Comparison with Other Tools

| Feature | ikigai | tmux | Jupyter | VS Code |
|---------|--------|------|---------|---------|
| Multiple conversations | ✓ | N/A | ✓ (notebooks) | ✗ |
| Git worktree integration | ✓ | ✗ | ✗ | ✗ |
| Isolated contexts | ✓ | ✓ (panes) | ✓ (kernels) | ✗ |
| Shared knowledge | ✓ (memory docs) | ✗ | ✗ | ✗ |
| Fast switching | ✓ | ✓ | ✗ | ✗ |
| Terminal native | ✓ | ✓ | ✗ | ✗ |

**Key differences:** tmux does terminal multiplexing without AI context. Jupyter has multiple notebooks but no git integration. VS Code has single AI conversation. ikigai combines AI conversations + git worktrees + shared memory.

## Related Documentation

- [git-integration.md](git-integration.md) - Git worktree workflows
- [mark-rewind.md](mark-rewind.md) - Checkpoint system
- [commands.md](commands.md) - Command reference
- [workflows.md](workflows.md) - Example workflows
