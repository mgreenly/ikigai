# Phase 4 — Conform the landing page to the cron canonical template

*Realizes design Decision 6 (conform to cron). Structural / markup-only — covers
no `R-XXXX-XXXX` ids. Depends on Phases 1–3 (the landing page, its route, and its
nginx fragment already exist; this phase only re-skins the page).*

crm's landing page diverged from the suite-canonical cron page. This phase makes
its markup and tokens identical to cron's, preserving only this service's data
(the service name and version stay dynamic via `{{.Service}}` / `{{.Version}}`).
**Read D6 for the canonical source files and the exact substitution table.**

**What gets changed (markup + tokens only — no Go logic, no nginx, no handler
signature):**

- **`crm/internal/web/landing.html`** — make it identical to
  `cron/internal/web/landing.html` except the three substitutions in D6's table:
  the `<title>` suffix (`· crm`), the `.eyebrow` text (`Contacts CRM`), and the
  lead `<p>` (`Crm keeps contacts, organizations, and deals in SQLite and publishes typed contact events to the event plane.`). Everything else — the inline `<style>`, the
  `<h1>{{.Service}}</h1>`, the `Service / Version / API` `<dl>` (API value
  `POST /mcp`), and the `/static/tokens.css` link — is copied from cron verbatim.
- **`crm/internal/web/static/tokens.css`** — replace with a byte-for-byte copy
  of `cron/internal/web/static/tokens.css`. Leave `static/fonts/` as is.
- **`crm/internal/web/web_test.go`** — if any existing assertion pins the prior
  markup's literal text (the old eyebrow / title / description / card structure),
  update it to the new canonical text. The behavioral assertions from D1–D3
  (status `200`; the body contains the injected service name and version;
  `Content-Type: text/html; charset=utf-8`; the page references the app's own
  `/static/` assets and no cross-service URL) **still hold under cron's markup** —
  keep them.
- Touch nothing else: the handler, the routes, `cmd/crm/main.go`, and the nginx
  fragment are unchanged. No schema change — no migration.

**Done when (structural content check — no id-tagged test):**

- `diff crm/internal/web/static/tokens.css cron/internal/web/static/tokens.css`
  (run from the repo root) prints nothing.
- `crm/internal/web/landing.html` differs from `cron/internal/web/landing.html`
  on only the three lines in D6's table (the `· crm` title suffix, the
  `Contacts CRM` eyebrow, and the `Crm keeps contacts, organizations, and deals in SQLite and publishes typed contact events to the event plane.` lead paragraph); all other lines are
  identical.
- The suite is green: `cd crm && go build ./...`, `cd crm && go vet ./...`,
  `cd crm && gofmt -l .` (prints nothing), `cd crm && go test ./...`, and
  `bin/check-migrations crm`.
