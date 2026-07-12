# Phase 24 — True up `dropbox/CLAUDE.md` to the built service (structural; docs-only)

*Realizes no Verification ids (structural — docs truth). Depends on phase 22
(the reference-based `put`) and phase 23 (the kind/subject event addressing);
it documents both, so it runs after them.*

`dropbox/CLAUDE.md` is badly stale: it still describes the service as a
**one-way, download-only** mirror ("the box never writes back"), a **read-only**
MCP surface of **4 tools** (`health`/`reflection`/`list`/`get`), an index that
"does not track directories", and `file.created`/`file.modified`/`file.deleted`
events — all contradicted by the built filesystem API (D14–D18), the eight-tool
MCP surface with reference-based `put` (D19), and the kind/subject event
addressing (D22). Rewrite the stale sections to the current truth:

- The intro and daemon/producer model: bidirectional — downloads flow down
  unchanged, service writes flow up asynchronously through the durable
  `upload_queue` + uploader (suite is authority, overwrite; Dropbox is a
  replica); drop "one-way", "download-only", "the box never writes back".
- The MCP section: the eight-tool surface (`list`, `get`, `put`, `mkdir`,
  `delete`, `move` + chassis `health`/`reflection`); `put`'s two forms —
  reference-based `source_url` (server-side fetch, loopback+registry-port
  confined, uncapped) primary, capped 25 MiB `content_base64` convenience —
  and the `source_unavailable` addition to the error codes.
- The loopback surface: the write/discovery routes (`PUT`/`DELETE /content`,
  `POST /mkdir`, `POST /move`, `GET /stat`, `GET /list`) beside `GET /content`,
  pointing at `dropbox/docs/filesystem-api.md`; directories are first-class
  (empty dirs exist; `list` entries carry `kind`).
- The events section: kind/subject addressing — kinds `create`/`modify`/
  `delete`, subject = the mirror display path, canonical keys like
  `dropbox:create/bills/aws/2026-06.pdf`, family-shaped registry, payload
  fields (no `event` discriminator), the `origin` field.
- The no-backup section: restated honestly for the write direction — the
  mirror remains reconstructible from Dropbox, but a wipe discards
  **queued-but-unpushed local writes** (`upload_queue` rows), so recovery is
  re-bootstrap plus acceptance of that loss.
- The load-bearing download invariants (crash/replay, cursor, poison,
  hash-verify, case-folding, longpoll) remain and stay documented.

**Done when:** the suite is green (design Conventions commands, from
`dropbox/` — no source changes expected, this proves nothing broke) and, run
against `dropbox/CLAUDE.md` only:

- `grep -nE "download-only|one-way|never writes back|no write verbs|4 tools|file\.created|file\.modified|file\.deleted" dropbox/CLAUDE.md`
  returns **no matches** (the stale claims are gone);
- `grep -c "source_url" dropbox/CLAUDE.md`, `grep -c "mkdir" dropbox/CLAUDE.md`,
  `grep -c "dropbox:create" dropbox/CLAUDE.md`, and
  `grep -c "upload_queue" dropbox/CLAUDE.md` each return ≥ 1 (the current
  truth is present).
