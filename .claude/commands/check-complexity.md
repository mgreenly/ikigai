Run complexity checks and fix complex functions using sub-agents.

**Usage:**
- `/check-complexity` - Run make complexity and fix complex functions

**Action:** Executes the harness script which runs `make complexity`, parses failures, and spawns sub-agents to reduce function complexity. Uses escalation ladder (sonnet:think → opus:think → opus:ultrathink) and commits on success.

---

Run `check-complexity --no-spinner` and report the results.
