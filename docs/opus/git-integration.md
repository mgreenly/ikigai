# Git Integration

**Status:** Early design discussion, not finalized. Some details need experience to refine.

## Overview

Helper agents work in git worktrees for branch isolation. Sub-agents inherit their parent's working directory.

## Behavior by Environment

### Git Repository (Bare or Regular)

**Main agent:**
- Runs in current branch/worktree
- For bare repo, typically the `main` worktree

**Helper agents:**
- Each runs in its own worktree
- Agent name = branch name
- Branch auto-created from HEAD (with user confirmation)
- Required: helper agents cannot share branch with main agent

**Creating a helper agent:**
```
/agent new feature-auth
```
- Creates branch `feature-auth` if it doesn't exist
- Creates worktree for that branch
- Agent works in the new worktree

**Re-creating a closed agent:**
```
/agent new feature-auth
```
- If worktree exists, uses it
- Agent resumes in existing worktree

### Not a Git Repository

**Main agent:**
- Runs in current directory

**Helper agents:**
- Run in same directory as main agent
- No branch isolation
- All agents share working directory

### Sub-Agents

- Inherit parent's working directory
- No worktree creation
- No git involvement

## Worktree Lifecycle

**Creation:**
- Happens when helper agent is created
- User confirms branch/worktree creation

**Persistence:**
- Worktrees persist after agent is closed
- Agent state (conversation) is separate from worktree

**Cleanup:**
- Manual or future tooling
- Separate concern from agent lifecycle

## Status Line

Git state shown in status line:
- Current branch
- Dirty/clean state
- Ahead/behind remote

## Git Commands

No built-in git commands. Agents use bash for git operations.

Git usage patterns documented in `.agents/skills/git.md` for consistency.

## Open Questions (Need Experience)

- Confirmation UX for branch/worktree creation
- Handling dirty working directory when switching agents
- Merge workflow patterns
- Worktree cleanup tooling
- Behavior when branch already exists but no worktree

## Related

- [agents.md](agents.md) - Agent types and lifecycle
- [architecture.md](architecture.md) - Threading model
