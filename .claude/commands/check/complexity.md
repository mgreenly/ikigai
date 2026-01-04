Run complexity checks and fix complex functions using sub-agents.

**Usage:**
- `/check-complexity` - Run make check-complexity and fix complex functions

**Action:** Executes the harness script which runs `make check-complexity`, parses failures, and spawns sub-agents to reduce function complexity. Uses escalation ladder (sonnet:think → opus:think → opus:ultrathink) and commits on success.

---

Run `.claude/scripts/check-complexity --no-spinner` and report the results.
