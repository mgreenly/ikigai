# Phase 17 — `ask` rewritten as the subject-extraction pipeline

*Realizes design Decision 9 (`ask`, `internal/ask`) and the D10 (`internal/mcp`) ask-output change it forces. Depends on Phase 08/16 (the old ask agent this replaces), Phase 02 (the `SubjectStore`/`PageStore` domain stores), and Phase 03 (`llm.JSON`/`llm.CallSite`).*

D9 has been re-decided: `ask` no longer does a keyword pre-flight probe over page
bodies and no longer runs an agentic tool-loop. It now answers the way the product
promises — **extract the subjects the question names, resolve each to a wiki
subject by exact normalized name, read those subjects' pages, and synthesize a
cited answer from them** — as a deterministic three-stage pipeline where the LLM is
invoked twice (once to extract names, once to synthesize) through the existing
`llm.JSON[T]` seam. There is no keyword retrieval, no tool-loop, and no
source/claims drill-down: ask draws on the compiled pages only. This phase replaces
the Phase 08/16 implementation and rewires its two consumers so the build stays
green.

**What gets built (the observable end state):**

- `internal/ask` rewritten:
  - `Asker` now holds `subjects *wiki.SubjectStore`, `pages *wiki.PageStore`,
    `c *llm.Client`, and two call sites `extractSite, synthSite llm.CallSite`.
    `New(subjects, pages, c, extractSite, synthSite) *Asker`. The
    `retrieve.Service`, `SourceReader`, `maxIter`, and `readSourceCap` fields are
    gone.
  - `Answer` is `{Found bool; Text string; Citations []Citation}` — the `Sources`
    field is **removed**. `Citation` stays `{Subject, Title string}`.
  - `Ask(ctx, owner, question)` runs **extract → resolve & gather → synthesize**:
    1. **Extract (one LLM call)** — `llm.JSON[extractResult]` over `extractSite`
       returns the candidate subject **names** (`extractResult{ Subjects []string }`).
    2. **Resolve & gather (deterministic, no LLM)** — for each name:
       `normalize` → `SubjectStore.GetByNormName` → on hit `PageStore.GetBySubject`;
       dedup resolved subjects by id, first-seen order. Strict normalized-exact
       only (a partial/variant name resolves to nothing). Best-effort partial:
       every name that resolves is gathered, the rest skipped. **Zero resolved →
       return `Answer{Found:false, Text:<fixed honest-empty sentence>, Citations:nil}`
       and make NO synthesis call.**
    3. **Synthesize (one LLM call)** — `llm.JSON[answerResult]` over `synthSite`,
       fed **only the gathered page bodies**. A `Found:true` answer must carry ≥1
       citation (else downgraded to honest-empty); each citation is validated
       against the gathered set and a non-matching citation is dropped.
  - The keyword pre-flight probe, the `search`/`read_page`/`read_source`
    in-process tools, and the `Converse` tool-loop are all deleted. Two default
    call sites: `extractSite` (`anthropic.ModelSonnet46`, `Temperature 0`,
    `DisableReasoning()` — deterministic NER) and `synthSite`
    (`anthropic.ModelSonnet46`, `Reasoning: Level("low")`, provider-default temp).
- `cmd/wiki/main.go`: constructs the new `Asker` from the subject/page stores and
  the LLM client; the `retrieve.NewService(...)`/`SearchLimits` locals that fed the
  old `Asker` are removed (leaving `internal/retrieve` unimported for Phase 18).
- `internal/mcp`: the `ask` tool result no longer includes `sources` — it returns
  `{found, answer, citations:[{subject,title}]}`.

**Done when:**

- R-644V-3WUS — a test asserts a single extraction call maps the question to a list
  of candidate subject names and the pipeline resolves from exactly those names
  (scripted mock returns the list).
- R-65CR-HOLH — resolution is strict normalized-exact: a name resolves only via
  `GetByNormName` on its normalized form; a partial/variant name that is not an
  exact normalized subject name resolves to nothing (no page gathered).
- R-66KN-VGC6 — best-effort partial: with several extracted names where only some
  resolve, every resolving subject's page is gathered (deduped) and passed to
  synthesis, and unresolved names are skipped without failing the call.
- R-67SK-982V — honest-empty gate: when no extracted name resolves, `Ask` returns
  `Found:false` with the fixed text and empty citations, and makes **no synthesis
  LLM call**.
- R-5UPD-VVNA — a `Found:true` answer carries ≥1 citation; a synthesis reply
  claiming found with no citation is downgraded to honest-empty.
- R-5VXA-9NDZ — each surfaced citation is one of the pages gathered this turn; a
  citation to a subject/page not in the gathered set is dropped.
- R-690G-MZTK — pages-only grounding: only the gathered page bodies feed
  synthesis; no raw-claims or original-source text reaches the synthesis call, and
  no source/claims read path exists on the `Asker`.
- R-5X56-NF4O — `ask` performs no writes under any path (including honest-empty and
  parse-failure) — DB state is identical before and after a call.
- R-6A8D-0RK9 — `Ask` returns the `Answer{Found,Text,Citations{Subject,Title}}`
  contract synthesized by the deterministic extract → resolve → synthesize
  pipeline (parsed from the synthesis call's structured result).
- Tests run against a real temp DB + a scripted/capturing mock provider (no live
  LLM call); the suite is green (`go build ./...`, `go vet ./...`, `gofmt -l .`
  with no output, `go test ./...`, `bin/check-migrations wiki`).
