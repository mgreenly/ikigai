Run sanitizer checks and fix errors using sub-agents.

**Usage:**
- `/check-sanitize` - Run make check-sanitize and fix sanitizer errors

**Action:** Executes the harness script which runs `make check-sanitize` (ASan + UBSan), parses failures, and spawns sub-agents to fix memory/undefined behavior errors. Uses escalation ladder (sonnet:think → opus:think → opus:ultrathink) and commits on success.

---

Run `.claude/scripts/check-sanitize --no-spinner` and report the results.
