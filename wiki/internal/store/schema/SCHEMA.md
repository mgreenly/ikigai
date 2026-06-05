# SCHEMA — the wiki's structure, conventions, and invariants

You are the wiki's ingest/maintenance agent. This file is your `CLAUDE.md` /
`AGENTS.md`: the single source of truth for how this knowledge base is laid out
and the rules you must never break. Read it first, every time, before you write a
single page.

This wiki is a **persistent, compounding knowledge base** in the spirit of
Andrej Karpathy's "LLM wiki": raw documents are *compiled once* into curated,
cross-linked markdown pages and kept current — not re-derived per query. You do
the bookkeeping the human abandons; the human curates sources and asks questions.

---

## 1. Layout

Everything lives under one collection root (the wiki is owner-scoped and
collection-keyed; the default collection is `default`):

```
index.md          the catalog — the navigation entry point (READ THIS FIRST)
log.md            append-only chronological operation log
raw/              immutable ingested originals — NEVER edit, rename, or delete
sources/          one page per ingested raw doc — the provenance anchor
concepts/         ideas / topics / methods
entities/         people / orgs / tools / products / places / works
events/           dated things that happened
synthesis/        compiled answers filed back in (the compounding artifact)
.search/          the search index (machine-owned — do not touch)
```

`raw/` is physically separate from the curated page tree so the never-modify-raw
invariant is structurally obvious. Each raw doc is stored as
`raw/<sha256>.md` — keyed by the content hash, with provenance frontmatter the
service stamps on (you never write into `raw/`).

## 2. The default type set

The taxonomy is **broad, not narrow**: a small set of wide types, with finer
distinctions captured in a freeform `kind:` field rather than by inventing new
directories. The service enforces no taxonomy — it is schema-driven, so new
types can appear without a code change, but prefer these five:

| type | what it is | examples of `kind:` |
|---|---|---|
| **source** | one page per ingested raw doc — the provenance anchor | `chat`, `url`, `pdf`, `note` |
| **concept** | an idea / topic / method ("ideas") | `method`, `theory`, `pattern` |
| **entity** | a person / org / tool / product / place / work ("identities") | `person`, `org`, `tool`, `place`, `work` |
| **event** | something that happened, dated ("events") | `release`, `incident`, `meeting` |
| **synthesis** | a compiled answer filed back in (compounding) | — |

Capture breadth through `kind:` metadata, not by proliferating types. Lint exists
to consolidate synonymous types/pages and keep the open vocabulary coherent.

## 3. Frontmatter conventions

Every curated page opens with a `---`-fenced YAML frontmatter block, then the
markdown body. Conventional keys:

```yaml
---
type: concept              # one of: source | concept | entity | event | synthesis
kind: method               # freeform sub-classification (breadth lives here)
title: Human Readable Title
source: <provenance>       # where this came from (a source page, a URL, a chat)
collection: default        # the collection this page belongs to
tags: ["a", "b"]           # optional
---
```

A **source** page additionally records the provenance the service stamped onto
the raw doc: its `sha256`, `ingested_at`, and the caller-supplied
`title`/`source`/`tags`. A source page is the bridge from an immutable `raw/`
doc to the curated knowledge pages derived from it — every knowledge page traces
back to at least one source.

## 4. Index-first navigation

`index.md` is the catalog and the entry point. **Always read `index.md` first**
to orient before reading or writing pages — it is how both you and the query side
find what already exists, so you update rather than duplicate. When an
integration pass creates or meaningfully changes pages, update `index.md` so the
catalog stays an accurate map of the wiki, and append a line to `log.md`.

## 5. The four invariants (hard rules — never violated)

1. **Provenance.** Every curated page traces back to a `source` (even when the
   source is a chat snippet). No page without a provenance trail.
2. **Immutable raw.** Originals in `raw/` are never edited, renamed, or deleted.
   Re-ingesting identical bytes is always a safe no-op. This is what makes
   unattended ingest safe.
3. **Flag, don't overwrite.** When new information contradicts an existing page,
   surface the contradiction (note it on the page / in the log) — do not silently
   clobber the prior claim. The human rules on meaning.
4. **Append, don't destroy.** Supersede rather than delete. Mark superseded
   knowledge, keep it, and link it from its successor. `log.md` is append-only.

## 6. The integration pass (what ingest does)

Given a freshly stored raw doc, you: read the raw doc → write or update its
`source` page → update the touched `concept` / `entity` / `event` pages (one
source can touch many pages) → update `index.md` → append to `log.md`. Search is
cheap precisely because you did this integration work up front: the query side
reads whole, already-integrated pages, never raw fragments.
