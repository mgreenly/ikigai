# Why Release Branches with Worktrees?

**Decision**: Use dedicated release branches with git worktrees for all release development, never work directly on main.

**Rationale**:

**Primary reason**: AI agents cannot reliably ignore deprecated code. When rebuilding functionality, the old implementation must be completely removed from the filesystem before the new implementation begins. Otherwise, agents will reference and integrate the wrong code regardless of instructions.

**Secondary benefits**:
- **Main stays pristine**: Users never see degraded functionality during delete-then-rebuild refactors
- **Parallel isolation**: Multiple worktrees allow simultaneous work on hotfixes and features without branch switching
- **Clear release boundaries**: Branch lifetime matches release lifecycle (creation → tag)
- **Bisect-friendly history**: Every commit on main is functional, making git bisect reliable
- **Uniform workflow**: Same pattern for all releases eliminates decision fatigue

**Workflow Structure**:

```
main (always pristine, always builds clean)
  ↓ branch
rel-NN (release branch, may have degraded state during refactors)
  ↓ branch
feature-XYZ (individual features, pass quality gates before merge)
  ↓ squash merge
rel-NN (accumulates feature commits)
  ↓ FF-merge when complete
main → tag vN.N.N
```

**Worktree Layout**:

```
projects/ikigai/              # bare repo
  ├── main/                   # main worktree (never has release/ folder)
  ├── rel-NN/                 # release worktree (release/ folder lives here)
  ├── feature-XYZ/            # feature worktree (implementation)
  └── .archives/rel-NN/       # optional manual archive of release/ folder
```

**The release/ Folder**:
- Gitignored everywhere
- Only exists in release worktree during active development
- Contains: plan, tasks, user-stories, research artifacts (CDD pipeline)
- Deleted after FF-merge to main (ephemeral planning artifact)
- Optionally archived outside git if preservation needed

**Quality Gates**:
- Features must pass all checks before squash merge to release branch
- Release branch must pass all checks before FF-merge to main
- All checks: unit tests, coverage, sanitizers (ASan, UBSan, TSan), valgrind, helgrind
- No degraded commits reach main

**Hotfix Handling**:
- Branch from main, fix, quality gates, merge to main
- Finish all active feature branches
- Rebase release branch onto updated main
- Continue feature development

**Alternatives Considered**:

**Work directly on main**:
- Rejected: Main would have degraded state during delete-then-rebuild refactors
- Rejected: Users pulling main mid-release would experience broken functionality
- Rejected: Cannot guarantee "main always builds clean" during transitions

**Conditional branching** (release branch only for destructive refactors):
- Rejected: Decision fatigue ("do I need a branch this time?")
- Rejected: Inconsistent patterns confuse both humans and agents
- Rejected: Instructions must document two workflows

**Feature flags**:
- Rejected: Adds complexity to code
- Rejected: Doesn't solve AI agent confusion (both code paths still visible)
- Rejected: Violates "no backwards-compat hacks" principle

**Trade-offs**:

**Pros**:
- Main never degraded
- AI agents never see deprecated code
- Bisect always works (every main commit functional)
- Parallel development via worktrees
- Clear release boundaries
- One workflow, no special cases

**Cons**:
- Rebase overhead when main advances (hotfixes)
- Extra branch management
- Must finish features before rebasing release branch

**Design Principle**: The overhead of release branches is trivial compared to the cost of AI agents integrating wrong code or users experiencing degraded main. Uniformity and reliability outweigh convenience.
