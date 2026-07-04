# ledger — cleanup findings

## High-priority (named migrations)
- none — the AGENTS.md "Manifest / deploy" section (ledger/AGENTS.md:119-131) already describes the CURRENT tar.gz model (`bin/ship` → `opsctl stage` → `opsctl deploy`, versioned release dir + atomic swap + rollback); no flat-bin / single-binary-copy language survives. No old service-naming-scheme hardcoding found. See Notes for a registry-adjacent nginx.conf comment.

## Other stale info
- ✅ **DONE 2026-07-03** — ledger/AGENTS.md:14 — cites `project/notes/PLAN.md §1–2`; that path does not exist. The design lives in `project/design/` (design.md, D01–D08). (dead file path)
- ✅ **DONE 2026-07-03** — ledger/AGENTS.md:21 — points reader to `project/notes/PLAN.md` as "the ledger design"; dead path (see above). (dead file path)
- ✅ **DONE 2026-07-03** — ledger/AGENTS.md:51 — cites `project/notes/PLAN.md §2–4`; dead path. (dead file path)
- ✅ **DONE 2026-07-03** — ledger/AGENTS.md:108 — cites `project/notes/PLAN.md §6`; dead path. (dead file path)
- ledger/AGENTS.md:87 — describes an `internal/server` package (routing, requireIdentityHeaders gate, /feed handler, security headers); no such package exists. Actual packages are db/ids/ledger/mcp/web; routing/gating now lives in the appkit chassis. (dead/renamed package)
- ledger/AGENTS.md:95 — lists `internal/logging, internal/ids` as local packages; `internal/logging` does not exist (only `internal/ids` remains; logging is from the chassis). (dead package ref)
- ledger/project/design/design.md:126 — lists `internal/server` among the "existing" packages; the package is now `internal/web` (server routing moved to the chassis). (dead/renamed package)
- ✅ **DONE 2026-07-03** — ledger/project/design/design.md:18 — references `ledger/project/notes/PLAN.md`; dead path. (dead file path)
- ✅ **DONE 2026-07-03** — ledger/project/product/product.md:14 — references `ledger/project/notes/PLAN.md` as owner of the domain surface; dead path. (dead file path)

## Notes
- Registry migration: registry/ now exists (currently design/plan-phase only; not yet a built service). ledger/etc/nginx.conf:12-13 carries a forward-looking comment "When the service registry lands ledger's port becomes 3101; update the literal below then" — port is already 3101 and registry now exists, so the note is confusingly worded but not wrong. ledger/etc/manifest.env still declares PORT=3101 and AGENTS.md still says "the binary owns its own identity"; registry has not yet taken over identity/inventory at build time, so I did NOT flag these as stale. Low confidence — worth a second look once registry is built.
- design.md/product.md are the landing-page work's design/product docs; the `internal/server` name there may reflect the package name at authoring time (later renamed to internal/web). Still currently misleading, hence flagged.
