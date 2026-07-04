# gmail — cleanup findings

## High-priority (named migrations)
- ✅ **DONE 2026-07-03** — gmail/etc/nginx.conf:12-13 — Comment "(When the service registry lands gmail's port becomes 3202; update the literal below then.)" frames the service registry as a future/unlanded thing to update later. The top-level `registry/` now exists and is the source of truth for names/inventory/ports; the port is already 3202. The parenthetical is stale registry-migration framing.

## Other stale info
- gmail/project/README.md:9-16 — "**Status: scaffold.** ... gmail has **no spec and no live build loop yet** — the spine documents below are empty placeholders." Contradicted by reality: `project/product/product.md` and `project/design/design.md` are fully written, `project/design/D01.md`–`D08.md` + `INDEX.md` exist, and `project/loops/{gather,build,verify}.md` + `run` are present. (contradictory status)
- ✅ **DONE 2026-07-03** — gmail/project/design/design.md:57-58 — Lists the fixed verbs as "`serve`/`version`/`manifest`/`migrate`/`backup`/`restore`". CLAUDE.md defines the fixed verb set as serve/version/manifest/migrate/schema and states backup/restore are **not** binary verbs (owned by opsctl). The canonical list in `cmd/gmail/main.go:7-8` correctly reads serve/version/manifest/migrate/schema. design.md's verb set is superseded. (superseded verb set)
- gmail/project/design/D05.md:13-21 — The "gmail note" claims "gmail does not yet carry a `gmail/AGENTS.md`/`CLAUDE.md`; its identity doctrine currently lives in the `cmd/gmail/main.go` package comment." But `gmail/AGENTS.md` now exists (with `CLAUDE.md` as its symlink) and already states the landing-page truth with no "no UI" line — i.e. the D05 work is done and this pre-state note is stale. (superseded / done-work note)

## Notes
- gmail/etc/nginx.conf:5-6 and D04.md:61 describe the `opsctl setup gmail` symlink to `/opt/.../etc/current/nginx.conf` — this is the CURRENT symlink-swap model, not stale; no flat-bin/per-binary-copy deploy language found anywhere in `gmail/`.
- D04.md:61 refers to "this service's fixed registry port" — this reads as consistent with registry-as-source-of-truth, so not flagged as stale (unlike nginx.conf's future-tense framing).
