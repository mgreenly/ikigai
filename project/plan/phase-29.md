# Phase 29 — opsctl deploy: three-symlink swap, unconditional backup, no manifest regen

*Realizes design Decision 10 (deploy) and Decision 2 (the symlink swap). Depends on Phase 24, 26, 27, 28.*

`opsctl deploy <svc> v<ver>` promotes a staged version in fixed order: (1) **unconditional** `opsctl`
S3 backup before any mutation; (2) `<bin> migrate` (forward-only) then `chown` `state/` back to
`<svc>:<svc>`; (3) atomic swap of **all three** symlinks (`bin/run`, `etc/current`, `share/current`) to
`<new>`; (4) nginx **reload** (the system symlink already resolves through `etc/current`); (5) restart +
`is-active`; (6) prune. The old `<bin> manifest` regeneration step and the conditional local
`backups/` snapshot are **removed**; deploy never writes `etc/current/manifest.env`. The now-dead
`Layout.PreMigrationBackup`/`BackupsDir`/`ManifestPath` (regen) usages are dropped here.

**Done when:**
- `deploy_test.go` tagged `R-863N-LLT9` asserts a deploy whose schema does **not** advance still issues
  exactly one backup `Put`, ordered before migrate/swap.
- A test tagged `R-87BJ-ZDJY` asserts the recorded seam-call sequence is backup → migrate → swap(3
  symlinks) → nginx reload → restart/is-active → prune, with **no** `manifest` verb run and **no** write
  to `etc/current/manifest.env`.
- Tests tagged `R-3TIQ-ML04`, `R-1A79-JG03`, `R-3VYJ-E4HI` assert: `bin/run` resolves to the new
  `libexec/<svc>-v<new>` (no legacy `releases/`/`current`); all three symlinks resolve to the **same**
  `<new>`; and no symlink ever dangles across the swap.
- `bin/test` exits 0.
