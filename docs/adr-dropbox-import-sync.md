# ADR — Importing Dropbox-authored artifacts into scripts, prompts, and sites

Status: **accepted** (v1 scope). Date: 2026-06-09.

## Context

The dropbox service keeps a private, download-only local mirror of one Dropbox
app folder (`/home/mgreenly/Dropbox/apps/ikigai-onebox` in dev;
`/opt/dropbox/data/mirror` on the box). A natural authoring loop has emerged:
a user creates an artifact **locally** — a Python script, a prompt, or a static
site's files — drops it in Dropbox, lets it sync to the box, and then wants the
**corresponding service** (scripts / prompts / sites) to pick it up. They also
want to keep iterating locally and **re-pull** the updated version.

Today the only ways to read a mirror file from another service are the MCP
`get` tool (built for *off-box* agents — base64-in-JSON, 25 MiB cap) or the
loopback `GET /content` byte route. There is no affordance on the consuming
services to *adopt* a mirror file as one of their own artifacts, and no way for
a peer to **enumerate** a subtree over plain HTTP. This ADR defines that
affordance.

Direction is strictly **Dropbox → service**, never service → Dropbox. dropbox
is download-only; nothing here writes back to Dropbox.

## The shape of the problem: two primitives

Artifacts fall into two kinds, and they are different *operations*, not the same
operation at different sizes:

| service | artifact | the operation |
|---|---|---|
| **scripts** | `create(name, body)` — one text blob | **assign a field** from one file |
| **prompts** | `user_prompt` + others — structured fields | **assign a field** from one file |
| **sites** | a tree: `create(slug)` + `file_write`×N + `publish(tier)` | **reconcile a tree** to a subtree |

- **scripts / prompts → `import`**: a *point* operation. One Dropbox file becomes
  one text field on one record. No membership, nothing to delete.
- **sites → `sync`**: a *set* operation. A Dropbox subtree becomes the site's
  working tree — which introduces deletion (a file removed upstream must be
  removed from the site), a declared boundary (which subtree), and exclusive
  ownership (the subtree owns the tree).

The single-file case is the degenerate one-file version of the tree case, but
they keep distinct verb names because the tree case mirrors deletes and the
single-file case never does (see Decision 4).

## Where the logic lives

The import/sync logic lives **on each consuming service**, never on dropbox.
Only the consumer knows its own artifact shape (one body / structured fields /
a file tree). dropbox stays a read-only daemon that knows nothing about its
consumers; it gains exactly one new read-only, loopback-only route (`/list`,
Decision 3) and is otherwise untouched.

Bytes move **service → dropbox over loopback HTTP** (`/content` + the new
`/list`), not through the agent's context and not via shared filesystem access.
The mirror stays private (`0750`); the "exploit the local filesystem" option
(a shared unix group over the mirror) was considered and **rejected** for v1 —
loopback `/content` already gives streamed, un-base64'd, uncapped access without
breaking the private-mirror decision, and these artifacts are small.

## Decisions

### 1. v1 is on-demand only

v1 ships **on-demand verbs** (`import` / `sync`). There is no event-driven
auto-sync. The on-demand verb is the primitive an auto-sync would be built on,
so it ships first and proves the reconcile semantics.

**Deferred (fast-follow):** auto-sync — subscribe the consumer to dropbox
`file.*`, filter by a path prefix (a new primitive; today's trigger filters on
event *type*, not path), debounce a save-burst into one apply, and reconcile.

### 2. Re-running upserts, keyed on a recorded source path

scripts and prompts `name` columns are **not unique**, so a naive "import =
create" would pile up duplicates on every re-pull. Therefore:

- Each consumer records the originating Dropbox path as a nullable `source_path`
  column (one migration per service via `bin/new-migration`; no edits to existing
  migrations).
- `import` / `sync` **upsert keyed on `source_path`**: re-importing the same
  Dropbox path updates the same artifact rather than creating a second one. This
  is what makes the "edit locally, re-pull to update" loop work.
