# Phase 6 — Login composition: control-plane tagline plus a name-origin colophon (name → its two parts)

*Realizes design Decision 7 (login composition). Independent of Phases 01–05 —
it touches only the logged-out (`{{else}}`) branch of the index template and one
appended `app.css` block; no routing, view-model, schema, or profile/landing
change.*

Enrich the logged-out login page: keep its control-plane tagline and sign-in
control exactly as they are, and add below the sign-in button a quiet colophon
that explains the **ikigenba** portmanteau **hierarchically** — the name, then its
**two subcomponents** (*ikigai*, *genba*) subordinate beneath it. It is one name
with two parts, **not** three peer lines.

**What gets built (the observable end state):**

- The `{{else}}` (logged-out) branch of `dashboard/ui/html/index.html` keeps the
  wordmark, the `Your account's control plane` heading, the `Sign in to manage
  access tokens, connected agents, and the box's MCP services.` line, and the
  `Sign in with Google` CTA (`href="/login"`) **verbatim**.
- A `name-origin` colophon is appended **after** the sign-in `<a>`, inside
  `.signin-wall`, with exactly two levels (markup per D7):
  - **One** lede paragraph (`name-origin-lede`): names `ikigenba`, gives its
    meaning (`where your livelihood actually gets done`), and calls it a portmanteau
    of two Japanese words.
  - **One** definition list (`name-origin-parts`) of **exactly two** items —
    `ikigai` (生き甲斐) and `genba` (現場) — each with its surviving fragment marked
    (`<b class="seam">iki</b>gai`, `<b class="seam">genba</b>`) and a one-clause
    gloss. No third sibling line.
- New `.name-origin*` rules are appended to `dashboard/ui/static/app.css` after the
  `.signin-wall` block, semantic tokens only: muted/subtle ink, one `--color-border`
  top rule, narrow `max-width: 420px`. The two parts are made **subordinate** to the
  lede via a left hairline rule + indent on `.name-origin-parts`; the `.seam`
  fragments are full-ink so they read down to spell the name. The lede override is
  written to out-specify `.signin-wall p` (use `.name-origin .name-origin-lede`). No
  accent color anywhere in the block — it must not rival the CTA.
- The colophon lives **only** in the logged-out branch; the logged-in landing/home
  branch is untouched. No new route, link, control, view-model field, or schema.

**Done when:**

- R-DB17-ORIG — a test asserts the logged-out `GET /` renders the colophon as a name
  with two subcomponents (not three peers): a single lede naming `ikigenba`, calling
  it a portmanteau, with its meaning text (`where your livelihood actually gets
  done`); and a `name-origin-parts` list of exactly two items — `ikigai`/生き甲斐 and
  `genba`/現場 — each with a `seam`-marked fragment and a gloss; the parts list is
  structurally distinct from the lede and there is no third sibling explanation line.
- R-DB18-KEEP — a test asserts the logged-out `GET /` body still contains the
  control-plane framing verbatim: `Your account's control plane`, the `Sign in to
  manage access tokens, connected agents, and the box's MCP services.` line, and the
  `Sign in with Google` CTA linking to `/login`.
- R-DB19-LAND — a test asserts the signed-in `GET /` landing/home body omits the
  `name-origin` block (the colophon is logged-out only).
- Tests are co-located in `dashboard/internal/server/*_test.go`, `package server`,
  named for the behavior asserted.
- The suite is green: `cd dashboard && go build ./...`, `go vet ./...`,
  `gofmt -l .` (no output), `go test ./...`, `bin/check-migrations dashboard`.
