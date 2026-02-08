Story: #280

## Objective

Update `.claude/library/pipeline/SKILL.md` to document the dependency feature.

## Changes

### Goal Commands table

Add `--depends` to `goal-create` usage:
```
| `goal-create` | `--story <N> --title "..." [--spot-check] [--depends "N,M"] < body.md` | Create goal (draft) |
```

### Key Rules section

Add:
```
- **Dependencies** -- Goals can declare `Depends: #N, #M` in body; orchestrator waits for dependencies to reach `goal:done` before picking up the goal
```

## Acceptance Criteria

- Pipeline skill accurately reflects the new `--depends` flag
- Dependency behavior is documented

Story: #280
