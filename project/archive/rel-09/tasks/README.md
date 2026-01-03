# rel-08 Tasks

Task files for orchestrated execution of the web search feature implementation.

## Task Execution

Tasks are tracked in `.claude/data/tasks.db` and executed via the `/orchestrate` skill.

Execution order is defined in `order.json`. Array position determines sequence.

## Task Files

| Task | Group | Status |
|------|-------|--------|
| (none yet) | | Pending |

## Task Lifecycle

```
pending → in_progress → done
              ↓
         escalate → pending (higher capability)
              ↓
            fail (max level reached)
```

## Commands

- `/task-init` - Initialize database
- `/task-import` - Import from this directory
- `/task-next` - Get next pending task
- `/task-start <name>` - Mark in_progress
- `/task-done <name>` - Mark complete
- `/task-fail <name>` - Mark failed
- `/task-escalate <name>` - Bump model/thinking level
- `/task-list [status]` - List tasks
- `/task-stats` - Show metrics

See `.claude/library/task/SKILL.md` for full documentation.
