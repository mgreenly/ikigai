# wiki — cleanup findings

## High-priority (named migrations)
- ✅ **DONE 2026-07-03** — wiki/etc/nginx.conf:13 — Comment: "(When the service registry lands wiki's port becomes 3100; update the literal below then.)". The registry has landed and assigns wiki **3001** (Core block); 3100 is **crm** (Apps block) per registry/project/design/D02.md. The comment's premise is stale and contradicts registry-as-source-of-truth — a future agent could wrongly re-point wiki to 3100. The port literals (3001) are correct; only the comment is wrong.

## Other stale info
- ✅ **DONE 2026-07-03** — wiki/project/research/subject-dedup-research.md:204 — States the appkit dispatcher verb set is `serve|version|manifest|migrate|schema|backup|restore` and cites `appkit/appkit.go:244-260`. Actual dispatcher (appkit/appkit.go ~218-224) is `serve/version/manifest/migrate/schema` with **no** backup/restore verbs; per repo CLAUDE.md backup/restore are box-level opsctl operations, not binary verbs. Both the verb set and the line cite (244-260 is now unrelated code) are stale/contradictory. (superseded verb set / dead line cite)

## Notes
- Numerous "registry" mentions in wiki/project/design/ (D03, D06, D10, D16, D28, research.md) refer to wiki's **internal subjects registry** (a knowledge-base concept), NOT the suite service `registry/` folder — NOT stale, do not flag.
- D10.md frames an "eight-verb" then "thirteen/fifteen-verb" MCP surface; this is properly documented as superseded in-place (D16/D27) and is self-consistent — not stale.
- etc/manifest.env still emits `MODEL_ID=claude-sonnet-4-6` (matched by wiki/internal/wiki/wiki.go:40) while D19 introduces per-site `<PREFIX>_MODEL` knobs "replacing MODEL_ID". Code shows both coexist (MODEL_ID manifest key + per-site resolveCallSite), so this is not clearly stale — left unflagged, but worth an eye if MODEL_ID was meant to be fully removed.
