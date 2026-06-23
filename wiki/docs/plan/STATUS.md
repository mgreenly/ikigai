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
Phase 13 ✅ realizes D5 — robust JSON carving (`extractJSON`) replaces the fence-only `stripCodeFence`, recovering the JSON span amid prose/odd-fence/stray-backtick decoration that failed an ingest with `invalid character '`' looking for beginning of value`
Phase 14 ✅ realizes D6 — honor extract's designed call site at the composition root via `extract.DefaultCallSite` (Temperature 0, DisableReasoning, MaxParseRetries 2), ending the wiring drift that ran extraction at provider defaults with zero retries
Phase 15 ✅ realizes D7 — honor compile's designed call site at the composition root via `compile.DefaultCallSite` (Temperature 0, DisableReasoning), replacing the bare-`CallSite` construction that ran compile at provider defaults with reasoning on
Phase 16 ✅ realizes D9 — `ask` parses the agent's final message through the shared `llm.ExtractJSON` carve and its duplicate private `stripCodeFence` is deleted, so the agent's JSON answer survives the same decoration extract/compile already tolerate
Phase 17 ✅ realizes D9, D10 — `ask` rewritten as the subject-extraction pipeline (extract names → strict normalized-exact resolve → gather pages → synthesize), replacing the keyword pre-flight + tool-loop; `Answer` drops `Sources` and the MCP `ask` output drops `sources`
Phase 18 ✅ realizes D8, D3, D4 — remove the keyword retrieval lane: delete `internal/retrieve` and `PageStore.Search`, drop the `pages_fts` sync from the commit path, and `DROP TABLE pages_fts` via a new forward migration
Phase 19 ✅ realizes D11 — subject addressing: derived `type/slug` path (`slug`/`Path`) + `SubjectStore.GetByPath` forward-compare resolution with `ErrSubjectNotFound`/`ErrAmbiguousPath`, no new column or migration
Phase 20 ✅ realizes D12 — page links read-time: `Mentions` whole-word exact-normalized detection, `Service.PageWithLinks` outbound+inbound projection, and `RenderFooter` markdown footer; no table, no write-path change
Phase 21 ✅ realizes D9 — `ask` citations carry the `type/slug` path (mapped from the validated subject) instead of the internal subject id; `Citation` becomes `{Path,Title}`
Phase 22 ✅ realizes D10 — MCP path I/O: `page`/`claims` accept a path via `GetByPath`, `subjects`/`status`/page/ask-citations emit paths, the `page` body carries the D12 footer, ambiguous path → clean tool error (still eight verbs)
Phase 23 ✅ realizes D10 — MCP surface paths-out conformance: `subjects`/`claims`/`page`/`status` results carry public `type/slug` paths and no internal ULID, `claims` is keyed by path (result `{id,text,job}`), and the legacy `subject_id` wiring is removed so both Specs expose one identical eight-verb surface
Phase 24 ✅ realizes D13 — the LLM-call footprint: the `internal/llm` recorder seam (Recorder/CallRecord, CallSite.Stage, WithJobID ctx, per-round-trip detached recording) and the SQLite `LLMCallStore` over a new append-only `llm_calls` table
Phase 25 ✅ realizes D14 — job lifecycle & control: the `aborted` state, abort (pending+working via a per-job cancel map) and re-run (terminal→pending, claims replaced), on a rebuilt atomic, idempotent `integrate` commit
Phase 26 ✅ realizes D15 — cursor pagination: the pure `internal/page` codec (keyset cursor, limit clamp) and the four paginated list seams (jobs/subjects/claims/llm_calls) with a `subjects (name,id)` index
Phase 27 ✅ realizes D16 — MCP surface expansion: the `jobs`/`abort`/`rerun`/`llm_calls` verbs plus the reshaped paginated `subjects`/`claims`, taking the surface to twelve verbs and wiring the new Service methods into both Specs
Phase 28 ✅ realizes D18 — output-token budget & honest truncation: `CallSite.MaxTokens` applied to GenSettings, `sendText` truncation detection → recorded non-retriable `ErrTruncated`, and non-zero extract/compile budgets off the 4096 adapter default
Phase 29 ✅ realizes D17 — DB concurrency: `db.OpenRead` query-only read pool + `wiki.Conns{Read,Write}`, `NewService` over two handles, and per-statement SELECT→Read / mutation+tx→Write routing across every store
Phase 30 ✅ realizes D14, D17 — integrate conformance: rebuild `Service.integrate` to D14(b) — extract + resolve + every `compile` build the write-set in memory with NO tx held, then one conditional commit (delete-claims → upsert subjects → insert claims → upsert/delete pages → guarded terminal-status update, rollback on 0 rows), ending the recorder-vs-tx deadlock that wedged the single writer across the LLM phase
Phase 31 ✅ realizes D14, D4 — worker boot sweep: `UPDATE jobs SET status='pending' WHERE status='working'` on the write handle, run once at worker startup before the first claim, requeuing crash/restart-orphaned `working` jobs (safe because Phase 30's atomic integrate guarantees an orphan wrote nothing)
Phase 32 ✅ realizes D15 — jobs store: `JobFilter.Statuses []string` (match-any `status IN`), `ListJobs` newest-first (DESC keyset, reverse index scan, no migration), and new `CountJobs` filtered total; composition-root adapter mapped minimally to keep green
Phase 33 ✅ realizes D16, D10 — MCP surface: `jobs` `status` becomes an enum-published array (match-any, fail-loud naming the valid set, newest-first), new `jobs_count` verb + `WithJobsCountService`, taking the surface to thirteen verbs and wiring `CountJobs` into both Specs
Phase 34 ✅ realizes D18 — apply D18(d)'s extract/compile output budgets the code never got: raise both `defaultMaxTokens` from the 4096 adapter default to 16384, and tighten the R-MW86-M158 test from `> 0` to `>= 16384` so a silent fall-back to the adapter ceiling fails the suite
Phase 35 ✅ realizes D19 — per-call-site configuration: each site gets a `DefaultCallSite()`, the fail-loud `resolveCallSite` env parser + per-site `ManifestExtras`, `Config.ModelID`→`Config.CallSites`, the `ask`→`ask-subject`/`ask-synthesis` split, and the rewired composition root — retiring the single global model
Phase 36 ✅ realizes D20 — the extract eval harness `internal/eval`: `Case`/`GoldSubject` types, `LoadCase`/`LoadDataset` with fail-loud boundary validation, and `Run` invoking the real production `extract.Extract`
Phase 37 ✅ realizes D21 — eval scoring: deterministic subject match via the exported `wiki.Normalize`, the pinned Opus-4.8 `Judge` over `llm.JSON`, per-matched-subject claim verdict, and `Score`/`Aggregate` precision-recall metrics
Phase 38 ✅ realizes D22 — the `cmd/eval-extract` binary: flags via the D19 parsers, real extractor + judge, stamped `Scorecard` (human + `-json`), the `eval.NewJSONLRecorder` traceability sink, and one shipped blessed gold case under `testdata/eval/extract/`
Phase 39 ✅ realizes D21, D22 — correct the pinned judge call site to Opus-4.8 / high thinking / no temperature (the prior `Temperature 0` + `low` reasoning is rejected by the provider under extended thinking) and add the live, operator-run `eval-extract` end-to-end check the mock suite cannot make (R-DWI0-C7E2 rewritten, R-ME5L-HXJ3 new)
