# sites — cleanup findings

## High-priority (named migrations)
- none — sites contains no flat-bin/single-binary/per-binary-swap deploy language (docs use current `bin/ship`/`bin/bump`), and nothing that hardcodes an old naming scheme or contradicts registry-as-source-of-truth. `manifest.env` (PORT=3004) and D04.md:93 ("fixed registry port") are consistent with the top-level `registry/` inventory.

## Other stale info
- ✅ **DONE 2026-07-03** — project/design/design.md:59 — lists the fixed binary verbs as `serve/version/manifest/migrate/backup/restore`. This is a superseded verb set: the current set is `serve/version/manifest/migrate/schema` (see cmd/sites/main.go:6 and root CLAUDE.md), and `backup`/`restore` are explicitly NOT binary verbs — they are box-level ops owned by `opsctl`. (superseded verb set)

## Notes
- design.md:56 / D04.md:93 pin PORT=3004 as a literal; this matches registry/ (sites 3004) and CLAUDE.md's :3000–:3006 range, so not flagged.
- D02.md:14 / sites/internal/sites/sync.go reference DROPBOX_BASE_URL default http://127.0.0.1:3200 — verified correct against dropbox/etc/manifest.env (PORT=3200); not stale.
- Many "service name" mentions in product/design refer to the landing-page display string ("sites"), not the naming/inventory scheme, so they are not registry-migration contradictions.
