Run Valgrind Helgrind and fix thread errors using sub-agents.

**Usage:**
- `/check-helgrind` - Run make check-helgrind and fix thread errors

**Action:** Executes the harness script which runs `make check-helgrind`, parses failures, and spawns sub-agents to fix thread synchronization errors. Uses escalation ladder (sonnet:think → opus:think → opus:ultrathink) and commits on success.

---

Run `check-helgrind --no-spinner` and report the results.
