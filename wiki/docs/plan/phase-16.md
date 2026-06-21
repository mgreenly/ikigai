# Phase 16 — `ask` parses through the shared `llm.ExtractJSON` carve

*Realizes design Decision 9 (`ask`, `internal/ask`), covering the new id R-7SXQ-B9AX. Depends on Phase 13 (the exported `llm.ExtractJSON` carve) and Phase 08 (the ask agent and its JSON answer parse).*

Phase 08 built `ask` with its own ported private `stripCodeFence` (a second copy
of the fence-only stripper that D5 replaced for the extract/compile path). It has
the same brittleness: an agent answer wrapped in an unanticipated fence form — an
extra backtick, a prose preamble, a stray leading `` ` `` — leaves junk in front
of the `{` and fails the `json.Unmarshal` into `Answer`. D9 has been re-decided to
parse the agent's final message through the **single shared carve** rather than a
private copy. This phase removes the duplication.

**What gets built (the observable end state):**

- `ask` parses the agent's final message with `llm.ExtractJSON` (the exported
  carve from Phase 13) before `json.Unmarshal` into `Answer`.
- The private `stripCodeFence` in `internal/ask` (and any test asserting its
  fence-only behavior) is **deleted** — there is exactly one
  JSON-from-a-decorated-reply implementation in the service.
- An agent answer carrying a fence, a prose preamble / trailing commentary, or a
  stray leading `` ` `` still unmarshals into `Answer`.

**Done when:**

- R-7SXQ-B9AX — a test asserts `ask` parses the agent's final message through the
  shared `llm.ExtractJSON` carve (no private fence-stripper): an answer wrapped in
  a fence, a prose preamble/trailing commentary, or a stray leading `` ` `` still
  unmarshals into `Answer`, where the old fence-only strip would have failed.
- The rest of D9 stays green — R-5THH-I3WL (honest-empty gate), R-5UPD-VVNA
  (citation downgrade), R-5VXA-9NDZ (citation resolution), R-5X56-NF4O
  (read-only), R-5YD3-16VD (Answer contract) — so swapping the parser does not
  disturb ask behavior.
- The suite is green (`go build ./...`, `go vet ./...`, `gofmt -l .` with no
  output, `go test ./...`, `bin/check-migrations wiki`).
