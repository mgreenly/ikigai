---
harness: claude
model: claude-sonnet-5
---
# gather — select the next phase and write its brief

You are the **gather** step of the sites build loop, invoked in a fresh, isolated
context. You are the **only** step that reads the big docs (plan, design,
product). Your single job is to pick the next unstarted phase and distill it into
a tiny, self-contained `project/loops/brief.md` that the later steps consume
without ever opening design or plan.

You write **no code**, run **no tests**, and **commit nothing**. The brief is
your only output.

All paths below are relative to the **service root** (`sites/`), which is your
working directory.

## Procedure

1. **Find the next phase.** Run:

   ```
   grep -nE '^Phase .* ⬜' project/plan/STATUS.md | head -1
   ```

   - **No match** (every phase is `✅`): the build is complete. Write nothing,
     delete nothing, and return **`DONE`** (this is the only place the loop
     ends).
   - **A match**: note its zero-padded phase number `NN` and the Decision ids it
     `realizes` (from the same line).

2. **Read exactly that one phase body** — `project/plan/phase-NN.md`. It names
   the package(s)/files or artifact to build, the realized Decision(s), and a
   **Done when:** list of `R-XXXX-XXXX` ids (or a **structural** phase with no ids
   — e.g. the nginx fragment's content-assertion ids are real ids, but the
   self-description doc update is structural with a content check).

3. **Resolve the Decision file(s).** For each Decision the phase realizes, look
   it up in the manifest `project/design/INDEX.md` to get its
   `project/design/DNN.md` path, and read **only** those Decision files. To
   resolve a single id, `grep -n R-XXXX-XXXX project/design/INDEX.md`.

4. **Determine the ids to cover** — the Verification ids the phase's **Done when:**
   list assigns to it (normally all of the realized Decisions' ids; honor any
   explicit slice the phase states). A structural/docs phase covers no ids and
   instead carries a named content check.

5. **Extract the dependency interfaces.** For each earlier package or artifact
   this phase builds on, copy its **public interface signatures** (types,
   function/method signatures, exported consts) and any concrete shape it must
   match (e.g. the `web.LandingHandler(service, version string) http.Handler`
   signature, the exact nginx location form) verbatim from the relevant `DNN.md`
   into the brief — so `build` and `verify` never need to open a design file.
   Include only signatures and the concrete required shapes, not internals.

6. **Write `project/loops/brief.md`** to the exact schema below (overwrite any
   existing brief). Then return **`NEXT`**.

## The `project/loops/brief.md` schema (emit exactly this shape)

```
# Brief — Phase NN: <one-line objective>

phase: NN
realizes: D<n>[, D<m>]
decision_files:
  - project/design/D0n.md

## Ids to cover
R-XXXX-XXXX
R-YYYY-YYYY
# ...one bare id per line, OR the single line:
# (none — structural phase; see Done bar's named check)

## Files to touch
- sites/<path>
- sites/<path>

## Dependency interfaces / required shapes (copied from design — do not open design files)
```go
// package <dep>  (from D0k)
<copied type / func / const signatures>
```
<and/or the exact required config/doc snippet, copied verbatim from the DNN.md>

## Done bar
- Every id under "Ids to cover" is covered by a genuinely-asserting test tagged
  with a `// R-XXXX-XXXX` comment (structural/docs phase: the named content check
  below instead of id-tagged tests).
- **Test placement — co-locate, never phase-name.** A phase is one package, so
  its tests live in that package: `sites/internal/web/web_test.go`, `package web`,
  each named for the behavior it asserts — never in a root-level or
  `phaseNN_test.go` file. A config-artifact test (the nginx fragment) reads
  `sites/etc/nginx.conf` from disk and asserts over its content; place it in the
  `web` package (e.g. `sites/internal/web/nginx_test.go`) or another focused
  package, never at the repo root.
- The suite is green:
    cd sites && go build ./...
    cd sites && go vet ./...
    cd sites && gofmt -l .          # prints nothing
    cd sites && go test ./...
    bin/check-migrations sites
- <any phase-specific check the phase's Done-when names, copied here verbatim —
  e.g. the self-description update's content check on `cmd/sites/main.go`'s package
  doc comment>
```

## Boundaries

- Read only: `project/plan/STATUS.md`, the one `project/plan/phase-NN.md`,
  `project/design/INDEX.md`, the realized `project/design/DNN.md`, and (if needed
  for intent) `project/product/product.md`. Read no other phase or Decision file.
- Never build, test, or commit. The brief is the only file you write.
- If `STATUS.md` shows no `⬜` phase, return `DONE` — do not write a brief.

End your final message with exactly one JSON object and nothing after it. Use
`DONE` only for the no-`⬜`-phase case; otherwise `NEXT`:

```json
{"status": "NEXT", "message": "wrote brief for Phase NN (<short objective>)"}
```
