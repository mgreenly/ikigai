# Phase 33 — adopt dashboard into the new layout

*Realizes the dashboard slice of design Decision 8 (per-service adoption) and Decision 11 (authored portable manifest). Depends on Phase 23, 31, 32.*

dashboard (the apex/`DEFAULT` app) adopts the new contract: committed `dashboard/etc/manifest.env`
(authored, portable) and `dashboard/etc/nginx.conf` (the apex location fragment), optional
`dashboard/share/…`. Data paths resolve via appkit `composeDataPaths` (Phase 23); non-state is recreated
on boot. The in-binary `Spec.Backup`/`Spec.Restore` S3 branch was already removed in Phase 31; dashboard
inherits `opsctl backup/restore dashboard`, and the apex TLS cert is the opsctl cert stream's concern
(D07, Phase 08a) — not authored here.

**Done when:**
- A test tagged `R-8DF1-W89F` asserts `dashboard/etc/manifest.env` is portable (no `/opt/…`, no
  `<APP>_DB_PATH`/`<APP>_GENERATION_PATH`).
- A Go unit test tagged `R-8IAN-FB87` asserts `manifest.Emit(dashboard-spec)` byte-agrees with the
  committed `dashboard/etc/manifest.env`.
- A boot smoke tagged `R-4LKF-FB23` asserts a freshly set-up `/opt/dashboard/` boots and passes `health`.
- `bin/test` exits 0.
