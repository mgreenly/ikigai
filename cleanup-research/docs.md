# docs — cleanup findings

## High-priority (named migrations)

### Migration #1 — service names/registry now in top-level `registry/`
- docs/service-registry-design.md:71 — declares the authoritative `name → port` table lives "in **`appkit`**". Superseded: the registry is now a standalone, zero-dependency repo-root `registry/` module (see `registry/project/design/D01.md` + `product/product.md`, which explicitly says it does NOT edit `appkit`). The whole doc predates that decision.
- docs/service-registry-design.md:126-139 — "Source of truth and `manifest.env`" section still asserts the table lives in Go inside appkit and appkit's `manifest` verb emits `PORT`; contradicts the standalone-module design now being built.
- docs/service-registry-design.md:166 — "Decisions resolved / Authority — a literal `name → port` map in `appkit`" — same superseded authority claim.
- docs/app-layout.md:47-51 — forward note calls the service registry "designed-but-not-yet-implemented" and points to `service-registry-design.md`; it is now being implemented as the standalone `registry/` module, and the doc it points at carries the superseded "in appkit" placement.

### Migration #2 — deploy format now tar.gz bundle + three-symlink swap
- Non-archive docs are CURRENT (docs/app-layout.md and root deploy.md already describe the tar.gz bundle, versioned `libexec/etc/share` slots, and the three-symlink swap). Stale flat/`releases/`-style deploy descriptions survive only in archive:
- docs/archive/versioning.md:23,43,150,183-184 — old model: `/opt/<app>/releases/<version>/` layout, single `current` symlink swap, on-box `manifest` regeneration on deploy. (archived)
- docs/archive/adr-deployment-redesign.md:76,86,98-99,148-149,190,193-210,313-349 — single-binary artifact, single-symlink `current -> releases/<version>` swap, on-box manifest regen, per-binary `backup`/`restore` verbs + `Spec.Backup/Restore` hooks, `data/<app>.db` path. (archived)
- docs/archive/runbook-d2-ledger-box-prototype.md:185,236-257,382-471 — `/opt/ledger/releases/<version>/`, "regenerate manifest.env", `data/ledger.db` path, single `current` swap. (archived)
- docs/archive/runbook-dashboard-box-cutover.md — same-generation box-cutover runbook against the old releases/-dir model. (archived)

## Other stale info
- ✅ **DONE 2026-07-03** — docs/positioning-onepage.md:31-32 — lists the per-service binary "lifecycle verbs" as `serve, version, manifest, migrate, backup, and restore`. Stale: `backup`/`restore` were removed from the binary verb set (now `serve/version/manifest/migrate/schema`); backup/restore are box-level `opsctl` operations. NOT archived — genuinely misleading. (superseded verb set)

## Notes
- ✅ **RESOLVED 2026-07-03** (removed the reference) — docs/README.md is MISSING. Root CLAUDE.md and the design/plan convention both point to `docs/README.md`, but no such file exists; the matching two-doc (`<slug>-design.md` → `<slug>-plan.md` → `/finish`) convention content sits at docs/archive/README.md, which self-labels the surrounding folder as holding "pre-convention documents." So the live pointer is broken and the convention doc appears to have been archived — a future agent following CLAUDE.md will hit a dead path. **Action taken:** the `docs/README.md` pointer was deleted from root `AGENTS.md`/`CLAUDE.md` (the self-contained "In short:" convention summary was kept). `docs/README.md` remains intentionally absent. (Note per-service work now uses the `project/` spine convention seen in `registry/project/`, distinct from this suite-level two-doc flow.)
- Archive caveat: docs/app-layout.md:195-196 already explicitly marks `docs/archive/versioning.md` and `docs/archive/adr-deployment-redesign.md` as "Superseded … predate the state/+cache/ split," so their staleness is documented and expected — flagged above only for completeness. All docs/archive/ deploy-model docs are intentionally historical.
- docs/backups-design.md and docs/app-layout.md are self-labeled "normative target / normative — code in compliance" forward docs; they describe old-vs-new deliberately and are internally consistent, so not treated as stale. app-layout.md:83,98 still lists the `backups/` tier as "vestigial" while backups-design.md:148-155 says that tier "disappears" — a minor unresolved internal divergence, both acknowledged, not misleading.
