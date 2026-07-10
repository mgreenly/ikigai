# dropbox — Plan Status

This is the **manifest**: one line per phase in build order, and the **only**
place a phase's status marker lives. Each phase line begins with the literal word
`Phase` and carries `✅` (done) or `⬜` (not started). The build loop finds its
next unit of work with `grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`,
reads only that phase's `project/plan/phase-NN.md`, builds it, and on completion
flips that one marker. This file deliberately carries **no bare status glyph**
anywhere but on a phase line, so the anchored grep matches only phase lines.

Phase 01 ✅ realizes D1, D2, D3 — landing handler + embedded Carbon template/assets in `internal/web`, wired at `GET /{$}` (ungated in-process) in `cmd/dropbox/main.go`
Phase 02 ✅ realizes D4 — nginx fragment: add the exact-match session-gated `= /srv/dropbox/` location, validated by a content-assertion test
Phase 03 ✅ realizes D5 — purge the stale "no UI" line from `dropbox/CLAUDE.md` and state the landing-page truth (structural; docs-only)
Phase 04 ✅ realizes D6 — conform the landing page (markup + tokens) to the cron canonical template; per-service data only (eyebrow + description + title); structural content check
Phase 05 ✅ realizes D7 — add the top-left Home link to the dashboard landing page; covers R-HOME-6P8T
Phase 06 ✅ realizes D8 — self-serve the landing fonts and eliminate the FOUT: relativize the `tokens.css` link, `font-display: swap`→`optional` + self-served `src` in tokens.css, preload display+body fonts with `crossorigin`, and add the session-gated nginx `location /srv/dropbox/static/`; covers R-LQXL-095Q, R-LS5H-E0WF, R-LTDD-RSN4, R-LULA-5KDT, R-LVT6-JC4I
Phase 07 ✅ realizes D9 — adopt `registry`: add `require registry` + `replace registry => ../registry` to go.mod; resolve dropbox's own port via `registry.MustPort("dropbox")` (Spec.Port + the `DROPBOX_PORT` content-base default) and the `file.*` reflection example origin via `registry.BaseURL("dropbox")` at the composition root, env overrides unchanged; covers R-QKGB-OPNE, R-QJ8F-AXWP
Phase 08 ✅ realizes D10 — prove no `127.0.0.1:30xx` literal remains in dropbox's Go source (source-scan guard) and re-point the manifest/nginx tests at `registry` so a renumber fails a dropbox test; covers R-QLO8-2HE3, R-QMW4-G94S
Phase 09 ✅ realizes D11 — serve the web surface from `share/www` through the chassis: move landing.html + static assets, set `WWW: true`, render via `rt.WWW()`, delete `internal/web` (tests relocate to `cmd/dropbox` over the shipped tree), boundary-crossing `DROPBOX_WWW_PATH` export in `bin/start`; covers R-QO40-U0VH, R-QPBX-7SM6 (retained D1/D2/D3/D4/D7/D8/D10 ids re-proven on the new substrate)
Phase 10 ✅ realizes D12 — MCP surface over `appkit/mcp`: `internal/mcp` becomes `Instructions` + `Tools(svc)` (the `list`+`get` tools) + `NewHandler`, local transport and local health/reflection deleted, surface stays exactly four tools; covers R-QQJT-LKCV
Phase 11 ✅ realizes D13 — delete the `internal/db` `Open`/`Migrate` wrappers (migrations embed + load/outbox guards remain) and true up `dropbox/CLAUDE.md` to the converted service (structural)
Phase 12 ✅ realizes D14 — streaming byte I/O in the mirror (`WriteFrom`/`Open`, fixed copy buffer, hash on the write stream) and the `GET /content` read route over `http.ServeContent` (Range support); covers R-JV0A-6XDB, R-JW86-KP40, R-JXG2-YGUP
Phase 13 ✅ realizes D15 — first-class directories: additive migration `004_directories`, the `directories` store surface, and `Service.Stat`/`List` reporting dirs (empty-dir listable, recursive delete/move); covers R-JZVV-Q0C3, R-K13S-3S2S, R-K2BO-HJTH, R-K3JK-VBK6
Phase 14 ⬜ realizes D18 (slice: pull + reflection) — add the `origin` field to the file event payload, thread the sentinel `dropbox` through the download apply path, and surface `origin` in the reflection schema/example; covers R-KPHR-R6WO, R-KQPO-4YND
Phase 15 ⬜ realizes D17 (slice: queue) — additive migration `005_upload_queue` and the per-path coalescing queue store; covers R-KC2V-JPR1, R-KDAR-XHHQ
Phase 16 ⬜ realizes D17 (slice: client) — Dropbox write methods (`Upload` overwrite + `upload_session` >150 MiB, `CreateFolder`, `DeletePath`, `Move`), hermetic request-shape test + the `-tags live` real-Dropbox smoke; covers R-KJE9-UC77, R-KEIO-B98F, R-KFQK-P0Z4, R-KGYH-2SPT
Phase 17 ⬜ realizes D17 (slice: uploader) — the uploader worker: drain due rows, echo-suppress via the stored returned rev, exponential backoff + poison retention, health backlog counts; covers R-KKM6-83XW, R-KLU2-LVOL, R-KN1Y-ZNFA
Phase 18 ⬜ realizes D16 and D18 (slice: write origin) — `Service.Write`/`Mkdir`/`Delete`/`Move` over the streaming mirror + atomic index/event/enqueue seam, the loopback routes (`PUT`/`DELETE /content`, `POST /mkdir`, `POST /move`, `GET /stat`), path confinement, and the write event's `origin` = caller client id; covers R-K4RH-93AV, R-K5ZD-MV1K, R-K77A-0MS9, R-K8F6-EEIY, R-K9N2-S69N, R-KAUZ-5Y0C, R-KO9V-DF5Z
Phase 19 ⬜ realizes D19 — MCP write tools (`put`/`mkdir`/`delete`/`move`) over the same Service methods, capped 25 MiB base64 `put`, expanded eight-tool surface; covers R-KRXK-IQE2, R-KT5G-WI4R, R-KUDD-A9VG
Phase 20 ⬜ realizes D20 — ship the `dropbox/docs/` filesystem-API reference and the route-coverage guard that fails when a registered filesystem route is undocumented; covers R-KVL9-O1M5, R-KWT6-1TCU
