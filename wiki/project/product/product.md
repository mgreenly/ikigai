# wiki — Product

**Authority: intent.** This document owns *why* the wiki exists, *for whom*, what is in and out of scope, and what we **promise** the user — in outcome terms only. Mechanism, data formats, schema, exit codes, and test assertions live in `project/design/design.md`. Where the two touch observable behavior, product states the *promise* and design states the *exact, checkable proof*; that boundary keeps product, design, and plan from overlapping.

## Problem

Knowledge worked out through an agent evaporates when the session ends, and the next conversation starts cold. Retrieval over raw chunks re-derives understanding every time — nothing accumulates, cross-references go undrawn, contradictions unreconciled — while a hand-maintained wiki collapses under its own upkeep. The user is left with no durable, compounding, inspectable store of what they've learned.

Running such a store surfaces three more pains. The ingest is LLM-backed, and LLMs fail invisibly — most often by not returning the structured content the ingest needs; without a durable record of what each call sent, received, retried, and spent there is no way to sift, stop, or cleanly redo jobs, and every fix is a guess. Each ingest is minutes of work, so if it holds the store while it runs, status checks, questions, and new ingests all stall — and one hung job wedges the whole service. And every LLM call site is a quality-and-cost choice (which model, thinking level, generation settings) made blind today, with no way to measure a change before shipping it or to tune one call site without moving them all.

## Purpose

The wiki is ikigenba's knowledge base: an MCP service that *compiles* ingested text into durable per-subject pages and answers questions from them. Its primary surface is **MCP** (agents work through tools, not screens), but it now also serves one small **human web page** — a landing page at its mount root showing the service name and version — so a logged-in person who opens `…/srv/wiki/` in a browser sees a styled page, not a raw error. That page is the seed of wiki's own web surface; v1 is deliberately just name+version. Each ingest breaks text into the **subjects** it concerns and the **claims** it makes, then writes or updates each subject's page, so knowledge **compounds** — the LLM does the upkeep humans abandon; the user curates what goes in and asks. The owner can **merge** two names for the same thing into one page. Around that core the wiki records **every LLM call** and lets the user steer the ingest queue (list, abort, re-run); reads are always served and never wait on the slow, strictly-serial ingest work. Every call site is configured **independently**, and the developer can take a call site **off-line and evaluate** it against a human-blessed gold dataset to choose what runs in production on evidence rather than guesswork.

## Users

- **The owner, through an agent with the wiki's MCP.** Ingests topics mid-conversation, later asks questions answered from everything ingested, and merges two names for the same thing on spotting a duplicate. Never edits pages by hand.
- **The developer/operator, on the same MCP surface.** Sees inside — subjects, claims, pages, job state, and every LLM call's exact request and response — and acts: aborts a wedged job, re-runs one after fixing code or a prompt **off-line** (applied by restarting the service). First-class, not an afterthought.
- **The same developer, choosing each call site's model — off-line.** Runs a call site (`extract` today) against a gold dataset under a chosen model/thinking/config, reads a completeness-and-accuracy scorecard, and configures each call site independently on that evidence.

## Scope

The wiki does this and only this:

