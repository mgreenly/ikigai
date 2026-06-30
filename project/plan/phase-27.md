# Phase 27 — opsctl backup/restore deltas: version-embedding key, no pre-restore snapshot

*Realizes design Decision 7 (opsctl-owned backup/restore, S3-only). Depends on Phase 24.*

Brings the existing opsctl backup/restore (built in Phase 08/08a, over the `ObjectStore` seam) in line
with the reworked D07: the snapshot object key embeds the **producing version** read from `bin/run`'s
target — `<svc>/<svc>-<version>.<UTC>.tar.gz` — so rollback can resolve a snapshot to its binary version
from the key alone; and the restore path takes **no** pre-restore safety snapshot (the `pre-restore/`
namespace is gone). Restore stays interactive with no `--yes` bypass.

**Done when:**
- An opsctl unit test tagged `R-82FY-GAL6` seeds a `bin/run` target and asserts `backup` writes the
  object under a key embedding that exact version (a key with no embedded version fails).
- A test tagged `R-4J4M-NRKP` asserts restore refuses without interactive confirmation (no `--yes` path
  exists) **and** makes no `Put` to S3 (no pre-restore snapshot written).
- Substrate is the `fakeStore` + real `tar`/filesystem; `bin/test` exits 0. (The previously-green
  R-4GOT/R-4HWQ/R-4KCJ/R-TAOX/R-TBWT remain covered by Phase 08/08a.)
