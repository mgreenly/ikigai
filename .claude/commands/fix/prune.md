Remove dead code via Ralph.

**Usage:**
- `/fix:prune` - Pick random candidate and run Ralph to verify/remove it
- `/fix:prune --function NAME` - Target specific function
- `/fix:prune --no-pr` - Skip pull request creation
- `/fix:prune --duration 4h` - Set time budget (default: 2h)
- `/fix:prune --model opus` - Use specific model (default: sonnet)

**Process:**
1. Runs check-prune to get candidates
2. Picks a random candidate (or specified function)
3. Generates `pruning-<function>-ralph-goal.md` in project root
4. Runs Ralph to verify and remove the function (or mark as false positive)

**Outcomes:**
- Function removed: PR created with the change
- False positive: Function added to whitelist, no changes

---

Run `.claude/scripts/fix-prune --no-spinner {{args}}` - this will exec into Ralph.
