# appkit — Plan Status

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
Phase 04 ✅ realizes D4 (in-repo part) — retire the sibling path: drop all `etc/manifest.env` (sibling) reads/refs across `appkit/inventory`, `bin/registry`, `bin/start`, fix the stale `ManifestPath` comment in `opsctl/internal/opsctl/layout.go`, add the no-`current`→not-listed guard test; covers R-YSVR-SL00. (D4's live-box proof is an operator step in `plan.md` § Operator steps, deliberately not a phase here so the loop converges.)
Phase 05 ✅ realizes D5 — `appkit/config` WWW-root resolution: `Config.WWWPath` composed `<root>/<app>/share/current/www` on box, `./share/www` dev, `<APP>_WWW_PATH` override wins; covers R-LWOU-OWWQ, R-LXWR-2ONF, R-LZ4N-GGE4
Phase 06 ✅ realizes D6 — new `appkit/web` package: `Load`/`Render`/`Static` over an on-disk root (parse-once templates, disk-read static, woff2 mime, no autoindex); covers R-M0CJ-U84T, R-M1KG-7ZVI, R-M2SC-LRM7, R-M408-ZJCW, R-M585-DB3L
Phase 07 ✅ realizes D7 — chassis integration: `Spec.WWW`, serve-time `web.Load` fail-loudly, auto-mounted `GET /static/`, `Router.WWW()`, strict additivity for non-WWW specs; covers R-M7NY-4UKZ, R-M8VU-IMBO, R-MA3Q-WE2D, R-MBBN-A5T2
Phase 08 ✅ realizes D8 — new `appkit/mcp` package: JSON-RPC transport over a declared tool table (initialize/tools-list/tools-call, identity threading, error taxonomy, duplicate/reserved-name rejection, result helpers); covers R-MCJJ-NXJR, R-MDRG-1PAG, R-MEZC-FH15, R-MG78-T8RU, R-MHF5-70IJ, R-MIN1-KS98, R-MJUX-YJZX
Phase 09 ✅ realizes D9 — standard `health` + `reflection` tools auto-registered by `appkit/mcp.New` from Options (Envelope health, registry/subscriptions reflection with `event_type` detail + unknown-type error); covers R-ML2U-CBQM, R-MMAQ-Q3HB, R-MNIN-3V80, R-MOQJ-HMYP
Phase 10 ✅ realizes D10 — chassis-owned consumer loops: `Spec.Consumers` table (Source/Subscriptions/Handler factory), `appkit/config` feed-URL+From resolution over `registry` defaults, CONSUMES=/reflection derivation, legacy-field conflict rejection, per-entry `consumer.Run` on the serve context with `ConsumerID = Spec.App`; covers R-4199-A0U9, R-42H5-NSKY, R-44WY-FC2C, R-464U-T3T1, R-47CR-6VJQ, R-48KN-KNAF, R-49SJ-YF14
Phase 11 ✅ realizes D11 + D9 (revised) — event-routing conformance: family-based `Spec.Events` registry, kind-keyed `reflection` (index `{kind, subject, description}`, `kind` detail, unknown-kind error), feed pass-through gating by kind, fixtures cut over to kind/subject envelopes; ⛔ externally ordered after eventplane plan phases 01–04 and before every service's conformance phase; covers R-7EK6-8030, R-7FS2-LRTP, R-7GZY-ZJKE, R-7I7V-DBB3, R-7JFR-R31S, R-7LVK-IMJ6
