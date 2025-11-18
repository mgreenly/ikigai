# Git Integration

**The Killer Feature:** Git-aware multi-agent parallelization with automatic worktree management.

## Why This Matters

Most AI coding agents treat git as an afterthought. ikigai makes git a first-class citizen.

**The Problem with Other Agents:**
- Work on a single branch
- Require manual branch switching
- Risk context confusion across branches
- No parallel feature development
- Git commands hidden behind tools

**ikigai's Solution:**
- Each agent can own a git worktree on its own branch
- Physical isolation: separate directories, no branch switching
- Parallel development: multiple features simultaneously
- Git-aware status line and commands
- Seamless merge workflows

## Core Concept: Agent = Branch = Worktree

```
project/
├── main/                           # Root worktree (main branch)
│   ├── .git/                       # Git repository
│   ├── src/
│   └── [root agent works here]
│
└── .worktrees/
    ├── oauth-impl/                 # Helper worktree (feature-oauth branch)
    │   ├── src/
    │   └── [oauth-impl agent works here]
    │
    └── error-refactor/             # Helper worktree (refactor-errors branch)
        ├── src/
        └── [error-refactor agent works here]
```

**Key Insight:**
- **Root agent** stays on `main` in primary worktree
- **Helper agents** work on feature branches in `.worktrees/`
- Each agent has its own working directory
- No branch switching conflicts
- No context pollution

## Getting Started

### Create Agent with Worktree

```bash
# In root agent (main branch)
/agent new oauth-impl --worktree
```

**What happens:**
1. Creates new branch `oauth-impl` from current HEAD
2. Creates worktree in `.worktrees/oauth-impl/`
3. Creates new agent named "oauth-impl"
4. Switches to new agent
5. Agent's working directory is `.worktrees/oauth-impl/`

### Agent Works in Isolation

```bash
# Now in oauth-impl agent on feature-oauth branch
[Agent: oauth-impl] [Branch: oauth-impl] [Worktree: .worktrees/oauth-impl]

[You]
Implement OAuth token validation

[Assistant]
I'll create the OAuth module...
[Creates files, runs tests, makes commits]
```

**Meanwhile, root agent continues normal work:**

```bash
Ctrl-\ m  # Switch to main agent

[Agent: main] [Branch: main] [Worktree: ~/projects/ikigai/main]

[You]
Continue working on the API endpoints

[Assistant]
Looking at src/api/...
```

**Both agents work simultaneously:**
- No branch switching
- No merge conflicts
- No context confusion
- Each agent sees its own working tree

### Merge When Ready

```bash
# In oauth-impl agent
[You]
Tests all pass. Let's merge to main.

/agent merge

Merging oauth-impl → main...
  3 commits merged
  0 conflicts
  All tests pass
Merge complete!
```

### Clean Up

```bash
/agent close

Closing agent oauth-impl...
  Deleting worktree .worktrees/oauth-impl
  Deleting branch oauth-impl
  Memory documents preserved
Switched to main agent
```

## Workflows

### Parallel Feature Development

Develop multiple features simultaneously without branch switching.

```bash
# Root agent on main
[Agent: main] [Branch: main]

# Create first feature
/agent new oauth-impl --worktree
[Work on OAuth...]

# Switch back to main
Ctrl-\ m

# Create second feature
/agent new error-handling --worktree
[Work on error handling...]

# Create third feature (research, no worktree needed)
/agent new api-research
[Research API patterns, create memory docs...]

# Check status
/agent list

Active Agents:
• main - Branch: main
  Worktree: ~/projects/ikigai/main

• oauth-impl - Branch: oauth-impl
  Worktree: ~/projects/ikigai/main/.worktrees/oauth-impl
  3 commits ahead of main

• error-handling - Branch: refactor-errors
  Worktree: ~/projects/ikigai/main/.worktrees/error-handling
  1 commit ahead of main

• api-research - No worktree
  Created #mem-156 (api/patterns)
```

**Merge features as they complete:**

```bash
/agent oauth-impl
/agent merge
/agent close

/agent error-handling
/agent merge
/agent close

/agent api-research
# No merge needed, just close
/agent close

# All features integrated to main
# All memory docs preserved
# Clean repository state
```

### Experimental Branches

