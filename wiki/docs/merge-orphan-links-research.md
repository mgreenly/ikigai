# Merge Orphans Inbound Short-Name Mentions — Research

**Status: informational, non-contractual.** This doc feeds the author of a
design change and nothing downstream consumes it (the autonomous build reads
only product, design, plan). It records the bug, what the current code and specs
actually do, prior art, the solution options, and a recommendation. Rewrite in
place as the goal evolves — never append. Sibling to `subject-dedup-research.md`
(the aliases/merge feature this bug follows from).

---

## 0. The load-bearing constraint (read first)

**Owner's decree:** *Access through an alias must always appear, to every other
system and to the user, as access via the canonical subject name.* An alias is a
pure **internal redirect** — never a thing of its own. Anywhere a name resolves
*through* an alias (a link match, an `ask` resolution, a re-ingest), the result
is rendered/treated as the **surviving canonical subject**: its canonical name,
its `type/slug` path, its page. An alias never gets its own path, never appears
in a subject listing, never surfaces as a distinct link target. This constraint
*decides* the ambiguous parts of every option below — most importantly: an
alias-matched mention links to the canonical name/path, not the variant text.

This is exactly the convergent industry model (Wikidata redirect, SKOS
`prefLabel`/`altLabel`): the variant is a durable resolution key onto one
canonical entity, never an entity itself.

**Owner's second decree — two name forms only, no space form.** A subject carries
exactly two representations and the confusing third (`normalize()`, the space
form) is **eliminated**:
- **Original text** — the subject's `name` as ingested, **never lost**, stored on
  the subject. This is the form **prose** must always use: page bodies, link
  *display text*, `ask` answers and their citations. Prose is original, always.
- **Hyphen form** (`Normalize()`, `[a-z0-9-]`) — the **database key** and the
  **addressing handle**: `type/slug` paths, identity, dedup, lookups, **alias
  keys**, link *targets*.

The dividing line is **prose vs identifier**, not user vs operator. There is *no*
strict rule that a user never sees the hyphen form — it is the natural handle for
identifier-shaped interactions and may be the *more* useful form there (e.g.
naming `from`/`to` in a merge, a subject listing, a link target). What is strict
is the converse: **prose results always render the original text**, never the
slug.

There is no separately-conceived "space form." Link/alias *matching* (an internal
computation, not a surface) runs in the **hyphen form**: normalize the page body
with `Normalize()` too, then match a subject's/alias's hyphen key as a
hyphen-bounded run. The hyphen already encodes word boundaries, so this is
behaviorally identical to today's space-form whole-phrase match (verified against
edge cases: "Vasarian" ∌ "vasari", "Francis IV" ∌ "francis-i"). Collapsing onto
one normalizer is what makes §2d's alias-key mismatch disappear by construction.

---

## 1. The bug

The wiki computes page links as a **read-time projection**: `Mentions(body,
others)` scans a page body and links every *other subject whose name occurs in
it* (whole-word, normalized). Links are not stored.

An owner can **merge** two subjects — fold a "loser" into a surviving "winner."

**Symptom (observed on the live corpus):** after merging `entity/vasari` →
`entity/giorgio vasari`, a third page (`entity/francis i`) whose prose says
"Vasari records that…" lost its rendered link to Vasari entirely. It did *not*
redirect to the survivor — it points at **nothing**. The body still says the
short name "Vasari"; no subject is named exactly "Vasari" anymore; the survivor
is "Giorgio Vasari," which the body does not contain → no match → orphaned
mention.

**Generalization:** every merge that removes a subject whose *short/variant name*
is what other pages used in prose silently un-links those inbound mentions. The
remaining duplicate stubs (Verrocchio, Melzi, …) are all this bare-surname shape,
so the bug recurs on every future merge of that kind.

---

## 2. What the current code actually does (verified against HEAD, post Phase 45–47)

The branch moved a lot since the bug was first noticed; this is current truth.

### 2a. The link matcher — names only, never aliases
`internal/wiki/links.go`:
- `Mentions(body, others []Subject)` (≈L28–41) normalizes the body with
  `normalize()` (space form) and, for each candidate **subject**, checks
  `containsWholePhrase(normalizedBody, normalize(subject.Name))`.
- It matches against **current subject names only**. Aliases are never loaded or
  consulted.
- `PageWithLinks` (≈L87–129) feeds it `others = listAllSubjects(...)` — the
  `subjects` table only. The MCP `page` handler is the caller
  (`cmd/wiki/main.go` ≈L279–297 → `PageWithLinks`).

### 2b. The merge job — **omits the alias insert** (an in-scope defect)
`internal/wiki/service.go` `mergeSubjects` (≈L428–515): recompiles the winner's
page, then in one write-tx deletes the loser's page, `claims.RepointSubject`,
`aliases.RepointSubject` (repoints *existing* aliases off the loser),
`subjects.Delete(loser)`, `pages.Upsert(winner)`.

