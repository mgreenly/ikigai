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

**Always exists:**
- Created on ikigai startup
- Default name: "main"
- Can be renamed: `/agent rename <name>`
- Cannot be closed
- Typically works on main branch in primary directory

**Your home base:**
- Long-running conversation
- Coordinates overall work
- You switch to helpers as needed

### Helper Agents

**Created on demand:**
- `/agent new [name]` - Create helper
- Optional worktree for physical isolation
- Ephemeral - close when done
- Own conversation history and context

**Use cases:**
- Feature development on separate branch
- Research without worktree
- Refactoring in isolation
- Parallel experimental work

## Creating Agents

### Basic Agent (No Worktree)

```bash
/agent new research

Agent (research): Hello! What should I work on?

[You]
Research OpenAI streaming API patterns and create a memory doc

[Assistant]
I'll research comprehensively and document the findings...
[Researches, creates #mem-streaming]
Done! Created #mem-124 (openai/streaming)

# Switch back to main
Ctrl-\ m

# Reference research
[You]
Implement streaming using #mem-124

[Assistant]
[References memory doc]
Based on the research...
```

**Best for:**
- Research tasks
- Documentation
- Analysis
- Planning

### Agent with Worktree

```bash
/agent new oauth-impl --worktree

Creating agent oauth-impl...
  ✓ Created branch oauth-impl
  ✓ Created worktree .worktrees/oauth-impl
  ✓ Switched to worktree

Agent (oauth-impl): Hello! What should I work on?

[You]
Implement OAuth authentication module

[Assistant]
I'll implement the OAuth module in src/auth/...
[Creates files, tests, commits]
```

**Best for:**
- Feature development
- Refactoring
- Experiments
- Any code changes

## Switching Between Agents

### Quick Switch (Ctrl-\)

Press `Ctrl-\` to open agent switcher:

```
┌─────────────────────────────────────┐
│ Switch to Agent:                    │
│                                     │
│ > _                                 │
│                                     │
│ main - Branch: main                 │
│ oauth-impl - Branch: oauth-impl     │
│ research - No worktree              │
└─────────────────────────────────────┘
```

Type to fuzzy search, Enter to switch.

### Toggle (Ctrl-\ Ctrl-\)

Double-tap `Ctrl-\` to toggle between current and last agent:

```bash
# In main
Ctrl-\ Ctrl-\  → oauth-impl

# In oauth-impl
Ctrl-\ Ctrl-\  → main
```

**Perfect for:**
- Checking on background work
- Quick back-and-forth
- Coordinating parallel work

### Jump to Main (Ctrl-\ m)

From any agent, `Ctrl-\ m` jumps to root:

```bash
# In any helper
Ctrl-\ m  → main agent
```

### Command-Based Switch

```bash
/agent <name>

/agent main
/agent oauth-impl
/agent research
```

Supports fuzzy matching:
```bash
/agent oauth     → oauth-impl
/agent res       → research
```

## Listing Agents

```bash
/agent list

Active Agents:
• main (you are here) - Branch: main
  Worktree: ~/projects/ikigai/main
  Messages: 45 | Marks: 0
  Last activity: now

• oauth-impl - Branch: oauth-impl
  Worktree: ~/projects/ikigai/main/.worktrees/oauth-impl
  Messages: 23 | Marks: 2
  Last activity: 5 minutes ago
  Ahead of main by 3 commits

• research - No worktree
  Messages: 8 | Marks: 0
  Last activity: 15 minutes ago
  Created: #mem-124, #mem-125
```

**Shows:**
- Agent name and status
- Branch and worktree (if applicable)
- Message count and marks
- Last activity time
- Git status (commits ahead/behind)
- Memory documents created

## Closing Agents

### Standard Close

```bash
/agent close

# If agent has worktree:
Close agent oauth-impl?
  • Merge branch oauth-impl to main
  • Delete worktree
  • Delete branch
  • Preserve memory documents

