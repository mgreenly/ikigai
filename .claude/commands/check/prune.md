Remove dead code using sub-agents.

**Usage:**
- `/check-prune` - Find and remove all dead code
- `/check-prune --limit 5` - Process only the first 5 candidates

**Action:** Executes the harness script which runs dead-code analysis, comments out functions to verify they're unused, fixes affected tests, and removes the functions. Uses escalation ladder and commits on success.

---

Run `.claude/scripts/check-prune --no-spinner {{args}}` and report the results.
