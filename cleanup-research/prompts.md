# prompts — cleanup findings

## High-priority (named migrations)

### Service names / registry
- ✅ **DONE 2026-07-03** — prompts/etc/nginx.conf:12-13 — Stale forward-looking note: "When the service registry lands prompts's port becomes 3101; update the literal below then." The registry has landed; prompts stays 3002 (main.go:102 `Port: 3002`, D10:115 calls 3002 "prompts's fixed registry port"), and 3101 is now **ledger's** port (main.go:68). The prediction is both obsolete and wrong. Frames the registry as future work rather than the current source of truth.

### Deploy format → tar.gz
- prompts/Makefile:2-4 — Stale deploy model: "The bin/ scripts are the production deploy spine (setup/deploy/start/stop on the box)." Per root deploy.md the deploy spine is now `bin/bump` → `bin/ship` (tar.gz bundle) → `opsctl stage` (versioned slots) → `opsctl deploy` (three-symlink swap). No `bin/setup` or `bin/deploy` exists in prompts/bin (only secrets/start/stop/teardown); those operations moved to opsctl.
- prompts/bin/teardown:1,9,65-66 — Describes itself as "the inverse of bin/setup" (no bin/setup exists) and, in preserve mode, removes the **flat** `/opt/prompts/bin` + `/opt/prompts/etc` layout. The current tar.gz/opsctl model uses versioned release slots behind a `current` symlink (nginx.conf:5 itself references `/opt/prompts/etc/current/nginx.conf`), not a single flat `/opt/prompts/bin`. The ssh+systemd+nginx teardown mechanics look pre-opsctl. (See Notes — some uncertainty on whether teardown was fully superseded.)

## Other stale info
- prompts/cmd/prompts/main.go:20-23 — Package-doc comment is self-contradictory and stale: "It is neither an event-plane producer nor a consumer — no /feed, no consumer loop, no background worker." The actual `appkit.Spec` in the same file wires `Feed: "/feed"` (119), `Consumes`/`Subscriptions` (105-111), `Producer` (125), and `Workers` (145). CLAUDE.md, product.md, and D10 all describe prompts as a producer **and** consumer with a `/feed`. (contradictory architecture statement)
- prompts/cmd/prompts/main.go:22 — Same comment: "Its only secret, ANTHROPIC_API_KEY, is read env-only…". prompts now supports four providers (anthropic/openai/google/zai) and reads four provider keys — product.md:23,35-39, D03, and bin/secrets:12-18 all seed/validate ANTHROPIC/OPENAI/GEMINI/ZAI keys. (superseded feature / stale constant)
- ✅ **DONE 2026-07-03** — prompts/bin/start:20 — Calls `bin/build`, which no longer exists (only secrets/start/stop/teardown in bin/); main.go:16 explicitly notes the "deleted bin/build run-wrapper". Dead script reference — the `if [[ ! -x "$BIN" ]]; then bin/build; fi` branch would fail. (dead path)

## Notes
- bin/secrets:128 ("Apply with: bin/ship (then opsctl stage/deploy)…") is CURRENT and correct — not flagged.
- Design/product docs consistently use port 3002 for prompts and match the current 4-provider + landing-page architecture; no stale ports or naming found there.
- The `agent` → `prompts` service rename (commits 7fdaee0/17aac2b) is clean — no lingering old `agent` service-name references found (only legitimate `agentkit`/generic "agent" usages remain).
- bin/teardown: flagged for the flat `/opt/prompts/bin|/etc` layout, but I'm not fully certain teardown-as-a-concept was retired vs. just needing its on-box paths updated for versioned slots — verify against opsctl's provisioning verbs before editing.
