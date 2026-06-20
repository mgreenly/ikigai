# Phase 4 — The extract stage: source text → subjects + claims

*Realizes design Decision 6 (the extract stage `internal/extract`). Depends on Phase 3 (the llm.JSON seam) and Phase 2 (the closed-set type rules).*

A single-pass, tool-less, structured LLM call over `llm.JSON`: the whole source
text plus a mechanical header → subjects, each with its claims.

**What gets built (the observable end state):**

- `internal/extract/`: the `ExtractedSubject` and `DocumentHeader` types, the
  `Extractor` (`New(c, site)` + `Extract(ctx, header, text) ([]ExtractedSubject, error)`),
  and the ported `DefaultExtractPrompt` — **dropping the `aliases` field and the
  within-doc alias/co-reference clause**, keeping verbatim the salience gate and
  the claims discipline, the dialog-awareness clause, a worked example (rewritten
  without aliases), closed-set type, `occurred_at` for events, and relative-time
  resolution anchored on the header's received-on date.
- The `validate` hook passed into `llm.JSON`: every subject's `Type` ∈
  `{entity,event,concept}`; `Name` non-empty after trim; ≥1 claim each with
  non-empty text — a violation is a parse-level failure that triggers the bounded
  re-prompt. After a valid parse, `OccurredAt` is coerced `""` for non-events.
  Empty `[]` is an honest, successful outcome — extract never invents a subject.
- Call site defaults (configurable via `ManifestExtras`): `anthropic.ModelSonnet46`,
  `Temperature: 0`, `DisableReasoning()`, `MaxParseRetries: 2`.

**Done when:**

- R-VYU0-BPAX — `validate` rejects an extraction containing a subject whose `Type`
  is outside `{entity,event,concept}` (a parse-level failure → re-prompt).
- R-W01W-PH1M — a valid extraction retains `OccurredAt` only for `Type=="event"`
  and empties it for `entity`/`concept`.
- R-W19T-38SB — a source with no salient subject yields an empty
  `[]ExtractedSubject` and a successful (non-error) extract — no invented subject.
- R-W2HP-H0J0 — the extract call site is configured with `Temperature: 0` and
  `DisableReasoning()` (confirmed via the capturing mock provider).
- Tests run against the mock provider (no live LLM call); the suite is green.
