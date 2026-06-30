# Phase 25 — opsctl setup materializes the new install tree

*Realizes design Decision 1 (the /opt/<svc>/ install tree). Depends on Phase 24.*

`opsctl` `setup` materializes the D01 tree against a temp `OPSCTL_ROOT`/`SysRoot`: `state/` (0711
traverse-only), `state/<svc>.db` (0640), `state/www/{public,private}` (0750), `cache/`, `libexec/`,
`bin/`, **`etc/`**, **`share/`** — and creates **no** `backups/` dir. Ownership is requested through the
`Owner` seam (`<svc>:<svc>` for `state/`+DB, `<svc>:web` for `state/www/**`). The stable system symlink
`/etc/nginx/conf.d/locations/<svc>.conf` is created pointing at `/opt/<svc>/etc/current/nginx.conf`
(a symlink, set once), and the `web` group is created/applied.

**Done when:**
- `setup_test.go` tagged `R-3SAU-8T9F` asserts the mode bits (`state/` 0711, DB 0640, `state/www/**`
  0750, `etc/`/`share/` present root-owned) **and** no `backups/` dir **and** the recorded `Owner` calls.
- A test tagged `R-LHY1-6IS8` asserts the system symlink resolves to `…/etc/current/nginx.conf`.
- A test tagged `R-VB77-BU5O` asserts the `(mode, owner)` plan entails web-group read of `state/www/**`
  and no read/list of the DB or `state/`.
- A structural test tagged `R-VCF3-PLWD` asserts a shipped fragment serves `state/www/public` directly
  and gates `state/www/private` behind the `/srv/<svc>/` `auth_request`.
- `bin/test` exits 0.
