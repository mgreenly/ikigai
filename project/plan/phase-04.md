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
- R-3SAU-8T9F — `setup` creates the tree with `state/` mode `0711` owned
  `<svc>:<svc>`, the DB at `0640 <svc>:<svc>`, `state/www/{public,private}` at
  `0750 <svc>:web`, and a writable `cache/` (real filesystem, temp root).
- R-VB77-BU5O — real uid/gid-dropped reads: a process whose groups are exactly
  `{web}` can read `state/www/{public,private}` files and is denied reading the DB
  and denied listing `state/`.
- R-VCF3-PLWD — the generated nginx fragment serves `state/www/public` with no
  `auth_request` and `state/www/private` behind the `/srv/<svc>/` introspection
  `auth_request` (structural assertion on the fragment + a through-nginx
  integration check that unauth gets `public/`, is refused `private/`).
