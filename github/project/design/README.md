# github — Design

**Authority: shape and its proof.** This directory owns *how* `github` is built
and *how each behavior is proven*. The product (`project/product/README.md`) owns
the *why* and the caller-facing promises; design states the **exact, checkable
form** of those promises and never re-declares the why. Design *uses* the
product's contractual constants (org `ikigenba`, loopback port `3203`, bot-only
attribution, no `/feed`) by value but does not own them. This is the **single,
current** statement of the architecture: when a decision changes its file is
rewritten in place (never stacked); the history of how it got here lives in the
plan.

## Requirement ids

- Each Decision ends with a **Verification** list: the concrete behaviors that
  decision requires.
- Every Verification item carries a **minted id** of the form `R-XXXX-XXXX` — a
  stable, unique handle for that one behavior.
- The ids live inline in these Verification lists and **nowhere else** — there is
  no separate requirements document.
- Design's responsibility for ids ends at **minting them into this doc**. How
  coverage is measured, what counts as covered, and when work is "done" are not
  design's concern and are owned by downstream phases.

## Conventions

Shared facts every Decision leans on.

- **Language / module.** Go (`go 1.26`); module path `github`, a new module at
  `github/` under the repo root. It is built on the shared `appkit` chassis and,
  like every deployable service, is release-versioned (`github/VERSION`). It wires
  `appkit` (and, only where a shared type is needed, `eventplane`) via committed
  `replace` directives, exactly as `gmail` does: `replace appkit => ../appkit`.
- **Package layout.** `cmd/github/main.go` is the composition root (`appkit.Main`
  over the Spec); domain packages live under `internal/`:
  `internal/githubapp` (the appkit Spec), `internal/gh` (the GitHub auth + REST
  client), `internal/mcp` (the domain tool registrations over the shared
  `appkit/mcp` transport — D8), `internal/db` (the embedded migration set),
  `internal/web` (the landing page + embedded assets). The loopback PR route lives
  in `internal/gh`.
- **Non-secret config, read at the composition root.** `internal/githubapp.Spec`'s
  `Handlers` hook reads `IKIGENBA_APP_ID`, `IKIGENBA_GITHUB_ORG`, and
  `IKIGENBA_APP_PRIVATE_KEY` from the environment once, at the boundary, and passes
  them into the client. The private key value is **never logged**. No package below
  the composition root reads the environment. (`IKIGENBA_APP_CLIENT_SECRET` exists
  but is unused in v1.)
- **Zero new third-party dependencies.** The GitHub client and the RS256 app-JWT
  signing use only the Go standard library (`crypto/rsa`, `crypto/x509`,
  `crypto/sha256`, `encoding/pem`, `encoding/base64`, `encoding/json`,
  `net/http`); the JSON-RPC transport is the shared `appkit/mcp` (D8), not
  hand-rolled here. No `go-github`, no JWT library, no `x/oauth2`. The module's
  dependency set matches the chassis's existing closure (sqlite via appkit) and
  adds nothing.
- **Build / typecheck command.** `GOWORK=off go build ./...` from the module root
  (`github/`). Forcing `GOWORK=off` matches the deterministic production build and
  proves the module resolves standalone via its `replace` directives.
- **Test command.** `GOWORK=off go test ./...` from the module root. Unit tests are
  fully offline: the GitHub client is exercised against an injected
  `http.RoundTripper` stub (the same technique `gmail`'s client tests use), and the
  nginx fragment against a parse/grep test. No unit test performs live network I/O.
- **"The suite is green"** means: `GOWORK=off go build ./...` succeeds **and**
  `GOWORK=off go test ./...` passes with no failures and no `SKIP`, from `github/`,
  and `gofmt -l .` is empty and `go vet ./...` is clean.
- **The real-substrate proof is `health`.** Correctness of the app→installation
  authentication cannot be proven by a stub (a stub accepts any JWT). The design
  therefore designates the `health` verb as the end-to-end proof: run against the
  live suite (`bin/start`) it drives a *real* authenticated GitHub call and its
  success is the observable that the credentials actually work. The id that hinges
  on the real GitHub contract (`R-DMUT-QF4A`) names that live substrate explicitly;
  the offline unit suite proves request *construction*, `health` proves the
  request is *accepted*.
- **Bot-only attribution.** Write paths pass no owner-identifying author,
  committer, or body marker to GitHub; the only owner record is a structured log
  line (`X-Owner-Email` + verb) emitted at MCP dispatch. This is asserted directly
  on the request the client builds (the outbound body/headers carry no owner PII)
  and on the emitted log line.

## Layout

The design is **split for addressability** so a build phase reads only the one
Decision it realizes:

- `project/design/INDEX.md` — the manifest: each Decision → its file, plus a
  sorted `R-id → Decision/file` reverse map. Regenerated whenever a Decision is
  added or its Verification ids change.
- `project/design/DNN.md` — one self-contained file per Decision (zero-padded;
  referenced in prose and the plan as `D<N>`).
- `project/design/README.md` — this spine only.

Design is rewritten in place, not append-only: a changed Decision is rewritten in
its `DNN.md` and `INDEX.md` is regenerated; a new Decision adds a `DNN.md` and an
INDEX entry. History lives in the plan.
