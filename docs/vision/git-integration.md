# Git Integration

**The Killer Feature:** Git-aware multi-agent parallelization with automatic worktree management.

## Why This Matters

Most AI coding agents treat git as an afterthought. ikigai makes git a first-class citizen.

**The Problem with Other Agents:**
- Work on a single branch
- Require manual branch switching
- Risk context confusion across branches
- No parallel feature development

**ikigai's Solution:**
- Each agent can own a git worktree on its own branch
- Physical isolation: separate directories, no branch switching
- Parallel development: multiple features simultaneously
- Git-aware status line and commands
- Seamless merge workflows

## Core Concept: Agent = Branch = Worktree

```
project/
├── main/                   # Root worktree (main branch)
│   ├── .git/
│   └── src/
│
└── .worktrees/
    ├── oauth-impl/         # Helper worktree (oauth-impl branch)
    │   └── src/
    └── error-refactor/     # Helper worktree (refactor-errors branch)
        └── src/
```

**Key Insight:** Root agent stays on `main`, helper agents work on feature branches in `.worktrees/`. Each agent has its own working directory with no branch switching conflicts.

## Getting Started

### Create Agent with Worktree

```bash
/agent new oauth-impl --worktree
```

**What happens:**
1. Creates new branch `oauth-impl` from current HEAD
2. Creates worktree in `.worktrees/oauth-impl/`
3. Creates new agent with working directory in worktree
4. Switches to new agent

### Agent Works in Isolation

```bash
[Agent: oauth-impl] [Branch: oauth-impl] [Worktree: .worktrees/oauth-impl]

[You] Implement OAuth token validation
[Assistant creates files, runs tests, makes commits]
```

**Meanwhile, root agent continues:** Switch back with `Ctrl-\ m`. Both agents work simultaneously with no branch switching or context confusion.

### Merge When Ready

```bash
/agent merge

Merging oauth-impl → main...
  3 commits merged
  All tests pass
Merge complete!
```

### Clean Up

```bash
/agent close

Closing agent oauth-impl...
  Deleting worktree
  Deleting branch
  Memory documents preserved
Switched to main agent
```

## Workflows

### Parallel Feature Development

Develop multiple features simultaneously without branch switching.

```bash
# Root agent on main
/agent new oauth-impl --worktree
/agent new error-handling --worktree
/agent new api-research  # No worktree for research

# Check status
/agent list

# Merge features as they complete
/agent oauth-impl
/agent merge && /agent close
```

**Result:** All features developed in parallel, merged cleanly, repository stays organized.

### Experimental Branches

Try multiple approaches, keep the best.

```bash
/mark before-experiments

/agent new experiment-a --worktree
[Implement and benchmark approach A]

/agent new experiment-b --worktree
[Implement and benchmark approach B]

# Compare results, keep best
/agent experiment-b
/agent merge && /agent close

# Discard others
/agent experiment-a
/agent close --no-merge
```

### Long-Running Refactors

Major refactor in isolation while main branch stays stable.

```bash
/agent new refactor-errors --worktree
[Refactor works for hours/days in separate worktree]

# Meanwhile, main agent continues
Ctrl-\ m
[Work on other features, bug fixes on main]

# When refactor complete
/agent refactor-errors
/agent merge && /agent close
```

### Bug Fixes on Multiple Branches

Fix bugs without disrupting current work.

```bash
# Working on feature
[Agent: main] [Branch: feature-api]

# Bug reported on main
/agent new bugfix-auth --worktree --branch=bugfix/auth-leak
[Fix bug, write test, verify fix]

# Merge to main
/agent merge main
/agent close

# Back to feature work
Ctrl-\ m
```

## Git Commands

### /branch - Branch Status

```bash
/branch

Current Branch: oauth-impl
Worktree: .worktrees/oauth-impl

Status:
  Modified:   src/auth.c
  Untracked:  tests/unit/auth_test.c

Ahead of main by 3 commits
```

### /branch --all - All Branches

Shows all worktrees and branches across all agents.

### /commit - Commit Changes

```bash
/commit "Implement OAuth token validation"

# Or without message to open editor
/commit
```

### /diff - Show Changes

```bash
/diff              # All changes
/diff src/auth.c   # Specific file
```

## Advanced Patterns

### Stacked Branches

Create branches from branches:

```bash
# Main agent on feature-api
/agent new api-auth --worktree --branch=feature-api-auth

# Work on api-auth based on feature-api
# Merge back to feature-api first, then to main
```

### Worktree Reuse

```bash
/agent close --keep-branch

# Later, create new agent for same worktree
/agent new oauth-impl --worktree --branch=oauth-impl
```

### Manual Git Operations

ikigai doesn't lock you in:

