# dropbox — cleanup findings

## High-priority (named migrations)
- ✅ **DONE 2026-07-03** — dropbox/etc/nginx.conf:12 — Stale registry note: "(When the service registry lands dropbox's port becomes 3200; update the literal below then.)" frames the service registry as not-yet-landed and the port as pending, but the port is already the literal 3200 and service names/inventory now live in the top-level `registry/`. The whole parenthetical is superseded.
- dropbox/project/design/D04.md:78-79 — Describes 3200 as "dropbox's fixed registry port" with templating framed around the registry not yet substituting it; reads as pre-registry-migration framing. (Milder than nginx.conf:12; the literal-port claim itself is still true.)

## Other stale info
- dropbox/CLAUDE.md:148 — Lists `internal/server` as an in-repo package ("routing, the RFC 9728 protected-resource metadata...requireIdentityHeaders, the ungated /health route..."). No `internal/server` directory exists; the chassis (appkit) owns the server/routing now (see design.md:55-71). Stale package layout. (dead path / superseded architecture)
- dropbox/CLAUDE.md:157-158 — Lists `internal/logging` and `internal/ids` as in-repo packages ("structured slog + request-id, ULID, carried unchanged"). Neither directory exists; only `internal/{db,dropbox,mcp,web}` are present. These moved to the appkit chassis. (dead path)
- dropbox/CLAUDE.md:116-161 — The "Package layout" section omits the `internal/web` package (landing page handler + embedded assets), which is now a real, shipped package (design.md:129-134). Incomplete/superseded layout. (superseded)
- ✅ **DONE 2026-07-03** — dropbox/project/design/design.md:60 — Lists the fixed appkit verbs as `serve/version/manifest/migrate/backup/restore`. The canonical verb set (root CLAUDE.md; dropbox/CLAUDE.md:242) is `serve/version/manifest/migrate/schema`; backup/restore are explicitly NOT binary verbs (box-level opsctl operations). This doc both adds removed verbs and omits `schema`. (superseded verb set — contradicts dropbox's own no-backup decision at CLAUDE.md:275)
- ✅ **DONE 2026-07-03** — dropbox/CLAUDE.md:21 — `project/notes/PLAN.md` cited as "the full dropbox design". No `project/notes/` directory exists; the design now lives in `project/design/`. (dead path)
- ✅ **DONE 2026-07-03** — dropbox/project/product/product.md:15 — References `project/notes/PLAN.md` as owning the daemon/MCP/feed decisions. Dead path (see above). (dead path)
- ✅ **DONE 2026-07-03** — dropbox/project/design/design.md:19 — References `dropbox/project/notes/PLAN.md` as the owner of the existing domain. Dead path. (dead path)
- ✅ **DONE 2026-07-03** — dropbox/etc/nginx.conf:28 — Comment cites "PLAN.md §4" as the source for the /content identity-header guard. Dead reference (`project/notes/PLAN.md` no longer exists). (dead path)

## Notes
- dropbox/project/README.md:15 says end-user docs live in `dropbox/docs/`, but no `docs/` directory exists. Reads as aspirational rather than a factual pointer; flagging as uncertain.
- References to `bin/run` symlink (cmd/dropbox/main_test.go:153-156) are NOT stale — the current tar.gz/three-symlink deploy (root deploy.md:136) swaps `bin/run`, `etc/current`, `share/current`. Left unflagged.
- `project/plan/` subtree was skipped per scope rules.
