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
Phase 05 ✅ realizes D6 — purge the stale "single hybrid page / don't split" rule from `dashboard/AGENTS.md` and state the three-page truth
Phase 06 ✅ realizes D7 — login composition: keep the control-plane tagline + sign-in CTA verbatim and add a name-origin colophon below the CTA on the logged-out `/` that explains the ikigenba portmanteau as a name + its two subcomponents (ikigai, genba)
Phase 07 ✅ realizes D8 — eliminate the web-font FOUT: switch the four @font-face blocks in tokens.css to font-display: optional and preload the display + body fonts (crossorigin) in both head-bearing templates (index.html, profile.html); apex src already origin-correct; covers R-P97M-GIJ1, R-PAFI-UA9Q, R-PBNF-820F
Phase 08 ✅ realizes D7 — reword the logged-out login sub-line to "Sign in to access your services." (replacing the stale tokens/agents/MCP enumeration) in index.html and update the pinned assertion in index_test.go; covers R-DB18-KEEP
Phase 09 ✅ realizes D7 — tighten the name-origin colophon copy (drop the "livelihood" lede gloss, shorten both part glosses) AND add a pronunciation guide ("EE-kee-GEN-buh") at the foot of the colophon, in index.html + app.css; update the pinned assertions in index_test.go; covers R-DB17-ORIG, R-O7K1-XEN7
Phase 10 ✅ realizes D7, D9 — make "Sign in to access your services." the login `<h1>` and remove the "Your account's control plane" heading (index.html + index_test.go), AND remove the site-wide footer from every template (index login+landing, profile, pat_created) and delete its `.site-footer` CSS; covers R-DB18-KEEP, R-EFJZ-FRQ1
Phase 11 ✅ realizes D5 — replace the banner email text link with a monogram profile avatar (solid accent circle + uppercased first-letter initial) linking to `/profile`, furthest right after sign-out, full email carried as aria-label/title; add an `OwnerInitial` view field + `ownerInitial` helper in index.go, an `.avatar` CSS rule (remove dead `.identity .owner`), and update landing_composition_test.go; covers R-XO4W-LKAI
Phase 12 ✅ realizes D10 — shared banner chrome: make the signed-in wordmark a home link (`<a href="/" class="wordmark">`) on landing + profile (login page keeps `<p>`); rewrite the profile banner to sign-out + monogram avatar (drop the bordered Home button and `<span class="owner">` email text), add `OwnerInitial` to `profileData` via the `ownerInitial` helper; style the wordmark link (no underline, text color) and add `.identity .avatar:hover` darkening to `--accent-700`; update the profile/landing banner tests; covers R-VTIE-IUFA, R-VUQA-WM5Z, R-VVY7-ADWO
Phase 13 ⬜ realizes D5 — service list adopts the shared `.list`/`.row` chrome instead of the one-off `<table>` (name link left, MCP URL + copy button right, no header row) and each row's raw MCP URL gets a reused `.copy-btn`; remove both `.section-intro` paragraphs (connect-agent + services) from the signed-in landing; delete the `.services-table` CSS, add the `.service-row` copy-button/URL treatment, generalize the app.js `.copy-btn` source lookup to `.snippet, .service-row`; update landing_composition_test.go + connect_section_test.go; covers R-OF1Q-VEDC, R-OG9N-9641, R-OHHJ-MXUQ
