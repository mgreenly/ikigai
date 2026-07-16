# repos — Plan Status

This is the manifest: one line per phase in build order, and the **only** place
a phase's status marker lives. Each phase line is a Markdown bullet beginning
with `- Phase`, carrying `✅` (done) or `⬜` (not started). The build loop finds
its next work with `grep -nE '^- Phase .* ⬜' project/plan/STATUS.md | head -1`,
reads only that phase's `project/plan/phase-NN.md`, and on completion flips
that one marker. This file deliberately carries **no bare status glyph**
outside the phase lines, so the anchored grep matches only phase lines.

- Phase 01 ✅ realizes R-EMGN-7X72, R-ENOJ-LOXR, R-EOWF-ZGOG — module scaffold & data model (migrations + Store)
- Phase 02 ✅ realizes R-EXFQ-NUVB, R-EYNN-1MM0, R-EZVJ-FECP, R-F13F-T63E, R-F3J8-KPKS — repo lifecycle & git custody (Git wrapper, token injection, EnsureRepo/CloneRepo)
- Phase 03 ✅ realizes R-F4R4-YHBH, R-F5Z1-C926, R-F76X-Q0SV, R-F8EU-3SJK, R-F9MQ-HKA9, R-FAUM-VC0Y, R-FC2J-93RN — session engine (worktree-per-session runner, confined toolset, queue/cap/TTL, model config)
- Phase 04 ✅ realizes R-FDAF-MVIC, R-FEIC-0N91, R-FFQ8-EEZQ, R-FGY4-S6QF, R-FI61-5YH4, R-FKLT-XHYI, R-FLTQ-B9P7 — issue protocol (github peer client, label lifecycle, .ikibot/check gate, PR creation)
- Phase 05 ✅ realizes R-EQ4C-D8F5, R-ERC8-R05U, R-ESK5-4RWJ, R-ETS1-IJN8, R-EUZX-WBDX, R-EW7U-A34M — GitHub-fact intake (webhooks consumer, dispatch table, loop suppression, double-trigger guard)
- Phase 06 ✅ realizes R-FT54-LW5D, R-FUD0-ZNW2, R-FVKX-DFMR, R-FWST-R7DG, R-FY0Q-4Z45, R-G0GI-WILJ — outcome events & state retention (producer families, worktree pruning + sweeps)
- Phase 07 ✅ realizes R-FN1M-P1FW, R-FO9J-2T6L, R-FPHF-GKXA, R-FQPB-UCNZ, R-FRX8-84EO — MCP tool surface (nine structured verbs)
- Phase 08 ✅ realizes R-EISY-2LYZ, R-EL8Q-U5GD — composition root & chassis boot (Spec, manifest, wiring)
- Phase 09 ✅ realizes R-G1OF-AAC8, R-G2WB-O22X, R-G448-1TTM — nginx fragment & canonical landing page
- Phase 10 ✅ realizes R-TY2R-GFRU, R-TZAN-U7IJ — consumer offset store (feed_offset migration byte-identical to consumer.SchemaSQL + drift guard + live consumer-engine boot proof)
- Phase 11 ✅ realizes R-C9CO-ODYU — absolute state root (ResolveStateRoot at composition root → cwd-independent worktree create/inspect/push)
- Phase 12 ⬜ realizes R-2U0F-NNXH — route webhook intake through the Enqueuer seam (single session-construction point: LogPath + doorbell + branch naming set only in Enqueue)
- Phase 13 ⬜ realizes R-2V8C-1FO6 — unmask complete() so an errored run reports its real reason instead of "no commits produced"
