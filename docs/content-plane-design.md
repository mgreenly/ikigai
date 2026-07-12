# Design — The Content Plane (bytes by reference)

This document defines the suite's third plane. The **auth plane** decides who a
request is from (nginx introspects, injects identity headers, services trust
them). The **event plane** moves small facts between services (`/feed` outboxes,
SSE, cursors). The **content plane** moves *file bytes*: it is the rule set for
how a file gets from the service that holds it to the service that needs it.

The short version: **agents pass references; services move bytes.** A file is
named by a loopback URL plus integrity metadata, and that reference is the only
file-shaped thing that ever appears in an MCP result, an event payload, or a
prompt. Bytes flow service-to-service over loopback HTTP, never through a
model's context.

## Context — what we are replacing

File content currently crosses the suite in three inconsistent ways:

1. **By reference, correctly.** dropbox's events carry a `content_url`
   (`http://127.0.0.1:3200/content?path=…`) and consumers fetch current bytes
   from its loopback `GET /content`, with an optional `rev` pin that returns
   409 if the content moved on. This is the pattern the plane generalizes.
2. **By value, through model context.** dropbox's MCP `get` returns
   `content_base64` inline. For an agent this means an entire file is decoded
   into context: token bloat, a 25 MiB hard cap, and unreliable round-tripping
   of binary data through a language model. Workable for a small text file,
   wrong as the general mechanism.
3. **Not at all.** gmail can enumerate a message's attachments (filename, size,
   mime type) but exposes no way to obtain their bytes, so an emailed PDF is
   unreachable by any consumer.

The immediate forcing function is the bill-processing workflow (gmail
attachment → Dropbox folder → extraction → ledger), but the plane is
deliberately workflow-agnostic: any future "take a file from here, put it
there, operate on it" composes from the same few verbs.

## The core principle

> **A file crosses the suite as a reference. Only the two services at the ends
> of a transfer ever touch the bytes, and they exchange them over loopback
> HTTP.**

Two kinds of "having a file" fall out of this:

