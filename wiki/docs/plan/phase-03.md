# Phase 3 — The LLM seam: json-mode helper over the shared Provider

*Realizes design Decision 5 (the LLM seam `internal/llm`). Depends on Phase 1.*

Turn the Phase-1 `llm.Client` shell into the structured-output seam that extract
(D6), compile (D7), and ask (D9) all reuse, so structured JSON generation and its
bounded retry live in exactly one place.

**What gets built (the observable end state):**

- `internal/llm/`: `stripCodeFence` (ported from wiki.bak — strips a leading
  ```` ```json ````/```` ``` ```` and trailing ```` ``` ```` plus surrounding
  whitespace; a no-op on bare JSON); the `CallSite` config struct (Model,
  Temperature, Reasoning, System, MaxParseRetries); the generic
  `JSON[T any](ctx, c, site, userText, validate) (T, error)` helper that builds a
  **fresh** `*agentkit.Conversation` per call, sends once, drains the stream,
  takes the final message text, `stripCodeFence` → `json.Unmarshal` into `T` →
  caller's `validate(&T)`, re-prompting with a corrective note up to
  `MaxParseRetries` and otherwise returning an error (never a silent zero `T`);
  and `Converse(site, tools)` building a fresh tool-bearing Conversation for the
  agent path.
- A **capturing/scripted mock `agentkit.Provider`** for tests (canned, optionally
  fenced/over-cap/invalid responses; records the Conversation it was handed). No
  test makes a live LLM call; the suite is green offline with no
  `ANTHROPIC_API_KEY`.

**Done when:**

- R-J8QP-BETB — `stripCodeFence` strips fenced (```` ```json ````…```` ``` ````
  and bare ```` ``` ````) responses plus surrounding whitespace to parseable
  JSON, and is a no-op on already-bare JSON.
- R-J9YL-P6K0 — `JSON` returns the validated `T` for a well-formed (including
  fenced) response, with the caller's `validate` applied.
- R-JCEE-GQ1E — `JSON` retries on unmarshal/validate failure up to
  `MaxParseRetries` (bad-then-good succeeds; always-bad returns an error, never a
  silent zero `T`).
- R-JDMA-UHS3 — each `JSON` call builds a fresh Conversation carrying no history
  from a prior call (two sequential calls show the provider no accumulated
  messages).
- R-JEU7-89IS — the Conversation is constructed with the `CallSite`'s Model,
  System, Temperature, and Reasoning (confirmed via the capturing mock provider).
- The suite is green.
