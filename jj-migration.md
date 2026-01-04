# Git to Jujutsu (jj) Migration

This document tracks the conversion of all git references in `.claude/` to use jj instead.

## Command Mappings

| Git Command | jj Equivalent | Notes |
|-------------|---------------|-------|
| `git status --porcelain` | `jj status` | Different output format |
| `git status` | `jj status` | |
| `git add <file>` | (not needed) | jj auto-tracks all files |
| `git add -A` | (not needed) | jj auto-tracks all files |
| `git commit -m "msg"` | `jj commit -m "msg"` | Creates new working copy automatically |
| `git checkout -- .` | `jj restore` | Restore working copy to parent |
| `git checkout -- <file>` | `jj restore <file>` | Restore specific file |
| `git clean -fd` | (not needed) | jj tracks everything, use `jj restore` |
| `git reset --hard` | `jj restore` | |
| `git diff` | `jj diff` | |
| `git log` | `jj log` | Different format, more visual |
| `git push` | `jj git push` | Pushes bookmarks to git remote |
| `git push origin <branch>` | `jj git push --bookmark <bookmark>` | |
| `git push --force` | `jj git push --bookmark <bookmark>` | jj handles force-push automatically |
| `git pull` | `jj git fetch` + `jj rebase` | Separate fetch and rebase |
| `git fetch` | `jj git fetch` | |
| `git branch <name>` | `jj bookmark create <name>` | Bookmarks replace branches |
| `git checkout -b <name>` | `jj new` + `jj bookmark create <name>` | Or just `jj new` and name later |
| `git checkout <branch>` | `jj edit <bookmark>` | Edit existing commit |
| `jj new <bookmark>` | Create new commit on top of bookmark |
| `git merge` | `jj new <a> <b>` | Creates merge commit |
| `git rebase` | `jj rebase -d <dest>` | |
| `git revert HEAD` | `jj backout -r @-` | Creates inverse commit |
| `git stash` | (not needed) | jj auto-saves everything |
| `git tag -a v1.0 -m "msg"` | `jj git push --bookmark v1.0` | Use bookmark, push as tag |
| `git bisect` | `jj` has no built-in bisect | Use `jj log` + manual testing |

## Conceptual Differences

### No Staging Area
- jj has no index/staging area
- All changes are automatically part of the working copy commit
- `git add` commands should be removed entirely

### Working Copy is Always a Commit
- In jj, `@` (working copy) is always a commit being edited
- No need to "create a branch before working"
- Bookmarks are just names pointing to commits

### Bookmarks vs Branches
- jj "bookmarks" = git "branches"
- Bookmarks are optional - commits exist independently
- Multiple bookmarks can point to the same commit

### Clean Workspace Checks
- Git: `git status --porcelain` returns empty string if clean
- jj: `jj status` says "The working copy has no changes." or "Working copy changes:"
- For scripting: check if `jj diff --stat` has output

---

## Files to Update

### Skills (Full Rewrite)

#### `.claude/library/git/SKILL.md`
**Action:** Rename to `jj/SKILL.md`, rewrite entirely
- Replace all git commands with jj equivalents
- Update conceptual explanations
- Remote config: `github.com:mgreenly/ikigai.git` (unchanged, jj uses git remotes)

#### `.claude/library/git-strict/SKILL.md`
**Action:** Rename to `jj-strict/SKILL.md`, rewrite entirely
- Same quality gate requirements, different commands
- Tag creation: `jj bookmark create v1.0 && jj git push --bookmark v1.0`

#### `.claude/library/scm/SKILL.md`
**Action:** Rewrite with jj workflow
- "Never lose work" philosophy maps well to jj (everything is tracked)
- Remove git stash references
- Update checkpoint patterns

#### `.claude/library/release/SKILL.md`
**Action:** Update git commands in release process
- Lines 88-109: squash/tag/push workflow
- Lines 138-152: amend/filter-branch operations

