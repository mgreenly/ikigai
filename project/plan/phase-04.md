# Phase 04 — setup materializes the install tree

*Realizes design Decision 1 (the `/opt/<svc>/` install tree) — the behavioral ids. Depends on Phase 03 (the Layout path scheme).*

opsctl's `setup` materializes the full `/opt/<svc>/` tree with correct ownership
and permissions, and generates the per-service nginx fragment. The observable end
state: `setup` against a temp `OPSCTL_ROOT` creates `state/`, `cache/`, `libexec/`,
`bin/`, `etc/`, `backups/` with `state/` traverse-only and group-private, the DB
private, and `state/www/{public,private}` readable by the shared `web` group; the
generated nginx fragment serves `public/` directly and gates `private/` behind the
`/srv/<svc>/` introspection. The default-private-then-open ordering is used so a
skipped re-open fails safe (403 on `www/`), never a leaked DB.

**Done when:** `bin/test` exits 0 and these design Verification ids are each
covered by a clearly-named test:
- R-3SAU-8T9F — `setup` creates the tree with the right **mode bits** on a real
  temp root (`state/` `0711`, DB `0640`, `state/www/{public,private}` `0750`,
  writable `cache/`) **and** requests ownership `<svc>:<svc>` for `state/`+DB and
  `<svc>:web` for `state/www/**` via the stubbed `Owner` seam (asserted on the
  recorded calls) — runs unprivileged.
- R-VB77-BU5O — the access-control model is asserted **by construction**: the mode
  bits plus the `Owner` ownership plan entail, under the Unix permission model,
  that a `web`-group process reads `state/www/{public,private}` and cannot read the
  DB or list `state/` — a permission-model assertion over `(mode, uid, gid)`, not a
  privileged live read.
- R-VCF3-PLWD — the generated nginx fragment serves `state/www/public` with no
  `auth_request` and `state/www/private` behind the `/srv/<svc>/` introspection
  `auth_request` — **structural assertion on the fragment only** (byte/field),
  matching the suite's existing `nginx_test.go` convention (the through-running-
  nginx check is on-box, outside the gate).
