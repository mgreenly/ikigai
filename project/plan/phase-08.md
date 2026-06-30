# Phase 08 — opsctl backup/restore core + ObjectStore seam

*Realizes design Decision 7 (opsctl-owned backup/restore — the core verbs) and Decision 5 (the archive/restore round-trip ids R-46XM, R-49DF, R-4ALB). Depends on Phase 04 (the tree / state/ to snapshot) and Phase 06 (the state÷cache boundary and non-state reset).*

Backup/restore become first-class opsctl verbs, uniform across all services, built
on an `ObjectStore` seam (`Put`/`Get`/`List`/`Delete`) with an `awsCLIStore`
implementation (shells `aws`, instance role, region from `IKIGENBA_AWS_REGION`
default `us-east-2`, bucket from `IKIGENBA_BACKUP_BUCKET`) and an in-memory
`fakeStore`. Backup = stop unit · archive `state/` · upload an immutable
timestamped object + update `latest` · start unit, with count-based retention
(default N=30) that never prunes `latest` or `pre-restore/`. Restore = resolve key
(default `latest`) · **interactive confirmation, no `--yes` bypass** · push a
`pre-restore/` safety snapshot · stop · replace `state/` · reset the non-state
region · start. This retires the three per-service `bin/backup`/`bin/restore`
scripts and the dashboard in-binary S3 branch (the appkit local `backup --out`
verb is untouched as opsctl's pre-migration snapshot).

**Done when:** `bin/test` exits 0 and:
- R-4GOT-W83B — backup calls `System.Stop` before the snapshot and `System.Start`
  after (ordering via the seam); on snapshot failure it still restarts (stubbed
  `System` + fake store).
- R-4HWQ-9ZU0 — backup puts an immutable timestamped object and updates `latest`;
  with N+1 present, retention deletes oldest down to N and never deletes `latest`
  or `pre-restore/` — opsctl unit test against the `fakeStore` with a real
  `tar`/filesystem, unprivileged/in-gate (real-`aws` accept/read-back is the
  on-box check, outside the gate).
- R-4J4M-NRKP — restore refuses without interactive confirmation (no `--yes`
  exists) and pushes a `pre-restore/` snapshot before replacing `state/`.
- R-4KCJ-1JBE — full `backup`→`restore` reproduces `state/` byte-for-byte through
  the `ObjectStore` seam (`fakeStore`) with a **real `tar` and real filesystem**,
  proving the archive format and Store contract round-trip (real-`aws` wire
  interop is the on-box check).
- R-46XM-U25R — a backup archive contains `state/` contents (incl. `<svc>.db`) and
  **no** `cache/` entry and **no** `*.generation` entry.
- R-49DF-LLN5 — after restore, `state/` (DB bytes + every durable file) matches the
  restored snapshot.
- R-4ALB-ZDDU — after restore, the non-state region is a clean slate (pre-restore
  `cache/` contents and any stale `*.generation` are gone).
