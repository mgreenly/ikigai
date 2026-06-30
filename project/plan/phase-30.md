# Phase 30 — opsctl rollback: recover by S3 backup recency (-N)

*Realizes design Decision 10 (rollback) and Decision 2 (the symlink swap). Depends on Phase 24, 27.*

`opsctl rollback <svc> [-N]` recovers to a previous backup chosen by **recency** over the service's S3
snapshots (`-0` = most recent, default when no flag; nightly snapshots count). It reads the producing
version embedded in the resolved snapshot's key (D07), requires that binary to still be retained in
`libexec/` (else **fail loudly**), restores that snapshot's `state/` (stop · replace `state/` · reset
non-state), and swaps **all three** symlinks to the recorded version, then nginx reload · restart ·
is-active. A delayed rollback where the live DB's applied schema is ahead of the rolled-to binary
**fails loudly** via the forward-only downgrade guard.

**Done when:**
- `rollback_test.go` tagged `R-88JG-D5AN` asserts `rollback` with no flag resolves to the same snapshot
  as explicit `-0`.
- A test tagged `R-89RC-QX1C` asserts `-N` resolves the (N+1)th-newest snapshot, restores its `state/`,
  and swaps all three symlinks to the **key-embedded** version (not current/newest).
- A test tagged `R-8AZ9-4OS1` asserts rollback to a snapshot whose version is no longer in `libexec/`
  fails loudly (non-zero, no symlink swapped).
- A test tagged `R-8C75-IGIQ` asserts a downgrade-guard failure (applied > embedded schema) surfaces as
  a failed rollback, not a boot against an incompatible DB.
- A test tagged `R-3UQN-0CQT` asserts rollback repoints all three symlinks to `<old>` and `<old>`'s full
  set is still present.
- `bin/test` exits 0.
