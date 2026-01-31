Detect dead code candidates.

**Usage:**
- `/check:prune` - List all dead code candidates (JSON output)
- `/check:prune --file src/foo.c` - Check specific file only

**Output:** JSON with `ok` and `items` fields:
```json
{"ok": false, "items": ["function:file:line", ...]}
{"ok": true}
```

**Note:** This only detects CANDIDATES. Use `/fix:prune` to verify and remove dead code via Ralph.

---

Run `.claude/scripts/check-prune {{args}}` and report the results.
