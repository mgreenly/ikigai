# appkit — Plan Status (manifest read-path through `current`)

This is the **manifest**: one line per phase in build order, and the **only** place
a phase's status marker lives. Each phase line begins with the literal word `Phase`
and carries `✅` (done) or `⬜` (not started). The build loop finds its next unit of
work with `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`, reads only
that phase's `project/plan/phase-NN.md`, builds it, and on completion flips that one
marker. This file deliberately carries **no bare status glyph** anywhere but on a
phase line.

Phase 01 ✅ realizes D1 — `appkit/inventory.Read` globs `<root>/*/etc/current/manifest.env` (reads through the `current` deploy symlink, not the sibling); update inventory_test.go and add a symlink-follow test; covers R-YO06-9I18, R-YP82-N9RX
Phase 02 ✅ realizes D2 — `bin/registry` reads `<root>/<name>/etc/current/manifest.env` (single lookup + list loop); update bin/registry.test.sh; covers R-YQFZ-11IM
Phase 03 ✅ realizes D3 — `bin/start` stages a prod-shaped runtime manifest root under `tmp/opt/<svc>/etc/<ver>/manifest.env` + `current` symlink and points `DASHBOARD_MANIFEST_ROOT`/`REGISTRY_ROOT` at it; live `/services` smoke lists all services incl crm; covers R-YRNV-ET9B
Phase 04 ⬜ realizes D4 (in-repo part) — retire the sibling path: drop all `etc/manifest.env` (sibling) reads/refs across `appkit/inventory`, `bin/registry`, `bin/start`, fix the stale `ManifestPath` comment in `opsctl/internal/opsctl/layout.go`, add the no-`current`→not-listed guard test; covers R-YSVR-SL00. (D4's live-box proof is an operator step in `plan.md` § Operator steps, deliberately not a phase here so the loop converges.)
