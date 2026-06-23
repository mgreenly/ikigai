# Phase 39 — Judge call-site correction + live eval verification

*Realizes design Decision 21 (the judge call site — corrected pin) and Decision 22 (the `cmd/eval-extract` binary — live verification). Depends on Phase 37 (the `internal/eval` `Judge` + `DefaultJudgeCallSite`) and Phase 38 (the `cmd/eval-extract` binary + the shipped gold case).*

The pinned judge default is corrected to a configuration the provider actually runs, and the eval gains the live, end-to-end check the mock suite structurally cannot make. The observable end state:

- `eval.DefaultJudgeCallSite()` returns `{Stage:"judge", Model: anthropic.ModelOpus48, Reasoning: agentkit.Level("high"), MaxTokens: 16384, MaxParseRetries: 2}` with **no temperature set** — replacing the prior `Temperature 0` + `Reasoning Level("low")`, which the provider rejects under extended thinking (`temperature` may only be `1`/unset when thinking is enabled). The yardstick is held fixed by pinning `(model, effort)`, not by temperature.
- The `-judge-reasoning` default reflects the new pin (`high`); flag layering through the D19 parsers is otherwise unchanged.
- A live, operator-run end-to-end verification exists for `eval-extract`: with a real `ANTHROPIC_API_KEY`, running it over the committed gold case completes without error and prints a populated scorecard, driving the real extract *and* judge call sites against the API. This check is separate from the mock `go test ./...` suite (it needs a key and spends tokens), and is the substrate-adequate counterpart to the judge's field-shape assertion.

**Done when:**
- R-DWI0-C7E2 (rewritten) — a clearly-named mock test asserts `DefaultJudgeCallSite()` is `Model == anthropic.ModelOpus48`, `Reasoning == agentkit.Level("high")`, **no temperature set**, `MaxTokens >= 16384`, `Stage == "judge"` — and the suite is green.
- R-ME5L-HXJ3 — a **live** run of `eval-extract` over the committed gold case, with `ANTHROPIC_API_KEY` present, exits 0 and prints a populated scorecard (the real extract and judge both ran). This id is satisfied by an actually-observed live run, not a mock test — it cannot live in `go test ./...`; record that the run was made and completed.