- **Ingest text** — accept text, return a job handle immediately, integrate in the background. Each ingest infers the **subjects** and **claims**; raw text and claims are kept **permanently**.
- **Maintain per-subject pages** — write or update one coherent, deliberately **lossy** page per subject from its claims; a later mention updates the page rather than creating a duplicate.
- **Merge two subjects** — the owner names the **survivor** and the one to **fold in**; claims move to the survivor, its page is rewritten, the folded subject and page are removed, and the folded name thereafter **routes to the survivor** for ingest, `ask`, links, and direct `type/slug` lookup. Background (returns a handle), owner-approved only, no claim lost, **irreversible**. The owner can list merges performed, newest-first, paged.
- **Track and control jobs** — report each job as `pending`/`working`/`done`/`failed`/`aborted`; list newest-first filtered by **any combination of states** and/or a **time window**, paged; **count** matches without retrieving them; **abort** a pending/working job (nothing partial committed) into a distinct `aborted` state; **re-run** a terminal job from its original text, **replacing** earlier claims. Filterable states are discoverable; an invalid state is **refused with a clear error** naming the valid set; re-running a pending/working job is refused.
- **Record and inspect every LLM call** — for extract, compile, ask, and the **embedding** calls that power search, store the exact request, exact response, parameters actually used (provider, model, generation or dimension settings), stage/job/attempt, error flag, and token usage; each retry is its own record; kept **permanently** and **accumulating across re-runs**; listable, filterable by job/stage/time, paged. ask's calls and query-embedding calls are recorded unattached to a job; page-embedding calls attach to their ingest job.
- **See ingest progress** — by handle, report a job's state and, when finished, the subjects it produced.
- **Ask** — find the wiki pages relevant to the question and synthesize a **cited** answer from **wiki content only** (the compiled pages, never raw claims or source text); best-effort and partial; when the wiki holds nothing on the question, answer that it holds nothing.
- **Inspect** — list subjects (filter by type and name, paged), view a subject's claims (paged), view its page.
- **Link pages** — each page shows the subjects it points to and those that point to it, both ways; a link exists when a subject's name — or a name **merged into it** — appears on another page, matched by exact normalized name (nothing fuzzy), shown by the survivor's name.
- **Readable names** — every subject is named by its `type/slug` path (e.g. `entity/acme-corp`), accepted wherever a subject is requested; internal ids are never shown.
- **Page large results** — jobs, subjects, claims, and LLM-call records return a bounded page plus a cursor; any filter narrows the set **before** paging.
- **Report liveness** — standard health, and a reflection that is **empty** (no event plane in this release).
- **Serve a landing page** — at the mount root, present a small styled web page showing the service **name** and **version** to a person who opens it in a browser. It is for **logged-in humans** (gated by the dashboard browser session, like any web page in the suite — any signed-in user may view it); agents continue to use MCP. v1 shows only name+version and reads nothing about the viewer; the page exists to be the foundation wiki's web surface grows from.
- **Stay responsive** — serve every read without waiting on an in-progress ingest, accept a new ingest immediately, integrate ingests **one at a time** (each visible all-at-once on completion), and let no slow or wedged job hold anything hostage (it can still be aborted).
- **Configure each call site independently** — `extract`, `compile`, and `ask` each get their own model, thinking level, and generation settings, with no two forced to match and no global; the **embedding** call site gets its own model and dimensions (one embedding model serves both page and query embedding, since the two must share a vector space); the configuration is inspectable. (Evaluation adds one eval-only call site, the **judge**, configured the same way.)
- **Evaluate `extract` off-line against a gold dataset** — outside the live service, run the **real production extraction logic** over curated cases (each a document paired with the **human-blessed** subjects and claims and a **difficulty** label), varying model/thinking/config; the **production prompt** runs by default, with an opt-in **alternative instruction prompt** to compare. General in intent, but only **extract** is evaluable this release; compile and ask wear the call-site shape but aren't wired to a dataset.
- **Score extraction completeness and accuracy** — per case and aggregated: which expected **subjects** were found/missed/added (by normalized type-and-name) and, for matched subjects, which **claims** were covered/missed/added (covered = matched **by meaning** by a **pinned strong judge** held fixed across runs). Report the **actual items and claim text**, not just counts, laid out like a diff so the developer judges over- vs under-extraction; stamp every run with the exact model, thinking level, config, and judge. **Scores only** — no pass/fail, no gate.

It deliberately does **nothing else** — in particular it does not: process more than one ingest at a time; touch the event plane (reflection is empty); **detect or propose** duplicates, or **merge or link** subjects by fuzzy/semantic name matching (merge, links, and direct `type/slug` lookup stay exact — an un-merged variant like "Gygax" for "Gary Gygax" resolves to nothing for those); **un-merge**; run background lint/dedup/contradiction/staleness maintenance; answer `ask` from raw claims or source text (it answers only from the compiled pages); structure pages differently per kind; draw a link across differently-named subjects by guessing; prune, cap, or roll up cost over the recorded LLM footprint; allow interactive editing of a job, prompt, or page (changes are off-line, applied by restart); ingest anything but text; let `ask` write anything back; or, for evaluation, score any call site but `extract`, A/B-test prompts beyond `extract`, gate or render a verdict, auto-trust un-blessed gold, or run through the live MCP service.

## Contractual constants

