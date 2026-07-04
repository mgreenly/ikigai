# design — cleanup findings

## High-priority (named migrations)
- none (this folder is a UI design-token system; nothing touches service names/registry or deploy format)

## Other stale info
- design/carbon.md:6 — references `design-bible.html` as "the full living reference" but no such file exists anywhere in the repo (dead file path) — ✅ **DONE** (dead clause deleted)
- design/tokens.css:6 — declares "Source of truth: design-bible.html (theme: Carbon)" — same nonexistent file; a future agent told to edit the authoritative source would have nowhere to go (dead file path / broken source-of-truth pointer) — ✅ **DONE** (dead pointer line removed; this file IS the source)

## Broader scope (found during cleanup, beyond design/)
The same dead `design-bible.html` source-of-truth comment was copied into the
per-service `tokens.css` at `<svc>/internal/web/static/tokens.css:6` for all 11
UI services (crm, cron, dropbox, ledger, gmail, notify, prompts, scripts, sites,
wiki). These were **repointed to `design/tokens.css`** (the real upstream). — ✅ **DONE**

## Notes
- The only files present are carbon.md, tokens.css, example.html — all internally consistent with each other. The staleness is solely the two pointers to the missing design-bible.html.
- No plan/ subdirectory exists under design/, so nothing was skipped.
