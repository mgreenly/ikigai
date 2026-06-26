# Phase 65 — Docs purge: retire the false "no UI" line, state the landing-page truth

*Realizes design Decision 39 (the stale-doc consequence). **Structural / docs
phase — no R-ids.** Edits only `wiki/AGENTS.md` (and its `CLAUDE.md` symlink —
they are one file); touches no Go code, no nginx, no migration. Naturally
sequenced after Phases 63–64 so docs flip true only once the page and its gate
exist.*

wiki's `AGENTS.md`/`CLAUDE.md` opening still declares the service is
"…a knowledge base (ingest / search / ask-RAG) exposed as **MCP** … Loopback-only
domain service over SQLite, **no UI** and no token logic; nginx (owned by the
dashboard) is the trust boundary." With Phases 63–64 landed, the **"no UI"**
clause is false: wiki serves a human landing page. Per the suite doctrine — docs
state current truth; purge what the change falsified, rewrite in place, no
archaeology, no footnotes — correct that sentence.

- **Purge "no UI."** Rewrite the clause to state the current truth: wiki serves
  its MCP surface **and** a session-gated human **landing page** (service name +
  version, Carbon-styled) at the mount root.
- **Keep "no token logic" — it is still true.** The landing page reads no token
  and runs no token logic; nginx remains the sole trust boundary (the cookie gate
  is `/_session-authn`, owned by the dashboard, asserted in nginx — not in wiki).
- Do **not** rewrite the rest of the file (the build-loop discipline section, the
  design/plan workflow, the deploy pointer); this is a single-clause correction,
  not a rewrite.

`AGENTS.md` is the real file; `CLAUDE.md` is a symlink to it — edit `AGENTS.md`
(a refusal to write through the symlink is expected and fine).

**Done when:** the Go suite stays green (this phase changes no Go) **and** the
named structural check passes:

- `wiki/AGENTS.md` no longer asserts "no UI" anywhere, and its opening states the
  landing-page truth (a human web page served beside MCP, session-gated).
- The "no token logic" / "nginx is the trust boundary" statement remains intact
  (it is still true).
- `CLAUDE.md` still resolves to the same content (the symlink is unbroken).

(Structural phase: "Ids to cover" is **(none — structural phase)**; the build
loop verifies the green suite plus the docs check above, not an `R-id` test.)
