Run file size checks and fix oversized files using sub-agents.

**Usage:**
- `/filesize` - Run make filesize and fix oversized files

**Action:** Executes the harness script which runs `make filesize`, parses failures, and spawns sub-agents to split oversized files. Uses escalation ladder (sonnet:think → opus:think → opus:ultrathink) and commits on success.

---

Run `check-filesize --no-spinner` and report the results.
