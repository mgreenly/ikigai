# Phase 15 — honor compile's designed call site at the composition root

*Realizes design Decision 7 (the compile stage `internal/compile`), covering the new id R-4DS4-RXYX. Depends on Phase 05 (the compile stage and its `llm.CallSite`).*

D7 specifies the compile call site as `Temperature: 0` and `DisableReasoning()`
(deterministic, reasoning-off page synthesis). As with extract, the composition
root (`cmd/wiki/main.go`) builds the compiler from a bare `llm.CallSite{Model:
cfg.ModelID}`, so the running compile stage uses provider-default temperature
with reasoning **on** rather than the designed configuration. No ingest has
failed on this — compile recompiles deterministically enough to succeed — but it
is the same latent wiring drift, and a non-deterministic compile undercuts the
full-recompile-from-claims guarantees. This phase makes compile's designed call
site enforceable and wired.

**What gets built (the observable end state):**

- `internal/compile` exposes `DefaultCallSite(model string) llm.CallSite`
  returning `{Model: model, Temperature: ptr(0), Reasoning: DisableReasoning()}`
  (`maxTighten` stays a `New` parameter, default 2; no `MaxParseRetries` — the D5
  `ExtractJSON` carve covers compile's parse path).
- The composition root constructs the compiler from `compile.DefaultCallSite`
  (overriding only model / `ManifestExtras`), never from a hand-built bare
  `llm.CallSite`.
- As a result the running compile stage is the designed deterministic,
  reasoning-off configuration.

**Done when:**

- R-4DS4-RXYX — a test asserts `compile.DefaultCallSite` carries `Temperature: 0`
  and `DisableReasoning()`, and that the compiler the composition root builds is
  constructed from it.
- R-FWOT-NRHN stays green — the compile call site uses `Temperature: 0` and
  `DisableReasoning()` (now satisfied through `DefaultCallSite`).
- The rest of D7 stays green — R-FQLB-QWS6, R-FT14-IG9K, R-FU90-W809,
  R-FVGX-9ZQY — so the wiring change does not disturb compile behavior or the 12k
  enforcement.
- The suite is green (`go build ./...`, `go vet ./...`, `gofmt -l .` with no
  output, `go test ./...`, `bin/check-migrations wiki`).