Try multiple approaches, keep the best.

```bash
# Checkpoint before experiments
/mark before-experiments

# Try approach A
/agent new experiment-a --worktree
[Implement approach A...]
[Run benchmarks, tests...]

# Try approach B
Ctrl-\ m
/agent new experiment-b --worktree
[Implement approach B...]
[Run benchmarks, tests...]

# Compare results
/agent experiment-a
[You]
What are the benchmark results?

[Assistant]
Approach A: 125ms avg latency...

/agent experiment-b
[You]
What are the benchmark results?

[Assistant]
Approach B: 89ms avg latency...

# Keep approach B
/agent experiment-b
/agent merge

# Discard approach A
/agent experiment-a
/agent close --no-merge
```

### Long-Running Refactors

Major refactor in isolation while main branch stays stable.

```bash
# Main agent on main branch
/agent new refactor-errors --worktree

# Refactor agent works for hours/days
[Agent: refactor-errors] [Branch: refactor-errors]

[You]
Refactor all error handling to use ik_result_t consistently

[Assistant]
Starting comprehensive refactor...
[Works through 20+ files]
[Runs full test suite after each change]
[Maintains 100% coverage]
[Makes incremental commits]

# Meanwhile, main agent continues
Ctrl-\ m
[Work on other features, bug fixes on main]

# When refactor complete
/agent refactor-errors
/agent merge
All tests pass
100% coverage maintained
Merge complete!

/agent close
```

### Bug Fixes on Multiple Branches

Fix bugs without disrupting current work.

```bash
# Working on feature
[Agent: main] [Branch: feature-api]

# Bug reported on main
/agent new bugfix-auth --worktree --branch=bugfix/auth-leak
[Fix bug in isolation]
[Write regression test]
[Verify fix]

# Merge to main
/agent merge main
/agent close

# Back to feature work
Ctrl-\ m
```

## Git Commands

### /branch - Branch Status

Show current branch and git status:

```bash
/branch

Current Branch: oauth-impl
Worktree: ~/projects/ikigai/main/.worktrees/oauth-impl

Status:
  Modified:   src/auth.c
  Modified:   src/auth.h
  Untracked:  tests/unit/auth_test.c

Ahead of main by 3 commits:
  abc1234 Implement OAuth token validation
  def5678 Add OAuth configuration
  ghi9012 Create auth module structure

Behind main by 0 commits
```

### /branch --all - All Branches

Show all worktrees and branches:

```bash
/branch --all

All Branches and Worktrees:

• main - ~/projects/ikigai/main (agent: main)
  Clean working directory
  Last commit: 5ae6b6e Implement OpenAI client module

• oauth-impl - ~/projects/ikigai/main/.worktrees/oauth-impl (agent: oauth-impl)
  Modified: 3 files
  Ahead of main by 3 commits
  Last commit: abc1234 Implement OAuth token validation

• refactor-errors - ~/projects/ikigai/main/.worktrees/error-refactor
  Clean working directory
  Ahead of main by 1 commit
  Last commit: xyz7890 Refactor error handling

No orphaned worktrees detected
```

### /commit - Commit Changes

Commit in current worktree:

```bash
# With message
/commit "Implement OAuth token validation"

[oauth-impl abc1234] Implement OAuth token validation
 3 files changed, 145 insertions(+), 12 deletions(-)
 create mode 100644 src/auth.c

# Without message (opens editor)
/commit

[Opens $EDITOR with template:]
# Commit message
#
# Changes to be committed:
#   modified:   src/auth.c
#   modified:   src/auth.h
#   new file:   tests/unit/auth_test.c
```

### /diff - Show Changes

```bash
/diff

diff --git a/src/auth.c b/src/auth.c
index 123abc..456def 100644
--- a/src/auth.c
+++ b/src/auth.c
@@ -10,6 +10,15 @@
+ik_result_t ik_auth_validate_token(const char *token) {
+    // Implementation
+}
...

# Specific file
/diff src/auth.c

# Staged vs unstaged
/diff --staged
```

## Advanced Patterns

### Stacked Branches

Create branches from branches:

```bash
# Main agent on feature-api
[Agent: main] [Branch: feature-api]

# Create sub-feature from feature-api
/agent new api-auth --worktree --branch=feature-api-auth

[Works on api-auth based on feature-api]

# Merge back to feature-api
/agent merge feature-api

# Then merge feature-api to main
/agent main
/agent merge main
```

