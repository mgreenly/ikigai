# Phase 26 — The browser wiring gate: one chromedp scenario proving the controls actually work

*Realizes design Decision 23 (the minimal headless-Chrome wiring proof). Depends on Phase 25 (the implemented controller and final layout it drives).*

Add the single real-browser test that closes the gap Phase 24 left open: proof
that `initController` connects the goja-tested logic to a live DOM. One
`github.com/chromedp/chromedp` test-only dependency, one test harness, one
headless-Chrome session, five wiring assertions — and a mechanical guarantee
that neither test dep can leak into the shipped binary. Logic boundaries stay
goja's (Phase 23); nothing here re-proves them.

- **`sites/go.mod`** — add `github.com/chromedp/chromedp` (test-only; the
  transitive `cdproto` is a large but build-cache-absorbed `go.sum` addition).
- **Harness (test code beside the existing landing tests in `cmd/sites`)** — an
  `httptest.Server` serving the **real landing render**: the `GET /{$}` handler
  over a real migrated store seeded with **12 sites** (`docs`, `dashboard`,
  `blog`, and nine fillers none of which contains the subsequence `dsb`, with
  distinct non-lexical creation timestamps) plus the repo-real `share/www`
  static assets. No nginx, no auth, no cookies — the session gate is nginx's
  layer (D4/D18), structurally absent here per D23. Chrome comes from `PATH`
  (`google-chrome`), launched headless with a fresh temp profile via
  `chromedp.NewExecAllocator` + `NewContext`, the whole session under a ~30s
  `context.WithTimeout`. The harness may retry the **launch** once; it never
  retries a scenario assertion, and it never skips — no Chrome means a red
  suite (D23's hard requirement, already stated in design Conventions).
- **The scenario (one session, in order)** — navigate; wait for the search
  input and pager to become visible (boot); type `dsb` → exactly one
  `dashboard` row (filter); clear the query, click the Slug header twice →
  ascending then descending order with matching `aria-sort` (sort); click
  Clear → full default-ordered first page, empty box (clear); `Page 1 of 2` →
  Next → `Page 2 of 2` with the remaining 2 rows → Prev (page).
- **Dependency boundary test** — a test asserting the production import graph
  (`go list -deps ./cmd/sites`, or the equivalent via `golang.org/x/tools`-free
  `go list` exec) contains neither `github.com/chromedp/chromedp` nor
  `github.com/dop251/goja`.

**Done when:** the sites suite is green (`cd sites && go build ./...`, `go vet
./...`, `gofmt -l .` prints nothing, `go test ./...` — now including the
browser test against the locally present `/usr/bin/google-chrome`), AND each id
is covered:
- R-87B9-J644 — after navigation the search input and pager region become visible within the bounded wait (the controller booted: island parsed, `hidden` removed); an empty-stub controller fails by timeout.
- R-88J5-WXUT — typing `dsb` (real key events) leaves exactly one table-body row whose slug anchor text is `dashboard`.
- R-89R2-APLI — clicking the Slug header yields slug-ascending rows + `aria-sort="ascending"` on that header; a second click yields the exact reverse + `aria-sort="descending"`.
- R-8AYY-OHC7 — after filtering and sorting, clicking Clear empties the search input and restores the full first page in default created-at-descending order.
- R-8DER-G0TL — with 12 seeded rows the pager reads `Page 1 of 2`; Next → `Page 2 of 2` with the remaining 2 rows; Prev returns to the first 10.
- R-8EMN-TSKA — the production import graph of `./cmd/sites` contains neither `github.com/chromedp/chromedp` nor `github.com/dop251/goja`.
