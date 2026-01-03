Run coverage checks and fix gaps using sub-agents.

**Usage:**
- `/check-coverage` - Run make check-coverage and fix coverage gaps

**Action:** Executes the harness script which runs `make check-coverage`, parses gaps, and spawns sub-agents to add missing test coverage. Uses escalation ladder (sonnet:think → opus:think → opus:ultrathink) and commits on success.

---

Run `check-coverage --no-spinner` and report the results.
