# Phase 06 — Normalize the composition root (Spec inline in `cmd/cron/main.go`)

*Realizes design Decision 8 (composition-root normalization; structural). No
dependency on the other conversion phases — sequenced first so every later phase
(WWW, MCP, registry, db) edits `cmd/cron/main.go`, the reference location, and the
relocated tests land in `cmd/cron` directly.*

Observable end state:

- `cron/cmd/cron/main.go` declares `func cronSpec() appkit.Spec` carrying the exact
  body of today's `cronapp.Spec()` — the `App`/`Mount`/`Port`/`MCP`/`Feed`/
  `Migrations` identity, the dynamic `Publishes` provider, the `ManifestExtras`,
  the `Handlers` hook (store construction, the `GET /{$}` landing mount, the gated
  `POST /mcp` mount), the `Producer` hook, and the `Workers` slice — with
  `var store *crontab.Store` / `var worker *tick.Worker` declared at the top of
  `cronSpec()` and captured by the closures (the crm/notify deferred-construction
  pattern). `main()` is `appkit.Main(cronSpec())`. The `Spec` value is fully formed
  when returned — no post-construction `.Handlers`/`.Workers` mutation.
- `cron/internal/cronapp/` is deleted (nothing imports it).
- `cron/cmd/cron/main_test.go`'s manifest byte-equality test calls `cronSpec()`
  instead of `cronapp.Spec()` and drops the `cron/internal/cronapp` import; no
  assertion changes.
- `internal/web/web_test.go`'s `TestCompositionRootMountsLandingWithoutIdentityWrapper`
  re-points its source path from `../cronapp/spec.go` to `../../cmd/cron/main.go`;
  the mount-string assertions are unchanged (those exact lines move verbatim into
  `main.go`).

**Done when:** the suite is green — `cd cron && go build ./...`,
`cd cron && go vet ./...`, `cd cron && gofmt -l .` (no output),
`cd cron && go test ./...`, and `bin/check-migrations cron` all succeed with zero
failures — and:

- `ls cron/internal/cronapp 2>/dev/null` reports no such directory, and
  `grep -rn "internal/cronapp" cron --include=*.go` returns no matches;
- `grep -n "func cronSpec" cron/cmd/cron/main.go` matches, and
  `grep -n "appkit.Main(cronSpec())" cron/cmd/cron/main.go` matches;
- the existing behaviors are re-proven against the relocated root with no
  assertion changes: the manifest byte-equality test (`R-8IAN-FB87`), the
  landing/MCP mount-partition tests (`R-ROUT-2P8Q`, `R-ROUT-4R1S`, `R-ROUT-6T3U`),
  and the boot-from-opsctl-layout health smoke (`R-4LKF-FB23`) all pass.