### Worktree Reuse

Close agent but keep worktree:

```bash
/agent close --keep-branch

Agent closed
Branch oauth-impl preserved
Worktree preserved at .worktrees/oauth-impl

# Later, create new agent for same worktree
/agent new oauth-impl --worktree --branch=oauth-impl

Agent created for existing worktree
```

### Manual Git Operations

ikigai doesn't lock you in:

```bash
# Open another terminal
cd ~/projects/ikigai/main/.worktrees/oauth-impl
git status
git log
git rebase main
git push origin oauth-impl

# ikigai stays in sync
# Status line updates automatically
```

## Status Line Integration

The status line always shows git context:

```bash
[Agent: main] [Branch: main] [Clean]

[Agent: oauth-impl] [Branch: oauth-impl] [Modified: 3] [Ahead: 2]

[Agent: research] [No worktree]
```

**Indicators:**
- `[Clean]` - No uncommitted changes
- `[Modified: N]` - N modified files
- `[Untracked: N]` - N untracked files
- `[Ahead: N]` - N commits ahead of base branch
- `[Behind: N]` - N commits behind base branch
- `[Conflict]` - Merge conflicts present
- `[No worktree]` - Agent without worktree

## Memory Documents and Git

Memory documents are stored in PostgreSQL, **outside** the git repository.

**Why:**
- Available across all worktrees
- Persist after branches are deleted
- Not tied to git history
- Can reference multiple commits/branches

**Example:**

```bash
# In oauth-impl agent
[You]
Document the OAuth implementation patterns

[Assistant]
Created #mem-156 (oauth/patterns)

Includes:
- Token validation approach
- Refresh logic pattern
- Error handling strategy
- Relevant commits: abc1234, def5678

# In any other agent
[You]
Implement authentication using #mem-156

[Assistant]
[References patterns from memory doc]
[Works in different worktree]
[Applies same patterns]
```

**Memory docs can reference git state:**

```json
{
  "id": 156,
  "alias": "oauth/patterns",
  "content": "...",
  "metadata": {
    "branch": "oauth-impl",
    "commits": ["abc1234", "def5678", "ghi9012"],
    "worktree": ".worktrees/oauth-impl",
    "created_by": "oauth-impl"
  }
}
```

## Implementation Details

### Worktree Creation

```c
res_t create_agent_worktree(const char *agent_name, const char *branch_name) {
    // 1. Create branch from HEAD
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "git branch %s", branch_name);
    TRY(exec_git_command(cmd));

    // 2. Create worktree
    char worktree_path[PATH_MAX];
    snprintf(worktree_path, sizeof(worktree_path),
             ".worktrees/%s", agent_name);

    snprintf(cmd, sizeof(cmd),
             "git worktree add %s %s", worktree_path, branch_name);
    TRY(exec_git_command(cmd));

    // 3. Set agent working directory
    agent->cwd = talloc_strdup(agent, worktree_path);

    return OK(worktree_path);
}
```

### Git Status Monitoring

```c
// Called periodically (every N seconds or on demand)
res_t update_git_status(ik_agent_t *agent) {
    if (!agent->worktree_path)
        return OK(NULL);  // No worktree

    // Run git status --porcelain
    char *output;
    TRY(exec_git_command_in_dir(agent->worktree_path,
                                  "git status --porcelain",
                                  &output));

    // Parse output
    agent->git_status.modified_count = count_modified(output);
    agent->git_status.untracked_count = count_untracked(output);

    // Run git rev-list to check ahead/behind
    TRY(exec_git_command_in_dir(agent->worktree_path,
                                  "git rev-list --count HEAD..main",
                                  &output));
    agent->git_status.behind_count = atoi(output);

    TRY(exec_git_command_in_dir(agent->worktree_path,
                                  "git rev-list --count main..HEAD",
                                  &output));
    agent->git_status.ahead_count = atoi(output);

    return OK(NULL);
}
```

### Merge Operation

