# Phase 28 — `content_url` on loopback `/stat` and `/list` file entries

*Realizes design Decision 25 (`content_url` on lookup responses). Depends on
phase 27 (canonical paths — the rendered URL carries the D24-normalized
path).*

Observable end state:

- `Service` carries a `ContentBase string` field, set at the composition root
  from the same registry-resolved loopback base already wired to
  `NewOutboxProducer`.
- The loopback `GET /stat` response for a file, and every `kind: "file"`
  entry in loopback `GET /list`, carry
  `content_url = contentURL(ContentBase, <canonical path>)`; directory
  stats and `kind: "dir"` entries carry no `content_url`; an unset
  `ContentBase` omits the field. The MCP `list`/`get` tools remain without
  `content_url` anywhere.
- `docs/filesystem-api.md` documents the field on the `/stat` and `/list`
  sections: present on file entries, absent on directories, dereferenced via
  loopback `GET /content`.

**Done when:** the suite is green (design Conventions commands, from
`dropbox/`) and:

- R-59OM-EIY8 is covered by a test asserting the file `/stat` response's
  `content_url` equals `<contentBase>/content?path=<url-encoded canonical
  path>` and the directory `/stat` response has no `content_url` key.
- R-5AWI-SAOX is covered by a test asserting every `kind: "file"` entry in a
  mixed `/list` page carries the URL-encoded `content_url` and no
  `kind: "dir"` entry does.
- R-5C4F-62FM is covered by a test asserting the MCP `list` and `get`
  `structuredContent` contain no `content_url` key anywhere.
- R-5DCB-JU6B is covered by a round-trip test against a real HTTP server
  (real `Service` + tempdir mirror + real index behind an `httptest.Server`
  mounting the shipped routes, `ContentBase` set to the server's URL): write
  a file, `GET /stat`, then `GET` the returned `content_url` verbatim and
  receive exactly the written bytes.
