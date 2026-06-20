# Phase 5 — The compile stage: full recompile from claims, 12k enforced

*Realizes design Decision 7 (the compile stage `internal/compile`). Depends on Phase 3 (the llm.JSON seam) and Phase 2 (wiki.Subject / wiki.Claim).*

Rebuild one subject's page from its **complete claim set** — identity + claims
only, never a prior page body, never the source document — guaranteeing a body of
≤ 12,000 characters by construction.

**What gets built (the observable end state):**

- `internal/compile/`: the `PageCharCap = 12000` const, the `Compiler`
  (`New(c, site, log)` + `Compile(ctx, s wiki.Subject, claims []wiki.Claim) (title, body string, err error)`)
  with **no prior-page-body parameter**, and the ported `DefaultMergePrompt`
  simplified to phase 1 (task framing from claims only, fold/prose discipline,
  conflicting-accounts handling, lead discipline, the 12k size cap, the
  `{title, body}` output schema, a worked example).
- The three-layer 12k enforcement: the prompt instruction; the Go
  recompile-tighter loop (`validate` asserts title/body non-empty, then measure
  `utf8.RuneCountInString(body)`, re-prompt with a corrective note up to
  `maxTighten` times, and as a last resort truncate at a paragraph/rune boundary
  ≤ 12000 with a logged warning); backstopped by the D3 DB `CHECK`. `Compile`
  thus never returns a body > 12,000 characters.
- Call site defaults (configurable via `ManifestExtras`): `anthropic.ModelSonnet46`,
  `Temperature: 0`, `DisableReasoning()`, `maxTighten: 2`.

**Done when:**

- R-FQLB-QWS6 — `Compile` is invoked with subject identity + claims only and has
  no prior-page-body parameter (full recompile from claims; anti-poisoning).
- R-FT14-IG9K — `Compile` never returns a body exceeding 12,000 characters under
  any input.
- R-FU90-W809 — given an over-cap candidate followed by a within-cap candidate,
  `Compile` returns the within-cap candidate (recompile-tighter loop).
- R-FVGX-9ZQY — given only over-cap candidates, `Compile` truncates the final
  candidate to ≤ 12,000 characters at a boundary and logs a warning (last-resort
  path).
- R-FWOT-NRHN — the compile call site uses `Temperature: 0` and `DisableReasoning()`.
- Tests run against the mock provider (no live LLM call); the suite is green.
