# Phase 43 — adopt sites into the new layout (relocate www/, repoint nginx)

*Realizes the sites slice of design Decision 8 (per-service adoption) and Decision 11 (authored portable manifest). Depends on Phase 23, 31, 32.*

sites adopts the new contract and relocates its served content to the uniform shape: the old
`/opt/sites/www/{served/public,served/private,working}` becomes `state/www/{public,private}` (with any
working content under `state/`), owned `<svc>:web` under the `0711` traverse-only `state/` (D01). The
committed `sites/etc/nginx.conf` is the representative fragment that serves `state/www/public` directly
and gates `state/www/private` behind the `/srv/sites/` introspection. Committed `sites/etc/manifest.env`
(authored, portable) is added; data paths resolve via `composeDataPaths` (Phase 23).

**Done when:**
- A test tagged `R-8DF1-W89F` asserts `sites/etc/manifest.env` is portable (no `/opt/…`, no path-override
  lines).
- A Go unit test tagged `R-8IAN-FB87` asserts `manifest.Emit(sites-spec)` byte-agrees with the committed
  `sites/etc/manifest.env`.
- A boot smoke tagged `R-4LKF-FB23` asserts a freshly set-up `/opt/sites/` with content under
  `state/www/{public,private}` boots and passes `health`.
- `bin/test` exits 0.
