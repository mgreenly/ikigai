# Phase 31 — appkit verb-set reduction: drop manifest/backup/restore

*Realizes design Decision 11 (reduced verb set) and Decision 7 (no in-binary backup/restore). Depends on Phase 29, 30.*

With opsctl no longer execing the binary's `manifest`/`backup`/`restore` verbs (Phases 29/30), they are
removed: the appkit verb dispatcher exposes exactly **`serve` / `version` / `migrate` / `schema`**;
`appkit/manifest.Emit` survives as a **library function** (no longer a CLI verb); the `Spec.Backup` /
`Spec.Restore` hook fields are deleted, including dashboard's in-binary `Spec.Backup`/`Spec.Restore` S3
branch. Tests that drove the removed verbs are updated/removed so the suite stays green.

**Done when:**
- An appkit verb-dispatch unit test tagged `R-8EMY-A004` asserts the dispatcher accepts exactly
  `serve`/`version`/`migrate`/`schema`, that invoking `manifest` is an unknown-verb error, and that
  `manifest.Emit` is still callable as a library function.
- A test tagged `R-QQNU-T5M7` asserts invoking `backup` or `restore` is an unknown-verb error **and** a
  compile-level assertion that `Spec` carries no `Backup`/`Restore` hook field.
- `bin/test` exits 0.
