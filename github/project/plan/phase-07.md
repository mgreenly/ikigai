# Phase 7 — Upgrade the landing page to the canonical suite layout

*Realizes design Decision 6 (landing page, new ids `R-7NJI-UTHM`, `R-7ORF-8L8B`,
`R-7PZB-MCZ0`). Depends on Phase 6 (the `internal/web` seam and its wiring).*

## What gets built

`internal/web/landing.html` — replace github's minimal name+version template with
the **canonical suite landing layout** (the current `gmail/internal/web/landing.html`
is the reference), using github's own copy: a `<title>` of `<service> · github`; a
`Home` link (`<a class="home" href="/">Home</a>`) as the first element inside
`<main>`; the eyebrow `GitHub connector`; the one-line description `Github connects
the suite to GitHub through one shared GitHub App and exposes repository, pull
request, and issue actions as MCP tools.`; and a Service / Version / API details
grid whose API cell is `POST /mcp`. Dynamic `service`/`version` stay injected
through `html/template` so they render HTML-escaped.

`internal/web/web_test.go` — extend the existing offline tests so the new
canonical behaviors are covered by clearly-named tests tagged with their ids
(canonical layout/copy → `R-7NJI-UTHM`; escaping of hostile service/version →
`R-7ORF-8L8B`; `Home` link first inside `<main>` → `R-7PZB-MCZ0`), keeping the
existing `R-EVZ3-VXJZ` and `R-EX70-9PAO` tests green.

`internal/web/static/tokens.css` — fix the stale copy-comment that still reads
`for the gmail landing page` so it names github.

Left untouched because they are already canonical: `web.go`, `embed.go`, the
embedded font/CSS assets, `etc/nginx.conf` (its session-gated landing/static and
the correct github-specific `= /srv/github/pr` 404 already match the suite model),
and `internal/web/nginx_test.go`.

Observable end state: a logged-in browser at `/srv/github/` sees the same landing
layout the other services render — eyebrow, description, and a Service / Version /
API panel on the suite design system — instead of the bare name+version page.

## Done when

All hold on identical repo state, from `github/`:

- `GOWORK=off go build ./...` and `GOWORK=off go test ./...` exit 0; `gofmt -l .`
  empty; `go vet ./...` clean.
- Clearly-named offline tests cover and pass for `R-7NJI-UTHM` (`GET /` renders
  the `<service> · github` title, the `GitHub connector` eyebrow, the exact
  description, and a Service/Version/API grid with `POST /mcp`), `R-7ORF-8L8B` (a
  `<script>`-bearing service name and an `&`-bearing version render escaped, never
  as raw markup), and `R-7PZB-MCZ0` (the `Home` link is the first element inside
  `<main>`) — each id named in a test — and the pre-existing `R-EVZ3-VXJZ` and
  `R-EX70-9PAO` tests remain green.
- The stale-comment cleanup is verifiable and scoped to the shipped asset (not the
  `project/` docs): `grep -c 'gmail landing page' github/internal/web/static/tokens.css`
  returns `0`.
