# Phase 09 — Adopt `registry`: resolve cron's port by name and guard against drift

*Realizes design Decision 11. Depends on Phase 06 for the inline `Port` field and
on Phase 07 for the nginx-fragment tests already relocated to `cmd/cron` (this
phase de-literalizes their `proxy_pass` assertions). Depends on the repo-root
`registry` module and `go.work use ./registry` (out of scope — assumed satisfied;
`registry.go` carries `{"cron", 3005, Core}`).*

Observable end state:

- `cron/go.mod` carries `require registry v0.0.0` and the committed
  `replace registry => ../registry` (mirroring the `appkit`/`eventplane`
  replace-siblings).
- `cronSpec()` in `cmd/cron/main.go` sets `Port: registry.MustPort("cron")` — no
  `3005` integer literal remains in the Spec.
- The nginx-fragment tests (in `cmd/cron` since Phase 07) build their expected
  `proxy_pass` origins from `registry.BaseURL("cron")` (e.g. `"proxy_pass " +
  registry.BaseURL("cron") + "/;"` and the `/static/` variant) instead of the bare
  `http://127.0.0.1:3005/` literal; their behavioral assertions (the session gate,
  the exact-match vs prefix distinction) are otherwise unchanged.
- The manifest byte-equality test emits from `cronSpec().Port`
  (`registry.MustPort("cron")`) and still byte-matches the committed
  `etc/manifest.env` (`PORT=3005`).
- A source-scan guard test in `cmd/cron` walks every `*.go` under the `cron/`
  module and fails on any bare `127.0.0.1:30xx` loopback-address literal (needle
  assembled at runtime, skipping its own file).
- `etc/nginx.conf` and `etc/manifest.env` themselves are **not** edited — their
  literals stay; the tests now police them.

**Done when:** the suite is green — `cd cron && go build ./...`,
`cd cron && go vet ./...`, `cd cron && gofmt -l .` (no output),
`cd cron && go test ./...`, and `bin/check-migrations cron` all succeed with zero
failures — and:

- R-LTAF-KVJU, R-LUIB-YNAJ, R-LVQ8-CF18 (D11) are covered by clearly-named tests;
- `grep -n "require registry" cron/go.mod` and
  `grep -n "replace registry => ../registry" cron/go.mod` both match;
- `grep -n "registry.MustPort(\"cron\")" cron/cmd/cron/main.go` matches, and
  `grep -n "Port:[[:space:]]*3005" cron/cmd/cron/main.go` returns no matches;
- the source-scan guard is green:
  `grep -rnE "127\.0\.0\.1:30[0-9]{2}" cron --include=*.go | grep -v project/`
  returns no matches (the guard test assembles its needle at runtime, so it does
  not count itself).