- **Holding** — a service owns bytes and can serve them (`dropbox` owns its
  mirror, `gmail` holds attachment blobs at Google's end, `prompts` holds a
  run's sandbox files). Holders expose a **content endpoint**.
- **Accepting** — a service can ingest bytes named by a reference (`dropbox`
  accepts a file into the mirror, a future extraction connector accepts a
  document to analyze). Acceptors take a **`source_url`** and fetch it
  themselves.

An agent orchestrating a transfer holds neither: it carries the reference from
the holder's tool result to the acceptor's tool call.

## The reference

A content reference is the tuple:

```
{
  "content_url":  "http://127.0.0.1:3200/content?path=%2Fbills%2Faws%2F2026-06.pdf",
  "content_hash": "<64-hex content hash>",
  "size":         184230,
  "rev":          "015f3a…"        (optional; present when the holder versions)
}
```

- `content_url` is a **loopback** URL, resolvable only on the box. It appears
  URL-encoded and complete: the consumer never constructs or rewrites it.
- `content_hash` and `size` let a consumer verify what it fetched and let an
  acceptor detect a torn or moved-on source.
- `rev`, where the holder has revisions, supports the pin contract below.

## The holder contract (content endpoints)

Every service that holds bytes exposes a loopback `GET` content endpoint:

- **Streams the bytes** with a correct `Content-Type` and `Content-Length`;
  range and conditional requests are permitted but not required.
- **Loopback-private, by handler guard.** The handler returns 404 whenever it
  sees an nginx-injected identity header (`X-Owner-Email` or
  `X-Forwarded-Proto`), the same guard `/feed` and dropbox's `/content` use
  today. An nginx `location = … { return 404; }` fragment is optional
  defence-in-depth, never the mechanism. Content endpoints are **never**
  reachable from outside the box.
- **Pinnable.** If the holder versions its content, an optional `rev` (or
  equivalent) query parameter demands "exactly the bytes this reference named":
  a mismatch returns **409**, absence returns **404**. A consumer that fetched
  unpinned and finds `content_hash` differs knows a newer reference is in
  flight.
- **Addressed by the holder's own key.** dropbox addresses by mirror path;
  gmail addresses by `(message_id, attachment part)`; prompts addresses by
  `(run_id, sandbox path)`. The plane does not impose a shared namespace; the
  `content_url` encapsulates whatever the holder needs.

## The acceptor contract (`source_url` verbs)

Every agent-facing verb that ingests a file takes a **reference, not a body**:

- The MCP tool signature is `…(dest…, source_url)`. The service fetches the
  stream itself and applies its own domain rules (size caps, path validation,
  its normal write path). The agent's tool call and result stay small facts.
- **Fetch targets are confined.** An acceptor fetches only from
  `127.0.0.1`/`::1` on ports assigned in the service registry. Anything else
  is a validation error. The plane is internal plumbing; no service fetches an
  arbitrary URL on an agent's say-so.
- Service-to-service HTTP *beneath* the plane (e.g. dropbox's loopback
  `PUT /content`, whose request body is the stream) is unaffected: bodies are
  fine between services. The reference rule governs the agent-facing surface
  and the event plane.
- A content-bearing convenience variant (inline small text an agent authored)
  may exist alongside, with a tight size cap; the `source_url` form is the
  primary verb.

## The sanctioned crossing: prompts runs

A sandboxed prompts run is where references legitimately become bytes an agent
can operate on, and the crossing lands on **disk, not in context**:

- **`fetch(content_url, dest_path)`** — a sandbox tool executed by the runner
  (not by the model): it streams the URL, subject to the same loopback
  confinement, into the run's sandbox at `dest_path`, and returns only
  `{path, size, content_hash}`. The run then works on the file with its normal
  tools.
- **Run files are content, too.** prompts is itself a holder: a run's sandbox
  files are addressable by content URL, so a run's *product* (an extracted
  JSON, a generated report) can be handed onward by reference, e.g.
  `dropbox put(path, source_url=<run file URL>)`. What a run consumes and what
  it produces travel the same way.

## Conformance map

| service | holder | acceptor | status |
|---|---|---|---|
| dropbox | `GET /content` (path, `rev`→409) | fs API `PUT /content`; MCP `put(path, source_url)` | holder + fs API exist; MCP `put` is new |
| gmail | `GET /attachment` (message, part) + `content_url` in `read` results | — | new |
| prompts | run sandbox files by content URL | `fetch(content_url, dest)` sandbox tool | new |
| future connectors (extraction, etc.) | as needed | `extract(source_url)`-shaped verbs | drop-in by design |

Each row lands through that service's own `project/` spec loop; this document
is the shared contract they cite.

## Decisions resolved

- **References, not bytes, at every agent-facing and event-facing surface.**
  The reference is `{content_url, content_hash, size, rev?}`.
- **Holders expose loopback `GET` content endpoints**, guarded in the handler
  by the identity-header 404 rule, with the `rev`→409 pin contract where
  revisions exist.
- **Acceptors take `source_url`** and fetch server-side, confined to loopback
  registry ports.
- **The prompts sandbox is the one place references become agent-visible
  bytes**, via a runner-executed `fetch` onto sandbox disk; runs are also
  holders of their own files.
- **No shared blob store.** The plane is a convention over per-service
  endpoints, not a new storage service.

## Non-goals

- **Public or cross-box transfer.** Content endpoints never leave loopback;
  the plane assumes the single-box model throughout.
- **Streaming into model context.** There is deliberately no "read file over
  MCP" primitive beyond the existing small-file conveniences; large or binary
  content goes reference → sandbox disk.
- **A universal namespace or catalog.** Each holder keeps its own addressing;
  the plane standardizes the *shape* of a reference, not a global file tree.
- **Internet fetching.** `source_url` confinement is absolute; bridging
  external content into the suite is a connector's domain job (as dropbox and
  gmail already do), never a generic fetch.
