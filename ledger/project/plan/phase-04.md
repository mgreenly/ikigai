# Phase 4 — Conform the landing page to the cron canonical template

*Realizes design Decision 6 (conform to cron). Structural / markup-only — covers
no `R-XXXX-XXXX` ids. Depends on Phases 1–3 (the landing page, its route, and its
nginx fragment already exist; this phase only re-skins the page).*

ledger's landing page diverged from the suite-canonical cron page. This phase makes
its markup and tokens identical to cron's, preserving only this service's data
(the service name and version stay dynamic via `{{.Service}}` / `{{.Version}}`).
**Read D6 for the canonical source files and the exact substitution table.**

**What gets changed (markup + tokens only — no Go logic, no nginx, no handler
signature):**

- **`ledger/internal/web/landing.html`** — make it identical to
  `cron/internal/web/landing.html` except the three substitutions in D6's table:
  the `<title>` suffix (`· ledger`), the `.eyebrow` text (`Double-entry ledger`), and the
  lead `<p>` (`Ledger records an immutable double-entry journal in SQLite and publishes posting events to the event plane.`). Everything else — the inline `<style>`, the
  `<h1>{{.Service}}</h1>`, the `Service / Version / API` `<dl>` (API value
  `POST /mcp`), and the `/static/tokens.css` link — is copied from cron verbatim.
- **`ledger/internal/web/static/tokens.css`** — replace with a byte-for-byte copy
  of `cron/internal/web/static/tokens.css`. Leave `static/fonts/` as is.
- **`ledger/internal/web/web_test.go`** — if any existing assertion pins the prior
  markup's literal text (the old eyebrow / title / description / card structure),
  update it to the new canonical text. The behavioral assertions from D1–D3
  (status `200`; the body contains the injected service name and version;
  `Content-Type: text/html; charset=utf-8`; the page references the app's own
  `/static/` assets and no cross-service URL) **still hold under cron's markup** —
  keep them.
- Touch nothing else: the handler, the routes, `cmd/ledger/main.go`, and the nginx
  fragment are unchanged. No schema change — no migration.

**Done when (structural content check — no id-tagged test):**

- `diff ledger/internal/web/static/tokens.css cron/internal/web/static/tokens.css`
  (run from the repo root) prints nothing.
- `ledger/internal/web/landing.html` differs from `cron/internal/web/landing.html`
  on only the three lines in D6's table (the `· ledger` title suffix, the
  `Double-entry ledger` eyebrow, and the `Ledger records an immutable double-entry journal in SQLite and publishes posting events to the event plane.` lead paragraph); all other lines are
  identical.
- The suite is green: `cd ledger && go build ./...`, `cd ledger && go vet ./...`,
  `cd ledger && gofmt -l .` (prints nothing), `cd ledger && go test ./...`, and
  `bin/check-migrations ledger`.
