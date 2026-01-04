Run tests and fix failures using sub-agents.

**Usage:**
- `/check` - Run make check and fix test failures

**Action:** Executes the harness script which runs `make check`, parses failures from XML reports, and spawns sub-agents to fix failing tests. Uses escalation ladder (sonnet:think → opus:think → opus:ultrathink) and commits on success.

---

Run `.claude/scripts/check-all --no-spinner` and report the results.
