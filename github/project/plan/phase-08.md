# Phase 8 — Pin the landing page to the canonical layout, not just its content

*Realizes design Decision 6 (landing page, new ids `R-WYSR-NPL3`, `R-X00O-1HBS`).
Depends on Phase 7 (the canonical-content template and its content/escaping
tests).*

## What gets built

Phase 7 gave the page the canonical *content* (title, eyebrow, description,
Service/Version/API grid, Home link) and its tests all pass — but they only
assert that text is present. The shipped `internal/web/landing.html` drifted from
the canonical suite `<style>` and markup anyway: the `body` font names an
**undefined** `var(--font-sans)` (so it silently falls back to the browser
default serif), the `Home` link sits in normal flow instead of anchored top-left,
spacing is hardcoded pixels instead of `var(--space-*)` tokens, and the details
grid uses renamed classes (`.detail`, `.description`) the canonical `<style>`
never targets. This phase makes the shipped page actually render as the suite
does and pins that so it cannot silently drift again.

`internal/web/landing.html` — bring the template into line with the **canonical
suite landing layout** (the current `gmail/internal/web/landing.html` is the
reference for structure and `<style>`), carrying github's own copy: the body set
in `var(--font-body)`, the `Home` link anchored top-left in `var(--font-mono)`,
`var(--space-*)` token spacing, and the `dl`/`dt`/`dd`/`code` details grid — with
github's own `<title>` (`<service> · github`), eyebrow (`GitHub connector`),
one-line description, `POST /mcp` API cell, and `{{.Service}}`/`{{.Version}}`
still injected through `html/template` (HTML-escaped). No `var(--font-sans)` or
any other token absent from the embedded `static/tokens.css` survives anywhere in
the page.

`internal/web/testdata/landing.golden.html` — a new committed golden fixture: the
exact HTML `LandingHandler` renders for a fixed `service`/`version` pair, encoding
the canonical markup **and** `<style>`.

`internal/web/web_test.go` — extend the offline tests with clearly-named tests
tagged with their ids: token integrity (every `var(--…)` referenced by the
rendered page is defined in `static/tokens.css`) → `R-WYSR-NPL3`; golden render
(the full HTML for fixed inputs is byte-identical to `testdata/landing.golden.html`)
→ `R-X00O-1HBS`. The pre-existing D6 tests (`R-EVZ3-VXJZ`, `R-7NJI-UTHM`,
`R-7ORF-8L8B`, `R-7PZB-MCZ0`, `R-EX70-9PAO`) stay green against the realigned
template.

Observable end state: a logged-in browser at `/srv/github/` sees a page visually
identical to the other services — the Home link in the same top-left position and
the same monospace font, the body in the suite sans, the same spacing and grid —
not a serif, mispositioned variant that merely carries the right words.

## Done when

All hold on identical repo state, from `github/`:

- `GOWORK=off go build ./...` and `GOWORK=off go test ./...` exit 0; `gofmt -l .`
  empty; `go vet ./...` clean.
- Clearly-named offline tests cover and pass for `R-WYSR-NPL3` (every `var(--…)`
  the rendered page references is defined in `static/tokens.css`) and
  `R-X00O-1HBS` (the full render for fixed inputs equals
  `internal/web/testdata/landing.golden.html` byte-for-byte) — each id named in a
  test — and the pre-existing `R-EVZ3-VXJZ`, `R-7NJI-UTHM`, `R-7ORF-8L8B`,
  `R-7PZB-MCZ0`, and `R-EX70-9PAO` tests remain green.
- The undefined-token trap is gone from the shipped asset, scoped away from the
  `project/` docs that quote it: `grep -c 'font-sans' github/internal/web/landing.html`
  returns `0`.
