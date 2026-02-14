Create and queue a dead code removal goal.

**Usage:**
- `/fix:prune` - Pick random candidate and create a goal
- `/fix:prune --function NAME` - Target specific function

**Process:**
1. Runs check-prune to get candidates
2. Picks a random candidate (or specified function)
3. Creates a goal via ralph-plans API
4. Queues the goal

**Outcomes:**
- Goal created and queued via ralph-plans
- Ralph service will pick up and execute the goal

---

Run `.claude/scripts/fix-prune {{args}}`
