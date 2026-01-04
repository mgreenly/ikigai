---
description: Find the most critical alignment gap in release plan.
---

**Requires:** `$CDD_DIR` environment variable must be set to the workspace directory.

Check top-down alignment: `$CDD_DIR/README.md` (goals) -> `$CDD_DIR/user-stories/` (user perspective) -> `$CDD_DIR/plan/` (implementation). Verify plan implements what README promises and user-stories demonstrate. Check `$CDD_DIR/verified.md` to avoid re-checking resolved items. Report the most critical alignment gap and suggest a fix.
