# Phase 18 — Consumer loops through `Spec.Consumers`

*Realizes design Decision 15. Touches `cmd/prompts/main.go` and
`cmd/prompts/registry_test.go`. Depends on no earlier prompts phase; depends on
the appkit chassis providing `Spec.Consumers` (appkit plan Phase 10), consumed
through the committed `replace appkit => ../appkit` as a fixed external
contract. Independent of the web/MCP/shim phases; ordered first like notify.*

prompts hand-wires six event-plane consumer loops at the composition root (the
`workers` slice, `runConsumer`, the `var rt` capture, the `feedDefaults` map,
and the legacy `Consumes` + `Subscriptions` Spec fields). This phase replaces all
of that with the declared `Spec.Consumers` table; the chassis derives the
manifest `CONSUMES`, the reflection subscriptions, and the `consumer.Run`
workers. Behavior is unchanged: same six upstreams in the same order, same broad
`"*"` subscriptions, same fire-and-run fan-out, same per-source cursors
(`consumer_id == Spec.App`).

## Steps

Extract a testable `promptsSpec() appkit.Spec` (the notify shape) so `main()`
becomes `appkit.Main(promptsSpec())` and a test can inspect the returned Spec.

In **`cmd/prompts/main.go`**:

- **Add the `Consumers` table**, built once from `sources` in order
  (`cron`, `crm`, `ledger`, `dropbox`, `scripts`, `prompts`). Each entry:
  ```go
  appkit.Consumer{
      Source:        src,
      Subscriptions: consume.Subscriptions([]string{src}),
      Handler: func(rt *appkit.Router) consumer.Handler {
          logger := rt.Logger()
          fire := func(ctx context.Context, promptID, s, evType, eventID string, payload []byte) error {
              _, err := svcRef.RunByEvent(ctx, promptID, s, evType, eventID, payload)
              return err
          }
          return consume.Handler(fire, svcRef.PromptsForEvent, src, logger)
      },
  }
  ```
  (loop with `src := src` to capture per iteration).
- **Delete** the legacy `Consumes: sources` and `Subscriptions: func() … { consume.Subscriptions(sources) }`
  Spec fields (both derived by the chassis from the table; setting either
  alongside `Consumers` is a startup error), the `workers` slice construction in
  `main()`, the `var rt *appkit.Router` declaration and the `rt = r` assignment
  in `Handlers` (which becomes `Handlers: func(r *appkit.Router) error { return registerRoutes(r) }`),
  and the functions `runConsumer`, `feedURLEnv`, `fromEnv`, plus the
  package-level `feedDefaults` map (its doc comment too).
- **Remove now-unused imports** (`strings` — only `feedURLEnv`/`fromEnv` used it);
  keep `config` (still used by `registerRoutes`), `context`, `eventplane/consumer`,
  and the rest.
- **Keep unchanged:** the package-level `sources`, `svcRef` and `storeRef`
  captures (both set in `registerRoutes`; `svcRef` now feeds the `Consumers`
  factories, `storeRef` still feeds `Producer` — both run strictly after
  `Handlers`), and the **entire producer half** (`Feed: "/feed"`,
  `Events: prompt.Events`, `Producer`, `ManifestExtras`).

**No `.envrc` or `bin/start` change:** prompts already reads
`PROMPTS_<SRC>_FEED_URL` / `PROMPTS_<SRC>_FROM` with the chassis defaults
(`registry.BaseURL(src) + "/feed"` / `"tail"`), and `launch_prompts` already
exports the generic `PROMPTS_<SRC>_FEED_URL` form. Verify this by inspection; do
not touch either file.

In **`cmd/prompts/registry_test.go`**: delete `TestFeedDefaultsUseRegistryForEverySource`
(R-RG02-FEED) — the `feedDefaults` map it asserted is gone; the default feed-URL
resolution is chassis-owned now (appkit D10, `R-464U-T3T1`). Leave the other
registry tests (`R-RG01-PORT`, `R-RG03-DBOX`, `R-RG04-NLIT`) untouched; the AST
port test still finds the `appkit.Spec` literal in `main.go` (now inside
`promptsSpec()`), and the no-literal guard still passes (deleting the map removed
a `registry.BaseURL` call site, added no port literal).

## Tests to add

A test in `package main` (e.g. `cmd/prompts/consumers_test.go`) for the D15 ids.

## Done when

The suite is green (per design *Conventions*: from the `prompts/` directory,
`go build ./...`, `go vet ./...`, `gofmt -l .` emits no output, and
`go test ./...` all pass with no race violations) and:

- **R-DFV4-7W4Y** — a test calls `promptsSpec()` and asserts `.Consumers` has
  **exactly six** entries, in order `cron`, `crm`, `ledger`, `dropbox`,
  `scripts`, `prompts`, each entry's `.Subscriptions` deep-equal to
  `consume.Subscriptions([]string{entry.Source})`, and that the returned Spec's
  legacy `.Consumes` and `.Subscriptions` fields are both nil/unset. *(unit test
  over the returned Spec)*
- **R-DH30-LNVN** — a test sets the package-level `svcRef` to a real
  `prompt.Service` over a temp SQLite DB with a stubbed runner (the
  `consume`-smoke substrate), invokes a `Consumers` entry's `Handler(rt)` with a
  Router (built through the `server.New` Register seam, providing a logger),
  and asserts the returned `consumer.Handler`, fed a well-formed event of that
  entry's `Source` whose type a created prompt's trigger matches, fires exactly
  one run, and fed a well-formed event no trigger matches fires none. *(handler
  factory behavior over the real service)*
- the committed-manifest byte-equality test still passes with
  `CONSUMES=cron,crm,ledger,dropbox,scripts,prompts` unchanged (`R-8IAN-FB87`
  remains covered), and the opsctl-layout boot test still boots all six
  consumers against idle feed servers (`R-4LKF-FB23` remains covered);
- `grep -n "runConsumer\|feedURLEnv\|fromEnv\|feedDefaults\|Workers:\|var rt \*appkit.Router\|Consumes:" prompts/cmd/prompts/main.go`
  returns no matches (the legacy top-level `Subscriptions` field's absence is
  covered by R-DFV4-7W4Y's assertion that the returned Spec's `.Subscriptions` is
  nil — a grep cannot distinguish it from the per-entry `Subscriptions:` field);
- `grep -rn "R-RG02-FEED\|TestFeedDefaultsUseRegistryForEverySource" prompts --include=*.go`
  returns no matches (the retired id's test is deleted).
