Run all quality checks in a fix-point loop until stable.

**Usage:**
- `/check-quality` - Run all harnesses in sequence until clean pass

**Action:** Executes harnesses in order: filesize → check → complexity → sanitize → tsan → valgrind → helgrind → coverage. If any step makes commits, restarts from the beginning. Stops on first unrecoverable failure.

---

Run `.claude/scripts/check-quality --no-spinner` and report the results.