#### `.claude/library/task-authoring/SKILL.md`
**Action:** Update "Git Workflow Requirements" section (lines 164-349)
- Clean workspace checks
- Commit patterns
- Revert patterns

### Skillsets (Update References)

#### `.claude/skillsets/developer.json`
**Action:** Change `"git"` to `"jj"` in preload array (line 6)

#### `.claude/skillsets/meta.json`
**Action:** Change `"git"` to `"jj"` in preload array (line 7)

#### `.claude/skillsets/implementor.json`
**Action:** Change `"git"` to `"jj"` in preload array (line 3)

### Commands (Update Embedded Operations)

#### `.claude/commands/orchestrate.md`
**Action:** Update precondition check (line 36)
- `git status --porcelain` → `jj diff --stat` (check for empty output)

#### `.claude/commands/prune.md`
**Action:** Update lines 36-93
- Status check, checkout, commit operations
- Reference to git skill → jj skill

#### `.claude/commands/coverage.md`
**Action:** Update line 65
- `/load git` → `/load jj`

#### `.claude/commands/refactor.md`
**Action:** Update lines 21, 30
- `git status --porcelain` → jj equivalent
- `git rev-parse HEAD` → `jj log -r @ --no-graph -T 'commit_id'`

#### `.claude/commands/skillset.md`
**Action:** Update descriptions (lines 10-11, 47-48)
- Change "git" mentions to "jj"

### Harness Scripts (Python - Shared Pattern)

All harness scripts use the same git helper functions. Each needs updating:

| File | Functions to Update |
|------|---------------------|
| `.claude/harness/check/run` | `git_get_modified_files`, `git_has_changes`, `git_commit`, `git_revert` |
| `.claude/harness/coverage/run` | `git_get_modified_files`, `git_has_changes`, `git_commit`, `git_revert` |
| `.claude/harness/complexity/run` | `git_get_modified_files`, `git_commit`, `git_revert` |
| `.claude/harness/filesize/run` | `git_get_modified_files`, `git_commit`, `git_revert` |
| `.claude/harness/helgrind/run` | `git_get_modified_files`, `git_commit`, `git_revert` |
| `.claude/harness/prune/run` | `git_is_clean`, `git_get_modified_files`, `git_commit`, `git_revert` |
| `.claude/harness/sanitize/run` | `git_get_modified_files`, `git_commit`, `git_revert` |
| `.claude/harness/tsan/run` | `git_get_modified_files`, `git_commit`, `git_revert` |

**Replacement pattern for Python scripts:**

```python
# OLD
def git_get_modified_files() -> set[str]:
    code, stdout, _ = run_cmd(["git", "status", "--porcelain"])
    ...

def git_commit(...):
    run_cmd(["git", "add", f])
    run_cmd(["git", "commit", "-m", msg])

def git_revert(...):
    run_cmd(["git", "checkout", f])
    run_cmd(["git", "clean", "-fd"])

# NEW
def jj_get_modified_files() -> set[str]:
    code, stdout, _ = run_cmd(["jj", "diff", "--stat"])
    # Parse jj diff --stat output instead
    ...

def jj_commit(...):
    # jj auto-tracks, no add needed
    run_cmd(["jj", "commit", "-m", msg])

def jj_revert(...):
    run_cmd(["jj", "restore"])  # Restores all, or specific files
```

### Documentation (Minor Updates)

#### `.claude/README.md`
**Action:** Update lines 10, 105
- "gitignored" terminology still applies (jj respects .gitignore)
- No change needed if just referring to ignored files

#### `.claude/library/meta/SKILL.md`
**Action:** Update line 19
- "gitignored" → can keep as-is (jj uses .gitignore)

---

## Files to Remove

After creating the new jj skills, these directories should be deleted:

| Path | Reason |
|------|--------|
| `.claude/library/git/` | Replaced by `.claude/library/jj/` |
| `.claude/library/git-strict/` | Replaced by `.claude/library/jj-strict/` |

