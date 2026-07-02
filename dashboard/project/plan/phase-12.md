# Phase 12 — Shared banner chrome: wordmark→home, profile adopts the avatar, avatar hover

*Realizes design Decision 10 (`R-VTIE-IUFA`, `R-VUQA-WM5Z`, `R-VVY7-ADWO`, all new).
Touches `ui/html/index.html` (signed-in `{{if .Owner}}` banner wordmark),
`ui/html/profile.html` (banner rewrite), `internal/server/profile.go` (add
`OwnerInitial` to `profileData`), `ui/static/app.css` (wordmark-as-link styling +
`.avatar:hover`), and the banner assertions in the profile/landing server tests. No
route, no migration, no schema; the only server-logic change is populating
`OwnerInitial` on the profile view model via the existing `ownerInitial` helper.*

**1. Make the signed-in wordmark a home link (D10 / R-VTIE-IUFA).** In
`ui/html/index.html`, in the **logged-in `{{if .Owner}}` branch only**, change the
banner wordmark from `<p class="wordmark">ikigenba</p>` to
`<a href="/" class="wordmark">ikigenba</a>`. Do **not** change the logged-out
`{{else}}` login-page wordmark (it stays a `<p>`; there is no "home" distinct from the
login page). In `ui/html/profile.html`, change the banner wordmark the same way, to
`<a href="/" class="wordmark">ikigenba</a>`.

**2. Profile banner adopts the shared chrome (D10 / R-VUQA-WM5Z).** In
`ui/html/profile.html`, rewrite the `<nav class="identity">` so its right side matches
the landing banner — **sign-out first, monogram avatar last** — and **remove** both the
bordered "Home" button and the email text span:

```html
<nav class="identity">
  <form method="POST" action="/logout">
    <button type="submit" class="btn btn-danger-ghost btn-sm">Sign out</button>
  </form>
  <a href="/profile" class="avatar" aria-label="Profile — {{.Owner}}" title="{{.Owner}}">{{.OwnerInitial}}</a>
</nav>
```

The old `<a href="/" class="btn btn-secondary btn-sm">Home</a>` and
`<span class="owner">{{.Owner}}</span>` are deleted. In
`internal/server/profile.go`, add an `OwnerInitial string` field to `profileData` and
set `data.OwnerInitial = ownerInitial(owner)` (reuse the exported-within-package helper
already defined in `index.go`) where `data` is built. The profile body's "Signed in as
**{{.Owner}}**." line is unchanged — it remains the page's identity statement.

**3. Wordmark-link styling + avatar hover (D10 / R-VTIE-IUFA, R-VVY7-ADWO).** In
`ui/static/app.css`:
- Extend the `.banner .wordmark` rule so the wordmark keeps its current look as an
  anchor: add `color: var(--color-text);` and `text-decoration: none;` (an `<a>` would
  otherwise inherit the accent link color and underline). The existing font-family /
  size / weight / margin declarations stay.
- Add `.identity .avatar:hover { background: var(--accent-700); }` — the same darkening
  `.btn-primary:hover` uses — so the icon-only avatar has a visible clickable
  affordance. If the existing `.identity .avatar` rule lacks a `transition`, add a
  `transition: background <same duration/easing as .btn>;` so the hover matches the
  button feel.

**4. Update the banner tests (D10).** Co-locate in `internal/server/*_test.go`,
`package server`, driving the real route table via the existing `httptest` harness with
a live session (`dashboard_session` cookie), mirroring the profile/landing test setup:
- **R-VTIE-IUFA** — assert both the logged-in `GET /` body and the `GET /profile` body
  contain `<a href="/" class="wordmark">`; and (guard against regressing the login page)
  assert the logged-**out** `GET /` body still renders the wordmark as a `<p>` (no
  `<a href="/" class="wordmark">`).
- **R-VUQA-WM5Z** — on `GET /profile` (live session): assert the avatar link is present
  (`<a href="/profile" class="avatar"`, with the uppercased first-rune initial and
  `title="`+the owner email), that sign-out precedes the avatar (index of the
  `POST /logout` form markup < index of the avatar link), and that the body contains
  **no** `<span class="owner"` and **no** bordered Home button
  (`<a href="/" class="btn`).
- **R-VVY7-ADWO** — fetch the served `app.css` (via the static route, as the footer test
  does) and assert it contains a `.identity .avatar:hover` selector whose declaration
  sets `background` to `var(--accent-700)`.

Update or replace any existing profile-banner assertion that referenced the removed
`Home` button or `<span class="owner">` so the suite stays green.

**Done when:** the suite is green — `cd dashboard && go build ./...`,
`go vet ./...`, `gofmt -l .` (no output), `go test ./...`, and
`bin/check-migrations dashboard` (run from the repo root) all succeed with zero
failures (per design *Conventions*) — and these ids are covered:

- **R-VTIE-IUFA** — the signed-in landing and profile banners render the `ikigenba`
  wordmark as an `<a href="/" class="wordmark">`; the logged-out login page keeps it a
  `<p>`.
- **R-VUQA-WM5Z** — the profile banner renders the monogram avatar + sign-out (sign-out
  first) and contains no email-text span and no separate Home button.
- **R-VVY7-ADWO** — the served `app.css` defines `.identity .avatar:hover` darkening the
  fill to `var(--accent-700)`.
