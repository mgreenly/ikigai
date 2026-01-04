Run Valgrind Memcheck and fix memory errors using sub-agents.

**Usage:**
- `/check-valgrind` - Run make check-valgrind and fix memory errors

**Action:** Executes the harness script which runs `make check-valgrind`, parses failures, and spawns sub-agents to fix memory errors. Uses escalation ladder (sonnet:think → opus:think → opus:ultrathink) and commits on success.

---

Run `.claude/scripts/check-valgrind --no-spinner` and report the results.