**Missing:** it never calls `aliases.Insert(...)` to create the
`normalize(loser.name) → winner` record. The design **mandates** this:
`docs/design/D26.md` Phase C step 6 ("Insert the forward-routing alias
`normalize(loser.name) → winner`"), verified by **R-NGVA-LS02**. So this is a
**straight implementation bug against an existing, in-scope Decision** — not a
deferred feature.

**Consequence (bigger than the link bug):** because the loser-name alias is never
created, even the *documented* merge promises are currently broken — `ask`
resolving the folded name to the survivor (product L33/L96) and re-ingest routing
the folded name to the survivor both fail. The orphaned link is one symptom of a
merge that simply isn't leaving its redirect behind.

### 2c. Alias machinery exists and is cheap to read
`internal/wiki/aliases.go` + migration `…_create_aliases.sql`: table
`aliases(norm_name UNIQUE, subject_id, name, created_by, created_at)`. `Insert`,
`GetByNormName`, `RepointSubject(from,to)` all present. `Resolver.ResolveByName`
(`resolver.go` ≈L29–53) tries `subjects` first, then `aliases` — used by ingest
(`plannedSubject`) and `ask` (`gatherPages`). The whole alias set is small and
loadable in one query, so the read-time link projection can load it as cheaply as
it already loads all subjects.

### 2d. A second latent defect — normalizer mismatch in alias lookup → *delete the space form*
Two normalizers exist (`data_model.go`):
- `normalize()` (≈L86) — lowercase + space-join + diacritic-strip. Used **only**
  by link matching (`Mentions`) and, wrongly, as the alias-key normalizer.
- `Normalize()` (≈L94) — path-safe `[a-z0-9-]` slug. **Identity & paths** use
  this; it is what's stored in `subjects.norm_name` (path = `type/norm_name`,
  resolved by direct indexed lookup — D03/D11).

The defect: `SubjectStore.GetByNormName` applies `Normalize()` (hyphen) while
`AliasStore.GetByNormName` applies `normalize()` (space) to the *same* string the
`Resolver` hands both. For any **multi-word** alias name the forms differ
(`"giorgio vasari"` vs `"giorgio-vasari"`), so alias resolution diverges from
subject resolution. D25 is ambiguous about which form the alias key uses.

