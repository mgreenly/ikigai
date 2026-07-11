# webhooks — Design

**Authority: shape and its proof.** This directory owns *how* the webhooks
service is built and *how each behavior is proven* — its seams, public
interfaces, naming, struct/type definitions, data model, and the test plan.
`project/product/README.md` owns the *why* and the user-facing promises; design
states the **exact, checkable form** of those promises and never re-declares the
why. Design **uses** the product's contractual constants by value
(`/srv/webhooks/`, starting version `0.1.0`) but does not own them. This is the
**single, current** statement of the architecture — when a decision changes its
`DNN.md` is rewritten in place (stale decisions removed, not stacked); the
history of how it got here lives in the plan, not here.

## Requirement ids

- Each Decision ends with a **Verification** list: the concrete behaviors that
  decision requires.
- Every Verification item carries a **minted id** of the form `R-XXXX-XXXX` — a
  stable, unique handle for that one behavior.
- The ids live inline in these Verification lists and nowhere else — there is
  **no separate requirements document**.
- Design's responsibility for ids ends at **minting them into this doc**. How
  coverage is measured, what counts as a covered id, and when the work is "done"
  are **not** design's concern — downstream phases own that.

## Conventions

- **Language / toolchain:** Go (the repo targets `go 1.26`); module path
  `webhooks`, built on the shared `appkit` chassis over SQLite
  (`modernc.org/sqlite`, pure-Go, no cgo). In-repo libraries are consumed via
  committed `replace` directives (`appkit => ../appkit`,
  `eventplane => ../eventplane`).
- **Build / typecheck command:** `cd webhooks && go build ./...` (and
  `go vet ./...`). The production binary is built by `bin/ship webhooks`
  (`CGO_ENABLED=0 GOOS=linux GOARCH=amd64 GOWORK=off`, version/commit stamped) —
  not invoked during normal development.
- **Test command:** `cd webhooks && go test ./...`. "The suite is green" means
  this command exits 0 with no failures, alongside a clean `go build ./...` and
  `go vet ./...`. Tests run against **real SQLite** (temp-file DBs via
  `db.Open`, the suite convention) — never a mocked store — with a deterministic
  injected clock.
- **DB / migrations:** schema lives in `internal/db/migrations/` as ordered,
  immutable SQL applied forward-only by the appkit runner. New migrations are
  created with `bin/create-migration webhooks <name>` (timestamped); numbers are
  never hand-picked and committed migrations are never edited.
- **Loopback port:** `3006` (first free port; verified against all
  `*/etc/manifest.env`).
- **Time / IO:** time enters the domain through a `Clock` seam; tests inject a
  deterministic clock. The DB handle is the appkit-owned single-writer
  `*sql.DB` (`rt.DB()`); the producer outbox shares it.
- **Human landing page:** the mount root serves one minimal HTML landing page
  (service name + version) on the suite **Carbon** design system, byte-conformed
  to the **cron canonical** template (`cron/internal/web/landing.html` +
  `static/tokens.css` + woff2 fonts). webhooks embeds its **own** copy of the
  template and assets under `internal/web/` — no shared handler, no runtime
  dependency on the dashboard's assets. Only per-service data (eyebrow,
  description line, `{{.Service}}`/`{{.Version}}`) differs from the canonical.
  The page is served ungated in-process; the browser-session gate lives in the
  nginx fragment (D7).

## Layout

The design is **split for addressability** so the build loop reads only the one
Decision a phase realizes:

- `project/design/INDEX.md` — the manifest: each Decision → its file, plus a
  sorted `R-id → Decision/file` reverse map. Regenerated whenever a Decision is
  added or its Verification ids change.
- `project/design/DNN.md` — one self-contained file per Decision (zero-padded
  `D01.md`, `D02.md`, …; referenced in prose and the plan as `D<N>`).
- `project/design/README.md` — this spine: static cross-cutting facts only, no
  per-Decision detail.

Design is **rewritten in place**, not append-only (history lives in the plan): a
changed Decision is rewritten in its `DNN.md` and `INDEX.md` is regenerated; a
new Decision adds a `DNN.md` and an INDEX entry.
