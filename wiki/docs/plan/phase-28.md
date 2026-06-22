# Phase 28 — Output-token budget & honest truncation handling

*Realizes design Decision 18 (output-token budget & truncation). Depends on
Phase 03 and 13 (the D5 `internal/llm` seam: `CallSite`, `JSON`/`Converse`,
`ExtractJSON`), Phase 24 (the D13 recorder and `sendText` single round-trip
seam), and Phase 04 and 05 (the D6/D7 `DefaultCallSite` defaults).*

The LLM seam stops silently running at the agentkit adapter's 4096 default and
instead carries an explicit, generous output budget per stage, detects when a
generation is truncated at that ceiling, and surfaces it as a distinct,
recorded, non-retriable reason — so a too-large extract fails honestly instead
of dying on the opaque `unexpected end of JSON input` after wasting every parse
retry.

Built in `internal/llm` (the bulk) plus the two stage defaults:

- `llm.CallSite` grows one field, `MaxTokens int` (mirroring how D13 added
  `Stage`). `JSON[T]` and `Converse` set it on the Conversation's generation
  settings (`GenSettings{… MaxTokens: site.MaxTokens}`), so the configured
  ceiling reaches the provider; the D5 faithful-application guarantee now covers
  `MaxTokens` alongside Model/System/Temperature/Reasoning.
- `sendText` (the D13 round-trip seam, where reported usage is in hand) detects
  truncation: when output-token usage meets or exceeds `site.MaxTokens` (and
  `MaxTokens > 0`) it returns a distinct `llm.ErrTruncated` sentinel (wrapping
  the stage/usage) instead of the truncated text. The round-trip's `CallRecord`
  is still written, retaining the **truncated `Response`** with the truncation
  reason in `Err` — distinguishable from a clean success (empty `Err`) and a
  transport error (empty `Response`). When the provider reports no usage at all,
  detection cannot fire and the call falls through to the existing parse/retry
  path unchanged.
- `JSON[T]`'s retry loop treats `ErrTruncated` as **non-retriable** — returned
  immediately without consuming a `MaxParseRetries` iteration; a normal
  unmarshal/validate failure still retries as before.
- `extract.DefaultCallSite` and `compile.DefaultCallSite` each carry a non-zero
  `MaxTokens` generously above their realistic output (extract well above a
  per-document extraction, e.g. ~16384; compile above the 12,000-char page cap
  plus overhead, e.g. ~8192) — the exact integers are tunable headroom, the
  contract is "generously above realistic output." The composition root already
  builds the extractor/compiler from these defaults (Phase 14/15), so no further
  wiring change is needed.

**Done when:** R-MSKH-GPX5, R-MTSD-UHNU, R-MV0A-89EJ, and R-MW86-M158 are each
covered by a clearly-named test against a capturing/scripted mock provider (the
configured ceiling reaching the provider; a usage-at-ceiling truncated body
yielding `ErrTruncated` plus a `CallRecord` that retains the truncated body and
truncation reason; truncation terminal with exactly one round-trip even at
`MaxParseRetries > 0`; both stage defaults carrying a non-zero `MaxTokens`), and
the suite is green.