```bash
# In another terminal
cd .worktrees/oauth-impl
git status
git rebase main
git push origin oauth-impl

# ikigai stays in sync - status line updates automatically
```

## Status Line Integration

The status line always shows git context:

```bash
[Agent: main] [Branch: main] [Clean]
[Agent: oauth-impl] [Branch: oauth-impl] [Modified: 3] [Ahead: 2]
[Agent: research] [No worktree]
```

**Indicators:** `[Clean]`, `[Modified: N]`, `[Ahead: N]`, `[Behind: N]`, `[Conflict]`, `[No worktree]`

## Memory Documents and Git

Memory documents are stored in PostgreSQL, **outside** the git repository.

**Why:** Available across all worktrees, persist after branches are deleted, not tied to git history, can reference multiple commits/branches.

**Example:**

```bash
# In oauth-impl agent
[You] Document the OAuth implementation patterns
Created #mem-156 (oauth/patterns)

# In any other agent/worktree
[You] Implement auth using #mem-156
[Assistant references patterns from memory doc]
```

Memory docs can include metadata about git state (branch, commits, worktree).

## Implementation Details

### Worktree Creation

```c
res_t create_agent_worktree(const char *agent_name, const char *branch_name) {
    // 1. Create branch from HEAD
    TRY(exec_git_command("git branch %s", branch_name));

    // 2. Create worktree
    char worktree_path[PATH_MAX];
    snprintf(worktree_path, sizeof(worktree_path), ".worktrees/%s", agent_name);
    TRY(exec_git_command("git worktree add %s %s", worktree_path, branch_name));

    // 3. Set agent working directory
    agent->cwd = talloc_strdup(agent, worktree_path);

    return OK(worktree_path);
}
```

### Git Status Monitoring

Called periodically to update status line:

```c
res_t update_git_status(ik_agent_t *agent) {
    if (!agent->worktree_path)
        return OK(NULL);

    // Run git status --porcelain
    TRY(exec_git_command_in_dir(agent->worktree_path, "git status --porcelain", &output));

    agent->git_status.modified_count = count_modified(output);
    agent->git_status.untracked_count = count_untracked(output);

    // Check ahead/behind
    TRY(exec_git_command_in_dir(agent->worktree_path, "git rev-list --count HEAD..main", &output));
    agent->git_status.behind_count = atoi(output);

    return OK(NULL);
}
```

### Merge Operation

```c
res_t merge_agent_branch(ik_agent_t *agent, const char *target_branch) {
    if (!agent->worktree_path)
        return ERR(ERR_INVALID_STATE, "Agent has no worktree");

    char *main_worktree = get_main_worktree_path();
    res_t merge_result = exec_git_command_in_dir(main_worktree, "git merge %s", agent->branch_name);

    if (IS_ERR(merge_result)) {
        append_to_scrollback("Merge conflict! Please resolve manually.");
        return merge_result;
    }

    return OK(NULL);
}
```

## Comparison with Other Agents

| Feature | ikigai | Cursor | Aider |
|---------|--------|--------|-------|
| Multi-agent support | ✓ | ✗ | ✗ |
| Git worktree integration | ✓ | ✗ | ✗ |
| Parallel feature dev | ✓ | ✗ | ✗ |
| Branch-aware status | ✓ | ✗ | ✗ |
| Automatic worktree mgmt | ✓ | ✗ | ✗ |
| Physical isolation | ✓ | ✗ | ✗ |

**Result:** ikigai automates git workflows, enabling 10x faster development with no mental overhead.

## Git Configuration

Configure git behavior in `~/.ikigai/config.json`:

```json
{
  "git": {
    "worktree_base": ".worktrees",
    "merge_strategy": "merge",
    "delete_merged_branches": true,
    "cleanup_worktrees_on_close": true,
    "status_update_interval": 5
  }
}
```

## Best Practices

### 1. One Feature = One Agent

Create agent for each logical feature with worktree.

### 2. Research Agents Don't Need Worktrees

Use plain agents for research that creates memory docs.

### 3. Merge Early, Merge Often

Don't let branches diverge. Merge logical chunks promptly.

### 4. Use Memory Docs for Shared Knowledge

Document patterns that span branches in memory docs.

### 5. Name Branches Descriptively

Use names like `oauth-impl`, `refactor-error-handling` instead of `feature1`, `temp`.

## Future Enhancements

**GitHub/GitLab Integration:** Create PRs directly from agents with `/pr create`.

**Visual Branch Tree:** See branch hierarchy with `/branch --tree`.

**Cross-Branch Context:** Reference code from another branch automatically.

## Related Documentation

- [multi-agent.md](multi-agent.md) - Multi-agent conversation design
- [commands.md](commands.md) - Complete command reference
- [workflows.md](workflows.md) - Example workflows
