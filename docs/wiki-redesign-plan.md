# Plan — wiki redesign: a prose-pages knowledge service

The phased build plan for the wiki rewrite. The **how and the decisions** live
in `docs/wiki-redesign-design.md` (the source of truth — read it first, in full);
the full rationale behind any decision is in `docs/wiki-redesign-decisions.md`.
This file turns that design into a sequence of **subagent-sized phases**.

> Status: **draft** (2026-06-13), to be refined. The existing `wiki/` code is
> superseded — this is a clean rewrite, nothing is preserved.
>
> **The plan has two parts.** **Part I (P0a–P11)** builds the wiki service —
> starting at **P0a–P0c**, a shared-library expansion of the agentkit SDK: an
> **OpenAI** chat backend (P0a, ported from `/mnt/projects/ikigai-cli`), an **embeddings**
> lane (P0b), and **per-call cost logging** plus Anthropic effort parity (P0c) —
> all prerequisites the rest of the build assumes. **Part II (P12–P16)** builds the offline **evaluation
> harness** described in `docs/wiki-evaluation-research.md` — the tool that
> sweeps `model × effort × prompt` per inference site and scores the results.
> Part II depends on Part I:
> the harness scores the *production* call sites, so it cannot exist until they
> do. Throughout Part I we honor the **enablement obligations** (see below) that
> make those sites harness-callable; Part II then builds the rig, scorers,
> generators, and report on top. The harness is a measurement tool — it never
> gates a deploy or fails a build (research doc, "What this is *not*").

## Sizing principle

Each phase is one coherent unit a single subagent can complete from a cold start
within one context budget: it compiles, its tests pass, and it is independently
committable. Phases execute **strictly in the listed order**; each assumes the
committed output of all prior phases.

The dominant risk this plan is structured to defuse: the design's correctness
lives in the **seams between components** (the airtight end-of-run transaction,
the resolve→merge→commit pipeline shared by document and digest passes, the
cross-prompt invariants, the optimistic-commit/conflict loops). A naive
horizontal cut (extract, then resolve, then merge, …) would leave early phases
unverifiable because their load-bearing behavior only materializes once the
seam-partners exist. So we build **the spine first with stubbed integrators**
(P4–P5), then add real behavior as vertical slices *onto a working whole*
(P6a onward). Every phase from P4 on can be exercised end-to-end. So the swap
from stub to real integrator is **mechanical rather than a reshaping**, P4 pins
the two contracts every integrator hands off through — the `Manifest` type and
the `Integrator` interface — *before* any real integrator exists; P6a–P8 then
fill behavior into those fixed shapes instead of re-deriving the seam.

## Schema canonicity (a standing rule)

The complete, final SQLite schema lives in **one place — design §12
("Consolidated schema")** — and it is authoritative. P1 *transcribes* §12 into
migrations; no phase re-derives the DDL from prose. Two rules bind every phase:

- **No phase silently changes the schema shape.** A column, constraint, or index
  a phase needs must already be in §12. If it isn't, the phase **adds it to §12
  first** (and says so in its commit), then writes the migration — so §12 and the
  database never diverge.
- **Committed migrations are immutable** (`CLAUDE.md`, `bin/check-migrations`).
  Any later phase that finds the live schema lacking something §12 names adds a
  **new** corrective migration via `bin/new-migration` — it never edits an earlier
  one. A schema gap is therefore always recoverable forward, never a dead end.

This exists because P1's schema is the foundation of all of Part I, its artifacts
are immutable, and a P1 self-check written from P1's own reading cannot catch a
column P1 itself omitted — §12 is the external spec that both the migration and
the schema test are checked against.

## Dependency chain

```
Part I — the wiki service
P0a ──▶ P0b ──▶ P0c ──▶ P1 ──▶ P2 ──▶ P3 ──▶ P4 ──▶ P5 ──▶ P6a ──▶ P6b ──┐
oai     embed   cost+   schema scaffold ingest spine failure extract resolve│
chat    lib     a-eff   (stubs) policy  doors                       +match  │
                                                                            │
    ┌───────────────────────────────────────────────────────────────────────┘
    ▼
   P7a ──▶ P7b ──▶ P8 ──▶ P9a ──▶ P9b ──▶ P9c ──▶ P10 ──▶ P11 ──┐
   merge  commit  digest dups    sweep   stale   read    embed  │
   +store +conflict                                             ▼
Part II — the evaluation harness     P12 ──▶ P13 ──▶ P14 ──▶ P15 ──▶ P16
                                     design  rig    scorers  test    sweep
                                     lock                    sets    +report
```

