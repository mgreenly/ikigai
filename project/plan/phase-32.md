# Phase 32 — ship produces the versioned release bundle

*Realizes design Decision 4 (version production: bundle). Depends on Phase 22.*

`bin/ship` builds `main` static for `linux/amd64` (`GOWORK=off`), stamps the binary with the full
version (`VERSION` core + `+<sha>` build metadata via `-ldflags`), then **assembles a `tar.gz` bundle**
named `<svc>-<version>.tar.gz` carrying the built binary as `<svc>`, the service's `<svc>/etc/nginx.conf`
as `nginx.conf`, the service's `<svc>/etc/manifest.env` as `manifest.env` **byte-for-byte**, and the
contents of `<svc>/share/` under `share/` (absent when the service ships none), and `scp`s the bundle
(not a bare binary) to the box `/tmp`. `bin/bump` keeps the `v` and rejects a bare `VERSION`.

**Done when:**
- `bin/ship.test.sh` tagged `R-45PQ-GAF2` asserts the stamped `version` output equals `v<core>+<sha>`
  for the built commit and the produced bundle is named `<svc>-v<core>+<sha>.tar.gz`.
- `bin/ship.test.sh` tagged `R-P4CO-FY2L` lists/extracts the archive against a fixture service tree and
  asserts the binary as `<svc>`, `nginx.conf`, `manifest.env` byte-for-byte, `share/` contents, and **no**
  `share/` entry when the fixture has none.
- `bin/bump.test.sh` tagged `R-44HU-2IOD` asserts `bump patch` on `v0.7.1` writes `v0.7.2` and `bump`
  against a bare `0.7.1` file fails non-zero.
- `bin/test` exits 0.
