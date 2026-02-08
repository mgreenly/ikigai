Create and queue a dead code removal goal.

**Usage:**
- `/fix:prune` - Pick random candidate and create a goal
- `/fix:prune --function NAME` - Target specific function

**Process:**
1. Runs check-prune to get candidates
2. Picks a random candidate (or specified function)
3. Creates a GitHub issue goal from the template
4. Queues the goal for orchestrator pickup

**Outcomes:**
- Goal created and queued with label `goal:queued`
- Orchestrator will pick up and execute the goal
- No ralph process spawned, no local goal file written

---

Run `.claude/scripts/fix-prune {{args}}`
