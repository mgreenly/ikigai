# Phase 8 — ask: the honest-empty gate + grounded, cited, read-only agent

*Realizes design Decision 9 (`ask`, `internal/ask`). Depends on Phase 6 (retrieve.Service), Phase 2 (the page store + source reader), and Phase 3 (llm.Converse).*

Answer a question over the wiki's pages, drawing only on ingested content, with a
**deterministic honest-empty gate that runs before any LLM call**, strictly
read-only.

**What gets built (the observable end state):**

- `internal/ask/`: the `Answer`/`Citation` types and the `Asker`
  (`New(...)` + `Ask(ctx, owner, question) (Answer, error)`).
- **Pre-flight keyword probe** — `rs.Search(ctx, question, k)`; **zero hits →
  return `Answer{Found:false, Text:<fixed honest-empty sentence>, Citations:nil}`
  with NO LLM call.**
- **Hits exist → run the agent** — a per-request Conversation via
  `llm.Converse(askSite, askTools)` with three **in-process** `agentkit.NewTool`
  tools: `search` (→ `rs.Search`), `read_page` (→ subject's full body),
  `read_source` (→ `jobs.source_text`, size-capped at `readSourceCap`). Bounded
  `maxIter`; request `ctx` is the wall-clock budget. Final message → parse JSON →
  `Answer`.
- **Grounding / anti-fabrication:** a `Found:true` answer must carry ≥1 citation
  (else downgraded to honest-empty); each citation's subject id is validated
  against the page store and a non-resolving citation is dropped. The ported
  `DefaultAskPrompt` is trimmed to phase 1 (drop `lookup`/`timeline`/`related`).
  No write path exists — `ask` is strictly read-only, permanently.
- Call site defaults (configurable): `anthropic.ModelSonnet46`,
  `Reasoning: Level("low")`, provider-default temperature, `maxIter: 8`,
  `readSourceCap: 8192`.

**Done when:**

- R-5THH-I3WL — a question whose keyword probe returns zero hits yields
  `Found:false`, the fixed honest-empty text, empty citations, and **makes no LLM
  call**.
- R-5UPD-VVNA — a `Found:true` answer carries ≥1 citation; an agent reply claiming
  found with no citation is downgraded to honest-empty.
- R-5VXA-9NDZ — each surfaced citation resolves to a real page; a citation to a
  non-existent subject/page is dropped (no fabricated citations).
- R-5X56-NF4O — `ask` performs no writes under any path — DB state is identical
  before and after a call.
- R-5YD3-16VD — `Ask` returns the `Answer{Found,Text,Citations{Subject,Title},Sources}`
  contract parsed from the agent's final message, with three read-only in-process
  tools.
- Tests run against a real temp DB + mock provider; the suite is green.
