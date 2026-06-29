# Phase 09 — scheduled nightly box sweep

*Realizes design Decision 9 (scheduled nightly backup — systemd timer + box sweep). Depends on Phase 08 and Phase 08a (the backup it schedules, state + cert).*

opsctl writes a systemd timer during `init-box`/`setup` (the same mechanism as the
existing certbot renewal timer) that fires `opsctl backup --all` at 03:00
America/Chicago with `Persistent=true`. `opsctl backup --all` is a box sweep that
processes services one at a time (`stop · snapshot state/ → S3 · start`, D7) and
also takes the apex cert snapshot (no stop). The sweep is fail-soft: a failure on
one service or the cert is logged and the sweep continues, exiting non-zero if any
unit failed. Restore is never scheduled.

**Done when:** `bin/test` exits 0 and:
- R-RNKC-HAW8 — the backup timer unit opsctl writes carries
  `OnCalendar=*-*-* 03:00:00 America/Chicago` (the timezone present, not bare/UTC)
  and `Persistent=true` (structural assertion on the generated unit under a temp
  `SysRoot`).
- R-ROS8-V2MX — `opsctl backup --all` is fail-soft: with a stubbed `System` where
  one service's stop/snapshot fails, the sweep still backs up every other service
  and the cert, and exits non-zero (stubbed `System` + fake `ObjectStore`).
