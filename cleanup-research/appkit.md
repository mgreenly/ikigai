# appkit — cleanup findings

## High-priority (named migrations)
- none

Neither the registry-folder migration nor the tar.gz-bundle deploy migration
surfaced any stale text inside `appkit/`. The design docs correctly treat
`opsctl` as unchanged and describe the current `etc/current` symlink layout;
`bin/registry`/`REGISTRY_ROOT` references are consistent (see Notes for the one
open question).

## Other stale info
- appkit/manifest/manifest.go:2 — Package doc says the manifest is "the deploy-time identity file at /opt/<app>/etc/manifest.env." That is exactly the retired **sibling** path: this service's own design D4 (project/design/D04.md:1-16) says `/opt/<app>/etc/manifest.env` is dead and the on-box readable manifest now lives at `/opt/<app>/etc/current/manifest.env`. The comment points a future reader at a path that no longer exists on the box. (stale deploy-layout comment) — ✅ **DONE** (on-box path repointed to `/opt/<app>/etc/current/manifest.env`; the other `etc/manifest.env` refs on lines 9/49/63 are the committed source file and were correctly left alone)

## Notes
- The design docs (project/design/D01–D04, design.md, INDEX.md) and the build-loop
  prompts (project/loops/*.md) are the CURRENT, in-place design for the
  "manifest read-path through `current`" fix and are internally consistent —
  inventory.go, config.go, and manifest.go match them (inventory globs
  `*/etc/current/manifest.env`). Not stale.
- Possible tension with the registry migration (needs cross-folder confirmation,
  outside appkit/ scope): appkit treats the per-service `manifest.env` as the
  source of truth for service names/inventory (inventory.go derives Name from
  `env["APP"]`; D02 describes `bin/registry` reading manifests). If the top-level
  `registry/` folder is now the authoritative owner of service names/inventory,
  these manifest-as-source descriptions may be partially superseded. Could not
  verify from inside appkit/ — flagging for the coordinator, not asserting stale.
- The `Layout.ManifestPath` accessor that D4 (D04.md:9) said to correct/remove no
  longer exists in the code — that cleanup appears already done. Not stale.
- Comments referencing the "deleted bin/build run-wrapper" (config.go:4,
  appkit.go — PLAN refs) are historical rationale for code that moved into Go,
  not stale claims about current state.
