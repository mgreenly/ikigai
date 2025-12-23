# Task: Update Documentation for OpenAI Migration

**Model:** sonnet/none
**Depends on:** cleanup-openai-source.md, cleanup-openai-tests.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.


## Preconditions

- [ ] Clean worktree (verify: `git status --porcelain` is empty)

## Pre-Read

**Skills:**
- `/load source-code` - Verify path references

**Source:**
- `project/` - Project documentation
- `.claude/library/source-code/SKILL.md` - Source map to update

## Objective

Find and update all documentation references to the old `src/openai/` paths, replacing them with the new `src/providers/openai/` paths.

## Behaviors

**Step 1: Find all references**
```bash
grep -rn 'src/openai/' project/ .claude/
grep -rn 'tests/unit/openai/' project/ .claude/
```

**Step 2: Update each file found**

For each file containing old paths, update:
- `src/openai/client.c` → `src/providers/openai/client.c`
- `src/openai/http_handler.c` → `src/providers/common/http_multi.c`
- `src/openai/sse_parser.c` → `src/providers/common/sse_parser.c`
- `tests/unit/openai/` → `tests/unit/providers/openai/`

**Step 3: Update source-code skill**

The `.claude/library/source-code/SKILL.md` file lists all source files. Update the OpenAI section:

**Old:**
```markdown
## OpenAI Client

- `src/openai/client.c` - OpenAI API client...
- `src/openai/http_handler.c` - Low-level HTTP...
- `src/openai/sse_parser.c` - Server-Sent Events parser...
```

**New:**
```markdown
## Provider System

### Common Infrastructure
- `src/providers/common/http_multi.c` - HTTP client with curl...
- `src/providers/common/sse_parser.c` - Server-Sent Events parser...

### OpenAI Provider
- `src/providers/openai/client.c` - OpenAI provider implementation...
- `src/providers/openai/adapter.c` - Provider vtable adapter...
- `src/providers/openai/request.c` - Request serialization...
- `src/providers/openai/response.c` - Response parsing...
- `src/providers/openai/streaming.c` - Streaming handler...
```

**Step 4: Verify no stale references**
```bash
# Must return empty
grep -rn 'src/openai/' project/ .claude/ | grep -v 'src/providers/openai/'
```

## Files to Update

Run grep to identify, then update each:

| File | Section to Update |
|------|-------------------|
| `.claude/library/source-code/SKILL.md` | OpenAI Client section |
| `project/README.md` | If references OpenAI paths |
| `project/architecture.md` | If references OpenAI paths |
| Any ADRs mentioning `src/openai/` | Update paths |

## Postconditions

- [ ] `grep -rn 'src/openai/' project/ .claude/ | grep -v providers` returns empty
- [ ] `.claude/library/source-code/SKILL.md` updated with new provider structure
- [ ] All documentation references updated to `src/providers/` paths
- [ ] No broken links or stale path references
- [ ] Changes committed to git with message: `task: cleanup-openai-docs.md - <summary>`
  - If `make check` passed: success message
  - If `make check` failed: add `(WIP - <reason>)` and return `{"ok": false, "reason": "..."}`
- [ ] Clean worktree (verify: `git status --porcelain` is empty)



## Success Criteria

Return `{"ok": true}` only if all postconditions are met.
Return `{"ok": false, "reason": "..."}` if validation fails (still commit the WIP).