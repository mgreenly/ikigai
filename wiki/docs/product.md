# wiki — Product

**Authority: intent.** This document owns *why* the wiki exists, *for whom*, what is in and out of scope, and what we **promise** the user — stated once, in outcome terms. It does **not** state mechanism, data formats, exit codes, schema, or test assertions; those belong to `docs/design.md`. Where the two could overlap (observable behavior), this doc states the *promise* and design states the *exact, checkable proof of that promise*. That boundary is load-bearing: it keeps product, design, and plan from overlapping.

The wiki began as a deliberately **thin proving slice** — confirm the full ingest → knowledge → query spine works end-to-end on the cheapest substrate that still exercises it — and that spine now stands. This document describes the **current product**: that knowledge spine, a **runtime-footprint and job-control layer** added to make the LLM-backed ingest *troubleshootable* as we iterate on it, and a **non-blocking availability and write-consistency contract** that keeps the service responsive while that slow, LLM-backed ingest runs. Scope below covers all three. The "deliberately not" statements bound what the product does not yet do, not what it will never do; the items deferred from the proving slice (aliases, lint, semantic retrieval) remain deferred.

## Problem

When you work through an agent on a topic — researching, reasoning, accumulating sources and conclusions over many conversations — the knowledge evaporates when the session ends. The next conversation starts cold. The usual remedy, retrieval over raw document chunks, re-derives understanding from scratch on every question: nothing accumulates, cross-references are never drawn, contradictions are never reconciled. The alternative — a hand-maintained personal wiki — collapses under its own upkeep, because the bookkeeping of keeping pages current grows faster than the value and humans abandon it. The user is left with no durable, compounding, inspectable store of what they've learned.

Running that store surfaces a second pain. The ingest is backed by an LLM, and LLMs fail in ways you cannot see — most acutely, they fail to return the structured content the ingest depends on. When a job fails, the operator is left guessing: what exactly was the model asked, what did it actually return, how many times did the retry loop fire, how many tokens did it burn? And as ingests pile up there is no efficient way to sift them — to find what is stuck, what errored, what ran in the last hour — nor to stop a job that is wedged or runaway, nor to redo one cleanly after fixing the prompt that broke it. Without a durable, inspectable runtime footprint and basic control over the work queue, every fix is a shot in the dark.

A third pain is about staying out of the user's way. Each ingest is *minutes* of LLM work, and if that work is allowed to hold the underlying store while it runs, everything else grinds to a halt: a simple status check, a question, even submitting the next ingest all end up waiting on a job that won't finish for minutes — and a single hung job takes the whole service down with it. A knowledge base that goes unresponsive every time it is fed, and that one stuck job can wedge, is one the user stops trusting and stops feeding.

## Purpose

The wiki is ikigenba's knowledge base: an MCP service that *compiles* ingested text into a persistent set of per-subject pages and answers questions from that compiled knowledge. Its single job is to turn raw text the user feeds it into durable, queryable knowledge that **compounds** — each ingest reads the text, breaks it into the subjects it concerns and the claims it makes, and updates the page for each subject so the wiki reflects everything ingested so far. The LLM does the maintenance bookkeeping that humans abandon; the user curates what goes in and asks questions. Around that core, the wiki keeps a durable record of **every LLM call it makes** and gives the user direct control over the ingest queue — list, abort, and re-run jobs — so the LLM-backed machinery is inspectable and steerable rather than opaque. Throughout, the service stays out of its own way: reads are always served and never wait on the slow ingest work, while ingests are processed strictly one at a time so they never corrupt each other — a slow or hung job holds nothing hostage.

## Users

- **The owner, working through an agent** that has the wiki's MCP loaded. Mid-conversation they ask their agent to ingest a topic they've been discussing; later, in any session, they ask the wiki questions and get answers synthesized from everything they've ingested. They never edit pages by hand — the wiki writes and maintains them.
- **The developer/operator, working through the same MCP surface**, who needs to *see inside* the wiki and *steer* it — the subjects extracted, the raw claims, the generated pages, the state of every ingest, and the exact request and response of every LLM call — and then act on what they find: abort a wedged or runaway job, and re-run a job after fixing the code or prompt that broke it (a change made off-line and applied by **restarting the service**, never an interactive edit). This inspection-and-control need is first-class, not an afterthought. These capabilities live on the MCP surface today; a later release may move part of them behind a debug switch, out of the typical user's surface.

## Scope

The wiki does this and only this:

