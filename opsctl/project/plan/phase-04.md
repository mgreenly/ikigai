# Phase 04 — Deploy renders and installs the apex block for the DEFAULT app

*Realizes design Decision 4 (apex block on deploy). Depends on Phase 03 (D3's
`LoadEnvFile` puts `IKIGENBA_DOMAIN` into opsctl's process on the interactive
path, which this phase reads). Touches `internal/opsctl/deploy.go` and its test;
reuses `renderApexBlock` (`templates.go`), `writeFileAtomic`, and the
`System.NginxTest`/`NginxReload` seam unchanged.*

Add an apex-aware branch to `Deploy`, placed **after** the `etc/current` atomic
swap and **before** the existing `System.NginxReload`, so it renders from the
just-swapped release. The observable end state:

- Deploy reads the freshly-swapped `etc/current/manifest.env`. If its `DEFAULT`
  field is **true** (the apex/`DEFAULT` app — dashboard), deploy:
  1. resolves the apex domain from `os.Getenv("IKIGENBA_DOMAIN")`; if empty,
     returns a non-nil error naming `IKIGENBA_DOMAIN` and installs nothing (no
     write, no reload);
  2. reads the apex source `etc/current/nginx.conf`, renders it with
     `renderApexBlock(src, domain, manifest.PORT)` (the same substitution
     `init-box` uses), and `writeFileAtomic`s the result to `l.ApexBlockPath()`;
  3. runs `System.NginxTest` on the result **before** reloading; on failure
     returns an error and does **not** reload or restart (the running apex config
     stays live — nginx reload is graceful).
- If `DEFAULT` is **false** (every normal service), the branch is skipped
  entirely: no write to `l.ApexBlockPath()`, and the existing
  `etc/current`-symlink + reload path is the only nginx effect (unchanged).
- Deploy needs a small manifest read for the `DEFAULT` flag + `PORT`. Use the
  existing manifest parsing if one is exposed; otherwise add a minimal internal
  parser for `etc/current/manifest.env` (KEY=VALUE lines) scoped to this need —
  do not restructure manifest handling.

Non-goals for this phase: no change to the service-fragment path, the
three-symlink swap, migrate/backup/chown/restart/prune, or `init-box`. No
per-service fragment-emit restructure (research option a-ii). `deploy.md` at the
repo root should get a sentence noting deploy now (re)installs the apex block for
the DEFAULT app — but that doc edit is not gated by this phase's checks.

**Partial-Decision split.** D4's fifth id **R-MYS7-2H2R is a real-substrate
(live-box) check** — a real `nginx -t` against the real apex TLS cert plus the
`include …/locations/*.conf` still resolving after a real dashboard deploy. It is
not reproducible on identical repo state, so it is **operator-verified out-of-loop**
(the same treatment as D3's `R-6FE0-9WC4`) and is **not** part of this phase's
exit condition. This phase realizes the four unit-testable ids only.

**Done when** the suite is green — `GOWORK=off go build ./...` succeeds and
`GOWORK=off go test ./...` passes from `opsctl/` — and these ids are each covered
by a clearly-named test (temp `OPSCTL_ROOT` + the fake `System`, the existing
deploy-test substrate):

- **R-MSOP-5MDA** — deploying a staged release whose `etc/current/manifest.env`
  has `DEFAULT=true`, `PORT=<p>` and whose `etc/current/nginx.conf` holds
  `__DOMAIN__`/`__PORT__`, with `IKIGENBA_DOMAIN=<d>` set, leaves
  `l.ApexBlockPath()` containing `<d>` and literal `<p>` and **no** `__DOMAIN__`
  or `__PORT__` occurrence.
- **R-MTWL-JE3Z** — deploying a `DEFAULT=true` release with `IKIGENBA_DOMAIN`
  unset returns an error whose message contains `IKIGENBA_DOMAIN`, leaves
  `l.ApexBlockPath()` unchanged, and does not call `System.NginxReload`.
- **R-MV4H-X5UO** — with the fake `System.NginxTest` returning an error for a
  `DEFAULT=true` deploy, `Deploy` returns an error and neither `System.NginxReload`
  nor the unit restart runs (recorded call order shows `NginxTest` before any
  reload, and no reload/restart after a failed test).
- **R-MXKA-OPC2** — deploying a `DEFAULT=false` (normal service) release writes no
  file at `l.ApexBlockPath()` (the apex branch is not entered).
</content>
