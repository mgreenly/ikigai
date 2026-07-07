# Phase 09 — Consumer loops through `Spec.Consumers` + composition-root normalization

*Realizes design Decision 11. Touches only `cmd/scripts/main.go` and its test
`cmd/scripts/main_test.go` (all inside `scripts/`). Depends on no earlier scripts
phase for its content; depends on the appkit chassis providing `Spec.Consumers`
(appkit plan Phase 10), consumed through the committed `replace appkit => ../appkit`
as a fixed external contract. `internal/consume` is unchanged. No schema, no
`share/www`/MCP change. Env names are already the chassis convention
(`SCRIPTS_<SRC>_FEED_URL` / `SCRIPTS_<SRC>_FROM`), so there is no env migration and
no `bin/start`/`.envrc` change.*

Observable end state:

- `scriptsSpec()` returns one **fully-formed `appkit.Spec` literal** with
  `Handlers: registerRoutes` and a `Consumers` table declared **inside** it — five
  entries in the order `cron, crm, ledger, dropbox, prompts`, each
  `{Source: src, Subscriptions: consume.Subscriptions([]string{src}), Handler:
  scriptsConsumer(src)}`. `main()` is `func main() { appkit.Main(scriptsSpec()) }` —
  no post-construction `.Handlers`/`.Workers` assignment, no `var rt *appkit.Router`
  capture.
- A `scriptsConsumer(source string) func(*appkit.Router) consumer.Handler` factory
  returns `consume.Handler(svcRef.RunForEvent, svcRef.ScriptsForEvent, source,
  rt.Logger())`. The `svcRef` (and `storeRef`) package vars stay: `registerRoutes`
  sets `svcRef` before the chassis calls the factories.
- **Deleted from `main.go`:** `runConsumer`, the `workers` slice construction, the
  `var rt` capture, `feedURLEnv`, `fromEnv`, `feedDefault`, the `consumerID` const,
  the `sources` package var (its self-chaining TODO comment goes with it), and the
  legacy `Spec.Consumes` and `Spec.Subscriptions` fields. `Spec.Workers` is not set
  (scripts' only Workers were the consumer loops; the runner spawns per-run
  goroutines from the Service and the crash-recovery sweep runs inline in
  `registerRoutes` — neither is a Worker).
- The chassis resolves `SCRIPTS_<SRC>_FEED_URL` (default `registry.BaseURL(src) +
  "/feed"`) and `SCRIPTS_<SRC>_FROM` (default `"tail"`) and runs one
  `consumer.Run` per entry with `ConsumerID == "scripts"` — the identical
  resolution and cursors `runConsumer` produced. `etc/manifest.env` is
  byte-identical (`CONSUMES=cron,crm,ledger,dropbox,prompts`).

**Test edits (no domain assertion changes):**

- **`TestManifestLibraryByteEqualsCommittedFile` (`R-8IAN-FB87`, retained).** It
  builds `manifest.Fields{… Consumes: spec.Consumes …}`. After the conversion
  `spec.Consumes` is nil; source the expected `Consumes` from the table —
  `consumesFromConsumers(spec)` returning `[]string` of `spec.Consumers[i].Source`
  in order — so the emitted `CONSUMES=` stays byte-identical **and** a dropped or
  reordered upstream fails the manifest test (drift guard).
- **`TestScriptsBootsFromOpsctlLayoutAndServesHealth` (`R-4LKF-FB23`/`R-RUNS-BOOT`,
  retained).** It iterates the deleted `sources` var to build feed servers and set
  `SCRIPTS_<SRC>_FEED_URL`. Derive the upstream list from the table
  (`for _, c := range scriptsSpec().Consumers { … c.Source … }`) instead. Harness
  plumbing only — the boot/health assertions are unchanged.
- **`TestFeedDefaultUsesRegistryBaseURLs` (`R-RGST-PEER`) is DELETED** with the
  `feedDefault` function it covered — peer feed-URL default resolution is now
  chassis-owned (pinned by appkit's `R-464U-T3T1`/`R-47CR-6VJQ`), per D11.

## Add the D11 verification coverage (new tests in `cmd/scripts/main_test.go`, id-tagged)

- **R-8WN1-0VQI** — `spec := scriptsSpec()`; assert `len(spec.Consumers) == 5` in
  the exact order `cron, crm, ledger, dropbox, prompts`; each entry's
  `Subscriptions` deep-equals `consume.Subscriptions([]string{entry.Source})` (one
  broad `"*"` subscription for that source); and **both** `spec.Consumes == nil`
  **and** `spec.Subscriptions == nil` (no legacy field set alongside `Consumers`).
  Tag `// R-8WN1-0VQI`.
- **R-8XUX-ENH7** — invoke an entry's `Handler` factory with a Router (or drive
  `scriptsConsumer(src)` directly over a fake `RunForEvent`/`ScriptsForEvent`) and
  assert the returned `consumer.Handler`: fed a well-formed event whose
  `(source, type)` a stub `ScriptsForEvent` matches, it calls `RunForEvent` for that
  script and returns nil; fed a malformed envelope (`Type==""`/`ID==""`) it returns
  an error wrapping `consumer.ErrSkip`. Tag `// R-8XUX-ENH7`. (This exercises the
  same `consume.Handler` contract `internal/consume/consume_test.go` already pins,
  now reached through the factory seam.)

## Done when

The suite is green (per design *Conventions*: `cd scripts && go build ./...`,
`cd scripts && go vet ./...`, `cd scripts && gofmt -l .` (no output),
`cd scripts && go test ./...`, and `bin/check-migrations scripts` — trivially green,
no migration added) with zero failures, **and**:

- **R-8WN1-0VQI** and **R-8XUX-ENH7** are each covered by a clearly-named,
  genuinely-asserting test tagged with its id.
- `R-8IAN-FB87` (manifest byte-equality) still passes with
  `CONSUMES=cron,crm,ledger,dropbox,prompts` unchanged, its expected `Consumes`
  now derived from the `Consumers` table.
- `R-RUNS-BOOT`/`R-4LKF-FB23` (boot/health) still pass with the upstream list
  derived from the table.
- `grep -n "runConsumer\|feedDefault\|feedURLEnv\|fromEnv\|Workers:\|Consumes:\|Subscriptions:\|var rt \*appkit.Router" scripts/cmd/scripts/main.go`
  returns no matches.
- `grep -rn "R-RGST-PEER\|TestFeedDefaultUsesRegistryBaseURLs" scripts --include=*.go`
  returns no matches (the retired id's test is deleted with `feedDefault`).
- `grep -c "Consumers" scripts/cmd/scripts/main.go` is ≥ 1.
