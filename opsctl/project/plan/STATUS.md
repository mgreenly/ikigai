# opsctl — Plan Status

This is the manifest: one line per phase in build order, and the **only** place a
phase's status marker lives. Each phase line is a Markdown bullet beginning with
`- Phase`, carrying `✅` (done) or `⬜` (not started). The build loop finds its
next work with `grep -nE '^- Phase .* ⬜' project/plan/STATUS.md | head -1`, reads
only that phase's `project/plan/phase-NN.md`, and on completion flips that one
marker. This file deliberately carries **no bare status glyph**, so the anchored
grep matches only phase lines.

- Phase 01 ✅ realizes R-WP3M-PO1V, R-WQBJ-3FSK — Restore recreates `cache/` owned by the service user
- Phase 02 ✅ realizes R-65MT-7QEK — Stage unpacks on the OPSCTL_ROOT filesystem (no cross-device rename)
- Phase 03 ✅ realizes R-6AIE-QTDC, R-6BQB-4L41, R-6CY7-ICUQ — opsctl loads the box env file at startup
- Phase 04 ✅ realizes R-MSOP-5MDA, R-MTWL-JE3Z, R-MV4H-X5UO, R-MXKA-OPC2 — deploy renders + installs the apex block for the DEFAULT app (R-MYS7-2H2R live-box, operator-verified out-of-loop)
- Phase 05 ✅ realizes R-CIUC-KW66, R-CK28-YNWV, R-CLA5-CFNK, R-CMI1-Q7E9 — setup provisions the DEFAULT app without a locations fragment
- Phase 06 ✅ realizes R-CNPY-3Z4Y (re-realizes R-MSOP-5MDA, R-MTWL-JE3Z) — fix: deploy reads the apex domain from the environment, not the manifest
- Phase 07 ✅ realizes R-AQMT-9M04 (R-ARUP-NDQT live-box, operator-verified out-of-loop) — init-box creates the `web` group and adds nginx to it
- Phase 08 ✅ realizes R-AT2M-15HI, R-AUAI-EX87 — setup provisions the served `www` tree as `<app>:web`, setgid, via `ensureWWWPerms`
- Phase 09 ✅ realizes R-AVIE-SOYW, R-AWQB-6GPL (R-AXY7-K8GA live-box, operator-verified out-of-loop) — deploy re-asserts the served-tree `web` invariant after the state chown
- Phase 10 ✅ realizes R-AZ63-Y06Z (R-B0E0-BRXO live-box, operator-verified out-of-loop) — restore re-asserts the served-tree `web` invariant after replacing state
- Phase 11 ✅ realizes R-QFXB-VARQ, R-QEPF-HJ11 (retires R-AT2M-15HI) — retire the `working/` segment from opsctl's served-tree model
- Phase 12 ✅ realizes R-3K9X-IPJZ, R-3MPQ-A91D, R-3NXM-O0S2 (retires R-AQMT-9M04, R-ARUP-NDQT, R-QEPF-HJ11, R-AVIE-SOYW, R-AWQB-6GPL, R-AZ63-Y06Z) — replace the web-group served-tree model with service-user ownership across setup/deploy/restore
- Phase 13 ✅ realizes R-3LHT-WHAO — remove the orphaned served-tree seams (`Chmod`/`EnsureSystemGroup`/`AddUserToGroup`) and the dead `stateWWWFragment`
- Phase 14 ⬜ realizes R-WHC0-I9HL (R-WIJW-W18A live-box, operator-verified out-of-loop) — init-box installs the box-baseline PDF tooling (poppler-utils)
