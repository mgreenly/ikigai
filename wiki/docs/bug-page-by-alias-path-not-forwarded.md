# Bug — `page` by an alias path returns `found:false` instead of the canonical page

**Status: informational, non-contractual.** Records an observed bug and the code
path behind it, for whoever authors the fix (design + plan). Nothing downstream
consumes this doc (the autonomous build reads only product, design, plan).
Rewrite in place as understanding evolves. Sibling to
`merge-orphan-links-research.md` (the alias/merge feature this bug sits inside)
and `subject-dedup-research.md`.

---

## 1. Symptom (reproduced live)

After merging `entity/a-mathematical-theory-of-communication` (loser) into
`entity/the-mathematical-theory-of-communication` (winner), the loser path no
longer serves a page:

```
page  entity/the-mathematical-theory-of-communication   → full canonical page (OK)
page  entity/a-mathematical-theory-of-communication      → {"found": false}
```

The merge itself succeeded: the loser subject row is deleted, its claims were
repointed to the winner, and a forward-routing alias was minted —

```
aliases: a-mathematical-theory-of-communication → <winner subject id>   (1 row)
```

So the alias exists and is correct; the `page` tool simply does not consult it.

## 2. Expected behavior

Per the alias decree recorded in `merge-orphan-links-research.md` §0:

> *Access through an alias must always appear, to every other system and to the
> user, as access via the canonical subject name.* … Anywhere a name resolves
> *through* an alias … the result is rendered/treated as the surviving canonical
> subject: its canonical name, its `type/slug` path, its page.

A `page` lookup on an alias path is exactly "access through an alias." It should
**forward to the canonical subject and serve the winner's page** (rendered under
the winner's canonical path/title), not return not-found. The whole point of an
alias is to be a durable redirect; a 404 on the old path defeats it.

## 3. Root cause

The page tool's path→subject resolution is alias-blind. The chain:

- `cmd/wiki/main.go:94` wires the tool via `mcp.WithPagePathService(pageService)`.
- `pathPageService.PageByPath` (`cmd/wiki/main.go:279`) resolves the path with
  **`s.subjects.GetByPath(ctx, path)`**, then calls `PageWithLinks(subject.ID)`.
- `SubjectStore.GetByPath` (`internal/wiki/data_model.go:598`) runs a single
  query:

  ```sql
  SELECT id, name, norm_name, type FROM subjects WHERE norm_name = ?
  ```

  — against the **`subjects` table only**. No fallback to `aliases`.

When the path is an alias, that subject row is gone (the merge deleted it), so
`GetByPath` returns `ErrSubjectNotFound` → mapped to `sql.ErrNoRows`
(`cmd/wiki/main.go:282`) → the MCP handler emits `notFound` (`internal/mcp/mcp.go:749`)
→ `{"found":false}`.

## 4. Why this is an inconsistency, not just a missing feature

Alias resolution already exists in the codebase — it is simply not on this one
path:

- **Dedup / re-ingest** resolves through aliases: `resolver.ResolveByName`
  (`internal/wiki/resolver.go:33–41`) tries `subjects.GetByNormName`, then falls
  back to `aliases.GetByNormName`. A re-ingest of "A Mathematical Theory of
  Communication" *does* route to the winner.
- **Link projection** is alias-aware: `PageWithLinks` (`internal/wiki/links.go:95`)
  loads `aliases.ListAll` and folds alias keys into `subjectKeys`, so prose
  mentions of the old name link to the canonical subject (Phase 49).

So three of the four "resolve through an alias" surfaces (link match, re-ingest,
and — believed — `ask`) honor the decree, while the **direct `page`-by-path
lookup does not.** The fix closes that one gap; it does not introduce a new
concept.

## 5. Scope caveat (read before fixing)

The decree's *enumerated* examples are "a link match, an `ask` resolution, a
re-ingest" — it does not literally name the direct `page` tool lookup. So this is
arguably an **unhandled case** rather than a regression of shipped behavior. The
*spirit* ("to every other system and to the user … access via the canonical
subject") clearly covers it, which is why this is filed as a bug. But confirm the
intended scope against the live `design/DNN` Decisions before building — if the
Decision deliberately bounded alias resolution to link/ask/ingest, this becomes a
design change (new Decision) rather than a bug fix.

## 6. Fix direction (sketch — not a committed design)

The narrow, on-pattern fix: make the page **entry** resolution fall through to
the alias table, the same way `ResolveByName` already does. Options, smallest
first:

1. **Resolve at the call site.** In `pathPageService.PageByPath`, on
   `ErrSubjectNotFound`, split the path into `type/token`, try
   `aliases.GetByNormName(token)`, and if it hits, load the winner subject by id
   and continue into `PageWithLinks`. Localized; touches only `cmd/wiki/main.go`.
2. **Make `GetByPath` alias-aware.** Add the alias fallback inside
   `SubjectStore.GetByPath` so *every* path consumer benefits. Broader blast
   radius (the merge tool's `specMergePathResolver` also calls `GetByPath` — and
   it likely should **not** silently resolve a merge `from`/`to` through an
   alias), so this option needs care.

Recommendation: **option 1.** Page serving is the surface that wants transparent
forwarding; the merge resolver is a surface that probably wants the opposite
(operate on real subjects, not aliases). Keeping the fallback at the page call
site respects that asymmetry.

Open question for the fix author: when forwarding, should the served page's
`Path`/`Title` be the **winner's** (the decree says yes — "rendered/treated as
the surviving canonical subject"), and should the response signal that a redirect
happened, or appear identical to a direct canonical lookup? The decree implies
the latter (an alias is invisible), but a caller that asked for the old path may
benefit from knowing it was forwarded.

## 7. Verification once fixed

- `page entity/a-mathematical-theory-of-communication` returns the **winner's**
  page, with `path`/`title` = `the-mathematical-theory-of-communication` /
  its canonical title.
- A direct `page` on the canonical path is byte-identical to the forwarded one.
- A `page` on a genuinely unknown path still returns `found:false`.
- Regression guard: a merge tool `from`/`to` that names an alias still behaves as
  intended (does not silently resolve through the alias, if option 1 is taken).
