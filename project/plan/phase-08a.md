# Phase 08a — apex TLS cert backup/restore stream

*Realizes design Decision 7 (opsctl-owned backup/restore) — the apex TLS cert stream slice. Depends on Phase 08 (the backup/restore core + ObjectStore it extends).*

`opsctl backup dashboard` produces, **in addition to** the state object, a
dedicated cert snapshot of `/etc/letsencrypt/{archive,renewal,live}/<domain>` under
`dashboard/cert/` with its own `latest` and retention, resolved independently of
the state pointer. A dashboard restore lays the cert tree back via `tar -C /`
**without invoking certbot** (no reissue — the rate-limit-safe recovery path) and
resolves `cert/latest` independently so a DB rollback never drags the cert
backward. The cert archive holds the TLS private key, which opsctl handles as
opaque bytes — never read, printed, or logged.

**Done when:** `bin/test` exits 0 and:
- R-TAOX-5LKS — `opsctl backup dashboard` produces a cert object under
  `dashboard/cert/` and its own `cert/latest`, distinct from the state
  object/pointer (fake store unit + the real-S3 round-trip).
- R-TBWT-JDBH — a dashboard restore lays
  `/etc/letsencrypt/{archive,renewal,live}/<domain>` back from `cert/latest`
  matching the backed-up bytes, **without** invoking certbot (the issuance seam is
  never called during restore), resolving the cert pointer independently of the
  chosen state snapshot (real filesystem round-trip + stubbed `System` asserting no
  issuance call; cert bytes via the real object store).
