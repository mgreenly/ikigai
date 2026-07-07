# Phase 7 — Adopt `registry` at the composition root

*Realizes design Decision 11 (adopt `registry`; resolve gmail's own loopback port
by name at startup). Depends on the existing gmail `appkit.Spec` (currently
declared in `internal/gmailapp/spec.go` — this phase edits the `Port` field in
place, wherever the Spec lives; the later normalization phase moves the whole
Spec). Covers `R-9QEG-KF05`. **Read D11 for the exact call site and rationale.***

gmail stops hardcoding its loopback port literal and references itself **by name**
through the shared `registry` library, resolving **once at the composition root**.
This is behavior-preserving: `registry` already carries gmail's current value
(`gmail=3202`), so the resolved value is byte-identical to the literal it
replaces. gmail is a **producer, not a consumer** — there are no peer feed URLs to
route through `registry`; the adoption is exactly one call, gmail's own port.

**External precondition (assume satisfied; do NOT build it here).** The repo-root
`go.work` carries `use ./registry` and the `registry` module exists and is green.
Both are owned outside `gmail/`. No step in this phase edits `../go.work`,
`../registry/`, or any sibling module — the executor runs from `gmail/` and cannot
reach outside it.

**What gets changed (all inside `gmail/`):**

- **`gmail/go.mod`** — add `require registry v0.0.0` and a committed
  `replace registry => ../registry`, mirroring the existing `appkit` /
  `eventplane` in-repo replace-siblings. This is the only build-graph change.
- **The gmail `appkit.Spec`** (in `internal/gmailapp/spec.go` at this phase) —
  import `registry` and replace the `Port` literal:
  - `Port: 3202` → `Port: registry.MustPort("gmail")`.

  Leave every other Spec field, the connector/producer/worker wiring, and the
  `GMAIL_*` secret reads exactly as they are — only the `Port` value changes from
  a literal to a `registry` call.
- **`gmail/cmd/gmail/main_test.go`** — the manifest byte-equality test
  (`TestManifestLibraryByteEqualsCommittedFile`) already emits with `spec.Port`
  from the returned Spec, so it now emits `registry.MustPort("gmail")`
  transitively; add an explicit assertion tagged `// R-9QEG-KF05` that the
  composition root's port equals `registry.MustPort("gmail")` (== `3202`). If the
  Spec is obtained via `gmailapp.Spec()`, read `.Port` from it; the value must be
  `registry.MustPort("gmail")`, not a bare `3202`.
- Touch nothing else. **No schema change — no migration.** Do not edit
  `etc/manifest.env` or `etc/nginx.conf` (phase 08 re-points their *tests* at
  `registry`; the files' literals stay).

**Done when:**

- R-9QEG-KF05 — the composition root's listen port is `registry.MustPort("gmail")`
  (== `3202`), not a `3202` literal, proven by a genuinely-asserting test.
- `gmail/go.mod` requires `registry` with a committed `replace registry =>
  ../registry`.
- The suite is green: `cd gmail && go build ./...`, `cd gmail && go vet ./...`,
  `cd gmail && gofmt -l .` (prints nothing), `cd gmail && go test ./...`, and
  `bin/check-migrations gmail`.
