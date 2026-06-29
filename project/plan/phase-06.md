# Phase 06 — appkit state/cache config boundary + boot reconstruction

*Realizes design Decision 5 (the state/ ÷ cache/ backup boundary — config slice) and Decision 6 (boot-reconstruction invariant — the boot slice). Depends on no earlier phase.*

appkit's config resolves the service's DB path under `state/` and its event-plane
generation sidecar under `cache/`, the two directed **independently** (not
`db + ".generation"`), via `<APP>_DB_PATH` / `<APP>_GENERATION_PATH`. appkit's boot
path satisfies the boot-reconstruction invariant: on startup it (re)creates its
non-state region — `cache/` and any `runs/`-style dirs — assuming none of it
pre-exists, so a restore that wipes the non-state region leaves the service able
to come back. The generation sidecar continues to be minted lazily when absent.

The D5 archive/restore round-trip ids (R-46XM, R-49DF, R-4ALB) are realized by the
mechanism that makes and consumes archives — Phase 08.

**Done when:** `bin/test` exits 0 and:
- R-485J-7TWG — with the unit-style env applied, the resolved DB path is under
  `state/` and the resolved generation path is under `cache/`, independently
  directed (appkit config unit test).
- R-4E91-4OLX — a service started against an **empty** `/opt/<svc>/` non-state
  region (no `cache/`, no sidecar) recreates the dirs, mints the sidecar, and
  reaches a healthy state (real filesystem + the service binary boot smoke).
