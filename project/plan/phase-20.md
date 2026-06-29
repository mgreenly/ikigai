# Phase 20 — adopt sites into the new layout

*Realizes design Decision 8 (per-service adoption) — the R-4LKF slice for **sites**. Depends on Phase 04 (setup/tree, incl. the generic web group + nginx fragment), Phase 06 (config + boot reconstruction).*

sites adopts the uniform layout and is the first real user of the universal served
-content pattern: DB at `state/sites.db`, sidecar at `cache/sites.db.generation`;
binary as `libexec/sites-v<semver>` selected by `bin/run`; boot reconstruction per
Phase 06. Its former top-level `/opt/sites/www/{served/public,served/private,
working}` relocates to `state/www/{public,private}` (plus any working content under
`state/`), and the `web` group + per-service nginx fragment (Phase 04) are
repointed to serve it — `public/` direct, `private/` introspection-gated.
Backup/restore inherited from `opsctl` (D7), so served content is backed up as
plain `state/`.

**Done when:** `bin/test` exits 0 and:
- R-4LKF-FB23 (**sites slice**) — a freshly set-up `/opt/sites/` (DB under
  `state/`, served content under `state/www/{public,private}`, sidecar under
  `cache/`, binary under `libexec/`, `bin/run` symlink) boots and passes its
  `health` check, and nginx serves `state/www/public` while gating
  `state/www/private`.
