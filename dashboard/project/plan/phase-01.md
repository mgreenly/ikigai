# Phase 1 — Add the `/profile` route + page, session-gated; confirm the login/landing topology

*Realizes design Decision 1 (three-page topology and the route map) and 2 (the
profile route is session-gated in-process, redirect when signed out). Depends on
no earlier phase.*

Stand up the **profile** page and its route, gated to a signed-in owner, and lock
in the three-page topology on `/` (login when logged out, landing when logged in).
This phase creates the page **shell** showing the owner's email; the PAT and grant
blocks are moved onto it by Phases 02 and 03.

**What gets built (the observable end state):**

- A new `handleProfile` handler in `dashboard/internal/server/` (e.g.
  `profile.go`) that:
  - resolves the session via the existing `(*app).sessionOwner` seam;
  - **signed out** (no cookie / `session.ErrInvalid` / lookup miss) → redirects to
    `/` with `http.StatusFound` (302) and clears a present-but-dead cookie
    (`clearSessionCookie`), mirroring `handleIndex`'s cleanup — it renders **no**
    profile content;
  - **signed in** → renders a new `profile.html` template showing the owner's
    email (the PAT/grants sections arrive in Phases 02/03).
- `GET /profile` registered in `(*app).register` (`routes.go`).
- A new `html/profile.html` template added to the `template.ParseFS(ui.Files, …)`
  set in `server.go` (sharing the parse set with the existing partials so
  `pat_block`/`grants_block` resolve when Phases 02/03 embed them). It links to the
  same `/static/tokens.css` + `/static/app.css` Carbon assets the index uses.
- `GET /` is unchanged in this phase — it already branches login vs landing in
  `handleIndex`; this phase adds tests pinning that topology so later phases that
  strip PAT/grants from the landing don't regress the login page.

**Done when:**

- R-DB01-PG3A — a test asserts `GET /` with no/invalid session renders the login
  page (sign-in control present) and contains no PAT-create form, no grants block,
  and no email/profile link.
- R-DB02-LND7 — a test asserts `GET /` with a valid session renders the
  landing/home page: owner email shown, install instructions present, service list
  present.
- R-DB03-PRF9 — a test asserts `GET /profile` is a distinct registered route (not a
  fall-through to the index handler).
- R-DB04-GATE — a test asserts `GET /profile` with no/invalid session returns a 302
  whose `Location` is `/`, renders no profile content, and clears a
  present-but-invalid session cookie.
- R-DB05-SESS — a test asserts `GET /profile` with a valid session renders the
  profile page showing the owner's email.
- Tests are co-located in `dashboard/internal/server/*_test.go`, `package server`,
  named for the behavior asserted (e.g. `profile_test.go`).
- The suite is green: `cd dashboard && go build ./...`, `go vet ./...`,
  `gofmt -l .` (no output), `go test ./...`, `bin/check-migrations dashboard`.