- `source_path` also marks an artifact as **import-managed**, separating it from
  hand-authored artifacts.
- sites is already idempotent by its unique slug; it records the subtree path on
  the site row for symmetry.

### 3. Add a loopback `GET /list` to dropbox

Single-file `import` only needs `GET /content?path=…` (the path is known). But
sites `sync` must **enumerate** the subtree, and `list` exists only behind MCP
(`POST /mcp`), which is identity-gated and is the *off-box* transport. A peer
service cannot enumerate the mirror over loopback today.

dropbox gains a **`GET /list`** route — the loopback twin of `/content`:

- self-guarded exactly like `/content` (returns **404** on any nginx-injected
  identity header, `X-Owner-Email` or `X-Forwarded-Proto`),
- a thin wrapper over the existing `Service.List`,
- response mirrors the MCP `list` tool: `{files:[{path,size,hash,rev,updated_at}],
  next_cursor}`, cursor-paginated; the importer follows `next_cursor` to
  completion.

This completes the loopback consumer plane (enumerate **and** fetch over plain
HTTP) without bending the auth model (MCP-as-peer) or breaking the private mirror
(shared-group fs). It is read-only and loopback-only, fully consistent with
dropbox's identity.

### 4. Verb names: `import` for scripts/prompts, `sync` for sites

- `scripts.import(source_path)` and `prompts.import(source_path)` — single file.
- `sites.sync(slug, source_path)` — tree reconcile.

`import` reads naturally for the single-file pair, which never deletes anything.
`sync` is the honest word for sites: it is re-runnable **and** mirrors deletes.
The split follows the single-file / folder line.

### 5. prompts maps body → `user_prompt` only (dumb mapping)

A prompt is multi-field (`user_prompt` / `system_prompt` / `config` / `name`) but
a file is one blob. v1 maps the **file body → `user_prompt`**, derives the name
from the path, and leaves the other fields at their defaults. This keeps prompts
import the same shape as scripts import.

**Deferred:** a `---`-fenced frontmatter header to fill `system_prompt` / `config`
/ `name`.

### 6. sites `sync` is in-place reconcile: overwrite-all-present + delete-absent

`sync` mutates the existing `working/<slug>` tree in place to match the subtree:

- enumerate the subtree (via `/list`) and the working tree (via the existing
  `file_list`),
- **delete** every working file **absent** from the subtree (prune — intended
  behavior, not a footgun),
- **overwrite** every file **present** in the subtree (fetched via `/content`).

"Overwrite all present" sidesteps change-detection entirely. It is necessary
because the two services **cannot compare content hashes**: sites' `file_list`
reports **md5**, dropbox's `/list` reports the **Dropbox block-SHA256**
(`content_hash`) — different algorithms, no shared hash to diff. Overwriting
every present file is correct by construction (the working tree cannot drift from
the subtree) at the cost of rewriting unchanged files. For static sites over
loopback that cost is negligible; the only side effect is a refreshed mtime,
harmless for nginx serving.

**Deferred (optimization):** a per-file **sync manifest** (path → the
`content_hash`/`rev` last fetched) so a later sync fetches only genuinely-changed
files. This adds persistent state to sites; deferred until site size makes
re-fetching unchanged assets actually cost something.

### 7. sites `sync` reconciles the tree only — it does not publish

`sync(slug, source_path)`:

- **create-or-reuses** the site row + working tree if the slug is absent (so
  first-time setup is one content call),
- reconciles the tree (Decision 6),
- does **not** publish.

Publishing is a separate, explicit `publish(tier)` call because **tier
(public/private) is an exposure decision** that must not be implied by a content
sync. This does not hurt the update loop: `published` is a symlink into
`working/<slug>`, so reconciling the tree of an already-published site updates
the **live** site instantly, with no republish. The one-time `publish(tier)` is
the only extra step, and only the first time.