```c
res_t merge_agent_branch(ik_agent_t *agent, const char *target_branch) {
    if (!agent->worktree_path)
        return ERR(ERR_INVALID_STATE, "Agent has no worktree");

    // 1. Stash any uncommitted changes
    TRY(exec_git_command_in_dir(agent->worktree_path, "git stash"));

    // 2. Switch to main worktree
    char *main_worktree = get_main_worktree_path();

    // 3. Merge in main worktree
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "git merge %s", agent->branch_name);
    res_t merge_result = exec_git_command_in_dir(main_worktree, cmd);

    if (IS_ERR(merge_result)) {
        append_to_scrollback("Merge conflict! Please resolve manually.");
        return merge_result;
    }

    // 4. Success
    append_to_scrollback("Merge complete!");
    return OK(NULL);
}
```

## Comparison with Other Agents

| Feature | ikigai | Cursor | GitHub Copilot | Aider |
|---------|--------|--------|----------------|-------|
| Multi-agent support | ✓ | ✗ | ✗ | ✗ |
| Git worktree integration | ✓ | ✗ | ✗ | ✗ |
| Parallel feature dev | ✓ | ✗ | ✗ | ✗ |
| Branch-aware status | ✓ | ✗ | ✗ | ✗ |
| Automatic worktree mgmt | ✓ | ✗ | ✗ | ✗ |
| Physical isolation | ✓ | ✗ | ✗ | ✗ |

**Why this matters:**

Other agents require manual git management:
1. Create branch manually
2. Switch branch manually
3. Agent works
4. Switch back manually
5. Merge manually
6. Delete branch manually

ikigai automates this:
1. `/agent new feature --worktree`
2. Agent works in isolation
3. `/agent merge`
4. `/agent close`

**Result:** 10x faster workflows, no mental overhead, no mistakes.

## Git Configuration

Configure git behavior in `~/.ikigai/config.json`:

```json
{
  "git": {
    "worktree_base": ".worktrees",
    "auto_commit": false,
    "commit_message_template": "~/.ikigai/commit-template.txt",
    "merge_strategy": "merge",  // or "rebase"
    "delete_merged_branches": true,
    "cleanup_worktrees_on_close": true,
    "status_update_interval": 5  // seconds
  }
}
```

## Best Practices

### 1. One Feature = One Agent

Create agent for each logical feature:

```bash
/agent new oauth-impl --worktree
/agent new error-refactor --worktree
/agent new api-endpoints --worktree
```

### 2. Research Agents Don't Need Worktrees

Use plain agents for research:

```bash
/agent new research-oauth
# No worktree needed
# Creates memory docs
```

### 3. Merge Early, Merge Often

Don't let branches diverge:

```bash
# After completing logical chunk
/agent merge
/agent close

# Start next feature
/agent new next-feature --worktree
```

### 4. Use Memory Docs for Shared Knowledge

Document patterns that span branches:

```bash
# In oauth-impl agent
[Create #mem-oauth/patterns]

# In api-auth agent
[Reference #mem-oauth/patterns]
```

### 5. Name Branches Descriptively

Use descriptive names:

```bash
# Good
/agent new oauth-impl --worktree
/agent new refactor-error-handling --worktree

# Less good
/agent new feature1 --worktree
/agent new temp --worktree
```

## Future Enhancements

### GitHub/GitLab Integration

```bash
# Create PR directly from agent
/pr create

Creating pull request for branch oauth-impl:
  Title: Implement OAuth authentication
  Description: [Auto-generated from commits]
  Base: main
  Reviewers: [Auto-suggested]

PR created: https://github.com/user/repo/pull/123
```

### Visual Branch Tree

```bash
/branch --tree

* main (root agent)
  ├─* oauth-impl (oauth-impl agent) - 3 commits
  │  └─* oauth-tests (test-agent) - 1 commit
  ├─* error-refactor (refactor agent) - 5 commits
  └─* api-endpoints (api agent) - 2 commits
```

### Cross-Branch Context

```bash
# Reference code from another branch
[You]
Use the same pattern as in oauth-impl branch

[Assistant]
[Automatically checks out oauth-impl worktree]
[References code from that branch]
[Applies pattern to current branch]
```

## Related Documentation

- [multi-agent.md](multi-agent.md) - Multi-agent conversation design
- [commands.md](commands.md) - Complete command reference
- [workflows.md](workflows.md) - Example workflows