Continue? [Y/n] y

Merging oauth-impl → main... ✓
Deleting worktree... ✓
Deleting branch... ✓
Agent closed

Switched to main agent
```

### Close Without Merge

```bash
/agent close --no-merge

# Discard work
Close agent experiment-a without merging?
  ⚠ Branch will be deleted
  ⚠ Work will be lost
  ✓ Memory documents preserved

Continue? [y/N] y

Deleting worktree... ✓
Deleting branch... ✓
Agent closed
```

### Keep Branch

```bash
/agent close --keep-branch

# Merge but don't delete branch
Merging oauth-impl → main... ✓
Deleting worktree... ✓
Branch oauth-impl preserved
Agent closed
```

## Agent Naming

### Random Names

Default names use PascalCase adjective + Muppet character:

```bash
/agent new
Agent (BouncyGonzo): Hello!

/agent new
Agent (CheerfulFozzie): Hello!

/agent new
Agent (SparklingKermit): Hello!
```

**Benefits:**
- Memorable
- Easy to type
- Impossible to conflict
- Fun!

**Adjectives:** Bouncy, Cheerful, Sparkling, Analytic, Creative, Swift, Clever, Wise, Bold, Bright...

**Muppets:** Gonzo, Fozzie, Kermit, Rowlf, Animal, Scooter, Beaker, Bunsen, Statler, Waldorf...

### Custom Names

```bash
/agent new oauth-impl
Agent (oauth-impl): Hello!

/agent new api-research
Agent (api-research): Hello!
```

**Best for:**
- Descriptive feature names
- Project-specific naming
- Worktree-backed agents

### Renaming

```bash
/agent rename dev

Root agent renamed to "dev"

[Agent: dev] [Branch: main]
```

**Use case:** Rename root agent to match project context.

## Context Isolation

Each agent has completely isolated context:

### Independent Conversation History

```bash
# In main agent
[You]
Let's implement OAuth

[Assistant]
I'll design the OAuth flow...

# Switch to oauth-impl
/agent oauth-impl

[You]
What are we working on?

[Assistant]
I don't have prior context. What should I work on?

# No pollution from main agent!
```

### Independent Working Directory

```bash
# Main agent
[Agent: main] [Branch: main]
pwd → /home/user/projects/ikigai/main

# oauth-impl agent
[Agent: oauth-impl] [Branch: oauth-impl]
pwd → /home/user/projects/ikigai/main/.worktrees/oauth-impl
```

All file operations relative to agent's directory.

### Independent Scrollback

```bash
# Clear in one agent
/clear

