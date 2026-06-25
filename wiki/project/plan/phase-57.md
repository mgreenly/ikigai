# Phase 57 — Preparing the question: one analysis call (`QueryAnalysis` + `Analyze`)

*Realizes design Decision 36 (preparing the question: one analysis call). Depends on Decision 5 (the `llm.JSON[T]` seam and `llm.Client`) and Decision 19 (the `ask-subject` call site — Phase 35). Sequenced **before** fusion (Phase 58) because `SearchAnalyzed` consumes the `QueryAnalysis` type defined here; the `ask` rewrite (Phase 60) calls `Analyze`.*

The single LLM call `ask` makes about the question changes what it produces: instead of a bare list of subject names, a small structured analysis that the downstream fan-out (D33) routes per lane. Same call site as today — `ask-subject` — so no new call site, knob, or log stage.

- **`QueryAnalysis` (`internal/wiki`).** `{SubQueries []string \`json:"sub_queries"\`, Keywords []string \`json:"keywords"\`, Aliases []string \`json:"aliases"\`}` — the question split per subject (≤4), the salient terms, and likely alternate names. Lives in `internal/wiki` so both `internal/ask` (which produces it) and `internal/retrieve` (D33, which consumes it) can import it without a cycle.
- **`Analyze` (`internal/ask`).** `Analyze(ctx, c *llm.Client, site llm.CallSite, question string) (wiki.QueryAnalysis, error)` runs one `ask-subject` call through the existing `llm.JSON[QueryAnalysis]` helper (parsing, fenced-JSON carve, and bounded retries already handled), then applies the caps and fallback:
  - **Sub-queries capped at 4** — a response with more is clamped to the first four.
  - **Empty → whole question** — if the model returns no sub-queries, `Analyze` yields a single sub-query equal to the raw question, so search always has something to run (an empty keyword/alias list is likewise fine, never an error).
- **Prompt.** The `ask-subject` system prompt is rewritten to ask for exactly this JSON — break the question into the distinct subjects it asks about (one sub-query each, at most 4), list key terms and any alternate names, and nothing else (no answering, no speculation) — with a JSON schema and a worked example.

This repurposes D9's old stage 1 (which extracted bare subject names for exact resolution); the resolve-by-name stage it fed is replaced by hybrid retrieval in Phase 60. Honest-empty and citation guarantees stay with D09.

**Done when:** the suite is green (per design *Conventions*) and these ids are covered by clearly-named tests against the scripted/mock LLM (no live call):

- **R-QB7A-Z95U** — `Analyze` returns a `QueryAnalysis{SubQueries, Keywords, Aliases}` parsed from a single `ask-subject` call (mock returns the JSON; parsed fields match).
- **R-QCF7-D0WJ** — sub-queries are capped at 4: a response with more than four is clamped to the first four.
- **R-QDN3-QSN8** — empty-decomposition fallback: with no sub-queries returned, `Analyze` yields a single sub-query equal to the original question, without erroring.
