# Suite on-box layout, versioning & backup/restore — Plan Status

This is the manifest: one line per phase in build order, and the **only** place a
phase's status marker lives. Each phase line is a Markdown bullet beginning with
the literal `- Phase` and its zero-padded number, then a status marker — `✅`
(done) or `⬜` (not started) — then `realizes <Decision ids>` (or `realizes —`
for a pure structural phase), then `— <objective>`. The build loop finds its next
work with `grep -nE '^- Phase .* ⬜' project/plan/STATUS.md | head -1`, reads only
that phase's `project/plan/phase-NN.md`, and on completion flips that one marker
here. This file deliberately carries **no** bare status glyph outside these phase
lines, so the anchored grep matches only phase lines.

- Phase 01  ✅  realizes D3        — SemVer 2.0 version identity & ordering (opsctl)
- Phase 02  ✅  realizes D4        — bump/ship emit v-prefixed SemVer + convert VERSION files
- Phase 03  ✅  realizes —         — Layout path scheme for /opt/<svc>/ (structural; D1 paths)
- Phase 04  ✅  realizes D1        — setup materializes the install tree (perms, web group, nginx)
- Phase 05  ✅  realizes D2        — libexec/ + bin/run symlink swap for deploy/rollback/prune
- Phase 06  ✅  realizes D5, D6    — appkit state/cache config boundary + boot reconstruction
- Phase 07  ✅  realizes D6        — eventplane epoch re-mint by exclusion
- Phase 08  ✅  realizes D7, D5    — opsctl backup/restore core + ObjectStore seam + S3 round-trip
- Phase 08a ✅  realizes D7        — apex TLS cert backup/restore stream (dashboard)
- Phase 09  ✅  realizes D9        — scheduled nightly box sweep (systemd timer + backup --all)
- Phase 10  ✅  realizes D8        — adopt dashboard into the new layout
- Phase 11  ✅  realizes D8        — adopt crm into the new layout
- Phase 12  ✅  realizes D8        — adopt ledger into the new layout
- Phase 13  ✅  realizes D8        — adopt notify into the new layout
- Phase 14  ✅  realizes D8        — adopt dropbox into the new layout
- Phase 15  ✅  realizes D8        — adopt prompts into the new layout (classify sandboxes/, runs/)
- Phase 16  ✅  realizes D8        — adopt wiki into the new layout
- Phase 17  ✅  realizes D8        — adopt cron into the new layout
- Phase 18  ✅  realizes D8        — adopt gmail into the new layout
- Phase 19  ✅  realizes D8        — adopt scripts into the new layout (classify runs/)
- Phase 20  ✅  realizes D8        — adopt sites into the new layout (relocate www/, repoint nginx)
- Phase 21  ✅  realizes D8        — adopt webhooks into the new layout
- Phase 22  ✅  realizes D8        — live-box one-time converter (idempotent, non-destructive)
- Phase 23  ✅  realizes D11, D5   — appkit composeDataPaths + IKIGENBA_ROOT guard (additive)
- Phase 24  ✅  realizes —         — opsctl Layout: etc/<v>·share/<v>·three-symlink accessors (structural)
- Phase 25  ✅  realizes D1        — opsctl setup materializes the new install tree
- Phase 26  ✅  realizes D10, D2   — opsctl stage: unpack the versioned bundle (SHA guard)
- Phase 27  ✅  realizes D7        — opsctl backup/restore deltas (version-embedding key, no pre-restore)
- Phase 28  ✅  realizes D2        — opsctl prune: delete each old version as a complete set
- Phase 29  ✅  realizes D10, D2   — opsctl deploy: three-symlink swap, unconditional backup, no manifest regen
- Phase 30  ✅  realizes D10, D2   — opsctl rollback: recover by S3 backup recency (-N)
- Phase 31  ⬜  realizes D11, D7   — appkit verb-set reduction (drop manifest/backup/restore)
- Phase 32  ⬜  realizes D4        — ship produces the versioned release bundle
- Phase 33  ⬜  realizes D8, D11   — adopt dashboard into the new layout
- Phase 34  ⬜  realizes D8, D11   — adopt crm into the new layout
- Phase 35  ⬜  realizes D8, D11   — adopt ledger into the new layout
- Phase 36  ⬜  realizes D8, D11   — adopt notify into the new layout
- Phase 37  ⬜  realizes D8, D11   — adopt dropbox into the new layout
- Phase 38  ⬜  realizes D8, D11   — adopt prompts into the new layout (classify sandboxes/, runs/)
- Phase 39  ⬜  realizes D8, D11   — adopt wiki into the new layout
- Phase 40  ⬜  realizes D8, D11   — adopt cron into the new layout
- Phase 41  ⬜  realizes D8, D11   — adopt gmail into the new layout
- Phase 42  ⬜  realizes D8, D11   — adopt scripts into the new layout (classify runs/)
- Phase 43  ⬜  realizes D8, D11   — adopt sites into the new layout (relocate www/, repoint nginx)
- Phase 44  ⬜  realizes D8, D11   — adopt webhooks into the new layout
- Phase 45  ⬜  realizes D8        — live-box one-time converter delta
- Phase 46  ⬜  realizes —         — rewrite deploy.md for the new model (docs)
