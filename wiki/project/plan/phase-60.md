# Phase 60 — `ask` rewrite: analyze → retrieve → synthesize, with an honest-empty cosine floor

*Realizes design Decision 9 (`ask`: hybrid-retrieval pipeline, grounded/cited/honest-empty). Depends on Phase 57 (D36 — `Analyze`/`QueryAnalysis`) and Phase 58 (D33 — `SearchAnalyzed`/`retrieve.Result`), and on Decision 5 (`llm.JSON`), Decision 11 (public `type/slug` Path), and Decision 19 (per-call-site config). This is the last phase of the retrieval upgrade.*

`ask` stops finding pages by matching the question to a subject's exact name and instead answers from whatever the hybrid retriever surfaces — a deterministic three-stage pipeline: analyze → retrieve → synthesize. The LLM is invoked twice (analyze, synthesize) plus the query-embed inside the meaning lane; no exact-name resolution, no tool-loop, no source/claims drill-down. Strictly read-only, permanently.

In `internal/ask`, the `Asker` is rebuilt around the new flow (constructor and fields change from the exact-name shape to `{search retrieve.Retriever, pages *wiki.PageStore, c *llm.Client, analyzeSite, synthSite llm.CallSite, floor float64, finalK int}`):

1. **Analyze** — `Analyze` (Phase 57) turns the question into a `QueryAnalysis` via the `ask-subject` site.
2. **Retrieve** — `search.SearchAnalyzed(ctx, qa, finalK)` returns `retrieve.Result{Hits, TopDense, Pinned}`. **Honest-empty gate:** if **no** exact-name pin fired **and** `TopDense` is below `floor`, return `Answer{Found:false, Text:<fixed sentence>, Citations:nil}` and make **no synthesis call** (the deterministic anti-fabrication lever — the model never runs against irrelevant pages; the gate judges the raw cosine, not the fused rank score). A pin always clears the gate.
3. **Synthesize** — fetch the hit pages' bodies and run `llm.JSON[answerResult]` over **those page bodies only** (never raw claims or source text) on the `ask-synthesis` site.

**Grounding guarantees (unchanged, keep their ids):** a `Found:true` answer must carry ≥1 citation (a found-with-no-citation reply is downgraded to honest-empty); each citation is validated against the retrieved set (a citation to a page not retrieved this turn is dropped); each surviving citation is mapped from subject id to the public `type/slug` **Path** (D11) — never an internal id; `ask` performs **no** writes on any path.

**The relevance floor is a config knob.** `Asker.floor` defaults to **0.30** (deliberately conservative), moved without a recompile by a `_RELEVANCE_FLOOR` manifest knob parsed fail-loud like the D19 knobs. `analyzeSite`/`synthSite` default to `ask-subject`/`ask-synthesis`; both record to `llm_calls` with `JobID==""`.

**Retire the exact-name machinery.** Delete the old `gatherPages`/resolve-by-name path and the four exact-name tests it carried — `R-644V-3WUS`, `R-65CR-HOLH`, `R-66KN-VGC6`, `R-67SK-982V` (`internal/ask/ask_test.go`); their ids are not re-minted (the exact-name case survives only as D33's rank-1 pin). The composition root injects the hybrid retriever as the `Asker`'s `search`.

**Done when:** the suite is green (per design *Conventions*), the four retired tests are gone, and these ids are covered by clearly-named tests against a scripted/mock LLM and mock retriever (no live call):

- **R-BAFW-D24P** — `Ask` drives retrieval from the analyzed question: it calls `Analyze` then `SearchAnalyzed` and passes the **retrieved** pages (not exact-name-resolved subjects) to synthesis — a mock retriever's hits are exactly the pages fed to synthesis.
- **R-BBNS-QTVE** — honest-empty floor gate: no pin + `TopDense` below `floor` → `Found:false` with the fixed text, empty citations, and **no synthesis call**; a hit clearing the floor (or a pin) → synthesis runs.
- **R-BCVP-4LM3** — the floor is an effective knob: same retrieval result, floor above the top cosine → honest-empty (no synthesis call), floor below it → synthesis — the threshold, not a hardcode, decides.
- **R-5UPD-VVNA** — a `Found:true` answer carries ≥1 citation; a found-with-no-citation reply is downgraded to honest-empty.
- **R-5VXA-9NDZ** — each surfaced citation is one of the pages retrieved this turn; a citation to a page not in the retrieved set is dropped.
- **R-690G-MZTK** — pages-only grounding: only retrieved page bodies reach synthesis (no raw-claims or source text), and no source/claims read path exists on the `Asker`.
- **R-5X56-NF4O** — `ask` performs no writes under any path (including honest-empty and parse-failure) — DB state identical before and after.
- **R-6A8D-0RK9** — `Ask` returns the `Answer{Found, Text, Citations{Path, Title}}` contract from the deterministic analyze → retrieve → synthesize pipeline (parsed from the synthesis result).
- **R-05CG-3H6Y** — each citation identifies its page by the public `type/slug` Path (D11), mapped from the validated subject; no internal id appears in any citation.
