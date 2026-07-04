# eventplane — cleanup findings

## High-priority (named migrations)
- none

Notes on the two named migrations: eventplane is a **library, not a deployed
service**, so the tar.gz/opsctl-stage deploy-format migration does not touch it
(no bin release layout here). The `registry` type in `outbox/registry.go` is an
**event-type** registry (types a producer publishes), unrelated to the top-level
service-name `registry/` — not stale.

## Other stale info
- CLAUDE.md:8 — "It is a **sixth git repo under `ikigai/`**, wired for local dev by `ikigai/go.work`" contradicts the current single-mono-repo model (one `.git`; root `go.work`). Both the "sixth git repo" framing and the old `ikigai/` codename are stale. (architecture / dead path) ✅ **DONE 2026-07-03** (reworded to "a library module in the suite mono-repo, wired for local dev by the root `go.work`")
- CLAUDE.md:92 — "workspace mode via `ikigai/go.work`" — same stale `ikigai/` codename; the workspace file is the repo-root `go.work`. (dead path) ✅ **DONE 2026-07-03** ("via the root `go.work`")
- CLAUDE.md:32-35 — describes epoch re-mint on "the **binary `restore` verb** / `opsctl rollback`" and "the operator S3 `bin/restore`". The fixed verb set is `serve`/`version`/`manifest`/`migrate`/`schema` — there is **no `restore` binary verb**; restore/rollback is a box-level `opsctl` operation (see root CLAUDE.md and deploy.md rollback). The "binary restore verb" wording is superseded. (superseded verb set) ✅ **DONE 2026-07-03** (reworded; confirmed real trigger — `opsctl restore`/`opsctl rollback` call `removeGenerationFiles()` in opsctl/internal/opsctl/backup.go:311, deleting the `*.generation` sidecar in `cache/` so the next boot re-mints)
- outbox/outbox.go:305-306 — code comment: "any appkit restore re-mints (the **restore verb** removes the sidecar …), and the operator **bin/restore** does the same". Same non-existent `restore` verb reference as above. (superseded verb set) ✅ **DONE 2026-07-03** (reworded to `opsctl restore`/`opsctl rollback`)
- outbox/outbox.go:270-306 / feed.go:20 — references to being "restored from an older backup" are fine conceptually, but the surrounding mechanism is tied to the stale "restore verb" chokepoint described above; worth a second look when the restore/rollback wording is corrected. (follow-on to above) ✅ **REVIEWED — no change needed** (these describe generic restore-from-backup scenarios, not the stale verb naming; correct as-is)

## Notes
- The "binary restore verb" findings (CLAUDE.md:32-35, outbox.go:305-306) are medium-confidence: the epoch-remint-on-restore *behavior* is real and still needed; only the claim that it is triggered by a binary `restore` verb is stale (it's an `opsctl`-owned box-level operation). The prose should be reworded, not the mechanism removed.
- SKIPPED `project/plan/` per scope (no plan files present anyway — only `.keep`/`README`).
- `project/README.md` and the `project/` scaffold are placeholder/scaffold status ("no spec and no live build loop yet") — accurate self-description, not stale. ✅ **DONE anyway** (the "Status: scaffold" blockquote was removed here too as part of the suite-wide operator directive to strip scaffold/setup + `docs/` framing from every `project/README.md`; removed as an editorial call, not because it was stale, 2026-07-03).
