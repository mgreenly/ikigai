# Feature request — wiki page links

> **UNDER CONSIDERATION — DO NOT IMPLEMENT**
> This document is not a work order. It becomes actionable only when a human
> explicitly promotes it by creating `wiki-page-links-design.md`.

## Problem

The wiki's internal identity model (ULID subject IDs, SQLite rows) is opaque
to consumers. Agents reading pages via MCP see prose with no navigable
structure — they cannot discover related subjects without issuing a separate
search. The OKF standard surfaces the same gap: its file-path-as-identity model
makes cross-document links explicit and traversable; the wiki has no equivalent.

## Proposed approach

Keep the internal model (ULIDs, SQLite) unchanged. Make links explicit at
**render time**, not storage time. When a page is returned — via MCP or export
— append a footer with two sections: outbound subjects mentioned in this page,
and inbound subjects whose pages mention this one.

Links use human-readable `type/slug` paths (`entity/acme-corp`), not ULIDs.
The MCP surface accepts `type/slug` as a lookup key, translating to ULID at
the boundary. Callers never need to know ULIDs exist.

## Slug derivation

`type/slug` is derived from `subjects.type` + `subjects.canonical_name` using
the existing `normalize()` function (NFKC, lowercase, collapse whitespace,
strip diacritics) with spaces replaced by hyphens. Deterministic and
collision-free because `canonical_name` is unique per type in `subjects`.

## Footer format

```markdown
---
**Mentions:** [Acme Corp](entity/acme-corp) · [John Smith](entity/john-smith)
**Mentioned by:** [Deal 2024-Q3](event/deal-2024-q3)
```

Outbound before inbound. Both sections omitted if empty.

## Schema change required

One new table, populated inside the end-of-run commit:

```sql
CREATE TABLE page_links (
  from_subject TEXT NOT NULL,
  to_subject   TEXT NOT NULL,
  PRIMARY KEY (from_subject, to_subject)
);
CREATE INDEX page_links_to ON page_links (to_subject);
```

The primary key covers outbound lookups; the index covers inbound.

## Update mechanics

**On page write (end-of-run commit):** delete all rows `WHERE from_subject =
subject` then re-insert by scanning the new page body against the full alias
list. Same transaction as the page write and FTS5 sync.

**On merge:** delete `WHERE from_subject = loser` (page is gone). For rows
`WHERE to_subject = loser`: delete and let them rebuild naturally when those
pages are next compiled. The loser's aliases transfer to the winner, so
subsequent compiles resolve correctly. Accepted tradeoff: inbound links from
uncompiled pages are temporarily absent after a merge.

## MCP surface

`read_page` and `search` return the footer-augmented body. The `subject`
parameter on all read tools accepts either a ULID or a `type/slug` string; the
service resolves `type/slug` → ULID at the boundary via the aliases table.

## What this is not

This is not an adoption of OKF's storage model. Files on disk, file-path
identity, and directory bundles are rejected — they would break the end-of-run
transactional commit. OKF's `type/slug` path convention is borrowed only as a
human-readable identity surface for consumers.

## Origin

Emerged from a research session (2026-06-21) evaluating whether the Open
Knowledge Format standard should influence the wiki's design. OKF's core
contribution here is the observation that human-readable, path-like identifiers
improve agent navigability — not its file-on-disk storage model.