> **Phase-sizing note.** The old monolithic **P6** (registry primitives +
> `normalize` + extract + resolve + match + the manifest's first real producer —
> two of the system's hardest prompts plus the candidates design in one phase),
> **P7** (merge + the airtight commit + the two conflict loops + the §6.1 gate +
> the stub swap), and **P9** (three lint jobs that "differ wildly in shape and
> cost," design §6) each overran the one-subagent-cold-start budget the whole
> `/finish` model rests on (`docs/README.md`). They are split: **P6 → P6a**
> (registry primitives + `normalize` + extract) **/ P6b** (resolve + candidates +
> match + the manifest), cut along the `subjects[]` data contract design §4.2
> already pins; **P7 → P7a** (merge + the plain happy-path commit + the stub
> swap) **/ P7b** (optimistic commit + conflict loops + the §6.1 gate); **P9 →
> P9a** (`lint-dups` + the shared lint plumbing) **/ P9b** (`lint-sweep`,
> zero-LLM) **/ P9c** (`lint-stale`). The chain is otherwise unchanged —
> sub-letters keep every downstream edge intact.

Numeric order satisfies every edge. P0a–P0c (the shared-library work) underpin
every LLM call site (P2's wrapper) and the embedding lane (P11), and let Part II
sweep OpenAI models. P0b (embeddings) is P11's only hard dependency and P0a
(OpenAI chat) is what Part II's OpenAI sweep needs, so both could be resequenced
next to those consumers; they are front-loaded here to keep P1–P16 numbering
stable. P1 (schema) and P2 (scaffold) underpin everything else. P4's spine needs
P3's inbox rows to select. P6b consumes P6a's extracted `subjects[]`. P7a closes
the document-pass loop P6a–P6b open; P7b hardens its commit (optimistic
concurrency + the two conflict loops + the §6.1 gate). P8 reuses P6b–P7's
resolve→merge→commit.
P9a lands the shared lint plumbing P9b/P9c reuse; P9c consumes the `stale_notes`
P7a's merge writes. P10's read side needs pages to exist (P7+). P11 (the embedding lane) is designed now but
**sequenced last** — FTS5-first is build ordering only (design §9.3). **Part II
starts only after P11**: every inference site must exist and be harness-callable
before the harness that scores them can be built (research doc's
production-code-path principle). P12 resolves the eval design before any harness
code is written, the way P1 resolves the digest fork.

## Suggested package layout

A fresh `wiki/` aligned to component boundaries (names are guidance, not
mandate): `cmd/wiki` (verb dispatcher); `internal/config` (env → config,
per-call-site prompt/model/effort seam); `internal/inbox` (`Accept`, payload
store, `ReadPayload`); `internal/worker` (pool + selection); `internal/run`
(runs lifecycle, failure policy); `internal/integrate` (extract / resolve /
merge / compile, the manifest); `internal/page` (pages + registry: subjects,
aliases); `internal/index` (hybrid retriever, FTS5 + vector lane);
`internal/lint`; `internal/read` (ask / search / timeline); `internal/mcp`;
`internal/consume` (eventplane consumer doors); `internal/llm` (the config-driven
call wrapper shared by every call site).

## Eval-engine enablement (a cross-cutting requirement)

`docs/wiki-evaluation-research.md` builds an offline harness that sweeps
`{model} × {effort} × {prompt}` per inference site and scores the results. Its
foundational principle — **evaluate the production code path, never a
reimplementation** — is a constraint on *this* build. The harness (Part II)
can only ever score what Part I makes scorable, so these obligations are what
let Part II import and exercise the *real* call sites rather than a fork. Five
obligations fall on Part I; each is woven into the phase that owns it and
re-checked in that phase's **Eval hook**:

1. **Every inference site is a clean, externally-callable function**
   `f(cfg{prompt, model, effort}, input) → output`, with `(prompt, model,
   effort)` injected from config — never a constant, never read from env at the
   site, never buried un-callable inside the worker loop. "Production" is then
   just a pinned triple per site; the harness calls the *same* function with a
   different triple. The ten sites (research §"inference inventory"): extract,
   match, compile, merge, dup judge, canonical-name pick, ask, and the three
   retrieval lanes (candidates / search / sweep). This is design §10 made
   mechanical — the `internal/llm` seam (P2) plus a callable entry point per site.
2. **Every deferred knob is a config value, not a constant** — the harness tunes
   them: `WIKI_MATCH_EXCERPT_CHARS` (P6b), candidate FTS thresholds (P6b), per-lane
   sweep thresholds (P9b), RRF `k` and `WIKI_EMBED_MODEL`/`WIKI_EMBED_DIMS` (P11),
   the ask turn/token/wall-clock budget (P10).
3. **Outputs preserve the dangerous-direction signal** so an asymmetric scorer
   can read it: match returns its binary verdict **and** the `dup_pairs`
   side-channel as distinct outputs (P6b); compile emits per-claim `cites` and
   `occurred_at` (P8); the dup judge's ternary `merge | dismiss | can't-tell-yet`
   is preserved verbatim (P9a). Don't lump these into a single pass/fail.
4. **Free goldens are captured, not discarded** — the design's by-products are
   the harness's real-data anchors: `asks.question` (and answer/citations) stored
   (P10); the dup side-channel and `dup_flags` (P6b/P9a); ingested documents + their
   extract output reachable for extract goldens (P3/P6a). Nothing in these phases
   may drop data the research doc names as a golden source.
5. **Mechanical invariants stay deterministically checkable** — citation
   preservation (§6.1, P7b), write-set conformance + claim-cite presence (P7a) are
   exposed as the same pass/fail the harness scores merge on; `normalize`,
   older-ULID-wins, the version gate, brute-force cosine stay pure and unit-tested
   (research excludes them from inference scoring by construction).

These obligations add **no new phases** — they shape the phases below. The one
new artifact they justify is small and lands in P2: a documented **call-site
registry** (the canonical list of the ten sites with their injected-config
triple), so "is every site harness-callable" is a checklist, not an audit.

## Integration testing (a standing tier)

A second test tier runs the **real models against the real call sites** —
separate from the per-phase unit gates, which mock every LLM (from P6a onward)
and must stay deterministic, free, and offline so `go test ./...` remains the
trustworthy phase gate. This tier answers a different, narrower question: **can
the currently pinned production config actually run?** It exercises each call
site's **live `(prompt, model, effort)` triple** (design §10 / the P2 call-site
registry) end-to-end and asserts the output is *structurally valid* — never
whether it is *good*. Quality is Part II's graded sweep; this is binary liveness
and wiring.

What only a real call catches, and a mock never can: a prompt edit that makes the
model emit unparseable JSON, a wrong/renamed/deprecated model id, an effort level
the model rejects, real-API response-shape drift, or the abstention path /
citation gate failing to fire at all. What it deliberately does **not** catch — a
false-merge, a fabricated answer, weak extraction — is by construction a quality
judgment and belongs to Part II.

Rules that keep it honest:

- **Structural assertions only.** Each check asserts the mechanical invariants
  obligation 5 already exposes — page row + matching `[inbox-id]` citation,
  citation-preservation gate intact, a binary/ternary verdict that resolves, a
  vector of the configured dims — on *real* output. It never asserts content, so a
  nondeterministic model still yields a deterministic pass/fail.
- **Blunt fixtures only.** Cases where even a weak model is near-certain (identical
  name + corroborating claim → `same`; an obviously different type → `no_match`).
  Subtle / adversarial cases are Part II's, not this tier's — that is what keeps
  flake low.
- **Separate, gated, advisory.** Build-tag / env-gated (real calls need
  `ANTHROPIC_API_KEY` / `OPENAI_API_KEY` and the network), so it is **always in
  the tree but never in the unit gate**. Run on demand or on a cron; it is
  **advisory, never a deploy gate** — the same non-gating stance as Part II, for
  the same nondeterminism-and-cost reasons. A red run is a "go investigate"
  signal; mitigate residual flake with blunt fixtures, structural assertions, and
  a single retry.
- **Pinned to the live config.** Because it runs whatever triple config currently
  pins, the moment Part II's report (P16) swaps a default model or prompt this
  tier silently re-validates that the new pick at least *runs* — so a config
  change can't quietly ship a prompt the model chokes on.
- **Administered at named checkpoints, not merely present.** A tier that is "in
  the tree but never in the unit gate" is built and then never read: the unit
  gate mocks every LLM (from P6a onward) and a phase is *done* when it compiles
  and those mocked tests pass, so an integrator with a placeholder prompt (prompts
  are deferred config defaults — "Open items") can reach "all green" while its
  live `(prompt, model, effort)` triple has never once produced parseable,
  schema-valid output. To close that gap the orchestrator (or the coordinator)
  **runs the accumulated tier, with keys present, at three checkpoints**: after
  **P6a** (the first LLM site — extract — lands; catches an unparseable-JSON /
  wrong-model-id / mis-shaped-schema failure at the *first* site that has one,
  ~5 phases before P11 and while the fix is one prompt), after **P7a** (the first
  full document-pass slice — extract + match + merge live), and after **P11** (the
  full pipeline). A **red checkpoint pauses the march for investigation** — this
  is an explicit orchestrator stop-and-look, **not** a hard CI/deploy gate (the
  advisory, non-gating stance above is unchanged for the same
  nondeterminism-and-cost reasons). The checkpoints are *when the already-built
  signal is read*, nothing more.

Like the enablement obligations, this adds **no new phases** — it grows one slice
at a time inside the call-site phases that already exist. Each of **P6a–P11**
contributes its slice as its call sites land (see each phase's *Integration test*
line), so the full end-to-end real-model liveness check exists the moment P11
closes rather than being bolted on at the end — and the three checkpoints above
(after P6a, P7a, P11) are where the `/finish` march actually *runs* that signal,
so a hollow integrator can't ride a green unit gate all the way to P11 unnoticed.

---

## P0a — agentkit OpenAI chat backend (Responses API)

*Shared-library expansion; the deepest chat prerequisite. A near-mechanical
**port** of the proven backend in the sibling repo `/mnt/projects/ikigai-cli`
(`app-root/internal/provider/openai/openai.go`, governed by its
`reqs/providers.md`) into this repo's agentkit — not a green-field
implementation. P2's call-site wrapper resolves an OpenAI model through it, and
Part II can only sweep OpenAI once it exists. Blast radius across every
agent-backed service (prompts, dropbox, wiki) — hence its own phase with a
build-everything gate.*

The groundwork is already laid: `agentkit/provider` defines the provider-neutral
`Client` interface (R-S04B-QD3D), and `agentkit/model` already knows
`ProviderOpenAI`, the prefix→provider mapping, and a `gpt-5.5` registry entry
with effort vocabulary and pricing (`registry.go:168`). The two repos'
`provider.Client` interfaces are **identical except this repo added
`Request.MaxTokens`**, so the port is import-path + one-field work, not a
reshape.

- **`agentkit/provider/openai/`** — port the ikigai-cli backend, preserving its
  behavior: POST `/v1/responses` with SSE streaming (R-WWTI-LSSO; **Chat
  Completions is not used** — the Responses API is the only surface exposing
  `reasoning.encrypted_content`). Normalize the stream into the existing wire
  events; **OpenAI encrypted reasoning round-trips via `ThinkingBlock`**
  (`Text` = reasoning-item id, `Signature` = `encrypted_content`; `store:false`
  + `include:["reasoning.encrypted_content"]` — R-3D9Z-4ND7, satisfying
  R-ROBI-V64M for OpenAI). Native structured output via `text.format.json_schema`
  + `strict:true` with recursive strict-mode schema adaptation (R-3Z86-0IPP,
  R-3V3G-PYML); effort → `reasoning.effort` (R-22XS-LD6T); typed errors from
  `error.type` (R-574J-S9EP); usage from `response.completed` (R-ZEVA-05QR). The
  agent loop and wire codec **must not** import the subpackage (the interface is
  the only seam).
- **The port deltas (the only non-mechanical work):** honor this repo's
  `Request.MaxTokens` as `max_output_tokens` (the one interface field ikigai-cli
  predates); extend the composition-root `clientFactory`
  (`prompts/internal/runner/runner.go:43`) to dispatch `gpt-*` → `openai.New`;
  carry `SetBaseURL` for the offline test gate. Confirm the `gpt-5.5` registry
  entry (models, accepted effort levels, pricing — a model with unknown pricing
  cannot ship, R-ZZLK-I9CK).
- Key from `OPENAI_API_KEY`, presence-gated at the composition root (the
  `ANTHROPIC_API_KEY` pattern; absent key = OpenAI models unavailable, not a
  crash). Secret reaches the process via the `.envrc` pattern — never read.

**Touches:** `agentkit/provider/openai/` (new), `agentkit/model/registry.go`
(confirm OpenAI entries), the composition-root `clientFactory`,
`agentkit/CLAUDE.md`. **No wiki code yet.**
**Verify:** `go test ./agentkit/...`; an OpenAI `Stream` round-trips a tool call
and a reasoning signature against a **hand-authored SSE fixture over an
`httptest` server** (the `anthropic_test.go` pattern — `SetBaseURL`, no live
key, fully offline); structured-output, effort, and error-mapping cases mirror
the ikigai-cli tests; **all agent-backed services still build** (prompts,
dropbox, wiki) — the reason this seam is isolated.

## P0b — agentkit embeddings client (`agentkit/embed`)

*Shared-library expansion; a **net-new** capability with no prior art
(`/mnt/projects/ikigai-cli` has no embeddings). Embeddings are **not** a generation call,
so they live in a dedicated sibling library, not the chat `Client` interface.
**First and only consumer is P11**; built here so the shared library is touched
once. Same build-everything gate.*

- **`agentkit/embed/`** — a small provider-neutral interface, e.g.
  `Embedder.Embed(ctx, model string, dims int, texts []string) → ([][]float32,
  error)`, with an OpenAI implementation for `text-embedding-3-large` @
  configurable dims. **Unary JSON POST** (not SSE); **one HTTP request per
  `Embed` call** over the whole `texts` slice — the caller (P11's catch-up
  worker) owns chunking to the provider's array/token limits, so "one API call"
  stays literal and the lib hides no fan-out. `dims` passed through verbatim (the
  provider rejects an illegal value); typed errors reuse `provider.Error`;
  `input_tokens` read from the response's `usage.prompt_tokens`.
- **Pricing:** add an embeddings-pricing entry for `text-embedding-3-large` (the
  published per-million **input**-token rate; output/cache = 0) so the embed
  call's `cost_usd` (P0c) is real, not zero (R-ZZLK-I9CK — every billed model
  declares its rate). The chat `modelSpec` is chat-shaped (efforts,
  maxOutputTokens), so this is a small dedicated embeddings-pricing path, not a
  chat-registry row.
- Key from `OPENAI_API_KEY`, presence-gated at the composition root (absent key →
  the embedder is simply not constructed; P11's vector lane falls back to
  lexical-only, design §9.3). The constructor rejects an empty key (the
  `anthropic.New` / `openai.New` pattern).

**Touches:** `agentkit/embed/` (new), the embeddings-pricing data,
`agentkit/CLAUDE.md`. **No wiki code yet.**
**Verify:** `go test ./agentkit/...`; the client returns correct-dimension
vectors and surfaces `usage.prompt_tokens` against a **canned embeddings
response over an `httptest` server** (offline, no live key); an absent key is a
clean construction refusal, not a panic; **all agent-backed services still
build.**

## P0c — Per-call usage/cost logging + Anthropic effort parity

*Shared-library cross-cutting slice. Makes the AI library emit **one jsonl line
per API call** (chat and embed, every backend) into the app's structured log,
and closes the one remaining provider-uniformity gap (Anthropic effort).
Deliberately separate from P0a/P0b because it also modifies the **existing**
Anthropic backend. Depends on P0a + P0b (it wires logging into all three call
paths).*

- **One `slog` record per API call** (the requirement): every agentkit
  provider/embed call emits exactly one record at the point usage is known — per
  chat `Stream` completion (so a multi-turn agent run emits one line **per
  turn**, each turn being one HTTP request) and per `Embed` call. The sink is an
  injected `*slog.Logger` (stdlib — **no `agentkit`→`appkit` dependency**; the
  service passes its `appkit/logging` JSON logger in, so the line lands in the
  app's log as one JSON object — `appkit/logging/logging.go:42`); a **nil logger is a
  no-op**, exactly like `*trace.Tracer`. `trace.Tracer` stays the separate
  `--raw` debug channel; this accounting line is distinct and always-on.
- **Fields:** `provider`, `model`, `effort`, `input_tokens`, `output_tokens`,
  `cache_read_tokens`, `cost_usd` (from `model.ModelPricing(r).ComputeCost(...)`
  for chat, the embeddings rate for embed), `duration_ms`, and `stop_reason`
  (chat only). **Attribution without coupling:** agentkit knows no wiki
  call-sites — the caller pre-binds them, e.g. P2's `internal/llm` wrapper passes
  `logger.With(slog.String("call_site","extract"), slog.String("run_id", id))`
  and the backend only appends the accounting fields. This hands **Part II's
  cost/latency reporting** per-call, per-site data for free (research doc's
  report rows = score **+ cost + latency**).
- **Anthropic effort parity:** wire `req.Effort` → adaptive-thinking on the
  Messages API in `agentkit/provider/anthropic/` (today effort is silently
  dropped — `buildPayload` reads neither effort nor schema). Structured output
  already works uniformly via the agent loop's parse+validate+retry
  (R-WFWM-BKWX, `agentkit/agent/loop.go:197`), so **no structured-output work is
  needed** — only effort. Model + effort are **build-time config**, validated at
  startup per-model (R-ZX67-O1L1); there is no runtime selection — Part II
  varies the triple only by injecting an alternate config into the same call-site
  function.

**Touches:** `agentkit/provider/{anthropic,openai}/`, `agentkit/embed/`, a small
shared logging helper (record assembly), `agentkit/CLAUDE.md`.
**Verify:** `go test ./agentkit/...`; a captured `*slog.Logger` (buffer handler)
receives **exactly one JSON record per API call** for Anthropic chat, OpenAI
chat, and embed, each carrying a non-zero `cost_usd` matching the registry
computation and the caller's pre-bound `call_site`; a nil logger emits nothing;
an Anthropic request with effort sends adaptive-thinking on the wire; **all
agent-backed services still build.**

## P1 — Decisions lock + consolidated schema

*Design §12 (the authoritative schema final), plus §2.2, §4.1, §4.5, §5, §6, §7,
§9.2, §9.3, §11. The prerequisite for everything; resolves the open schema/fork
items before any code depends on them.*

- **Lock the one build-time fork — the digest claimable unit** (design §11).
  *Framing 1*: claim the whole cron row, run its bound entries sequentially
  (simplest selection; needs a leave-pending-if-locked-out path). *Framing 2*:
  the claimable unit is a `(cron-row, entry)` pair (uniform selection, pure
  completion-time-join stamp, no leave-pending case; costs a cron-row→entry
  expansion in selection). **Recommendation: Framing 2** — it keeps the
  selection critical section uniform and removes a special case from the spine,
  which matters most given P4 is built before any digest exists. Record the
  decision at the top of this file once locked.
- **Decommission the legacy wiki — treat the service as empty** (design §1: a
  clean rewrite, nothing preserved). The existing `wiki/` is a full prior
  service that the rewrite must clear *before* P2 scaffolds the fresh one: a Go
  tree (`cmd/wiki/main.go` +
  `internal/{ask,consume,ingest,jobstore,lint,mcp,search,store,ids}`, referencing
  the dead `wiki_ingest` / `wiki_jobs` schema) and **three committed, immutable
  migrations** (`001_schema_migrations.sql`, `002_wiki.sql`,
  `003_feed_offset.sql`). The wiki holds **no data worth keeping — the `int` box
  DB is disposable** — so the teardown is unconditional:
  - **`git rm` the entire legacy Go tree** — every `wiki/internal/**` package and
    `wiki/cmd/wiki/main.go` — so P2's "fresh `wiki/`" starts from an **empty
    `internal/`** with no stale files to overwrite-or-collide (several legacy
    package names — `consume`, `lint`, `mcp`, `search` — recur in the new layout
    with different contents; a partial overwrite would leave a broken build,
    blowing the phase's context budget).
  - **Do not delete, edit, or rename the three legacy migrations** —
    `bin/check-migrations` forbids it (immutability is CI-enforced; the obvious
    "just delete `002_wiki.sql`" reflex trips a red build). Instead make the
    **first new migration a drop-legacy step** —
    `DROP TABLE IF EXISTS wiki_ingest; DROP TABLE IF EXISTS wiki_jobs;
    DROP TABLE IF EXISTS feed_offset;` (plus their indexes) — so every DB, fresh
    box or existing, converges to the clean schema *after* the frozen old
    migrations replay forward. Create it first (via `bin/new-migration wiki
    drop_legacy`) so its timestamp sorts ahead of the consolidated DDL below;
    `001_schema_migrations.sql` is the shared appkit bookkeeping table and stays.
- **Transcribe design §12 (the schema final)** into ordered migrations via
  `bin/new-migration wiki <name>` (never hand-pick a number; never edit a
  committed migration), landing **after** the drop-legacy migration above. §12 is
  the single source — copy it table for table. Do **not** re-derive the DDL from
  the scattered riders in §2.2/§4.1/§6/§9.3, and do **not** add, drop, or rename a
  column/constraint/index relative to §12. If §12 looks wrong or incomplete, **fix
  §12 first** (and surface it), then transcribe — the migration and §12 must never
  diverge. §12 already incorporates the gaps a prose reading misses (`aliases.type`,
  `dup_flags.run_id`, the full `stale_notes` columns, the dropped `subjects.page`),
  so transcription is mechanical, not a synthesis.
- **The named literals are already pinned in §12.3** — the eventplane
  system-identity `owner` value (`system@ikigenba`) and the `wiki.row_dead_lettered`
  / `wiki.ingest_refused` payload shapes (§8). No literal is invented in this phase.

**Touches:** removal of the legacy `wiki/cmd/wiki/` + `wiki/internal/**` Go tree;
`wiki/internal/db/migrations/*.sql` (the drop-legacy migration + the consolidated
DDL transcribed from design §12); schema test; and the top of this file (locked
fork decision).
**Verify:** the legacy Go tree is gone (`grep -rn 'wiki_ingest\|wiki_jobs'
wiki/cmd wiki/internal` is empty); `bin/check-migrations` passes (old migrations
untouched, only adds); migrations load forward-only and drop the legacy tables;
downgrade guard intact; the schema test asserts every table/column/constraint/index
**against design §12** (checked against the external spec, not against this phase's
own reading — so an omission cannot hide in both the DDL and the test).
`go test ./wiki/internal/db/...`.

---

## P2 — Service scaffold + config-injection seam

*Design §1, §10. Establishes the chassis and the config discipline every later
phase plugs into.*

P1 has already `git rm`ed the legacy `wiki/` Go tree, so `internal/` starts
**empty** — every package below is net-new and overwrites no stale file.

- `cmd/wiki/main.go` on the appkit verb dispatcher: `serve` / `version` /
  `manifest` / `migrate` / `backup` / `restore`. `GOWORK=off` production build
  green; wired into `go.work` for local dev.
- `internal/config`: config-from-env at the composition root, including the
  **per-call-site seam** — a typed config carrying `{prompt, model, effort}` for
  each of the eight LLM call sites of design §10 (extract, match, merge, compile,
  ask, and the three lint calls — dup judge, fold, stale repair) plus the embed
  model/dims. (The harness-callable *registry* below is a superset — it also
  tracks the canonical-name pick and the three retrieval lanes, the research
  doc's ten inference sites.) Validated at the `serve`
  boundary, injected inward; **no call site ever reads env or a constant**
  (design §10). Defaults may be placeholders here — each call-site phase fills
  its real default.
- `internal/llm`: the thin wrapper a call site calls with injected
  prompt+model+effort (so adding a call site is uniform). Structured-call and
  agent-run shapes, over the agentkit provider abstraction — so a call site's
  `model` may resolve to **either** an Anthropic or an OpenAI model (the P0a
  backend), chosen purely by config. **The wrapper is the enablement seam** — a
  site that goes through it is automatically harness-callable with a swapped
  triple (enablement obligation 1).
- **The call-site registry**: a documented, single-source list of the ten
  inference sites (extract, match, compile, merge, dup judge, canonical-name
  pick, ask, candidates, search, sweep), each naming its injected-config triple
  and its callable entry point. Sites are added to it as their phases land; P2
  establishes the registry and the convention so "every site is harness-callable"
  is a checklist by the end, not a retrofit.
- MCP server skeleton (`internal/mcp`): register the tool surface
  (`ingest_text`, `ingest_url`, a status verb, `search`, `ask`, `timeline`) as
  stubs returning not-implemented; the `reflection` + `health` tools live.
- eventplane producer: declare the outbox and the **two** event types (§8); not
  yet emitted. eventplane **consumer** doors deferred to P3.

**Touches:** `wiki/cmd/wiki/`, `wiki/internal/{config,llm,mcp}/`, manifest emit,
`go.work`.
**Verify:** all verbs dispatch; `manifest`/`version` correct; MCP server starts
and lists tools; `go build` under `GOWORK=off`; no legacy package remains
(`grep -rn wiki_ingest wiki/internal wiki/cmd` empty); all 7 services still build.
**Eval hook:** the call-site registry exists; the `internal/llm` wrapper accepts
an injected `(prompt, model, effort)` triple and no site can reach a model
without it (obligations 1–2). A trivial test swaps the triple on a stub site.

---

## P3 — The ingest side: `Accept`, the inbox, front doors

*Design §2.1, §2.2, §8. The whole write path, no integration yet.*

- `internal/inbox`: the single `Accept(owner, kind, source, mime, title, tags,
  bytes) → (id, sha256, dup, err)` — hash, size, inline-vs-spill decision, row
  insert, worker nudge. Synchronous, transactional, **never calls an LLM**.
- Payload store: inline (`content` ≤ `WIKI_INBOX_INLINE_MAX`, default 4096) vs.
  content-addressed `blobs/<aa>/<sha256>` (write→fsync→insert). The lone
  `ReadPayload(row)` accessor; readers dispatch on the row, never the threshold.
- Size cap: refuse `> WIKI_INGEST_MAX_BYTES` (default 131072) loudly at the
  door; **no ingest-side chunking**. Refusal emits `wiki.ingest_refused` (plain
  outbox write, pre-accept).
- Front doors: `ingest_text` → `Accept` directly; `ingest_url` → fetch+extract
  server-side then `Accept`; the **receipt contract** (return inbox id + sha256
  + dup flag — not a job id). The status verb polls integration state against an
  inbox id.
- eventplane **consumer doors** (`internal/consume`): one goroutine per
  subscribed feed; cursor commits **after** `Accept` returns (at-least-once,
  hash dedup). Per-handler mapping: domain event → `Accept(kind=event)`; dropbox
  file event → fetch `content_url` → `Accept(kind=document)`; `cron.<name>` →
  `Accept(kind=event, source="cron:<name>")`. Consumer doors stamp the
  system-identity `owner` from P1. Non-interactive doors notify on refusal.

**Touches:** `wiki/internal/{inbox,consume,mcp}/`, blob dir handling, outbox.
**Verify:** text/url accept; inline & spill paths; `sha256` always stored; dup
flag on re-accept; oversized refusal emits the event; consumer cursor advances
only post-`Accept`; crash before integration loses nothing. Unit + table tests.

---

## P4 — The dispatcher-free worker spine (stubbed integrators)

*Design §3, §4.5 (runs lifecycle). The seam-forcing phase: the spine exists and
is verifiable before any real integrator does.*

- `internal/worker`: `WIKI_INTEGRATION_WORKERS` (default 4) identical goroutines,
  each looping over the **selection critical section** under one in-flight-set
  mutex: (1) a pending cron row whose bound job name(s) aren't in flight →
  `TryLock` job name; (2) else the oldest pending **document** row not in flight
  → `TryLock` row id; (3) else block on the wake `Cond`. **Cron before
  documents.** Event rows are invisible to selection. Run claimed work with **no
  lock held**, commit, drop the claim, loop.
- Per the locked fork (P1): if **Framing 2**, selection expands a cron row into
  `(cron-row, entry)` candidates; a cron row no job binds is stamped immediately
  as a no-op.
- The nudge is an **optimization, not the truth**: contentless wake; every wake
  (and boot) re-runs selection against the inbox. Wake sources: arrival nudge,
  run completion, shutdown (the `ineligible_until` timer joins in P5).
- **The swap-boundary contract** (`internal/integrate`): the two cross-phase
  types every integrator hands off through, pinned *here* — before any real
  integrator exists — so P6a–P8 drop in onto a working whole instead of reshaping
  the spine (this is the swap-is-mechanical bet of the Sizing principle made
  concrete):
  - **`Manifest`** — the in-memory work order, **never persisted** (the run id is
    its durable identity): extracted/compiled `subjects[]`, each annotated with
    its resolved subject_id + target page, a **per-page base `version` slot** (the
    `pages.version` the page was read at — design §3: "the manifest records the
    version merge read"; populated at merge-read time in P7a, consumed by P7b's
    optimistic-commit `WHERE subject=? AND version=?` guard — pinned **now** so
    P7b reads an existing field instead of reshaping the frozen contract), plus a
    **generalized `{text, cites[]}` claim shape** (the document pass fills `cites`
    with the one inbox row id; compile fills per-claim cites — §4.3, §5). Defined
    **empty-capable** so a stub can emit a minimal one (the stub fills the version
    slot with a dummy value to round-trip it — see Verify).
  - **`Integrator`** — the interface the document-pass stub, the cron/no-op stub,
    the real document pass (P7a), and the real compile (P8) **all** satisfy:
    claim → run → produce a `Manifest`. The shared resolve→merge→commit pipeline
    and the end-of-run transaction consume a `Manifest`, never integrator-specific
    data — which is exactly what lets merge "not tell which integrator ran" (P8).
  - **Manifest field obligations (the §12-style canonicity rule for the seam).**
    The `Manifest` is pinned here against an explicit enumeration of *every field
    each downstream consumer reads* — transcribed from the design, not whittled to
    the stub's minimal needs — so no field a later consumer needs is discovered
    only after the contract is frozen (the failure this split is built to prevent:
    P7b, the densest cold-start unit, silently reshaping a spine type):
    - **P6b** (first real producer): resolved subject_id, target page, claims.
    - **P7a** (merge + commit): the write-set pages, the `{text, cites[]}` claims,
      `occurred_at` (first-writer-wins, §4.1), merge's `superseded` source, and
      the point that **populates the per-page base `version`** at merge-read time.
    - **P7b** (optimistic commit): **reads the per-page base `version`** (design
      §3) plus a re-run-merge-for-one-page handle — the two things the conflict
      loop needs.
    - **P8** (compile): per-claim `cites` (the generalized claim shape carries
      these).

    Any field a later phase finds missing is added **here first** (and the
    producers updated), exactly as a schema gap is added to §12 first — the seam
    and its consumers never diverge.
- `internal/run`: the `runs` lifecycle — insert `running` before execution (the
  one write outside the commit); the **generic end-of-run transaction wrapper**
  that **takes a `Manifest` and** atomically writes the run's terminal
  `succeeded` + `integrated_by` stamp (real page/registry writes arrive in P7a,
  but the transaction *shape* — and its `Manifest` input — is built and tested
  here). **Stamp only at commit, never at claim.** Boot sweep flips orphaned
  `running` → `crashed`. `TryLock` keys: job-name for cron/digest/lint, row-id
  for the document pass.
- **Stub integrators**: a document-pass stub and a cron/no-op stub that
  **implement `Integrator`** and exercise claim → run → commit → stamp without
  LLM work — each **emitting a minimal `Manifest`** the end-of-run transaction
  round-trips (a stub can be told to fail, for P5).

**Touches:** `wiki/internal/{worker,run,integrate}/`.
**Verify:** rows get claimed exactly once (no double-claim under N workers);
cron-before-document priority; crash between claim and commit leaves the row
**pending** and restart re-selects it; boot sweep marks orphans `crashed`;
selection stays oldest-first (no starvation). The stub integrators **implement
the `Integrator` interface and round-trip a `Manifest` that carries every field
obligation above — including a populated per-page base `version` slot** through
the end-of-run transaction, so the seam's *completeness* (not merely its
existence) is a tested property *here*, not a P7b-time audit. Concurrency tests.

---

## P5 — Failure policy: bounded retries + dead-letter

*Design §7, §8. Completes the spine's resilience story on top of P4's stubs.*

- The pending predicate: `integrated_by='' AND dead_at IS NULL AND
  (ineligible_until IS NULL OR ineligible_until <= now)`.
- `ineligible_until` set on **every** failed run (no transient/persistent
  dispatch): `now + random(2–4) × avg_run_duration × 2^(failures−1)`,
  `avg_run_duration` from recent `runs` floored at 60s; `failures` counted since
  the last re-queue. Workers gain the **timer wake source** (armed to the
  earliest future `ineligible_until`).
- `dead_at` set at the threshold `WIKI_RUN_ATTEMPTS_MAX` (default 5); clears
  `ineligible_until` in the same UPDATE; **emits `wiki.row_dead_lettered`** in
  that transaction. `requeued_at` scopes the retry counter (re-queue = clear
  `dead_at`, grant a fresh budget). `runs` stays the single source of truth (no
  denormalized counter).
- The check lives **at failure time**, applied by whatever marks the run (and
  the boot sweep when marking `crashed`); selection stays policy-free. **All
  failures count** toward the threshold (no exempt type).

**Touches:** `wiki/internal/run/` (failure path), `wiki/internal/worker/` (timer
wake), outbox.
**Verify:** failed stub run backs off then re-selects; N failures dead-letter +
emit the event + clear `ineligible_until`; re-queue restores a fresh budget;
crashed orphans count one attempt. Time-driven tests with an injected clock.

---

## P6a — Document pass: registry primitives + extract (manifest input)

*Design §4.2, §4.1 (registry). The first real integrator's front half: the page
registry, the `normalize` function, and the extract call. Resolve/match/manifest
are P6b; merge/commit close the loop in P7a/P7b. Split from the old monolithic P6
— two of the system's hardest prompts (extract, match) plus the candidates design
in one phase overran the one-subagent cold-start budget — cut along the
extract-output data contract (§4.2) so each half carries one hard prompt.*

- `internal/page` registry primitives: `subjects` + `aliases`; the `normalize`
  function (NFKC, casefold, trim, collapse whitespace, strip diacritics —
  pure, versioned, rebuildable).
- **Extract** (`internal/integrate`): one full-context structured, tool-less LLM
  call (config-injected prompt+model+effort). Input = whole document + a
  mechanical **context header** ("received on", never "today is") + the subject
  schema. Output JSON: subjects `{type, kind, name, aliases, claims[],
  occurred_at}`. Within-document co-reference only; names as the document states
  them; self-contained claims; keep-it-relative on unresolvable time;
  salience gate (when in doubt, do not extract). Dialog-aware clause for
  transcripts.
- **The P6a→P6b seam** is the extract output: schema-valid `subjects[]` JSON, the
  data contract design §4.2 already pins. P6a ends there; P6b consumes it. Nothing
  in P6a touches resolution, the registry lookup, or the manifest.

**Touches:** `wiki/internal/{integrate,page,llm,config}/`.
**Verify:** extract goldens with a mocked LLM; `normalize` unit tests; extract
emits schema-valid `subjects[]` (the §4.2 contract / the P6b seam). No resolution,
no writes to pages yet.
**Eval hook:** extract registered as a harness-callable site (obligation 1);
extract output is reachable as a golden alongside its source document
(obligation 4).
**Integration test:** the standing tier's first slice (extract half) — real
extract on a blunt fixture document through the **live pinned triple**: assert
extract returns schema-valid `subjects[]` (required fields present, claims free of
pronouns / document-relative refs). Match's slice lands in P6b; no page assertions
yet — the commit lands in P7a.

---

## P6b — Document pass: resolve + match + the manifest (manifest out)

*Design §4.3, §4.1 (registry). The first real integrator's resolution half: the
mechanical resolve arms, the candidates step, the match call, and the manifest.
Consumes P6a's extracted `subjects[]`; merge/commit close it in P7a/P7b.*

- **Resolve**: per subject build the key set, one alias query → **one id**
  (resolved, no LLM) / **many ids** (straight to match, dup-flag the pair) /
  **zero ids** → candidates: two FTS queries (name/alias vs registry names; claim
  text vs page bodies), **zero candidates → create, no LLM**.
- **Match**: the one resolution LLM call — structured, tool-less, judges the
  shortlist at once, **binary** `same(id) | no_match`; identity not similarity;
  **doubt is no_match**; candidate-pair side channel feeds `dup_flags`. Excerpt =
  canonical name + full alias list + first `WIKI_MATCH_EXCERPT_CHARS` (default
  600) of page body.
- **Manifest**: populate the `Manifest` type **pinned in P4** (don't redefine it)
  — every extracted subject annotated with its resolved subject_id + target page
  + claims; the document pass fills the generalized `{text, cites[]}` claim shape
  with the one inbox row id. The **per-page base `version` slot is part of the
  pinned type** but is filled at merge-read time (P7a), not here — P6b leaves it
  unset, per P4's field obligations. P6b is the type's first real producer; its
  in-memory, never-persisted nature (the run id is its durable identity) is P4's
  contract.

**Touches:** `wiki/internal/{integrate,page,llm,config}/`.
**Verify:** resolve's three arms (one/many/zero ids) deterministic; match binary
contract; manifest assembled correctly. No writes to pages yet.
**Eval hook:** match and the candidates retrieval lane registered as
harness-callable sites (obligation 1); `WIKI_MATCH_EXCERPT_CHARS` and the
candidate FTS thresholds are config (obligation 2); match returns its verdict
**and** the `dup_pairs` side-channel as distinct outputs (obligation 3).
**Integration test:** the standing tier's match slice — real match on a blunt
fixture (live pinned triple): assert match returns a clean binary verdict whose
`same(id)` resolves to a real subject. No page assertions yet — the commit lands
in P7a.

---

## P7a — Document pass: merge + the plain end-of-run commit (happy path)

*Design §4.4, §4.5, §6 (the `stale_notes` writer). Closes the document-pass
loop — the airtight transaction the whole spine was built for — on the
**single-pass happy path**; P7b hardens it for concurrency. Split from the old
monolithic P7 so each half fits one cold-start context.*

- `internal/page` pages store: prose body + thin frontmatter (`subject`, `type`,
  `kind`, `title`); inline `[inbox-id]` citations; **lead discipline** (the
  cross-prompt obligation merge owes match). FTS5 kept current in the commit.
- **Merge**: one agent run per document (config-injected). Input = the manifest
  **only**, never the original document. Write set = the manifest's pages
  exactly; read set looser (neighbors). Fold each subject's claims as prose
  (weave new, corroborate known with the new citation, corral contradictions
  with both sides + citations). Tools: read + write pages only.
- **The one end-of-run transaction**, made real: updated/created pages + registry
  inserts + `dup_flags` + the run row + `integrated_by` (and `occurred_at`
  first-writer-wins from the manifest). Zero mid-run partial writes. This fills
  the generic end-of-run transaction wrapper P4 built and tested with stubs — now
  writing real pages/registry. **Merge records the base `version` it read for
  each page into the manifest's per-page version slot** (the value P7b's guard
  will consume — design §3's "the version merge read"); `version` is bumped per
  write, but the **conflict-handling `WHERE`-guard and the conflict loops are
  P7b**: P7a is the single-writer happy path.
- **`stale_notes` writer hook**: when merge touches a read-only neighbor page it
  contradicts, it appends a `stale_notes` row (with `cites`) **in its existing
  commit** (design §6). This is the producer side `lint-stale` (P9c) consumes —
  built here so P9c's work-list is not empty by construction.
- Replace the P4 document-pass **stub** with this real integrator — the **same
  `Integrator` interface**, emitting the **same `Manifest`**, so the spine is
  unchanged (the swap is mechanical, exactly as P4 set up and tested).

**Touches:** `wiki/internal/{integrate,page,run}/`.
**Verify:** full `ingest_text` → pages happy path (mocked LLM); the stub→real
swap leaves the spine green (P4's concurrency tests still pass); the manifest's
per-page base `version` slot is populated with the value merge read (so P7b's
guard has it); a `stale_notes` row is appended when merge contradicts a neighbor;
provenance chain (answer-less: page cites inbox id → `ReadPayload`). End-to-end
test through the spine.
**Eval hook:** merge registered as a harness-callable site (obligation 1); two of
merge's mechanical invariants — write-set conformance and claim-cite presence —
exposed as the deterministic pass/fail the harness scores (obligation 5; the
citation-preservation gate lands in P7b); the merged page's lead stays
match-recoverable (merge's closed-loop obligation to match).
**Integration test:** the first full document-pass slice — `ingest_text` a blunt
fixture through the **real** pipeline on the live pinned triple; assert a page row
exists with ≥1 `[inbox-id]` citation matching its source row, and the registry
subject + alias exist. Structure only — never whether the prose is *good*.

---

## P7b — Document pass: optimistic commit + conflict loops + the §6.1 gate

*Design §3 (optimistic commit / conflict loops), §6.1. Hardens P7a's commit for
the N-worker pool: the two conflict types, the bounded retry, and the
citation-preservation gate. Split from the old P7 so the intricate concurrency
machinery is its own cold-start unit on top of a working merge+commit.*

- **Optimistic commit**: the commit guarded by `WHERE subject=? AND version=?`
  — the version read from **the manifest's per-page base-`version` slot (pinned
  in P4, populated by merge in P7a)**, so P7b consumes an existing field and
  reshapes nothing; zero rows → conflict → roll back, re-read, **re-run merge
  only** for that page, recommit. **Duplicate-subject minting** caught by
  `UNIQUE(type, norm)` → roll back → **restart at resolve** for the colliding
  subject only (never re-extract).
- **Conflict loop** (shared by both conflict types): cap **3 commit attempts**
  then fail naming **conflict-retry exhaustion**; `conflicts` counted on `runs`;
  post-exhaustion re-selection delayed via `ineligible_until` (P5).
- **Citation-preservation gate** (§6.1): merge also emits a `superseded` list;
  at commit, `old − new` citations must equal the declared list, else **failed
  call** (retried in-run, never committed).

**Touches:** `wiki/internal/{integrate,page,run}/`.
**Verify:** conflict loop retries and exhausts cleanly; the lost-update conflict
re-runs merge only; duplicate-mint restarts at resolve; the citation gate rejects
undeclared loss; `conflicts` counted on `runs`; post-exhaustion delay applied.
Concurrency tests on top of P7a's happy path.
**Eval hook:** the third mechanical invariant — citation preservation — now
exposed as the same deterministic pass/fail the harness scores merge on
(completing obligation 5 for merge alongside P7a's two).
**Integration test:** re-ingest a fixture that forces a re-merge and assert the
§6.1 citation-preservation gate holds through a conflict-driven re-run (live
pinned triple). Structure only.

---

## P8 — Digest pass: compile + the jobs config + digest concurrency

*Design §5, §3 (cron rows as durable batch authorization), §6 (jobs config). The
first reuse of the shared resolve→merge→commit pipeline.*

- **`jobs` config** (config, not a table; design §6): `name`, `trigger`,
  `select` (digest-only). Selection treats digest and lint entries identically.
  **Boot-time partition check**: selectors must partition event rows — overlap →
  refuse to boot; a consumed source matched by no selector → surface it.
- **Compile** (`internal/integrate`): plays extract's role for event piles —
  same structured, tool-less, golden-testable call shape, targeting extract's
  output schema directly (**no prose-digest artifact**). Resolves `occurred_at`
  from event payloads. **Per-claim `cites`** (events presented with inbox ids
  visible). Implements the **same P4 `Integrator` interface** and emits a
  `Manifest`, entering the shared pipeline at **resolve** — merge can't tell
  which integrator ran.
- **Cron-fired wiring**: a `cron.<name>` row → the worker looks up bound entries
  and runs each bound digest as its own `runs` row with `caused_by` = cron-row
  id (a tiny fan-out). Default daily; cadence is config; **no volume trigger**.
- **Digest concurrency**: at most one in-flight run per batch-entry name
  (job-name `TryLock`, per the locked P1 fork); **stamp by id list, never by
  selector** (re-evaluating at commit would silently drop mid-run arrivals).
  **Cron-row stamping** = the worker-local **completion-time join** ("do all
  bound entries for this `caused_by` now have a *succeeded* run?" → stamp once,
  WHERE-guarded as a no-op on races). A failed run leaves the cron row pending
  (the retry authorization).
- **Batch failure policy** (§7): policy acts on `runs.caused_by` (the cron row),
  dead-letter granularity = the causing row only, never event rows.

**Touches:** `wiki/internal/{integrate,worker,run,config}/`, the no-op cron stub
from P4 replaced.
**Verify:** events accumulate as invisible rows; cron row fires the bound
digest(s); compile goldens; completion-time join stamps the cron row only when
all bound runs succeeded; partition-overlap refusal at boot; batch failure
leaves the cron row pending.
**Eval hook:** compile registered as a harness-callable site (obligation 1) and
emits per-claim `cites` + `occurred_at` as distinct, scorable outputs (the
citation-mis-attribution risk the harness measures — obligation 3).
**Integration test:** real compile on a blunt event pile (live pinned triple);
assert the emitted `subjects[]` carry per-claim `cites` that resolve to real inbox
ids and an `occurred_at`, and that the pile integrates through the shared pipeline
to a page.

---

## P9a — Lint: `lint-dups` + the shared lint plumbing

*Design §6 (and §6.1 already gated in P7b). The first lint job, and the home of
the `dup_flags` helper + `jobs`-config + `lint_run` plumbing every later lint job
reuses. Each lint job is a worker-selected job with one `runs` row per attempt;
failure policy applies verbatim. Split from the old monolithic P9 — whose three
jobs "differ wildly in shape and cost" (design §6) — so each fits one cold-start
context.*

- **`dup_flags` storage helper**: `FlagDup(x,y)` (sorts to canonical order,
  `INSERT … ON CONFLICT DO NOTHING`); `UNIQUE` + `CHECK(subject_a < subject_b)`
  already in P1. (Reused by P9b's sweep.)
- **`lint-dups`** — dup-queue consumption. A flag is evidence, not a verdict:
  first a dedicated **judge** call (both full pages + complete alias lists),
  binary-ish → **merge** / **dismiss** (permanent) / **can't-tell-yet** (stamp
  `judged_version_a/_b`; re-judge only when a page version advances).
  Subject-merge: **older ULID wins** mechanically (judge picks only the canonical
  *name*); loser hard-deleted in one transaction (repoint aliases, rewrite open
  `dup_flags`, **fold** prose, delete loser page+subject, set winner
  `canonical_name`, mark row `merged`). One run per trigger, **one transaction
  per pair**. **Judge and fold are two separate tool-less calls** (fold inherits
  the §6.1 citation gate).
- **The shared lint plumbing** (lands here with the first lint job): register
  lint entries in the `jobs` config (no selector); the manual `lint_run(job)` MCP
  verb is just another front door that `Accept`s a trigger row. P9b and P9c
  register their entries against this.

**Touches:** `wiki/internal/lint/`, `wiki/internal/{page,config,mcp}/`.
**Verify:** `lint-dups` in isolation (mocked LLM); `FlagDup` canonical-orders and
de-dups; subject-merge hard-delete leaves no dangling loser id; can't-tell-yet
skips until a version advances; one transaction per pair; `lint_run` `Accept`s a
trigger row. Per-job tests.
**Eval hook:** the dup judge and the canonical-name pick registered as
harness-callable sites (obligation 1); the judge's ternary verdict
(`merge | dismiss | can't-tell-yet`) preserved verbatim, not collapsed
(obligation 3); `dup_flags` retained as a golden source (obligation 4).
**Integration test:** real dup judge on a blunt obviously-same and
obviously-different pair (live pinned triple); assert the verdict is one of
`merge | dismiss | can't-tell-yet`, and on a `merge` the fold yields a body that
passes the §6.1 citation gate. Blunt pairs only — subtle identity stays Part II.

---

## P9b — Lint: `lint-sweep` (semantic duplicate sweep, zero-LLM)

*Design §6. The proactive walker that flags duplicates built from disjoint
streams (Bob-from-email vs Robert-from-CRM). Depends on P9a's `FlagDup` + jobs
plumbing. **Fully mechanical — zero LLM** — so it carries no integration-tier
slice (it has no live call site).*

- **`lint-sweep`** — semantic duplicate sweep, **flag-only**, **zero LLM**: for
  each subject run the same two candidate FTS queries (P6b); pairs above the flag
  threshold → `FlagDup`. Idempotent via the pair UNIQUE (settled pairs bounce
  off). Wide scan, rare cadence. Register the entry in the `jobs` config (no
  selector).

**Touches:** `wiki/internal/lint/`, `wiki/internal/config/`.
**Verify:** sweep flags pairs above threshold; idempotent on re-run; settled
(`merged`/`dismissed`) pairs are not re-flagged. Per-job test, no LLM.
**Eval hook:** the sweep's per-lane flag thresholds are config the harness sweeps
(obligation 2).

---

## P9c — Lint: `lint-stale` (staleness repair)

*Design §6, §6.1. Consumes the `stale_notes` rows P7a's merge writes. Depends on
P9a's jobs plumbing and P7a's writer hook.*

- **`lint-stale`** — staleness repair backed by `stale_notes` (the flag-only
  writers were built in P7a — merge/fold append a note with `cites` in their
  existing commit). Work list = open notes; one **tool-less call per subject**
  batching its open notes (page + notes + cited payloads in; rewritten page +
  per-note disposition out, one transaction). Inherits the §6.1 gate. Register
  the entry in the `jobs` config (no selector).

**Touches:** `wiki/internal/lint/`, `wiki/internal/{page,config}/`.
**Verify:** stale repair consumes open notes; one transaction per subject;
the rewritten page passes the §6.1 gate; each note's disposition is recorded.
Per-job test (mocked LLM).
**Eval hook:** the stale-repair call takes its `(prompt, model, effort)` from
injected config (design §10 — it is a call site, though not one of the ten
scored registry sites); its rewritten-page output inherits merge's mechanical
invariants exposed as deterministic pass/fail (obligation 5).
**Integration test:** real stale repair on a blunt fixture note (live pinned
triple); assert the rewritten page passes the §6.1 citation-preservation gate.
Structure only.

---

## P10 — The read side: ask + search + timeline

*Design §9.1, §9.2 (tools 1–5; `related` deferred), §9.3 (search verb contract,
lexical-only for now). Needs pages (P7+).*

- **Search verb** — zero-LLM, the public side door: input `query` + `limit`
  (default 3, cap 10); **registry-first** resolution (exact alias pins rank 1,
  the retriever fills the rest); a hit is the **whole page**; rank order only, no
  scores; nothing prepended. (Lexical FTS5 only here; the hybrid retriever
  primitive lands in P11 behind the same interface.)
- **`timeline(from, to)`** — public verb, zero-LLM registry query over
  `type=event` subjects with `occurred_at` in the window (ISO-8601 prefix args,
  lexicographic range). Description frames it as "list event subjects in a date
  window," never "answer questions about a period."
- **Ask** — hosted-ask-first, **synchronous**, **strictly read-only** (no
  writes, no flight lock, no transaction; runs fully parallel with integration).
  Inner agent (config-injected) with a server-side budget (max turns / tokens /
  wall-clock) and the **six** inner tools — `search`, `lookup`, `read_page`,
  `read_source` (→ `ReadPayload`, size-capped), `timeline`; **`related` is
  goldens-gated and NOT built**. **Answer contract**: page-level citations
  (subject id + title); inbox ids only when `read_source` was used; contradictions
  surfaced, never resolved.
- **`asks` table** lifecycle (insert `running`, finalize at end, boot sweep
  marks orphans `crashed`); stores `question`.
- **Steering tool descriptions**: `search` = exact/known-item fetch; `ask` = the
  default for any question ("answers come back cited; do not assemble answers
  from search results yourself"). Description text is part of the design surface.

**Touches:** `wiki/internal/{read,index,mcp}/`, `wiki/internal/db/` (asks).
**Verify:** search registry-first + whole-page contract; timeline window query;
ask happy path (mocked inner agent) returns page-cited answer; "wiki has nothing
on this" over fabrication; asks lifecycle + orphan sweep.
**Eval hook:** ask and the search retrieval lane registered as harness-callable
sites (obligation 1); the ask turn/token/wall-clock budget is config
(obligation 2); `asks.question` + answer + citations stored as the free real-data
golden source the research doc names (obligation 4).
**Integration test:** real `ask` over the fixture wiki (live pinned triple) on one
answerable question and one known-gap question; assert answer-xor-abstention,
every cited subject id resolves, and the gap question abstains. Real `search`
returns whole-page hits, registry-first.

---

## P11 — The embedding lane: hybrid retriever

*Design §9.3. Designed now, sequenced last — FTS5-first was build ordering only.
Slots behind the retriever interface P10 already consumes.*

- **One hybrid retriever primitive, two lanes (BM25 + vector), RRF-fused**
  (k=60, top ~50/lane in), serving three call sites: the search verb / ask's
  search tool, resolution's candidates step (P6b), and `lint-sweep` (P9b). Vector
  lane **independently switchable per call site**, on only when measurement shows
  lift. **Sweep does not fuse** — per-lane thresholds (eval-harness knobs).
- **Embedding unit = one vector per page**: canonical name + alias list + body
  (truncated). `page_vectors(subject, embedded_version, model, vector)`.
- **Write path = async catch-up**, never in the integration commit: an
  in-process embedder goroutine wakes on run completions (the nudge pattern),
  sweeps a work-list query (vector missing / behind `version` / wrong `model`),
  batches API calls. Stale vectors **serve until replaced** (reads never compare
  `embedded_version` to `pages.version`); **model match IS a read-side validity
  condition** (only `model = current` rows; cross-model cosine is garbage).
- **Vector storage = plain table, brute-force cosine scan in Go** (pure-Go
  chassis rules out sqlite-vec; ~5k × 1024-dim is <10ms — exact, testable, zero
  tuning). **Read-side degradation**: a failed read-time embed → lexical-only.
- **Provider**: the **P0b embeddings client** — OpenAI `text-embedding-3-large`
  @ 1024 dims (`WIKI_EMBED_MODEL` + `WIKI_EMBED_DIMS`); `OPENAI_API_KEY`
  presence-gated at the composition root (absent key → lexical-only, the
  `ANTHROPIC_API_KEY` pattern). A model/dims change is a config change the
  catch-up worker absorbs (first deploy and model change are the same code path).

**Touches:** `wiki/internal/index/`, `wiki/internal/config/`, the catch-up
goroutine, `wiki/internal/db/` (page_vectors).
**Verify:** brute-force cosine correctness; catch-up work-list (missing / stale /
wrong-model); RRF fusion ranking; per-call-site lane switch; absent-key and
read-failure both fall back to lexical; model-mismatch rows excluded from reads.
**Eval hook:** RRF `k`, `WIKI_EMBED_MODEL`/`WIKI_EMBED_DIMS`, and the per-lane
thresholds are config the harness sweeps (obligation 2); the per-call-site lane
switch is exactly the "lexical-only vs hybrid, per site, scored" deliverable the
research doc (§8–10) makes first-class — the retriever exposes both lanes so the
harness can measure recall lift vs cost. **Completes the call-site registry:**
all ten sites are now harness-callable (obligation 1 fully satisfied).
**Integration test:** real embed round-trip on the live `WIKI_EMBED_MODEL` /
`WIKI_EMBED_DIMS`; assert a `page_vectors` row with `len(vector) ==
WIKI_EMBED_DIMS` and `model == current`, and that the hybrid retriever returns
ranked whole-page hits. **Closes the standing tier** — the full Part I pipeline
now has an end-to-end real-model liveness check.

---

# Part II — the evaluation harness

The offline tool of `docs/wiki-evaluation-research.md`: a runner that sweeps
`{generation} → {prompt, data} × {model} × {effort}` over the **real** call
sites (Part I's enablement makes this possible) and produces a per-generation
comparison table of score + cost + latency, with the dangerous-direction error
surfaced separately. It is a measurement tool — never CI, never a deploy gate.
These phases start only after P11 (every site exists and is harness-callable).

## P12 — Eval design lock

*Research doc "Open questions for the design doc." A plan executes a settled
design; the research fixes the *what* and *shape* but leaves six questions open.
This phase resolves them and records the result in a new
`docs/wiki-evaluation-design.md` (keeping the design→plan convention intact),
the way P1 locks the digest fork before code depends on it. Mostly authorship +
decisions, minimal code.*

Resolve and record:
- **Golden authorship vs real-data harvest** — the mix per site, and whether
  real data seeds the generator or only validates it.
- **Judge-model independence** — fix the judge model for the rubric/answer
  judges (merge, ask); it **must not** be a model under test (self-preference);
  set panel size for subjective criteria.
- **Test-set storage & identity** — how a `(dataset + prompt)` bundle is named,
  versioned, and stored on disk, and how a run pins which bundle it used so
  results stay attributable after a set is superseded.
- **Where the harness lives & cost control** — `bin/` tool vs `opsctl` vs a Go
  test target; and **output caching** keyed on `(test-set, case, prompt, model,
  effort)` so re-scoring is free (the sweep is a cartesian product of paid calls
  — caching is load-bearing, not an optimization).
- **Saturation detection** — the rule for declaring a generation saturated (top
  configs all above a score with no separation) so "mint the next generation" is
  triggered, not vibes.
- **Decision presentation** — how a run's matrix is shown so a human can pick:
  dangerous-direction error + cost + latency beside the headline score, never a
  single lumped rank.

**Touches:** `docs/wiki-evaluation-design.md` (new).
**Verify:** all six questions answered with rationale; the dataset format
`(case_id, site, generation, failure_tag, input, gold)` and the four scorer
kinds are pinned; reviewed in full before P13.

## P13 — The rig

*Research doc §"High-level plan" piece 1. Build once, prove with one site.*

- Dataset format + loader: `(case_id, site, generation, failure_tag, input,
  gold)`.
- The **runner**: injects a `(prompt, model, effort)` triple into the **real**
  call-site function (via Part I's call-site registry), captures the raw output
  plus **cost and latency**. "Production" is just one pinned triple the runner
  can also evaluate.
- The **results table** `config × metric`, always reported **per generation**;
  the outer sweep dimension is the generation (its prompt + data).
- **Output caching** keyed on `(test-set, case, prompt, model, effort)` per P12,
  so a re-score with a changed scorer costs zero provider calls.
- Wire **one site end-to-end** to prove the rig — Match (its identity corpus is
  the headline case and is shared by three sites).

**Touches:** the harness tool (location per P12), reading Part I's call sites.
**Verify:** the rig runs Match over a tiny fixture, sweeps `model × effort`
(incl. an OpenAI model via P0a), writes the table, and reproduces a second run
entirely from cache (zero paid calls).

## P14 — The scorer library

*Research doc piece 2. Four kinds cover all ten sites; every scorer reports the
**dangerous direction separately** (the research's asymmetry principle).*

- **Set-alignment** (extract, compile): align predicted↔gold subjects; subject
  **precision and recall separately** (over-extraction its own counter); type
  accuracy; claim recall (fuzzy/LLM-judged); self-containedness check; compile
  adds compression ratio + per-claim citation precision/recall + `occurred_at`.
- **Asymmetric confusion** (match, dup judge): binary/ternary confusion with
  **false-merge** (and false-split / false-dismiss) as named separate axes; the
  `dup_pairs` side-channel as its own recall number; a `can't-tell-yet`-when-
  evidence-present laziness metric.
- **Recall@k + RRF** (candidates / search / sweep): recall@k for candidates
  (recall is king — a miss mints a permanent dup), a ranking metric for search,
  pair-discovery recall for sweep.
- **Mechanical-checks + rubric-judge-panel** (merge, ask): merge — the §6.1
  citation-preservation gate, write-set conformance, claim-cite presence (the
  same deterministic checks Part I exposes, reused here for free) + a rubric
  panel (lead-identity, woven-not-ledgered, contradictions-corralled, no loss,
  no hallucination); ask — answer correctness, citation faithfulness, abstention
  on the gap set (fabrication rate is the headline), contradiction-surfacing,
  with **retrieval failure decomposed from synthesis failure**. Judge model +
  panel size per P12.

**Touches:** the harness scorer library.
**Verify:** each scorer against hand-built known-good/known-bad inputs;
dangerous-direction rates reported separately, never lumped into accuracy.

## P15 — Per-site test sets / generators

*Research doc piece 3. One generator per site producing versioned
`(dataset + prompt)` bundles (generations); shared corpora are the structural
saving.*

- **Shared corpora**: one synthetic **identity corpus** → Match + dup judge +
  candidates + sweep (one truth, four consumers); one synthetic **wiki** (pages
  with known facts, gaps, contradictions) → ask + search. Plus standalone sets
  for extract, compile, merge (manifests built directly), canonical-name pick.
- Each generator spans **blunt → subtle** case shapes (per the research's
  per-site data designs); later generations weight toward subtle. Generations
  are the only escalation mechanism — no per-case difficulty labels.
- **LLM-author the goldens, then an adversarial verification pass** over them
  (the redesign's own verify pattern); anchor against real data where it exists
  (ingested documents for extract, the `asks` table for ask — the free goldens
  Part I captured).
- Bundle naming/versioning/storage per P12; the harness selects a bundle by
  name so a later harder generation stands up beside the first. **This also
  produces the cross-subject / temporal-span ask goldens** that gate `related`
  (design §9.2) — un-gating it becomes a measured decision.

**Touches:** the harness generators + committed test-set bundles.
**Verify:** a gen-1 bundle exists for every site; the adversarial pass ran; the
harness loads a bundle by name; the shared corpora feed their multiple consumers.

## P16 — The sweep + report

*Research doc piece 4. The product: the table that turns scores into a decision —
and licenses the config defaults Part I deferred.*

- Run the full matrix `generation → {prompt, data} × model × effort` across all
  sites, **including OpenAI models** (enabled by P0a) — model-per-role and
  effort-per-role are first-class sweep axes alongside the deferred knobs
  (`WIKI_MATCH_EXCERPT_CHARS`, candidate/sweep thresholds, RRF `k`, embed
  model/dims, ask budget).
- Produce the **comparison report** per P12's presentation rule: one row per
  config, cells = score **+ cost + latency**, dangerous-direction error
  surfaced separately, always against a named generation; apply the saturation
  rule.
- The retrieval **side-by-side** (lexical-only vs hybrid per call site, recall
  lift vs cost) — the deliverable that licenses or declines the vector lane at
  each site (research §8–10).
- **The feedback loop**: the report's chosen configs become Part I's config
  defaults (the pinned `(prompt, model, effort)` per site, the deferred knobs).
  Document that this is how defaults get set — a human reads the matrix and picks.

**Touches:** the harness sweep driver + report renderer.
**Verify:** an end-to-end sweep on gen-1 produces the report; cost/latency and
dangerous-direction errors present per config; a worked "pick a config" example;
re-running re-scores from cache without new paid calls.

# Open items the plan inherits (filled during execution, not blockers)

- **Exact prompts** for the LLM call sites and whether a shared invariants block
  (lead discipline, citation rules, salience polarity) is factored out: each
  call-site phase writes its prompt as the **config default** for that site; the
  Part II report then arbitrates candidates against the pinned production prompt.
  Only rough section-shapes exist today (design §11).
- **Exact models per call site + config defaults** (lint cadences, ask budget
  knobs, embed model/dims): pinned as provisional defaults in the owning Part I
  phase, then **set for real by P16's report** — that feedback loop is the
  reason Part II is in this plan rather than a separate track.
- **The six eval design questions** (judge-model independence, test-set storage,
  harness location + caching, saturation detection, golden authorship-vs-harvest,
  decision presentation) are resolved in **P12**, not left open — they are
  prerequisites internal to Part II, recorded in `wiki-evaluation-design.md`.
