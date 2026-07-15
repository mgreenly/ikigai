# Phase 8 — Composition root & chassis boot

*Realizes design Decision 1 (Spec, manifest, wiring). Depends on Phases 5
and 7.*

`cmd/repos/main.go` → `appkit.Main(reposSpec())`: the one inline `Spec`
(App/Mount/Port-from-registry/MCP/WWW/Feed/Migrations/Events/Consumers/
Handlers/Producer/Workers/Health), all `REPOS_*` env read here and only
here, the domain `Service` + engine constructed over `rt.DB()` and the state
root, boot-time model validation failing loudly, `POST /mcp` behind
`rt.RequireIdentity`, the dispatcher worker with its recovery sweep, and the
source-scan guard proving no loopback-port literal survives outside tests.
`etc/manifest.env` ships the emitted manifest. (The registry row, `go.work`
entry, and `bin/start` entry are operator-applied suite-level preconditions,
not edits this phase makes.)

**Done when:** R-EISY-2LYZ and R-EL8Q-U5GD are each covered by a
clearly-named test, and the suite is green per design Conventions.