### 8. Publish stays non-atomic in v1 (in-place rebuild)

Because publish is a symlink into the working tree, an in-place reconcile is live
as it runs — a viewer hitting the site mid-sync can briefly see a partially
updated tree. v1 **accepts this tearing window** on a single-owner box.

**Deferred (option):** atomic publish via generation dirs + a symlink flip
(build `working/<slug>.<gen>`, atomically repoint the served symlink, GC the old
generation) — which also yields free rollback. Not v1.

### 9. Source-of-truth rule is documented, not enforced

A sync-managed site is **owned by its Dropbox subtree**: a hand-written file not
present upstream is removed on the next `sync` (Decision 6's prune). This is
intended. It is **documented, not hard-enforced** — sites are not blocked from
hand-edits; the obvious mechanics (you ran *sync*, it synced) plus this rule
suffice on a single-owner box. **A site should be either sync-managed or
hand-managed, not both.**

## Asserted defaults

- **Binary/text:** `import` rejects non-UTF-8 input (a binary blob is not valid
  Python source / prompt text) with a validation error. `sync` is binary-safe
  (site assets — images, fonts).
- **Per-file size:** `import` enforces a modest text cap (1 MiB; a source file or
  prompt above that is almost certainly a mistake). `sync` has no per-file cap in
  v1 (`/content` streams; the 25 MiB limit was only the MCP `get` tool).
  Revisitable.
- **Reaching dropbox:** consumers read `DROPBOX_BASE_URL`
  (default `http://127.0.0.1:3005`) and derive `/content` and `/list` — the same
  loopback-URL-via-env shape notify uses for feeds.
- **rev / staleness:** import/sync fetch *current* bytes (ignore the `/content`
  rev-pin). The result reports what landed (scripts/prompts: artifact id + name;
  sites: counts of files written and deleted), so mirror-lag — the mirror can lag
  local disk while Dropbox is still uploading — is **visible** and a re-run is
  cheap. The box cannot detect "Dropbox is mid-upload"; it reports what it
  applied rather than claiming completeness.
- **Slug derivation (sites):** defaults to the source-path basename; if that is
  not a valid slug (lowercase alnum + hyphen, 1–63 chars), `slug` must be passed
  explicitly, else a validation error.

## Rejected alternatives

- **Push from dropbox** (`dropbox.export_to(service, …)`) — would force dropbox
  to learn every consumer's schema and write into other services, destroying its
  read-only, consumer-agnostic identity.
- **Pure agent orchestration** (`dropbox.get` → `scripts.create`) — works today
  for one small script with zero code, but round-trips bytes through agent
  context (fatal for a site tree or binary assets) and offers no idempotent
  re-pull. Kept as the manual fallback, not the design.
- **Shared-group filesystem read of the mirror** — relaxes the load-bearing
  "mirror stays private" decision and gives up case-fold / rev safety, for no
  benefit on small text artifacts that loopback `/content` already serves
  efficiently. Reconsider only for a future large/binary consumer.
- **Atomic publish, frontmatter, manifest-based true diff, auto-sync** — all
  deferred above; good ideas, not v1.

## Work items (v1)

- **dropbox:** add `GET /list` (loopback, self-guarded, wraps `Service.List`,
  cursor-paginated).
- **scripts:** migration adding `source_path`; `import(source_path)` verb
  (fetch `/content`, UTF-8 + 1 MiB validation, upsert on `source_path`).
- **prompts:** migration adding `source_path`; `import(source_path)` verb (body →
  `user_prompt`, same validation + upsert).
- **sites:** record subtree path on the site row; `sync(slug, source_path)` verb
  (create-or-reuse, enumerate via `/list`, overwrite-present + delete-absent via
  `/content` + `file_list`, no publish).
- **all consumers:** `DROPBOX_BASE_URL` config at the composition root.
</content>
</invoke>
