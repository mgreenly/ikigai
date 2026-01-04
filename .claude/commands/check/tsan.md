Run ThreadSanitizer checks and fix data races using sub-agents.

**Usage:**
- `/check-tsan` - Run make check-tsan and fix data races

**Action:** Executes the harness script which runs `make check-tsan`, parses failures, and spawns sub-agents to fix data races. Uses escalation ladder (sonnet:think → opus:think → opus:ultrathink) and commits on success.

---

Run `check-tsan --no-spinner` and report the results.
