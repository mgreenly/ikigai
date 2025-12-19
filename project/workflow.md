# Workflow

## Macro Workflow

```
        ┌──────────────────┐
        │    1. Research   │◀──┐
        └────────┬─────────┘   │
                 │             │
              pass ╌╌ fail ────┘
                 │
                 ▼
        ┌──────────────────┐
        │   2. Planning    │◀──┐
        └────────┬─────────┘   │
                 │             │
              pass ╌╌ fail ────┘
                 │
                 ▼
        ┌──────────────────┐
        │ 3. Implementation│◀──┐
        └────────┬─────────┘   │
                 │             │
              pass ╌╌ fail ────┘
                 │
                 ▼
        ┌──────────────────┐
        │   4. Quality     │◀──┐
        └────────┬─────────┘   │
                 │             │
              pass ╌╌ fail ────┘
                 │
                 ▼
        ┌──────────────────┐
        │ 5. Refactor Code │◀──┐
        └────────┬─────────┘   │
                 │             │
              pass ╌╌ fail ────┘
                 │
                 ▼
        ┌──────────────────┐
        │   6. Quality     │◀──┐
        └────────┬─────────┘   │
                 │             │
              pass ╌╌ fail ────┘
                 │
                 ▼
        ┌──────────────────┐
        │ 7. Refactor Tests│◀──┐
        └────────┬─────────┘   │
                 │             │
              pass ╌╌ fail ────┘
                 │
                 ▼
        ┌──────────────────┐
        │   8. Quality     │◀──┐
        └────────┬─────────┘   │
                 │             │
              pass ╌╌ fail ────┘
                 │
                 ▼
        ┌──────────────────┐
        │      DONE        │
        └──────────────────┘
```

## Quality Check (`/quality`)

The quality check runs seven verification steps in sequence: lint, check, sanitize, tsan, valgrind, helgrind, and coverage. Each step either passes or fails. When any step fails (and is fixed), the pipeline restarts from lint to ensure earlier checks still pass after the changes. Success requires all seven steps to pass in a single run with no code modifications.

```
                              ┌─────────────────────────────────────┐
                              │                                     │
                              ▼                                     │
                        ┌───────────┐                               │
                        │   lint    │──── fail ─────────────────────┤
                        └─────┬─────┘                               │
                             pass                                   │
                              │                                     │
                              ▼                                     │
                        ┌───────────┐                               │
                        │   check   │──── fail ─────────────────────┤
                        └─────┬─────┘                               │
                             pass                                   │
                              │                                     │
                              ▼                                     │
                        ┌───────────┐                               │
                        │ sanitize  │──── fail ─────────────────────┤
                        └─────┬─────┘                               │
                             pass                                   │
                              │                                     │
                              ▼                                     │
                        ┌───────────┐                               │
                        │   tsan    │──── fail ─────────────────────┤
                        └─────┬─────┘                               │
                             pass                                   │
                              │                                     │
                              ▼                                     │
                        ┌───────────┐                               │
                        │ valgrind  │──── fail ─────────────────────┤
                        └─────┬─────┘                               │
                             pass                                   │
                              │                                     │
                              ▼                                     │
                        ┌───────────┐                               │
                        │ helgrind  │──── fail ─────────────────────┤
                        └─────┬─────┘                               │
                             pass                                   │
                              │                                     │
                              ▼                                     │
                        ┌───────────┐                               │
                        │ coverage  │──── fail ─────────────────────┘
                        └─────┬─────┘
                             pass
                              │
                              ▼
                        ┌───────────┐
                        │  SUCCESS  │
                        └───────────┘
```
