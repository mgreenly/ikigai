# Phase 24 — opsctl Layout: per-version + three-symlink accessors

*Realizes the path scheme of design Decision 2 (versioned bundle) and Decision 1 (install tree); structural. Depends on Phase 22.*

This phase is **additive** to `opsctl/internal/opsctl/layout.go`: it introduces the accessors
the D02/D10 orchestration needs, leaving the legacy accessors (`BackupsDir`, `PreMigrationBackup`,
the single `ManifestPath`) in place so existing `deploy.go`/`backup.go` still compile. Those die in
the phases that obsolete them (27/29).

New `Layout` accessors land and are exercised: `EtcVersionDir(v)`, `NginxConfFile(v)`,
`ManifestFile(v)` (= `etc/<v>/manifest.env`), `EtcCurrentLink()` (`etc/current`),
`ActiveNginxConf()`/`ActiveManifest()` (`etc/current/{nginx.conf,manifest.env}`),
`ShareVersionDir(v)`, `ShareCurrentLink()` (`share/current`), and `LibexecBinary(v)` carrying the
full v-prefixed SemVer.

**Done when:**
- `layout_test.go` asserts each new accessor returns the exact `/opt/<svc>/…` path from D02 (per-version
  `etc/<v>`/`share/<v>`, the three active symlinks `bin/run`/`etc/current`/`share/current`).
- `go build ./...` in `opsctl` is clean and `bin/test` exits 0.
