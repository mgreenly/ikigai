# Phase 44 — adopt webhooks into the new layout

*Realizes the webhooks slice of design Decision 8 (per-service adoption) and Decision 11 (authored portable manifest). Depends on Phase 23, 31, 32.*

webhooks adopts the new contract: it grows committed shipped tiers in its source tree mirroring the install
layout — `webhooks/etc/manifest.env` (its **authored, portable** manifest — no box paths) and
`webhooks/etc/nginx.conf` (its location fragment) — plus an optional `webhooks/share/…`. Its data paths resolve
via appkit `composeDataPaths` (Phase 23): DB under `state/`, generation sidecar under `cache/`, and the
non-state region is (re)created on boot (D06). No per-service backup/restore script remains; it inherits
`opsctl backup/restore webhooks` (D07).

**Done when:**
- A test (shell or Go) tagged `R-8DF1-W89F` asserts `webhooks/etc/manifest.env` is portable: no absolute box
  path (`/opt/…`) and no `<APP>_DB_PATH`/`<APP>_GENERATION_PATH` line.
- A Go unit test tagged `R-8IAN-FB87` asserts `manifest.Emit(webhooks-spec)` byte-agrees with the committed
  `webhooks/etc/manifest.env`.
- A boot smoke tagged `R-4LKF-FB23` asserts a freshly set-up `/opt/webhooks/` (DB under `state/`, sidecar under
  `cache/`, binary under `libexec/`, the three symlinks, the **authored** `etc/<v>/manifest.env`) boots and
  passes its `health` check.
- `bin/test` exits 0.
