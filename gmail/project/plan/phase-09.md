# Phase 9 — Composition-root normalization: the `appkit.Spec` inline in `cmd/gmail/main.go`

*Realizes design Decision 13 (structural). Depends on phases 7–8 (the registry
adoption already edited the `Port` field in place); sequenced **before** the WWW
(phase 10) and MCP (phase 11) conversions so `main.go` is the single settled home
those phases edit. **Read D13 for the exact shape and rationale.***

gmail is the only converted-shape service that declares its `appkit.Spec` in a
separate `internal/gmailapp` package. This phase moves the Spec to the composition
root, matching crm (inline) and notify (`notifySpec()`), and deletes the
indirection. It is a **pure relocation** — no field changes, no behavior change.

Observable end state:

- `cmd/gmail/main.go` declares `func gmailSpec() appkit.Spec` (the former
  `gmailapp.Spec` body, verbatim: the shared `var engine *gm.Engine` stays declared
  inside `gmailSpec` and is captured by the `Handlers`/`Producer`/`Workers`
  closures, preserving appkit's `Handlers → Producer → Workers` call order), and
  `main()` is `appkit.Main(gmailSpec())`.
- `internal/gmailapp/` no longer exists.
- `cmd/gmail/main_test.go` calls the local `gmailSpec()` wherever it called
  `gmailapp.Spec()` and no longer imports `gmail/internal/gmailapp`; **no test
  assertion changes** — the manifest byte-equality and boot-health tests read the
  same Spec fields from the same values.

**Done when:** the suite is green — `cd gmail && go build ./...`,
`cd gmail && go vet ./...`, `cd gmail && gofmt -l .` (no output),
`cd gmail && go test ./...`, and `bin/check-migrations gmail` — and:

- `grep -n "func gmailSpec()" gmail/cmd/gmail/main.go` matches and
  `grep -n "appkit.Main(gmailSpec())" gmail/cmd/gmail/main.go` matches;
- `ls gmail/internal/gmailapp 2>/dev/null` reports no such directory, and
  `grep -rn "internal/gmailapp" gmail --include=*.go` returns no matches;
- the manifest byte-equality test still passes with no assertion change (only the
  Spec constructor it calls changed from `gmailapp.Spec()` to `gmailSpec()`).
