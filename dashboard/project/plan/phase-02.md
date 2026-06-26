# Phase 2 — Move personal-access-token management onto the profile page

*Realizes design Decision 3 (personal-access-token management moves to the profile
page). Depends on Phase 01 (the profile page and route exist).*

Relocate the PAT surface — create form, list, revoke — from the landing/home page
to the profile page, and point its post-action redirects at `/profile`. The PAT
capability is unchanged; only its location and redirect targets move.

**What gets built (the observable end state):**

- `handleProfile` now populates `PATs` for the signed-in owner via
  `a.pats.ListByOwner` (the same call `handleIndex` makes today, non-fatal on a
  transient list error), and `profile.html` renders the PAT **create form** plus
  the `pat_block` partial.
- `handleIndex` **stops** populating `PATs`; the landing branch of the index
  template **removes** the PAT create form and the `pat_block` section.
- `POST /pat/{public_id}/revoke` redirects to `/profile` (`http.StatusSeeOther`) on
  success instead of `/` (its same-origin + `requireSession` guards and
  not-found-on-mismatch behavior are unchanged).
- The `POST /pat` show-once confirmation (`pat_created.tmpl`) links back to
  `/profile` rather than `/` (the create mechanism and audit are unchanged).
- No change to the PAT store, data model, or route paths.

**Done when:**

- R-DB06-PATM — a test asserts `GET /profile` with a valid session renders the PAT
  create form and the owner's PAT list (`pat_block`), reflecting active PATs (e.g.
  create one via the store, assert it appears).
- R-DB07-PATR — a test asserts `POST /pat/{public_id}/revoke` redirects to
  `/profile` on success, and that the `POST /pat` confirmation response links to
  `/profile` (not `/`).
- R-DB08-PATX — a test asserts `GET /` with a valid session (the landing) contains
  no PAT-create form and no PAT rows.
- Tests are co-located in `dashboard/internal/server/*_test.go`, `package server`,
  named for the behavior asserted.
- The suite is green: `cd dashboard && go build ./...`, `go vet ./...`,
  `gofmt -l .` (no output), `go test ./...`, `bin/check-migrations dashboard`.