**Contents being removed:**
- `.claude/library/git/SKILL.md` - 52 lines
- `.claude/library/git-strict/SKILL.md` - 76 lines

**Note:** The `scm/SKILL.md` skill is being *updated*, not removed. It contains workflow patterns that apply to any VCS.

---

## Execution Order

1. **Create new skills first**
   - `.claude/library/jj/SKILL.md` (new)
   - `.claude/library/jj-strict/SKILL.md` (new)

2. **Update skillsets** (point to new skills)
   - `developer.json`, `meta.json`, `implementor.json`

3. **Update dependent skills**
   - `scm/SKILL.md`
   - `release/SKILL.md`
   - `task-authoring/SKILL.md`

4. **Update commands**
   - `orchestrate.md`, `prune.md`, `coverage.md`, `refactor.md`, `skillset.md`

5. **Update harness scripts**
   - All 8 Python scripts with new jj functions

6. **Delete old git skills**
   - `.claude/library/git/` directory
   - `.claude/library/git-strict/` directory

7. **Test**
   - Run `jj status` to verify working copy
   - Run a harness script to verify jj integration

---

## Resolved Questions

### 1. Modified File Detection

**Use `jj diff --summary`** - cleanest format for parsing:

```
A jj-migration.md     # Added
M src/main.c          # Modified
D old-file.txt        # Deleted
```

For scripting in Python:
```python
def jj_get_modified_files() -> set[str]:
    code, stdout, _ = run_cmd(["jj", "diff", "--summary"])
    if code != 0 or not stdout.strip():
        return set()
    files = set()
    for line in stdout.strip().split('\n'):
        if line:
            # Format: "A filename" or "M filename" or "D filename"
            parts = line.split(None, 1)
            if len(parts) == 2:
                files.add(parts[1])
    return files
```

### 2. Commit Hash Retrieval

**Use templates:**
```bash
jj log -r @ --no-graph -T 'commit_id'        # Full commit hash
jj log -r @ --no-graph -T 'commit_id.short()'  # Short hash
jj log -r @ --no-graph -T 'change_id'        # Change ID (jj's stable identifier)
```

Output examples:
- `commit_id`: `0af45143338fc23a8795bb1b7a40087684b502dd`
- `change_id`: `swkoqqwoxzlqsxotrqoqqqvwnwkmtwwy`

### 3. Force Push Handling

**jj handles this automatically.** There is no `--force` flag because:
- jj tracks bookmark movement and knows when force is needed
- Safety checks prevent accidental overwrites of others' work
- Use `jj git push --bookmark <name>` - it force-pushes when necessary

If push is rejected due to unknown remote state:
```bash
jj git fetch --remote origin
jj git push --bookmark <name>
```

### 4. Pre-commit Hooks

**jj does NOT support git hooks.** This is a fundamental design difference:

> "Pre-commit hooks fundamentally don't mesh with Jujutsu's 'everything is committed all the time' model. There's no staging area. As soon as you save a file, it's already committed."

**Workarounds:**
1. **Pre-push hooks** - Run checks before `jj git push` via shell alias
2. **CI enforcement** - Move checks to CI pipeline
3. **Aliases** - Use `jj util exec` in aliases for custom pre-commands

**For this project:** The pre-commit hook that rejects Claude attributions should be converted to either:
- A pre-push check, OR
- A CI check, OR
- A custom `jj` alias that runs checks before push

**Sources:**
- [jj-vcs/jj Discussion #403](https://github.com/jj-vcs/jj/discussions/403)
- [Automating Pre-Push Checks with Jujutsu](https://www.aazuspan.dev/blog/automating-pre-push-checks-with-jujutsu/)

### 5. Clean Workspace Detection

For scripting:
```python
def jj_is_clean() -> bool:
    code, stdout, _ = run_cmd(["jj", "diff", "--summary"])
    return code == 0 and not stdout.strip()
```

Or check `jj status` output for "The working copy has no changes."