- **Ingest text.** Accept a block of text and return promptly with a job handle; the work of integrating it happens in the background. Each ingest breaks the text, by inference, into the **subjects** it concerns and the **claims** it makes about them. The original raw text and the extracted claims are kept **permanently**.
- **Maintain per-subject pages.** For each subject, write or update a single coherent page that reflects the claims about it. A subject mentioned by a later ingest updates that subject's existing page rather than creating a duplicate. Pages are deliberately **lossy** compressions of the claims — the raw text and claims remain the durable record.
- **Track and control ingest jobs.** Report which ingests are `pending`, `working`, `done`, `failed`, or `aborted`. List them filtered by **state** and/or by **time window** (the two composing) and page through the result. **Abort** a job that is still pending or already working — a pending job never runs, a working job's in-flight LLM call is interrupted with nothing partial committed — landing it in a deliberate `aborted` state distinct from a failure. **Re-run** a job that has reached a terminal state (`done`, `failed`, or `aborted`) to reprocess it from its original text, **replacing** the claims of the earlier attempt; re-running a job that is still pending or working is refused.
- **Record and inspect every LLM call.** Every call the wiki makes to an LLM — during extract, during compile, and during ask — is recorded with the **exact request sent**, the **exact response received**, the **parameters it actually ran under** (provider, model, and the generation settings such as temperature and reasoning), which stage and (where applicable) which job and attempt it belongs to, whether it errored, and the **token usage** it consumed. Each bounded re-prompt is its own record. These records are kept **permanently** and **accumulate across re-runs**; they can be listed, filtered by job, stage, and time window, and paged through. ask's calls are recorded too, unattached to any job.
- **See ingest progress.** Using a job handle, report whether that one ingest is pending, working, done, failed, or aborted, and (when finished) the subjects it produced.
- **Ask.** Answer a natural-language question **agentically**: extract the subjects the question names, resolve each to a wiki subject by **exact normalized name**, read the pages of the subjects that resolve, and synthesize a cited answer from those pages — drawing only on ingested wiki content. When the question names several subjects, it gathers every one that resolves and uses each (**best-effort partial**); when none of the named subjects resolve, it answers that the wiki holds nothing on the question.
- **Inspect.** List the subjects (filtered by type and name, **paged**), view a subject's raw claims (**paged**), and view a subject's page.
- **Link pages together.** Each page surfaces the other subjects it points to and the subjects that point to it, so you can move from one subject to a related one without searching again. A link exists when a subject's name appears on another subject's page, matched by **exact normalized name** (the same match `ask` uses) — nothing fuzzy. Links go **both ways**: a page shows what it mentions and what mentions it.
- **Refer to subjects by readable name.** Wherever the wiki names a subject — in a listing, a citation, a link, or a lookup — it uses a human-readable `type/slug` path (for example `entity/acme-corp`), and accepts that same path when you ask for a subject. The wiki never shows the user an internal id.
- **Page large results.** Every listing that can grow without bound — jobs, subjects, a subject's claims, and the LLM-call records — returns a **bounded page** plus a **cursor** for fetching the next, so a caller pages through a large set instead of pulling it whole. Any filter narrows the set **before** paging.
- **Report liveness.** Standard health, and a reflection that is **empty** (the wiki is connected to no event plane in this release).
- **Stay responsive while it works.** Serve every read — job and subject inspection, claims, pages, LLM-call records, and `ask` — without ever waiting on an in-progress ingest, and accept a new ingest submission immediately even while another is processing. Ingests are integrated **one at a time**, each completing before the next begins, so concurrent work never overwrites or interleaves; a completed ingest's pages and claims become visible **all at once**; and a slow or wedged job never makes the rest of the service wait on it (it can still be aborted).

The wiki deliberately does **nothing else**. In particular it does not:

- process more than one ingest at a time — ingests are integrated strictly one after another, never in parallel; the queue drains serially by design;

- connect to the event plane or publish/consume any events (so reflection is empty);
- reconcile different names for the same thing — subjects are matched only by exact (normalized) name, so two names for one thing become two separate pages until the **alias machinery** (the planned early follow-on) reconciles them. `ask` resolves a question's named subjects by that **same** exact-normalized match, so a partial or variant name ("Gygax" for "Gary Gygax") resolves to nothing until aliases / fuzzy matching arrive in a later release;
- run any background maintenance, deduplication, contradiction-flagging, or staleness repair (no **lint machinery**);
- find an answer by searching the **words inside pages**, or by meaning — `ask` finds pages **only** by extracting and resolving the subjects the question names. If the answer lives in a page whose subject the question does not name (asking "who created Dungeons & Dragons?" when there is no "Dungeons & Dragons" subject, only a Gygax page that says so), `ask` will not find it and answers that the wiki holds nothing. Keyword and semantic / vector retrieval over page bodies are **later releases**;
- consult a subject's **raw claims or original source text** when answering — `ask` synthesizes from the compiled **pages** only; drilling into the raw record to recover what a page's compression lost is a later release;
- structure pages differently by kind — every page uses one generic shape, though each subject does carry its **type**;
- reconcile a link across differently-named subjects — links and `type/slug` lookups resolve by the **same** exact-normalized name match, so a page that refers to a subject by a variant or partial name draws no link until aliases / fuzzy matching arrive in a later release;
- **prune, expire, or cap** the recorded LLM-call footprint — it is kept whole, forever; trimming it is a later release;
- **aggregate or report cost over a time period** — per-call token usage is recorded, but rolling it up into spend-over-a-window is a later release;
- let you **edit a job, a prompt, or a page interactively** — re-running a job picks up code or prompt changes made **off-line** and applied by restarting the service; there is no in-place editing;
- ingest anything but text (no URL or file ingest);
- let `ask` write anything back — to persist an answer, the user ingests that answer's text through the normal front door.

