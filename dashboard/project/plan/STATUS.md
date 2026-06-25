# dashboard — Plan Status (web pages restructure)

This is the **manifest**: one line per phase in build order, and the **only** place
a phase's status marker lives. Each phase line begins with the literal word `Phase`
and carries `✅` (done) or `⬜` (not started). The build loop finds its next unit of
work with `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`, reads only
that phase's `project/plan/phase-NN.md`, builds it, and on completion flips that one
marker. This file deliberately carries **no bare status glyph** anywhere but on a
phase line, so the anchored grep matches only phase lines.

Phase 01 ✅ realizes D1, D2 — add the `/profile` route + page (session-gated, redirect-when-signed-out) and confirm the login/landing topology on `/`
Phase 02 ✅ realizes D3 — move personal-access-token management (form + list + revoke) onto the profile page; redirect PAT actions to `/profile`; remove PATs from the landing
Phase 03 ✅ realizes D4 — move OAuth grant management (live block + SSE + revoke) onto the profile page; redirect grant revoke to `/profile`; remove grants from the landing
Phase 04 ✅ realizes D5 — landing composition: link each service name to `/srv/<svc>/`, link the owner email to `/profile`, keep sign-out and install instructions on the landing
Phase 05 ⬜ realizes D6 — purge the stale "single hybrid page / don't split" rule from `dashboard/AGENTS.md` and state the three-page truth
