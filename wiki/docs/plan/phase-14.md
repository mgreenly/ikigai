# Phase 14 — honor extract's designed call site at the composition root

*Realizes design Decision 6 (the extract stage `internal/extract`), covering the new id R-4CK8-E688. Depends on Phase 04 (the extract stage and its `llm.CallSite`) and Phase 03 (the `llm.JSON` bounded-retry loop).*

D6 specifies the extract call site as `Temperature: 0`, `DisableReasoning()`,
`MaxParseRetries: 2` — deterministic extraction with a bounded re-prompt on a
parse/validate failure. But the composition root (`cmd/wiki/main.go`) builds the
extractor from a bare `llm.CallSite{Model: cfg.ModelID}`: temperature, reasoning,
and retries are all zero-valued, so the running service extracts at
provider-default temperature, reasoning **on**, and **zero** retries. The
designed values never take effect — a single parse failure kills the job with no
re-prompt (this is why the failed "Gary Gygax" ingests reported `after 1
attempt(s)`). This phase makes the designed call site enforceable and actually
wired, so the values cannot be silently dropped again.

**What gets built (the observable end state):**

- `internal/extract` exposes `DefaultCallSite(model string) llm.CallSite`
  returning `{Model: model, Temperature: ptr(0), Reasoning: DisableReasoning(),
  MaxParseRetries: 2}` — the package owns its designed defaults.
- The composition root constructs the extractor from `extract.DefaultCallSite`
  (overriding only model / `ManifestExtras`), never from a hand-built bare
  `llm.CallSite`.
- As a result the running extractor re-prompts on a parse/validate failure up to
  twice (`MaxParseRetries: 2`) and runs deterministically (`Temperature: 0`,
  reasoning off).

**Done when:**

- R-4CK8-E688 — a test asserts `extract.DefaultCallSite` carries `Temperature:
  0`, `DisableReasoning()`, and `MaxParseRetries: 2`, and that an extractor built
  from it re-prompts a parse/validate failure (a bad-then-good extraction
  succeeds end-to-end through the production-configured extractor) rather than
  failing on the first attempt.
- R-W2HP-H0J0 stays green — the extract call site is `Temperature: 0` and
  `DisableReasoning()` (now satisfied through `DefaultCallSite`, confirmed via the
  capturing mock provider).
- The rest of D6 stays green — R-VYU0-BPAX, R-W19T-38SB, R-XJBY-H8JZ,
  R-XKJU-V0AO — so the wiring change does not disturb extraction behavior.
- The suite is green (`go build ./...`, `go vet ./...`, `gofmt -l .` with no
  output, `go test ./...`, `bin/check-migrations wiki`).
