# Phase 26 — Content-plane acceptor: the `Fetch` sandbox tool + framing-prompt claims (fetch, PDF tooling)

*Realizes design Decisions 5 (rewritten: seven-tool set + the `sourcePortAllowed` seam), 20 (the `Fetch` tool), and 22 (PDF tooling claims). Depends on Phase 24 (the settled framing prompt this phase extends). Covers R-64QY-QN1H, R-65YV-4ES6, R-676R-I6IV, R-68EN-VY9K, R-69MK-9Q09, R-6AUG-NHQY, R-6I5U-Y474. **External precondition (claims only, nothing built here):** `pdftotext`/`pdftoppm`/`pdfinfo` are box-baseline provisioning owned by opsctl's spec (its D10).*

Observable end state:

- `prompts/internal/tools/` gains the `Fetch` tool per D21: `All(sandboxRoot, sourcePortAllowed func(int) bool)` returns seven tools; `Fetch(content_url, dest_path)` validates the URL (http, literal `127.0.0.1`/`::1`, explicit allowed port) and the sandbox-confined `dest_path` **before any network I/O**, streams the body to a temp file computing SHA-256 on the same pass, temp+renames to `dest_path`, and returns `{path, size, content_hash}` (64-hex lowercase). Redirects are not followed; the failure mapping (`validation:` / `not_found:` / `conflict:` / `source_unavailable:`) is pinned per D21.
- `prompts/internal/runner/` threads the seam: `runner.New(store, sb, ttl, manifestRoot, sourcePortAllowed)` (or an equivalent injectable field) and `execute` calls `tools.All(sandboxRoot, r.sourcePortAllowed)`; `cmd/prompts/main.go` derives the production allowed set from `registry.Services` (every registered port true — no port literal, R-RG04-NLIT stays green).
- `framing_prompt.go` names the seven tools in lowercase prose (…, grep, and fetch), gains the one-sentence fetch guidance (a content URL lands as a sandbox file) and the PDF claims sentence naming `pdftotext`, `pdftoppm`, `pdfinfo` as available in Bash. The "NO network access from bash" sentence and the D19 deferred-tools paragraph are unchanged; no individual service is named.
- The retired six-tool count test (R-K0MO-FDTH) is deleted with its behavior; its id stays in frozen phase-04 as history.

## Done when

The suite is green (design *Conventions* commands, from `prompts/`) and:

- **R-64QY-QN1H** — a clearly-named test asserts `tools.All(root, allowed)` returns exactly 7 tools named `Bash`/`Read`/`Write`/`Edit`/`Glob`/`Grep`/`Fetch`, each an `agentkit.Tool`.
- **R-65YV-4ES6**, **R-676R-I6IV**, **R-68EN-VY9K**, **R-69MK-9Q09** — each covered by a clearly-named test against a **real local `httptest` server** with its port injected through the seam, asserting exactly the behavior its D21 Verification line states (server-side stream + SHA-256 result; the four confinement discriminators with zero recorded requests; the escaping-`dest_path` rejection; the 404/409/refused/redirect failure mapping with no file at `dest_path`).
- **R-6AUG-NHQY** and **R-6I5U-Y474** — clearly-named tests assert the assembled framing prompt names `fetch` among the sandbox tools with the content-URL guidance, names `pdftotext`/`pdftoppm`/`pdfinfo` in Bash, and still contains no `ikigenba_` / per-service enumeration (R-9OJA-B2KU and R-A69O-ATWI stay green).
- `grep -rn "R-K0MO-FDTH" prompts --include=*.go` returns no matches (the retired count test is gone), and R-K1UK-T5K6's escape test stays green.
