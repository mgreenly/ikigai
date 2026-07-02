# Phase 14 — Name-origin colophon copy: quote both glosses, em-dash separators, drop "Japanese", center the pronunciation

*Realizes design Decision 7 (refines the existing `R-DB17-ORIG` copy and
`R-O7K1-XEN7` — no new ids). Touches `ui/html/index.html` (logged-out `{{else}}`
branch: the `name-origin-lede` line and the two `name-origin-parts` `<dd>` glosses),
`ui/static/app.css` (center the `.name-origin-say` foot line), and the pinned
name-origin assertions in `internal/server/index_test.go`. No route, no view-model
change, no migration, no schema. Confined to the logged-out login page's colophon.*

**1. Drop "Japanese" from the lede (D7 / R-DB17-ORIG).** In `ui/html/index.html`, in
the **logged-out `{{else}}` branch only**, change the colophon lede from
`<p class="name-origin-lede"><b>ikigenba</b> — A portmanteau of two Japanese words:</p>`
to `<p class="name-origin-lede"><b>ikigenba</b> — A portmanteau of two words:</p>`
(remove the word "Japanese"; the kanji still show the language). The literal spaced
em-dash after `</b>` is unchanged.

**2. Quote both glosses and separate each with an em-dash (D7 / R-DB17-ORIG).** In the
same `name-origin-parts` `<dl>`, both `<dd>` glosses must be quoted with curly quotes
(`&ldquo;…&rdquo;`) on the romaji-meaning clause and separated from the second clause
by a **spaced literal em-dash** (` — `, matching the em-dash already in the lede), not a
semicolon. Change:
- `<dd>&ldquo;reason for being&rdquo;; work worth doing.</dd>` →
  `<dd>&ldquo;reason for being&rdquo; — work worth doing.</dd>`
- `<dd>the actual place; where the work happens.</dd>` →
  `<dd>&ldquo;the actual place&rdquo; — where the work happens.</dd>`

The first gloss keeps its existing quotes; only its `;` becomes ` — `. The second gloss
gains quotes around `the actual place` and its `;` becomes ` — `. Nothing else in the
`<dt>`/`<div>` structure changes.

**3. Center the pronunciation foot line (D7 / R-O7K1-XEN7).** In `ui/static/app.css`,
in the `.name-origin .name-origin-say` rule, add `text-align: center;` (the block's
`.name-origin` is otherwise `text-align: left`, so this one line re-centers just the
pronunciation foot beneath the two definitions). All other declarations in that rule
are unchanged; add no new selector.

**4. Update the pinned name-origin assertions (D7).** In
`internal/server/index_test.go`, update the exact expected substrings to the new copy
(these are pinned `strings.Contains` checks that will otherwise fail):
- the lede expectation → `<p class="name-origin-lede"><b>ikigenba</b> — A portmanteau of two words:</p>`
- the ikigai gloss → `<dd>&ldquo;reason for being&rdquo; — work worth doing.</dd>`
- the genba gloss → `<dd>&ldquo;the actual place&rdquo; — where the work happens.</dd>`

The structural counts already asserted in that test (exactly one lede, exactly two
paragraphs in the aside, one parts list, two `dt`, two `dd`, two `seam`, two
`lang="ja"` spans, the `name-origin-say` foot last inside the aside) are all still
correct and stay as-is. Then add a CSS assertion, fetching the served `app.css` via the
static route (as the footer / avatar-hover tests do) and asserting it contains a
`.name-origin .name-origin-say` rule whose declaration block includes
`text-align: center` — covering the R-O7K1-XEN7 centering clause.

**Done when:** the suite is green — `cd dashboard && go build ./...`, `go vet ./...`,
`gofmt -l .` (no output), `go test ./...`, and `bin/check-migrations dashboard` (run
from the repo root) all succeed with zero failures (per design *Conventions*) — and
these ids remain covered with the refined copy:

- **R-DB17-ORIG** — the logged-out `GET /` colophon lede calls `ikigenba` a
  "portmanteau of two words" (no "Japanese"), and each of the two `name-origin-parts`
  glosses quotes its romaji meaning in curly quotes and separates the two clauses with a
  spaced em-dash (`&ldquo;reason for being&rdquo; — work worth doing.` and
  `&ldquo;the actual place&rdquo; — where the work happens.`).
- **R-O7K1-XEN7** — the pronunciation foot (`<p class="name-origin-say">` with
  `EE-kee-GEN-buh`) remains the last element inside the `name-origin` aside and the
  served `app.css` centers it (`.name-origin .name-origin-say { … text-align: center }`).
