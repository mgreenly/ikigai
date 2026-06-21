# wiki — Plan Status

This is the **manifest**: one line per phase in build order, and the **only**
place a phase's status marker lives. Each phase line begins with the literal word
`Phase` and carries `✅` (done) or `⬜` (not started). The build loop finds its
next unit of work with `grep -nE '^Phase .* ⬜' docs/plan/STATUS.md | head -1`,
reads only that phase's `docs/plan/phase-NN.md`, builds it, and on completion
flips that one marker. This file deliberately carries **no bare status glyph**
anywhere but on a phase line, so the anchored grep matches only phase lines.

Phase 01 ✅ realizes D1, D2 — module wires agentkit (published, no replace) and the service skeleton builds prod-shaped and serves, failing loud without ANTHROPIC_API_KEY
Phase 02 ✅ realizes D3 — the phase-1 data model: five-table schema, normalize, and the domain stores (incl. page-write-with-FTS-sync)
Phase 03 ✅ realizes D5 — the LLM seam: stripCodeFence, the generic JSON[T] json-mode helper with bounded retry, and Converse
Phase 04 ✅ realizes D6 — the extract stage: source text → subjects+claims over llm.JSON, salience gate + honest-empty
Phase 05 ✅ realizes D7 — the compile stage: full recompile from claims with three-layer 12k enforcement
Phase 06 ✅ realizes D8 — the retrieval seam: FTS5 keyword lane + Retriever interface + registry-first pin + SearchLimits
Phase 07 ✅ realizes D4 — the ingest pipeline: fire-and-return Ingest, JobStatus, and the single integrate/commit worker
Phase 08 ✅ realizes D9 — ask: the deterministic honest-empty gate + grounded, cited, read-only agent
Phase 09 ✅ realizes D10 — the eight-verb MCP tool surface behind RequireIdentity, with owner attribution
Phase 10 ✅ realizes D10 — MCP surface conformance: the full eight-verb surface (ingest/status/ask/subjects/claims/page/health/reflection), bare-named and wired into the composition root, superseding Phase 9's partial health-only surface
Phase 11 ✅ realizes D10 — MCP input schemas are valid JSON Schema: the object-schema helper omits `required` when empty so `subjects` no longer emits `"required": null`, which made strict clients reject the whole tools/list
Phase 12 ✅ realizes D6 — `occurred_at` is an optional, format-validated ISO-8601 prefix on every subject type (required only for events, retained as extracted), replacing the empty-unless-event rule that deterministically failed ingests like "Gary Gygax"
Phase 13 ⬜ realizes D5 — robust JSON carving (`extractJSON`) replaces the fence-only `stripCodeFence`, recovering the JSON span amid prose/odd-fence/stray-backtick decoration that failed an ingest with `invalid character '`' looking for beginning of value`
Phase 14 ⬜ realizes D6 — honor extract's designed call site at the composition root via `extract.DefaultCallSite` (Temperature 0, DisableReasoning, MaxParseRetries 2), ending the wiring drift that ran extraction at provider defaults with zero retries
Phase 15 ⬜ realizes D7 — honor compile's designed call site at the composition root via `compile.DefaultCallSite` (Temperature 0, DisableReasoning), replacing the bare-`CallSite` construction that ran compile at provider defaults with reasoning on
Phase 16 ⬜ realizes D9 — `ask` parses the agent's final message through the shared `llm.ExtractJSON` carve and its duplicate private `stripCodeFence` is deleted, so the agent's JSON answer survives the same decoration extract/compile already tolerate
