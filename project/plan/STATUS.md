# Suite on-box layout, versioning & backup/restore — Plan Status

This is the manifest: one line per phase in build order, and the **only** place a
phase's status marker lives. Each phase line begins with the literal word `Phase`
and its zero-padded number, then a status marker — `✅` (done) or `⬜` (not
started) — then `realizes <Decision ids>` (or `realizes —` for a pure structural
phase), then `— <objective>`. The build loop finds its next work with
`grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1`, reads only that phase's
`project/plan/phase-NN.md`, and on completion flips that one marker here. This file
deliberately carries **no** bare status glyph outside these phase lines, so the
anchored grep matches only phase lines.

Phase 01  ✅  realizes D3                  — SemVer 2.0 version identity & ordering (opsctl)
Phase 02  ✅  realizes D4                  — bump/ship emit v-prefixed SemVer + convert VERSION files
Phase 03  ⬜  realizes —                   — Layout path scheme for /opt/<svc>/ (structural; D1 paths)
Phase 04  ⬜  realizes D1                  — setup materializes the install tree (perms, web group, nginx)
Phase 05  ⬜  realizes D2                  — libexec/ + bin/run symlink swap for deploy/rollback/prune
Phase 06  ⬜  realizes D5, D6              — appkit state/cache config boundary + boot reconstruction
Phase 07  ⬜  realizes D6                  — eventplane epoch re-mint by exclusion
Phase 08  ⬜  realizes D7, D5              — opsctl backup/restore core + ObjectStore seam + S3 round-trip
Phase 08a ⬜  realizes D7                  — apex TLS cert backup/restore stream (dashboard)
Phase 09  ⬜  realizes D9                  — scheduled nightly box sweep (systemd timer + backup --all)
Phase 10  ⬜  realizes D8                  — adopt dashboard into the new layout
Phase 11  ⬜  realizes D8                  — adopt crm into the new layout
Phase 12  ⬜  realizes D8                  — adopt ledger into the new layout
Phase 13  ⬜  realizes D8                  — adopt notify into the new layout
Phase 14  ⬜  realizes D8                  — adopt dropbox into the new layout
Phase 15  ⬜  realizes D8                  — adopt prompts into the new layout (classify sandboxes/, runs/)
Phase 16  ⬜  realizes D8                  — adopt wiki into the new layout
Phase 17  ⬜  realizes D8                  — adopt cron into the new layout
Phase 18  ⬜  realizes D8                  — adopt gmail into the new layout
Phase 19  ⬜  realizes D8                  — adopt scripts into the new layout (classify runs/)
Phase 20  ⬜  realizes D8                  — adopt sites into the new layout (relocate www/, repoint nginx)
Phase 21  ⬜  realizes D8                  — adopt webhooks into the new layout
Phase 22  ⬜  realizes D8                  — live-box one-time converter (idempotent, non-destructive)
