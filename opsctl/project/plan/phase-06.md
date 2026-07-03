# Phase 06 — Fix: deploy reads the apex domain from the environment, not the manifest

*Corrects a phase-04 build defect. Phase 04 shipped
`internal/opsctl/deploy.go`'s `installApexBlockIfDefault` reading the apex domain
from `manifest["IKIGENBA_DOMAIN"]` — the app's `etc/current/manifest.env`, which
never carries the per-box domain — instead of `os.Getenv("IKIGENBA_DOMAIN")` as
design Decision 4 (`project/design/D04.md`) mandates. On the live box this failed
a real dashboard deploy with `deploy: DEFAULT=true requires IKIGENBA_DOMAIN` even
though `IKIGENBA_DOMAIN` was set in `/etc/ikigenba/env` (loaded into opsctl's
process by D3's `LoadEnvFile`). Phase 04's apex-domain test was written to match
the bug — it seeds the domain into the manifest — so it is green but does not
prove D4. This phase re-realizes the affected ids **correctly** and adds the
regression guard `R-CNPY-3Z4Y`. Touches `internal/opsctl/deploy.go` and its test
only.*

The observable end state:

- **Code.** In `installApexBlockIfDefault` (`deploy.go`), the apex domain is read
  from **`os.Getenv("IKIGENBA_DOMAIN")`** (trimmed), not from the parsed
  `manifest` map. If it is empty, return the existing loud error naming
  `IKIGENBA_DOMAIN`. `PORT` continues to come from `manifest["PORT"]` (that is
  correct per D4 and is **not** changed). No other deploy behavior changes.
- **Tests.** The apex-domain tests provide `IKIGENBA_DOMAIN` via the **process
  environment** (`t.Setenv`), and **never** place it in the manifest. The manifest
  in these tests carries only `DEFAULT`/`MOUNT`/`PORT`, matching what a real
  `dashboard/etc/manifest.env` holds. Any test that seeds the domain into the
  manifest is the phase-04 defect and must be removed/rewritten.

This phase re-realizes phase 04's `R-MSOP-5MDA` and `R-MTWL-JE3Z` correctly
(sourced from the environment) and adds `R-CNPY-3Z4Y`. `R-MV4H-X5UO` and
`R-MXKA-OPC2` are unaffected and remain covered by phase 04. The live-box
`R-MYS7-2H2R` remains operator-verified out-of-loop (the corrected deploy is what
the operator re-runs to complete the dashboard cutover).

Non-goals: no change to `PORT` sourcing, the `DEFAULT` discriminator (still read
from `manifest["DEFAULT"]`), the `nginx -t`-before-reload order, the three-symlink
swap, or anything outside `installApexBlockIfDefault` and its test.

**Done when** the suite is green — `GOWORK=off go build ./...` succeeds and
`GOWORK=off go test ./...` passes from `opsctl/` — and these ids are each covered
by a clearly-named test (temp `OPSCTL_ROOT` + the fake `System`), each of which
**fails against the phase-04 (manifest-sourced) implementation**:

- **R-CNPY-3Z4Y** — a `DEFAULT=true` release whose `etc/current/manifest.env`
  contains **no** `IKIGENBA_DOMAIN` key, with `IKIGENBA_DOMAIN=<d>` set in the
  environment (`t.Setenv`), deploys successfully and leaves `l.ApexBlockPath()`
  containing `<d>` with no `__DOMAIN__` occurrence. The manifest-sourced
  implementation errors here (domain absent from the manifest) — the falsifying
  case for the bug.
- **R-MSOP-5MDA** — with `DEFAULT=true`, `PORT=<p>`, `etc/current/nginx.conf`
  holding `__DOMAIN__`/`__PORT__`, and `IKIGENBA_DOMAIN=<d>` set **in the
  environment** (not the manifest), after `Deploy` `l.ApexBlockPath()` contains
  `<d>` and literal `<p>` and no `__DOMAIN__`/`__PORT__`.
- **R-MTWL-JE3Z** — with `DEFAULT=true` and `IKIGENBA_DOMAIN` **unset in the
  environment** (and absent from the manifest), `Deploy` returns an error whose
  message contains `IKIGENBA_DOMAIN`, leaves `l.ApexBlockPath()` unchanged, and
  does not call `System.NginxReload`.
