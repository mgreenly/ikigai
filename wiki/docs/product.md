# wiki — Product

**Authority: intent.** This document owns *why* the wiki exists, *for whom*, what is in and out of scope for this release, and what we **promise** the user — stated once, in outcome terms. It does **not** state mechanism, data formats, exit codes, schema, or test assertions; those belong to `docs/design.md`. Where the two could overlap (observable behavior), this doc states the *promise* and design states the *exact, checkable proof of that promise*. That boundary is load-bearing: it keeps product, design, and plan from overlapping.

This is the product for **release 1 (phase 1)** of the wiki. Phase 1 is deliberately a **thin, short-lived proving slice**: its job is to confirm the full ingest → knowledge → query spine works end-to-end on the cheapest substrate that still exercises it, so later releases can build outward. Scope below is written for phase 1; the "deliberately not" statements are bounded to this release, not forever.

## Problem

When you work through an agent on a topic — researching, reasoning, accumulating sources and conclusions over many conversations — the knowledge evaporates when the session ends. The next conversation starts cold. The usual remedy, retrieval over raw document chunks, re-derives understanding from scratch on every question: nothing accumulates, cross-references are never drawn, contradictions are never reconciled. The alternative — a hand-maintained personal wiki — collapses under its own upkeep, because the bookkeeping of keeping pages current grows faster than the value and humans abandon it. The user is left with no durable, compounding, inspectable store of what they've learned.

## Purpose

The wiki is ikigenba's knowledge base: an MCP service that *compiles* ingested text into a persistent set of per-subject pages and answers questions from that compiled knowledge. Its single job is to turn raw text the user feeds it into durable, queryable knowledge that **compounds** — each ingest reads the text, breaks it into the subjects it concerns and the claims it makes, and updates the page for each subject so the wiki reflects everything ingested so far. The LLM does the maintenance bookkeeping that humans abandon; the user curates what goes in and asks questions.

## Users

- **The owner, working through an agent** that has the wiki's MCP loaded. Mid-conversation they ask their agent to ingest a topic they've been discussing; later, in any session, they ask the wiki questions and get answers synthesized from everything they've ingested. They never edit pages by hand — the wiki writes and maintains them.
- **The developer/operator** bringing the service up, who needs to *see inside* the wiki — the subjects extracted, the raw claims, the generated pages, the state of in-flight ingests — to confirm phase 1 is actually working. In phase 1 this inspection need is first-class, not an afterthought.

## Scope

Phase 1 does this and only this:

- **Ingest text.** Accept a block of text and return promptly with a job handle; the work of integrating it happens in the background. Each ingest breaks the text, by inference, into the **subjects** it concerns and the **claims** it makes about them. The original raw text and the extracted claims are kept **permanently**.
- **Maintain per-subject pages.** For each subject, write or update a single coherent page that reflects the claims about it. A subject mentioned by a later ingest updates that subject's existing page rather than creating a duplicate. Pages are deliberately **lossy** compressions of the claims — the raw text and claims remain the durable record.
- **See ingest progress.** Report which ingests are pending, working, or done.
- **Ask.** Answer a natural-language question with a cited answer synthesized from across the pages, drawing only on ingested wiki content. Phase 1 retrieval is **keyword search** (matching the words in the question against the words in the pages).
- **Inspect.** List the subjects (with filtering), view a subject's raw claims, and view a subject's page.
- **Report liveness.** Standard health, and a reflection that is **empty** (the wiki is connected to no event plane in phase 1).

Phase 1 deliberately does **nothing else**. In particular it does not:

- connect to the event plane or publish/consume any events (so reflection is empty);
- reconcile different names for the same thing — subjects are matched only by exact (normalized) name, so two names for one thing become two separate pages until the **alias machinery** (the planned early follow-on) reconciles them;
- run any background maintenance, deduplication, contradiction-flagging, or staleness repair (no **lint machinery**);
- retrieve by meaning — phase 1 search matches words, not semantics. Semantic / vector ("hybrid") search is a **planned fast follow-on**, so design must keep retrieval behind a seam: adding the vector lane later must not reshape the read path or the `ask` flow;
- structure pages differently by kind — every page uses one generic shape, though each subject does carry its **type**;
- draw links between pages;
- expose a `rebuild` verb (a thin follow-on will re-trigger exactly what ingest already does);
- ingest anything but text (no URL or file ingest);
- let `ask` write anything back — to persist an answer, the user ingests that answer's text through the normal front door.

## Contractual constants

These are promised values the design must honor verbatim and never re-declare:

- **Page size cap: 12,000 characters.** No subject page exceeds this; the cap is what forces a page to stay a compressed summary rather than grow into an append-log.
- **Subject types are a closed set: `entity`, `event`, `concept`.** Every subject carries exactly one. (The finer per-kind subtype is not a structuring contract in phase 1.)
- **`ask` is strictly read-only — permanently, not just in phase 1.** It never creates or modifies any wiki content under any circumstance.

## What we promise (user-facing behavior)

- **Ingest is fire-and-return.** You hand the wiki text and immediately get a handle back; you are never blocked waiting for the text to be processed.
- **Ingest progress is visible.** Using that handle (or a listing), you can see an ingest move from pending → working → done.
- **Knowledge compounds per subject.** Once an ingest completes, each subject the text concerned has a page reflecting its claims. Feed the wiki a second piece of text about the same subject and that subject's page is updated — you get one evolving page per subject, not a pile of duplicates.
- **The raw record is permanent and inspectable.** The original text and the claims extracted from it remain retrievable after the page is built — pages are lossy, the source is not.
- **You can look inside.** You can list the subjects the wiki knows (filtered), and for any subject view its raw claims and its current page.
- **Ask is grounded and honest.** Ask a question and get back an answer synthesized from across the wiki's pages, with citations to where it came from. The answer draws **only** on ingested content. If the wiki holds nothing on the topic, the answer says so plainly — it never fabricates from the model's general knowledge.
- **Ask never changes anything.** Asking is purely a read; to keep an answer, you ingest it.
- **Standard liveness.** Health reports the service is up; reflection reports that the wiki publishes and subscribes to nothing.

## Success criteria (outcomes)

Each item is a result the user can confirm against the running service:

- I can ingest a block of text and get a job handle back without waiting for the text to be processed.
- I can watch that ingest go from pending/working to done.
- After it completes, I can list the subjects extracted from that text.
- For any of those subjects, I can view both its raw claims and its generated page.
- Ingesting a second piece of text that mentions the same subject (by the same name) updates that subject's existing page instead of creating a duplicate.
- The original raw text and its extracted claims are still retrievable after the page has been built.
- I can ask a question and get a cited answer drawn only from what I've ingested.
- Asking about a topic the wiki has nothing on returns an explicit "nothing here," not a made-up answer.
- Nothing I do through `ask` changes any subject, claim, or page.
- No page exceeds 12,000 characters.
- Health reports the service is up; reflection reports no published or subscribed events.
