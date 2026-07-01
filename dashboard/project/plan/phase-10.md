# Phase 10 — Make the sign-in line the login heading and remove the site-wide footer

*Realizes design Decisions 7 (`R-DB18-KEEP`, reworded) and 9 (`R-EFJZ-FRQ1`, new).
Two coupled edits folded into one build phase because both touch
`ui/html/index.html`. Touches `ui/html/index.html`, `ui/html/profile.html`,
`ui/html/partials/pat_created.tmpl`, `ui/static/app.css`, and the pinned assertions
in `dashboard/internal/server/index_test.go` (plus a footer-absence assertion,
likely alongside the profile/index server tests). No `internal/server` logic change,
no route, no migration, no view-model, no schema.*

**1. Promote the sign-in line to the heading (D7 / R-DB18-KEEP).** In
`ui/html/index.html`, in the `{{else}}`/logged-out `.signin-wall` branch, the
heading becomes the sign-in sentence and the old control-plane line and the separate
sub-line paragraph both go. Concretely: change `<h1>Your account's control
plane</h1>` to `<h1>Sign in to access your services.</h1>`, and delete the now-
redundant `<p>Sign in to access your services.</p>` paragraph beneath it. The
wordmark, the `Sign in with Google` CTA, and the name-origin colophon below the CTA
are unchanged.

Update `dashboard/internal/server/index_test.go` for `R-DB18-KEEP`: assert the body
contains `<h1>Sign in to access your services.</h1>` and the CTA (keep the wordmark
assertion); **remove** the old `<h1>Your account's control plane</h1>` and
`<p>Sign in to access your services.</p>` "must contain" entries; and **add**
assertions that the logged-out `GET /` body does **not** contain
`Your account's control plane`, does **not** contain the `<p>Sign in to access your
services.</p>` sub-paragraph form, and does **not** contain the older
`manage access tokens, connected agents, and the box's MCP services` enumeration.

**2. Remove the site footer everywhere (D9 / R-EFJZ-FRQ1).** Delete the
`<footer class="site-footer">…</footer>` element from **all four** locations:
- `ui/html/index.html` — both the logged-out branch footer (`{{.Host}}`) and the
  logged-in landing footer (`{{.Host}}`);
- `ui/html/profile.html` — the profile footer (`{{.Host}}`);
- `ui/html/partials/pat_created.tmpl` — the fragment footer
  (`<footer class="site-footer">ikigenba</footer>`).

And in `ui/static/app.css`, delete the `.site-footer { … }` rule together with its
`/* ---- Footer --- */` section comment. Leave all other markup and CSS untouched.

Add id-tagged coverage for `R-EFJZ-FRQ1`: assert that the logged-out `GET /`,
logged-in `GET /` (live session), and `GET /profile` (live session) bodies each
contain **no** `<footer` and no `site-footer` substring; that the PAT-created
fragment (from the create-PAT POST flow) contains no footer; and that the served
`app.css` contains no `.site-footer` selector. Co-locate these in
`internal/server/*_test.go`, `package server`, named for the behavior, driving the
real route table via the existing `httptest` harness (live session via the
`dashboard_session` cookie; mirror the existing profile/PAT test setup for the
fragment check).

**Done when:** the suite is green — `cd dashboard && go build ./...`,
`go vet ./...`, `gofmt -l .` (no output), `go test ./...`, and
`bin/check-migrations dashboard` all succeed with zero failures (per design
*Conventions*) — and these ids are covered:

- **R-DB18-KEEP** — the logged-out `GET /` renders the wordmark, an `<h1>` whose
  text is exactly `Sign in to access your services.`, and the `Sign in with Google`
  CTA linking to `/login`; and it contains **no** `Your account's control plane`
  text, **no** `<p>Sign in to access your services.</p>` sub-paragraph, and **no**
  old tokens/agents/MCP enumeration. *(httptest via `testServer`/`do`, logged-out
  `GET /`)*
- **R-EFJZ-FRQ1** — no page or fragment renders a footer: logged-out `GET /`,
  logged-in `GET /`, and `GET /profile` bodies, plus the PAT-created fragment,
  contain no `<footer`/`site-footer`; and the served `app.css` contains no
  `.site-footer` selector. *(httptest via `testServer`/`do`; live session cookie for
  the signed-in and profile checks)*