## Contractual constants

These are promised values the design must honor verbatim and never re-declare:

- **Page size cap: 12,000 characters.** No subject page exceeds this; the cap is what forces a page to stay a compressed summary rather than grow into an append-log.
- **Subject types are a closed set: `entity`, `event`, `concept`.** Every subject carries exactly one. (The finer per-kind subtype is not a structuring contract.)
- **A subject's public name is its `type/slug` path.** The user-facing identifier for every subject is its type plus a slug of its normalized name (for example `entity/acme-corp`). This path is the only subject identifier the wiki ever shows or accepts; internal ids are never exposed.
- **The ingest job lifecycle is a closed set of five states: `pending`, `working`, `done`, `failed`, `aborted`.** Every job is in exactly one. These are the names the user filters and reads jobs by. `aborted` (a deliberate cancellation) is distinct from `failed` (something went wrong); both are terminal, as is `done`.
- **`ask` is strictly read-only — permanently.** It never creates or modifies any wiki content under any circumstance. (Recording an ask call's request/response is bookkeeping about the call, not a change to wiki content.)

## What we promise (user-facing behavior)

- **Ingest is fire-and-return.** You hand the wiki text and immediately get a handle back; you are never blocked waiting for the text to be processed.
- **The wiki stays responsive while it ingests.** While an ingest is in its multi-minute processing phase, every read — checking a job's status, listing jobs or subjects, viewing claims or a page, listing LLM-call records, and `ask` — still returns promptly, and you can submit another ingest without waiting. Nothing you do has to sit behind a job that hasn't finished.
- **A stuck job never takes the wiki down.** A job that hangs or runs long holds nothing hostage: reads keep working, new submissions keep being accepted, and you can still `abort` the stuck job. The service never wedges because one ingest misbehaved.
- **Ingests never corrupt each other.** The wiki integrates one ingest at a time, each finishing before the next starts, so two jobs never write over one another. A completed ingest's pages and claims appear **all together** — you never observe a half-applied job, only the prior state or the fully-updated one.
- **Ingest progress is visible and siftable.** Using a handle you can watch one ingest move through `pending → working → done` (or to `failed`/`aborted`). You can also list your ingests filtered by **state** and/or **time window**, a page at a time — to find what's queued, what's running, what errored, what you aborted, or what ran in a given window.
- **You can stop a job.** Abort an ingest that is still queued or already running; a running job's in-flight LLM call is cut off and nothing half-done is left behind, and the job is marked `aborted` — plainly distinct from one that `failed`.
- **You can redo a job.** After fixing the code or prompt that broke an ingest and restarting the service, re-run that job; it reprocesses from its **original text** and the earlier attempt's claims are **replaced**, so a retry is a clean do-over, not a pile-up. Re-running a job that hasn't finished (still pending or working) is refused with a clear reason.
- **A failed ingest tells you why, plainly.** When a piece of text is too large for the model to digest in one pass — or the model otherwise can't return usable structured content — the job fails with a **clear, recorded reason** rather than a cryptic parse error, and leaves **nothing half-built** behind. The reason is on the job and in the LLM-call record, so you can see what actually happened and decide what to do.
- **Every LLM call is on the record.** For any call the wiki made to an LLM — in extract, compile, or ask, **including every retry** — you can see the exact **request**, the exact **response**, the **parameters it ran under** (provider, model, temperature, reasoning), whether it errored, and the **tokens** it used. The records are kept forever and **accumulate across re-runs**, so you can compare what a fixed prompt — or a corrected parameter — returns against what the broken one did.
- **Knowledge compounds per subject.** Once an ingest completes, each subject the text concerned has a page reflecting its claims. Feed the wiki a second piece of text about the same subject and that subject's page is updated — you get one evolving page per subject, not a pile of duplicates.
- **The raw record is permanent and inspectable.** The original text and the claims extracted from it remain retrievable after the page is built — pages are lossy, the source is not.
- **You can look inside.** You can list the subjects the wiki knows (filtered, a page at a time), and for any subject view its raw claims (a page at a time) and its current page. Everywhere a subject appears, it appears by its readable `type/slug` name, and you can ask for any subject by that same name.
- **Large lists come a page at a time.** Listing jobs, subjects, a subject's claims, or the LLM-call records hands back a bounded page and a cursor for the next; you page through a **filtered** set rather than pulling everything at once.
- **Pages are navigable.** When you read a page, it tells you which subjects it points to and which subjects point to it, each by its readable name. You can follow any of those names straight to that subject's page — no separate search.
- **You never see internal ids.** Every subject the wiki hands back is named by its `type/slug` path; the opaque internal id is never shown.
- **Ask works like an agent.** Ask a question and the wiki identifies the subjects your question names, reads the pages for the ones it actually has, and gives back an answer synthesized from those pages, with citations to them by their readable name. If your question names several subjects, it answers from every one it has a page for — it does not fail just because one named subject was never ingested.
- **Ask is grounded and honest.** The answer draws **only** on the pages it read. If none of the subjects your question names are in the wiki, it says so plainly — it never fabricates from the model's general knowledge, and a "nothing here" means no subject you named has a page, not that a word-match missed.
- **Ask never changes anything.** Asking is purely a read; to keep an answer, you ingest it.
- **Standard liveness.** Health reports the service is up; reflection reports that the wiki publishes and subscribes to nothing.

## Success criteria (outcomes)

Each item is a result the user can confirm against the running service:

- I can ingest a block of text and get a job handle back without waiting for the text to be processed.
- While an ingest is actively processing (its multi-minute LLM phase), I can check job status, list jobs and subjects, view claims and pages, list LLM-call records, and `ask` — and each returns promptly rather than waiting for the ingest to finish.
- While an ingest is processing, I can submit another ingest and get a job handle back immediately.
- A job that hangs or runs long does not make any read or any new submission wait on it, and I can still abort it; the service stays responsive throughout.
- When an ingest completes, all of its subjects and pages become visible together — I never observe a partially-applied job, only the state before it ran or the fully-updated state after.
- I can watch that ingest go from pending/working to done, and I can list my ingest jobs filtered by state (`pending`/`working`/`done`/`failed`/`aborted`) and/or by time window, paging through the result a page at a time.
- I can abort an ingest that is still pending or already working; it stops, leaves nothing partially committed, and shows up as `aborted` — distinct from `failed`.
- I can re-run a job that finished, failed, or was aborted; it reprocesses from its original text and **replaces** that attempt's claims rather than duplicating them. Trying to re-run a job that is still pending or working is refused with a clear reason.
- When I ingest text too large for the model to digest in one pass, the job fails with a **clear reason recorded on it** (not a cryptic parse error), leaves no partial subjects, claims, or pages behind, and that reason is visible in the job and in the LLM-call record.
- For any LLM call the wiki made — in extract, compile, or ask, including each retry — I can see the exact request, the exact response, the parameters it ran under (provider, model, temperature, reasoning), whether it errored, and the tokens it used.
- Those LLM-call records survive across re-runs: after I fix a prompt and re-run a job, the failed earlier attempt's request and response are still there alongside the new attempt's.
- I can list jobs, subjects, a subject's claims, and the LLM-call records a bounded page at a time, passing a cursor to fetch the next page, with any filter applied before paging.
- After an ingest completes, I can list the subjects extracted from that text.
- For any of those subjects, I can view both its raw claims and its generated page, and I refer to the subject by its readable `type/slug` name rather than an internal id.
- Every subject the wiki shows me — in a listing, a page, a link, or a citation — is named by its readable `type/slug` path, and I never see an internal id.
- When I read a page, I can see which subjects it points to and which subjects point to it, and I can follow any of those names to that subject's page.
- A page lists another subject as a link exactly when that subject's name appears on it; a subject the page does not name by its exact (normalized) name produces no link.
- Ingesting a second piece of text that mentions the same subject (by the same name) updates that subject's existing page instead of creating a duplicate.
- The original raw text and its extracted claims are still retrievable after the page has been built.
- I can ask a question that names a subject the wiki has a page for, and get a cited answer synthesized from that page — drawn only from what I've ingested, cited by readable name.
- I can ask a question that names several subjects; the wiki answers from every one it has a page for, and does not fail just because one of the named subjects was never ingested.
- Asking a question whose named subjects are *none* of them in the wiki returns an explicit "nothing here," not a made-up answer.
- Nothing I do through `ask` changes any subject, claim, or page.
- No page exceeds 12,000 characters.
- Health reports the service is up; reflection reports no published or subscribed events.
