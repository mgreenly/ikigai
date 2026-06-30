# Phase 46 — rewrite deploy.md for the new model

*Realizes no design Decision directly (pure docs of the built behavior). Depends on Phase 29, 30.*

The repo-root `deploy.md` runbook is rewritten to describe the as-built `bump → ship → stage → deploy`
sequence and the new recovery model: `ship` delivers a **versioned `tar.gz` bundle** (not a bare binary);
deploy takes an **unconditional** S3 backup first (not "only when the schema advances"); rollback restores
the latest/`-N` S3 snapshot **by recency** (not a local pre-migration `.db`); the DB lives at
`state/<svc>.db` (not `/opt/<svc>/data/`); deploy reloads the nginx fragment through `etc/current`; there
is **no** `manifest`-verb / on-box manifest-generation step; and box data paths come from
`IKIGENBA_ROOT`. It also records the one **out-of-repo operator prerequisite** D11 depends on — setting
`IKIGENBA_ROOT=/opt` in the Terraform-seeded `/etc/ikigenba/env` (managed in `~/projects/metaspot`) — as a
manual step, since that change is not verifiable in this repo's green gate and is therefore not its own
build phase.

**Done when:**
- A repo-root `bin/deploy-doc.test.sh` (run by `bin/test`) asserts `deploy.md` **contains** the new-model
  markers (bundle/`tar.gz` staging, an "unconditional" pre-deploy backup, `rollback -N` by recency,
  `state/`, `IKIGENBA_ROOT`, the three-symlink swap) **and does not contain** the stale-model strings (a
  `manifest`-verb regeneration step, "back up … when the schema advances", a local `backups/`
  pre-migration snapshot, the `releases/<v>/`+`current` two-level indirection, `/opt/<svc>/data/`). The
  absence assertions target `deploy.md` specifically, so the needle strings inside the test file do not
  self-match.
- `bin/test` exits 0.