Promised values the design must honor verbatim and never re-declare:

- **Page size cap: 12,000 characters.** No subject page exceeds it — the cap forces a page to stay a compressed summary rather than an append-log.
- **Subject types are the closed set `entity`, `event`, `concept`.** Every subject carries exactly one.
- **A subject's public name is its `type/slug` path** (e.g. `entity/acme-corp`) — the only subject identifier ever shown or accepted; internal ids are never exposed.
- **The job lifecycle is the closed set `pending`, `working`, `done`, `failed`, `aborted`.** Every job is in exactly one; `aborted` (a deliberate cancellation) is distinct from `failed`; `done`/`failed`/`aborted` are terminal.
- **`ask` is strictly read-only — permanently.** It never creates or modifies wiki content (recording its call is bookkeeping, not a change).
- **The production LLM call sites are `extract`, `compile`, `ask`, and `embed`** — each named, each configured independently (the single `embed` model serves both page and query embedding, which must share a vector space). Evaluation adds the **judge**, which exists only inside an evaluation run.
- **Subjects match by normalized `type`-and-`name`; claims match by meaning** (the pinned judge's ruling) — these define found/missed/added in the evaluation.

## What we promise (user-facing behavior)

- **Ingest is fire-and-return, and the wiki stays responsive.** You get a handle immediately; throughout an ingest's multi-minute run every read returns promptly and new ingests are accepted. A stuck job holds nothing hostage — reads and submissions keep working and you can still abort it.
- **Ingests never corrupt each other.** Integrated one at a time, each completing before the next; a finished ingest's pages and claims appear **all together**, never half-applied.
- **Knowledge compounds.** Each subject gets one evolving page; feeding more text about it updates that page instead of piling up duplicates.
- **You can merge, but not un-merge.** Name which of two duplicate subjects survives and which folds in; afterward asking either name, ingesting under the old name, following a link to it, or looking it up by its old path all land on the one surviving page, with no claim lost. Merging is **irreversible** this release. You can list the merges you've made.
- **Jobs are siftable and steerable.** Watch one by handle; list newest-first filtered by any combination of states and/or a time window, paged; get a **count** cheaply; abort a queued or running job; re-run a finished/failed/aborted job as a clean do-over. A bad filter state fails with a clear error; re-running an unfinished job is refused.
- **Failures are legible.** A too-large or otherwise unusable ingest fails with a **clear, recorded reason** (not a cryptic parse error) and leaves nothing half-built.
- **Every LLM call is on the record** — extract, compile, ask, and the embeddings that power search — request, response, parameters, error, tokens, every retry — kept forever and accumulating across re-runs, so you can compare a fixed prompt against the broken one.
- **The raw record is permanent;** pages are lossy, the source is not.
- **You can look inside and navigate.** List subjects, view any subject's claims and page, and follow a page's links (what it points to and what points to it) to related subjects — everything by readable `type/slug` name, never an internal id, a page at a time.
- **Ask works like an agent and is honest.** It finds the pages relevant to your question — by meaning and wording, not just exact names — and answers from them, cited by readable name; answers from those it has even if a related subject was never ingested; says "nothing here" plainly when the wiki holds nothing on the question; never fabricates from general knowledge; never changes anything.
- **Opening the wiki in a browser shows a page, not an error.** A logged-in dashboard user who navigates to the wiki's mount root gets a small, on-brand page naming the service and its version; someone without a valid session is turned away. Agents are unaffected — they keep working through MCP.
- **Each call site runs its own model,** independently configured and inspectable — no single global model; the embedding model is configured the same way.
- **You can measure a call site before trusting it.** Off-line, point `extract` at a gold dataset under a model/thinking/config (and optionally an alternative prompt) and read, per case and in aggregate, which subjects it found/missed/invented and which claims it covered/missed/added — actual items laid out like a diff, claim equivalence judged **by meaning** by a fixed strong judge. Every run is stamped with its model, thinking, config, and judge and traces back to the exact request and response; the gold is human-blessed, so a low score means the model fell short. Scores only — it gates nothing and mutates nothing.

## Success criteria (outcomes)

Each is a result the user can confirm against the running service:

- I ingest text and get a handle back without waiting for processing.
- During an ingest's multi-minute phase, job-status, jobs/subjects listing, claims, pages, LLM-call records, and `ask` all return promptly, and I can submit another ingest immediately.
- A hung or long job blocks no read and no submission, stays abortable, and the service stays responsive throughout.
- A completed ingest's subjects and pages become visible all at once — never partially applied.
- I watch a job go pending/working → done (or failed/aborted) by handle, and list jobs newest-first filtered by any combination of states and/or a time window, paged; one query naming several states (e.g. all but `done`) returns them together.
- I get the **count** of jobs matching a state/time filter without retrieving them, correct across many jobs.
- Filtering by an invalid state yields a clear error naming the valid five; the valid states are discoverable from the jobs surface.
- I abort a pending or working job; it leaves nothing partial and shows as `aborted`, distinct from `failed`.
- I re-run a finished/failed/aborted job; it reprocesses from original text and **replaces** that attempt's claims; re-running a pending/working job is refused with a clear reason.
- Text too large to digest in one pass fails with a clear reason recorded on the job and in the LLM-call record, leaving no partial subjects, claims, or pages.
- For any LLM call — extract, compile, ask, and the page/query embeddings, each retry — I see the exact request, response, parameters (provider, model, and generation or dimension settings), error flag, and tokens.
- LLM-call records survive re-runs: the earlier failed attempt's request/response remain alongside the new one's.
- I page through jobs, subjects, a subject's claims, and LLM-call records via cursor, with any filter applied before paging.
- After an ingest I list the subjects it produced, and for any subject view its claims and its page, referring to it by readable `type/slug` name.
- Every subject shown — listing, page, link, citation — is named by `type/slug`; I never see an internal id.
- A page shows the subjects it points to and those that point to it, and I can follow any to that subject's page; a link exists exactly when that subject's name — or a name merged into it — appears on the page, never a merely-variant name.
- Ingesting more text about the same subject (same name) updates its page, not a duplicate.
- I merge two subjects by naming survivor and folded; afterward one page holds both subjects' claims, the folded subject and page are gone, and its claims persist relabelled on the survivor.
- After a merge, asking the folded name, ingesting under it, following a link to it, and looking it up by its old `type/slug` path all return the surviving subject — identical to the survivor's own path, not a not-found.
- A merge returns a handle promptly and completes in the background; nothing merges unless I request it.
- I cannot un-merge in this release.
- I list merges performed, newest-first, paged — what folded into what, and when.
- Original raw text and extracted claims remain retrievable after the page is built.
- I ask a question about a subject the wiki has — without needing to name it exactly, even using different words than the page uses — and get a cited answer synthesized from the relevant page(s), drawn only from ingested content.
- I ask a question spanning several subjects; the wiki answers from those it has and does not fail because one was never ingested.
- Asking something the wiki holds nothing on returns an explicit "nothing here," not a fabricated answer.
- Nothing I do through `ask` changes any subject, claim, or page.
- No page exceeds 12,000 characters.
- Health reports the service up; reflection reports no published or subscribed events.
- As a logged-in dashboard user I open the wiki's mount root in a browser and see a styled page showing the service name and its version; without a valid session the page is refused, and the agent-facing MCP surface is unchanged.
- I configure `extract`, `compile`, `ask`, and the `embed` model independently, and confirm which model runs behind each.
- I run `extract` off-line against a gold dataset under a model and thinking level I choose, using the production extraction logic and (by default) the production prompt.
- I optionally hand `extract` an alternative instruction prompt (the production prompt runs when none is supplied; the live service is unaffected), and the run is stamped with which prompt produced it.
- Per case I get which subjects were found/missed/invented and which claims were covered/missed/added — actual subject identities and claim text, judged equivalent by meaning — plus the same aggregated across the dataset.
- I run the dataset under a second model or thinking level and compare directly; each run reports the exact model, thinking, config, and judge behind it.
- The claim judge is a fixed strong model that does not change when the model under test does.
- A low score reflects the model falling short of human-blessed gold, and any score traces back to the exact request and response.
- The run produces scores only — no pass/fail verdict, and it changes nothing in the wiki.
- The repository contains complete, blessed evaluation cases (document + expected subjects/claims + difficulty) spanning at least one **easy** and one **hard** case that the `extract` evaluation runs against.