# Other agents unaffected
/agent oauth-impl
# Conversation still intact
```

### Shared Memory Documents

**Only** memory documents are shared:

```bash
# In oauth-impl agent
[Create #mem-oauth/patterns]

# In main agent
[You]
Implement auth using #mem-oauth/patterns

[Assistant]
[References shared memory doc]
```

## Workflows

### Background Research

```bash
# In main agent
[You]
I need to research OpenAI streaming, but I want to keep working here

/agent new research

[You]
Research OpenAI streaming API thoroughly. Create memory doc when done.

[Assistant]
I'll research comprehensively...

# Switch back to main
Ctrl-\ m

[Continue working on current task]

# Check on research later
Ctrl-\ research

[Assistant]
Research complete! Created #mem-124 (openai/streaming)

# Close research agent
/agent close

# Use findings
Ctrl-\ m
[You]
Implement streaming using #mem-124
```

### Parallel Features

```bash
# Create multiple feature agents
/agent new oauth --worktree
[Specify OAuth work]

Ctrl-\ m
/agent new error-refactor --worktree
[Specify refactor work]

Ctrl-\ m
/agent new api-endpoints --worktree
[Specify API work]

# All work in parallel
# Switch between as needed
# Merge when each completes

Ctrl-\ oauth
/agent merge
/agent close

Ctrl-\ error
/agent merge
/agent close

Ctrl-\ api
/agent merge
/agent close
```

### Experimental Approaches

```bash
# Try multiple solutions
/mark before-experiments

/agent new approach-a --worktree
[Try solution A]

Ctrl-\ m
/agent new approach-b --worktree
[Try solution B]

# Compare
Ctrl-\ approach-a
[You]
What are the results?

Ctrl-\ approach-b
[You]
What are the results?

# Keep best
Ctrl-\ approach-b
/agent merge

# Discard others
Ctrl-\ approach-a
/agent close --no-merge
```

### Coordinated Work

```bash
# Main agent coordinates
[Agent: main]

[You]
I need three things:
1. Research OAuth patterns
2. Implement token validation
3. Write integration tests

# Delegate to helpers
/agent new research
[You]
Research OAuth patterns, create memory doc

/agent new oauth-impl --worktree
[You]
Implement token validation using patterns from research agent when ready

/agent new testing --worktree
[You]
Write integration tests for OAuth when implementation is ready

# Check progress
/agent list

# Coordinate handoffs
Ctrl-\ research
[Research complete]

Ctrl-\ oauth-impl
[You]
Research is done, see #mem-oauth/patterns

Ctrl-\ testing
[You]
Implementation in progress in oauth-impl agent

# Merge as complete
[Close agents when done]
```

## Agent Status Line

Always shows current context:

```bash
[Agent: main] [Branch: main] [Clean]

[Agent: oauth-impl | +2] [Branch: oauth-impl] [Modified: 3]

[Agent: research | +3] [No worktree]
```

**Indicators:**
- Agent name
- `| +N` - N other agents exist
- Current branch
- Git status (Clean, Modified, etc.)
- Worktree presence

## Memory Documents

Shared knowledge layer across all agents.

### Creating Memory Docs

```bash
# In any agent
[You]
Document the OAuth implementation patterns

[Assistant]
I'll create a comprehensive document...

Created #mem-156 (oauth/patterns) - OAuth Implementation Patterns

Sections:
- Token validation approach
- Refresh logic
- Error handling
- Security considerations
```

### Referencing Memory Docs

```bash
# In any other agent
[You]
Implement authentication using #mem-156

[Assistant]
[Loads memory doc into context]
Based on the OAuth patterns in #mem-156...
```

### Listing Memory Docs

```bash
/memory list

Memory Documents:
#mem-124 (openai/streaming) - OpenAI Streaming Patterns
  Created by: research | Size: 2.4KB

#mem-156 (oauth/patterns) - OAuth Implementation Patterns
  Created by: oauth-impl | Size: 3.1KB

#mem-178 (testing/patterns) - Integration Test Patterns
  Created by: testing | Size: 1.8KB
```

### Organizing with Aliases

```bash
/memory alias 124 openai/streaming
/memory alias 156 oauth/patterns
/memory alias 178 testing/integration

# Reference by alias
[You]
Implement using #mem-oauth/patterns
```

## Best Practices

### 1. One Agent per Feature

Don't overload agents:

```bash
# Good
/agent new oauth-impl --worktree
[Focus on OAuth only]

/agent new error-handling --worktree
[Focus on errors only]

# Less good
/agent new big-feature --worktree
[Try to do OAuth + errors + API + tests]
```

### 2. Research Agents Don't Need Worktrees

```bash
# Good
/agent new research
[Research only, create memory docs]

# Unnecessary
/agent new research --worktree
[Worktree created but never used]
```

### 3. Close Agents When Done

Don't accumulate stale agents:

```bash
# After feature complete
/agent merge
/agent close

# Not
[Leave agents open indefinitely]
```

### 4. Use Memory Docs for Shared Knowledge

```bash
# Good
[Agent creates #mem-patterns]
[Other agents reference #mem-patterns]

# Less good
[Copy-paste patterns between agent conversations]
```

### 5. Name Agents Descriptively

```bash
# Good
/agent new oauth-impl
/agent new error-refactor
/agent new api-research

# Less clear
/agent new temp
/agent new test
/agent new new-thing
```

## Advanced Patterns

### Agent Handoff

```bash
# Agent A does initial work
[Agent: impl-a]
[Create #mem-initial-impl]
/agent close

# Agent B continues
[Agent: main]
/agent new impl-b --worktree
[You]
Continue the work from #mem-initial-impl
```

### Nested Agents

```bash
# Main coordinates
[Agent: main]

# Level 1 helpers
/agent new frontend --worktree
/agent new backend --worktree

# Frontend agent creates sub-agent
[Agent: frontend]
/agent new ui-components --worktree --branch=frontend-ui

# Hierarchical work
```

### Agent Templates

```bash
# Future: agent templates
/agent new oauth-impl --template=feature

Auto-configured:
  ✓ Created worktree
  ✓ Added pre-commit hooks
  ✓ Initialized test structure
  ✓ Created #mem-oauth/notes
```

## Implementation Notes

### Agent Structure

```c
typedef struct ik_agent_t {
    char *name;                       // Agent name
    char *branch_name;                // Git branch (or NULL)
    char *worktree_path;              // Worktree path (or NULL)

    ik_message_t **messages;          // Conversation history
    size_t message_count;

    ik_mark_t **marks;                // Checkpoint stack
    size_t mark_count;

    ik_scrollback_t *scrollback;      // Display buffer

    int64_t db_agent_id;              // Database ID
    time_t created_at;
    time_t last_activity;

    // Git status
    struct {
        int modified_count;
        int untracked_count;
        int ahead_count;
        int behind_count;
    } git_status;
} ik_agent_t;
```

### Agent Manager

```c
typedef struct ik_agent_manager_t {
    ik_agent_t *root_agent;           // Always exists
    ik_agent_t **helper_agents;       // Dynamic array
    size_t helper_count;

    ik_agent_t *current_agent;        // Currently active
    ik_agent_t *previous_agent;       // For toggle

    ik_memory_store_t *memory_store;  // Shared memory docs
} ik_agent_manager_t;
```

### Switching Logic

```c
res_t switch_to_agent(ik_agent_manager_t *mgr, const char *name) {
    // Find agent by name (fuzzy match)
    ik_agent_t *target = find_agent_fuzzy(mgr, name);
    if (!target)
        return ERR(ERR_NOT_FOUND, "Agent not found");

    // Save previous for toggle
    mgr->previous_agent = mgr->current_agent;

    // Switch current
    mgr->current_agent = target;

    // Change working directory if worktree
    if (target->worktree_path) {
        chdir(target->worktree_path);
    }

    // Update status line
    update_status_line(mgr);

    // Redraw scrollback
    render_scrollback(target->scrollback);

    return OK(target);
}
```

## Comparison with Other Tools

| Feature | ikigai | tmux/screen | Jupyter | VS Code |
|---------|--------|-------------|---------|---------|
| Multiple conversations | ✓ | N/A | ✓ (notebooks) | ✗ |
| Git worktree integration | ✓ | ✗ | ✗ | ✗ |
| Isolated contexts | ✓ | ✓ (panes) | ✓ (kernels) | ✗ |
| Shared knowledge | ✓ (memory docs) | ✗ | ✗ | ✗ |
| Fast switching | ✓ | ✓ | ✗ | ✗ |
| Terminal native | ✓ | ✓ | ✗ | ✗ |

**Key differences:**
- tmux: Terminal multiplexing, no AI context
- Jupyter: Multiple notebooks, but no git integration
- VS Code: Single AI conversation
- ikigai: AI conversations + git worktrees + shared memory

## Related Documentation

- [git-integration.md](git-integration.md) - Git worktree workflows
- [mark-rewind.md](mark-rewind.md) - Checkpoint system
- [commands.md](commands.md) - Command reference
- [workflows.md](workflows.md) - Example workflows
