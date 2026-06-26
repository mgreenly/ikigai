# Phase 5 — Purge the stale "single hybrid page / don't split" rule from AGENTS.md

*Realizes design Decision 6 (purge the stale doc rule). Depends on Phases 01–04
(the three pages now exist, so the doc describes a built state).*

Make the dashboard's docs state current truth: the apex serves three pages. This is
a documentation-only phase — no Go code, no template, no test.

**What gets built (the observable end state):**

- In `dashboard/AGENTS.md` (the "Scope" section), the bullet that currently reads,
  in substance, *"Keep the apex `/` a single hybrid page. … Do not split it into a
  separate IAM console — that's product-scale breadth this scope rules out. The
  single page itself is still built to ship."* is **removed**.
- It is **replaced**, in place, with a bullet stating the three-page truth: the
  apex serves a logged-out **login** page, a logged-in **landing/home** page (the
  owner's email, the connect-your-agent install instructions, and the clickable
  service list — each name linking to that service's `/srv/<svc>/` page), and a
  session-gated **profile** page that holds the personal-access-token and OAuth-
  grant administration (reached by clicking the owner's email). State plainly that
  token/grant management lives on the **profile** page, not the landing.
- Only that one falsified bullet changes. The rest of the Scope section
  (bounded-breadth / production-depth, ≤3 services, no product-scale speculation)
  is left intact — it is still true. No "previously…" footnote; purge and rewrite.
- The edit goes through `AGENTS.md`; `dashboard/CLAUDE.md` is a symlink to it and
  updates automatically (do not write the symlink directly).

**Done when:**

- R-DB16-DOCS — `dashboard/AGENTS.md` no longer contains the "single hybrid page" /
  "do not split … IAM console" rule (a text search for that phrasing finds
  nothing), and it states the three-page truth — login / landing / profile, with
  token+grant management on the profile page (a text search for the three-page
  statement finds it).
- The suite is green: `cd dashboard && go build ./...`, `go vet ./...`,
  `gofmt -l .` (no output), `go test ./...`, `bin/check-migrations dashboard`
  (unaffected by a docs-only change, but must remain green).
