# Phase 4 — Landing composition: service-name links, email→profile, sign-out, install

*Realizes design Decision 5 (landing composition: service-name links, email→profile
nav, sign-out). Depends on Phases 02 and 03 (the landing is cleared of token/grant
administration).*

Finish the landing/home page: turn each service name into a link to that service's
own page, make the owner's email link to the profile page, and confirm sign-out and
the install instructions remain on the landing.

**What gets built (the observable end state):**

- The service-row view model (`serviceRows` in `inventory.go`) gains a
  mount-derived **href** field carrying each service's `Mount` (e.g. `/srv/crm/`,
  which already includes its trailing slash) from `inventory.Read`, alongside the
  existing display name and raw MCP `URL`.
- The landing branch of the index template renders each service **name** as a link
  to its mount root — `<a href="{{.Href}}">{{.Name}}</a>` — while keeping the raw
  MCP `URL` as reference text (for hand-wiring any client). Only the name is
  linked.
- The owner's email at the top of the landing is rendered as a link to `/profile`
  (`<a href="/profile">{{.Owner}}</a>`).
- The `POST /logout` sign-out form/button stays on the landing; the connect-your-
  agent install section (Claude/Codex snippets) stays on the landing. No change to
  `/services`, the install scripts, or `mcpResourceURL`.

**Done when:**

- R-DB12-LINK — a test asserts each service row on the landing links its **name** to
  the service's mount root `/srv/<svc>/` (the `Mount` href), not to the MCP resource
  URL.
- R-DB13-MAIL — a test asserts the owner's email on the landing is a link with
  target `/profile`.
- R-DB14-SOUT — a test asserts the sign-out control (the `POST /logout` form) is
  present on the landing.
- R-DB15-INST — a test asserts the landing retains the connect-your-agent install
  instructions (the Claude and Codex install snippets).
- Tests are co-located in `dashboard/internal/server/*_test.go`, `package server`,
  named for the behavior asserted.
- The suite is green: `cd dashboard && go build ./...`, `go vet ./...`,
  `gofmt -l .` (no output), `go test ./...`, `bin/check-migrations dashboard`.
