# cron — cleanup findings

## High-priority (named migrations)
- ✅ **DONE 2026-07-03** — cron/etc/nginx.conf:12 — Comment "(When the service registry lands cron's port becomes 3200; update the literal below then.)" is stale: the registry has landed (top-level `registry/`) and keeps cron at **3005** (registry/project/design/D02.md:35, registry/project/plan/phase-02.md:16). The predicted 3200 renumber never happened; the note now misleads.
- cron/project/design/D04.md:64-65 — Same registry-migration assumption baked into prose: describes 3005 as "cron's fixed registry port" and frames the literal as pending a future registry substitution. Registry is now source of truth and confirms 3005, so the "when registry lands" framing is superseded (matches the stale nginx.conf note).

## Other stale info
- ✅ **DONE 2026-07-03** — cron/project/design/design.md:57-58 — Fixed verb set listed as `serve`/`version`/`manifest`/`migrate`/`backup`/`restore`. This is superseded: CLAUDE.md states the fixed verbs are `serve`/`version`/`manifest`/`migrate`/`schema`, and "Backup and restore are NOT binary verbs — they are box-level operations owned by opsctl." Doc omits `schema` and wrongly lists backup/restore. (superseded verb set)
- cron/project/README.md:9-15 — "**Status: scaffold.** ... cron has **no spec and no live build loop yet** — the spine documents below are empty placeholders." Contradicts reality: product.md/design.md (+ D01–D07) are full spine docs, loops/ prompts exist, and the service is fully implemented under cron/cmd and cron/internal. (contradictory status statement)

## Notes
- cron/project/README.md:27 references a `notes/` folder that does not exist on disk (only bugs/, requests/, research/, product/, design/, loops/, plan/). Harmless "catch-all" description, but a dead path. (low confidence / minor)
- Deploy-format (tar.gz / versioned-slot / three-symlink swap) is CURRENT throughout: nginx.conf:5-6 and main_test.go:104-135 use `etc/current`/`share/current`/`bin/run -> libexec/cron-<version>` symlinks. No flat-bin deploy language found — clean.
