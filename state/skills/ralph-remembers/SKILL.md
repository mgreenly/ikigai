# ralph-remembers

`ralph-remembers` is the persistent document store for Ralph. The `mem-tool` (`libexec/mem-tool`) provides CLI access to it. All operations read a JSON object from stdin and write a JSON result to stdout.

## Environment Variables

| Variable | Description |
|----------|-------------|
| `RALPH_REMEMBERS_URL` | Base URL of the ralph-remembers service (e.g. `http://ralph-remembers.localhost:8000`) |
| `PROJECT_ORG` | GitHub organization (used to build project scope) |
| `PROJECT_REPO` | GitHub repository name (used to build project scope) |
| `IKIGAI_AGENT_ID` | Agent ID (used to build project scope) |

## Scoping

Documents live within a **scope** that namespaces them. Two scopes are supported:

- **default** (omit `scope` field) — project scope: `${PROJECT_ORG}/${PROJECT_REPO}`
- **global** — cross-project scope using zero UUIDs; documents accessible to all agents

## Actions

### create

Create a new document. Returns the created document JSON.

```json
{"action": "create", "body": "document content here"}
{"action": "create", "path": "notes/session.md", "body": "document content here"}
{"action": "create", "path": "notes/session.md", "body": "content", "scope": "global"}
```

Required: `body`. Optional: `path` (document title/path), `scope`.

Returns HTTP 409 as a clear error if a document with the same title already exists.

### get

Retrieve a document by path/title. Returns the full document JSON including body.

```json
{"action": "get", "path": "notes/session.md"}
{"action": "get", "path": "notes/session.md", "scope": "global"}
```

Required: `path`.

### list

List documents in scope. Returns an array of document metadata.

```json
{"action": "list"}
{"action": "list", "scope": "global"}
{"action": "list", "search": "session notes"}
{"action": "list", "path": "notes/", "limit": 20, "offset": 40}
```

Optional: `search` (full-text search, results ordered by relevance), `path` (exact-match title filter), `limit` (default 50, max 100), `offset` (pagination).

### delete

Delete a document by path/title.

```json
{"action": "delete", "path": "notes/session.md"}
```

Required: `path`.

### update

Update an existing document's body and optionally rename it. Looks up the document by `path`, then performs a PUT. Returns the updated document JSON.

```json
{"action": "update", "path": "notes/session.md", "body": "new content"}
{"action": "update", "path": "notes/session.md", "body": "new content", "title": "notes/renamed.md"}
```

Required: `path`, `body`. Optional: `title` (new name for the document).

### revisions

List revision history for a document. Looks up document by `path`, then fetches revision metadata list.

```json
{"action": "revisions", "path": "notes/session.md"}
```

Required: `path`.

### revision_get

Retrieve a specific historical revision of a document. Looks up document by `path`, then fetches the revision body.

```json
{"action": "revision_get", "path": "notes/session.md", "revision_id": "abc123"}
```

Required: `path`, `revision_id`.

## Error Responses

All errors are returned as JSON with exit code 0:

```json
{"error": "ERR_CONFIG: RALPH_REMEMBERS_URL not set"}
{"error": "ERR_PARAMS: body is required for create"}
{"error": "ERR_IO: connection refused"}
{"error": "ERR_CONFLICT: document with this title already exists"}
```

## Usage Example

```sh
# Store a document
echo '{"action":"create","path":"notes/standup.md","body":"- reviewed PRs\n- fixed bug"}' \
  | libexec/mem-tool

# Retrieve it
echo '{"action":"get","path":"notes/standup.md"}' | libexec/mem-tool

# Search for documents
echo '{"action":"list","search":"standup"}' | libexec/mem-tool

# Update with rename
echo '{"action":"update","path":"notes/standup.md","body":"updated","title":"notes/standup-2026-03-14.md"}' \
  | libexec/mem-tool

# View revision history
echo '{"action":"revisions","path":"notes/standup-2026-03-14.md"}' | libexec/mem-tool
```
