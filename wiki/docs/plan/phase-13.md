# Phase 13 — robust JSON carving replaces the fence-only `stripCodeFence`

*Realizes design Decision 5 (the LLM seam `internal/llm`), covering the rewritten R-J8QP-BETB and the new id R-4BCC-0EHJ. Depends on Phase 03 (the `internal/llm` json-mode helper and its `stripCodeFence`).*

Phase 03 built the `llm.JSON[T]` helper with `stripCodeFence`, ported from
wiki.bak, which recognizes only a clean leading ` ```json ` / ` ``` ` fence and
passes everything else through verbatim. In practice the extraction model
sometimes wraps its JSON in a fence form the matcher does not anticipate (an
extra backtick, a `json` info string fused to a backtick, a prose preamble),
leaving a stray `` ` `` in front of the opening `{`. That stray character reaches
`json.Unmarshal`, which fails with `invalid character '`' looking for beginning
of value` — and because the failure is in the LLM-output parse path it aborts the
whole ingest job (observed on the "Ernest Gary Gygax — Early Life" ingest). D5
has been re-decided: the helper now **carves the outermost JSON value** out of a
possibly-decorated reply instead of pattern-matching a fence. This phase realigns
`internal/llm` to that contract.

**What gets built (the observable end state):**

- A new **exported** `ExtractJSON(string) string` replaces `stripCodeFence`: it
  trims, picks whichever of the first `{` or first `[` opens earliest, and returns
  the span from there to the **matching last** `}` or `]`. Surrounding decoration
  — a ` ```json ` fence, a non-standard or extra-backtick fence, a prose preamble
  or trailing commentary, or a stray leading `` ` `` — falls outside the span and
  is discarded before `json.Unmarshal`. It is a no-op on already-bare JSON. It is
  exported so it can be the single carve reused by the agent path (`ask`, D9,
  Phase 16) — no second fence-stripper exists in the service.
- `llm.JSON[T]` calls `ExtractJSON` in place of `stripCodeFence`; the flow is
  otherwise unchanged (drain stream → carve → unmarshal → `validate` → bounded
  re-prompt).
- `stripCodeFence` and any test asserting its fence-only behavior are removed; the
  carve subsumes every case they covered.

**Done when:**

- R-J8QP-BETB — a test asserts `ExtractJSON` yields parseable JSON from a
  ` ```json `…` ``` ` fenced reply, from a bare ` ``` ` fenced reply, and from
  already-bare JSON (no-op), via the first-`{`/`[`-to-matching-last-`}`/`]` carve.
- R-4BCC-0EHJ — a test asserts `ExtractJSON` recovers the JSON when the reply
  carries decoration the fence-only matcher could not: a prose preamble and/or
  trailing commentary, a non-standard or extra-backtick fence, or a stray leading
  `` ` `` before the opening `{` — the carved span parses where the unmodified
  reply fails with a leading-character error.
- The remaining D5 ids stay green — R-J9YL-P6K0 (validated `T` for a fenced
  response), R-JCEE-GQ1E (bounded retry), R-JDMA-UHS3 (fresh Conversation), and
  R-JEU7-89IS (CallSite faithfully applied) — so the carve change does not disturb
  the rest of the seam.
- The suite is green (`go build ./...`, `go vet ./...`, `gofmt -l .` with no
  output, `go test ./...`, `bin/check-migrations wiki`).
