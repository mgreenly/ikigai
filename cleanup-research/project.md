# project — cleanup findings

## High-priority (named migrations)
- project/product/README.md:116-117 — Declares "**No designed-but-unbuilt service registry.** The per-service loopback port stays a configured value; collapsing it to a name-only lookup is separate, future work." This is now superseded: the top-level `registry/` module exists and is actively being designed+built (registry/project/ has populated product/design/plan) as exactly the authoritative `name → port` table this note calls out-of-scope/future. A future agent reading this would wrongly conclude no registry exists. (registry migration) — ✅ **DONE** (non-goal bullet deleted; registry is built/adopted/deployed, so the disclaimer is overtaken by events)

## Other stale info
- none

## Notes
- The tar.gz deploy-format migration is the *subject* of this project folder, not a stale reference in it. design/D02, D04, D08 and product/README.md's Problem section correctly describe the old bare-binary / `releases/`+`current` model as the superseded "before" and the versioned `tar.gz` bundle + three-symlink swap as current — consistent with root `deploy.md`. Not stale.
- Verb set within project/ is uniformly `serve/version/migrate/schema` with the `manifest` verb explicitly dropped (D07/D11) — current and internally consistent. (Root CLAUDE.md still lists the old `manifest` verb, but that file is out of scope for this folder.)
- design/README.md:8-9 says design "uses the suite's contractual constants (service names, …) by value but does not own them" — accurate and does not contradict registry-as-source-of-truth (it disclaims ownership rather than asserting it), so not flagged.
- D11:15 references `bin/registry` as a live manifest reader; `bin/registry` exists — not a dead path.
- plan/ was skipped per scope rules.
