# webhooks — cleanup findings

## High-priority (named migrations)
- webhooks/project/design/design.md:47 — Loopback port is described as "first free port; verified against all `*/etc/manifest.env`". Port/name ownership is now the top-level `registry/` name→port table (which pins `webhooks` = 3006 in its Core block); "verify against manifest.env scanning" is the pre-registry selection method and contradicts registry-as-source-of-truth. (Caveat: registry has not yet migrated consumers and webhooks still ships PORT=3006 in manifest.env, so the literal 3006 remains correct — only the *how the port is decided* framing is superseded.)

## Other stale info
- ✅ **DONE 2026-07-03** — webhooks/project/design/D01.md:50 — Lists the chassis "fixed verbs" as `serve|version|manifest|migrate|schema|backup|restore`. `backup`/`restore` were removed from appkit (appkit/appkit.go:214-227 dispatches only serve|version|manifest|migrate|schema; appkit_test.go `TestDispatch_BackupRestoreRemovedAndSpecHasNoHooks` asserts they 404). Backup/restore are now box-level opsctl operations, not binary verbs. (superseded verb set / removed feature)
- webhooks/project/design/D08.md:51 — "Backup/restore use appkit's default SQLite snapshot verbs (no `Spec.Backup`/`Spec.Restore` override)." Those verbs and the `Spec.Backup`/`Spec.Restore` fields no longer exist (removed from appkit `Spec`); statement references a removed feature. (superseded verb set / removed feature)
- webhooks/Makefile:3 — "The bin/ scripts are the production deploy spine (setup/deploy/start/stop on the box)." Inaccurate/stale: on-box deploy is now `opsctl` (stage/deploy unpack the tar.gz bundle + three-symlink swap per root deploy.md); `setup`/`deploy` are opsctl verbs, not `bin/` scripts (no bin/setup or bin/deploy exist), and `bin/start`/`bin/stop` are LOCAL dev launchers, not on-box. (Shared boilerplate — identical text in crm/gmail Makefiles.) (deploy-tooling description)

## Notes
- webhooks/project/research/research.md:261-264 — the port map (crm 3100, ledger 3101, dropbox 3200, notify 3201, gmail 3202, webhooks 3006) matches the current `registry/` table exactly, so it is NOT stale despite predating registry. The accompanying "3006 is the first free port" phrasing (also design.md:47, research.md:263) is the pre-registry selection method but lands on the correct value.
- research.md:280 "dashboard auto-discovers MCP services by globbing `*/etc/manifest.env` (appkit/inventory)" is about MCP service inventory (still current — dashboard reads manifest.env), distinct from the registry name→port migration; left unflagged.
- nginx.conf:6 (`/opt/webhooks/etc/current/nginx.conf`) and research.md:283-286 deploy path (bin/bump→bin/ship→opsctl setup/stage/deploy) are consistent with the current tar.gz + `current`-symlink model; not stale.
- D07.md:7 "webhooks's fixed registry port" reads as consistent with the new registry (port owned by the registry), not stale.
- `plan/` dir skipped per scope.