**Resolution (owner's second decree, §0): eliminate the space form entirely.**
`normalize()` exists only because prose can't be substring-matched against a
hyphen slug — but it can, if the body is run through `Normalize()` too and the
key matched as a hyphen-bounded run (§0). So the fix is not "pin to one form" but
**delete `normalize()`**: switch `Mentions` and the alias key to `Normalize()`,
leaving a single normalizer. This both unifies alias/subject resolution and
removes the source of confusion. Touches D03/D11 (already on `Normalize`), D12
(link matcher), D25 (alias key).

---

## 3. What the specs already decide (the scope question)

- **D12 (page links)** specifies exact-normalized whole-word match and
  **explicitly rejects** alias/variant linking: *"Fuzzy / substring name matching
  … Aliases/fuzzy are a later release. Rejected."* (R-ZY11-SUQS: "a body
  containing a variant/partial name … produces no link"). D12 also notes a
  materialized `page_links` table as a deliberately **deferred** optimization.
- **product.md L60** lists as an explicit non-goal: *"reconcile a link across
  differently-named subjects … draws no link **until aliases / fuzzy matching
  arrive in a later release**."* L39: links match by exact normalized name,
  "nothing fuzzy."
- **D25/D26 (aliases, merge)** scope aliases to **ingest + ask** resolution and
  mandate the merge-time alias insert — but say **nothing** about feeding aliases
  into link projection, and nothing about recompiling other referencing pages.

**Verdict.** The fix splits cleanly in two:
1. *Create the loser-name alias on merge* — **in scope today**; D26 already
   requires it; the code is simply missing it. Fixing this is a bug fix, not a
   product change. (Plus pin the alias normalizer form, §2d.)
2. *Make page links honor that alias* — **explicitly deferred** by D12 and
   product L60. This is "aliases arrive" for the link path; it needs a **product
   amendment + a new/amended Decision**, not just an implementation patch. Note
   the expansion is narrow: we bring **alias-aware exact-normalized** matching
   into scope; we keep **fuzzy/partial** matching deferred.

---

## 4. Prior art (how mature systems avoid this exact failure)

The one-sentence finding: **no mature system computes links by scanning prose
against current canonical names at read time.** They bind references to a stable
id + a durable alias/redirect layer, and treat a merged-away name as a redirect
record — not a string to be re-matched.

- **Wikidata (cleanest analogue):** merge consolidates into the survivor; the
  obsolete QID becomes a **permanent redirect, never deleted** ("deletion would
  invalidate possible references"). Existing references resolve through it. A bot
  later repoints references to collapse the indirection — but correctness never
  depends on that pass. Aliases ("also known as") and redirects are *separate*
  mechanisms: aliases = alternate surface forms → one canonical id.
- **MediaWiki:** persisted `pagelinks` built on save (never a read-time text
  scan); merge = `#REDIRECT`, referrers untouched and resolved through the
  redirect; `WhatLinksHere` is a table query. Pitfall: only **one** redirect hop
  is followed — sequential merges (A→B→C) need a collapse pass.
- **SKOS** gives the schema: `prefLabel` (canonical) / `altLabel` (resolving
  aliases) / `hiddenLabel` (matchable misspellings).
- **Note tools (Obsidian/Roam/Logseq):** all precompute a link index; the fork is
  what a link binds to — a **stable block/entity id** (Roam/Logseq → rename/merge
  re-resolves automatically) vs a **mutable title string** (Obsidian/Foam →
  forces a physical rewrite of every referrer on rename: leak-prone, slow,
  corruption-on-interrupt). "Merge → old name becomes a durable alias of
  survivor" is the feature their communities keep reinventing (e.g. Obsidian
  "Merge as Alias" / "Smart Rename").
- **Matching efficiency:** for matching prose against a large alias set, use a
  trie/automaton (Aho-Corasick, FlashText — O(text), independent of alias count),
  not naive O(aliases × text). Not needed at current corpus scale, but the path
  is known.

Convergent answer: **register the folded name as a durable alias of the survivor,
and resolve read-time matching against the full alias set — presenting every
match as the canonical subject.** That is precisely §0's constraint. Any
text-rewrite pass should be *non-load-bearing cleanup that retains the original
surface text*, never the correctness mechanism.

---

## 5. Solution options

### Option 1 — Alias-aware read-time matcher *(recommended)*
Keep links a read-time projection, but match the body against **subject names
AND alias keys**; an alias hit yields the **canonical subject**, so the rendered
link is the canonical name + `type/slug` (satisfies §0). Concretely:
- (bug, in scope) Fix `mergeSubjects` to `aliases.Insert(normalize(loser.name) →
  winner)` per D26/R-NGVA-LS02.
- (bug + simplification, in scope) Delete the space form `normalize()` (§0/§2d):
  switch `Mentions` and the alias key onto the single `Normalize()` hyphen form,
  matching against the hyphen-normalized body. Unifies alias/subject resolution.
- (feature, needs amendment) Load aliases into the link projection call site and
  have `Mentions` resolve an alias match to its canonical subject, de-duping when
  both a name and an alias of the same survivor match.
- (governance) Amend product L60 + add/extend a Decision off D12 to bring
  **alias-aware exact-normalized** linking into scope (keep fuzzy/partial out).

**Why:** smallest coherent change that makes the whole system correct at once;
one matching site; no migration; no write-path change; no extra LLM calls; reads
stay always-correct. Matches prior art and §0 exactly.
**Watch:** alias ambiguity/collision (rely on exact-normalized whole-word, never
substring — keeps us out of "fuzzy"); sequential-merge chains (already handled —
`aliases.RepointSubject` repoints a prior alias onto the new winner, so no
two-hop chain forms, unlike MediaWiki); perf (alias set tiny now; automaton later
if it grows).

### Option 2 — Recompile/rewrite referencing pages on merge (canonicalize-on-write)
On merge, find pages mentioning the loser and rewrite their prose to the
canonical name. **Heavier:** O(referencers) LLM calls per merge; touches many
pages; and it **doesn't actually fix the class** — a *new* page ingested later
that uses the old short name re-orphans, because nothing rewrites it. Prior art
warns this is leak-prone and should only ever be non-load-bearing cleanup.
**Rejected as the primary fix;** viable only as optional later cleanup layered on
Option 1.

### Option 3 — Materialized `page_links` table (stored backlinks on write)
The "right" long-term cost model per prior art (reads ≫ writes), and D12 already
flagged it as a deferred optimization. **But** it does not by itself solve the
alias problem (a stored link still has to resolve the variant to the survivor),
and it adds a migration + write-path step + drift management. **Orthogonal to
this bug; defer.** If adopted later, it composes with Option 1's alias resolution.

---

## 6. Recommendation

Ship **Option 1**, sequenced by scope:

1. **Bug fix + normalizer simplification, no product-promise change** — make
   `mergeSubjects` insert the loser-name alias (D26 step 6, currently missing),
   and **delete the space form `normalize()`** (§0/§2d), moving `Mentions` and the
   alias key onto the single `Normalize()` hyphen form so `Resolver` resolves
   consistently. This alone repairs the *documented* merge promises (`ask` +
   re-ingest routing of the folded name), independent of links.
2. **Scoped product + design amendment** — lift the product L60 / D12 deferral
   for the **merge-alias** case only: link projection resolves the body against
   the alias set as well as subject names, and an alias match renders as the
   canonical subject (§0). Keep fuzzy/partial matching deferred.
3. Then plan-mode → build via the existing ralph loop.

Leave Options 2 and 3 out of this change; Option 3 is a clean future optimization
that composes with this, Option 2 is at most optional cleanup.

**A failing repro test belongs in the first slice** (`internal/wiki/links_test.go`
or a merge test): merge a short-name subject into a longer-named survivor, assert
a third page that mentions the short name still links — to the *canonical* path.
