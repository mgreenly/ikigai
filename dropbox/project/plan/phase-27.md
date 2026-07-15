# Phase 27 — Canonical absolute paths at the Service seam + logged mutation errors

*Realizes design Decision 24 (canonical absolute paths + logged mutation
errors). Depends on phase 18 (the write Service methods + loopback routes) and
phase 26 (the current MCP tool surface).*

Observable end state:

- `internal/dropbox` gains the package-level `normalizePath` helper
  (`path.Clean("/" + p)`; empty input returned empty), and every externally
  reachable `Service` method — `Write`, `Mkdir`, `Delete`, `Move` (both
  arguments), `Stat`, `List` (prefix; empty stays empty), `Content` —
  canonicalizes its path argument(s) first, before validation, mirror, index,
  or event use. The sync engine's internal apply methods are untouched.
- A relative-path mutation now succeeds end to end: the mirror file lands, the
  index row and the emitted event subject carry the canonical `/`-prefixed
  path, and relative/messy spellings resolve to the same index key as their
  absolute equivalents — on the loopback routes and the MCP tools alike.
- `writeMutationError` (and the mutation handlers that call it) logs the
  underlying error at `ERROR` level via the Service's `*slog.Logger` before
  answering the 500 branch; 4xx branches stay unlogged and all response bodies
  are unchanged.
- `docs/filesystem-api.md`'s path paragraph states the contract: relative
  input is accepted and canonicalized; the absolute form is canonical and is
  what the index, events, and `content_url`s carry.

**Done when:** the suite is green (design Conventions commands, from
`dropbox/`) and:

- R-54T0-VFZG is covered by a regression test on the real substrate (real
  SQLite index + tempdir mirror + real eventplane outbox on the same DB)
  driving `PUT /content?path=e2e/artifact.txt` through the shipped handler
  wiring: 200, mirror file present, index path `/e2e/artifact.txt`, and an
  outbox `create` row whose subject is `/e2e/artifact.txt`.
- R-560X-97Q5 is covered by tests proving uniform resolution: write
  `/e2e/x` → `GET /stat?path=e2e/x` finds it and `GET /list?path=e2e` lists
  it; `POST /mkdir?path=e2e//sub/../sub2` indexes `/e2e/sub2`;
  `DELETE /content?path=e2e/x` removes it; `POST /move?from=e2e/a&to=e2e/b`
  moves the file indexed as `/e2e/a` — same index key, never a parallel entry.
- R-578T-MZGU is covered by a test through the MCP tool surface: `put` with
  `path: "e2e/mcp.txt"` succeeds and `get` with `path: "/e2e/mcp.txt"`
  returns the same bytes (and the reverse spelling pairing also resolves).
- R-58GQ-0R7J is covered by a test injecting a failing event sink (or store)
  into a mutation: the response is 500 and a captured `slog` handler holds one
  `ERROR` record carrying the underlying error text.
- `grep -n "accepted and canonicalized" dropbox/docs/filesystem-api.md`
  returns exactly one hit (the docs statement landed).
