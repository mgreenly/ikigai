# repos — Research

External ground truth the design depends on, collected so design never
re-derives it. Non-contractual; the build loop does not read it. The prior-art
survey that shaped the overall direction (label gates, branch namespaces,
worktree-off-canonical-state, ephemeral scoped tokens) lives at
`docs/repos-research.md` in the repo root; this file pins only the exact
external contracts v1 code touches.

## 1. GitHub webhook delivery — the fields v1 reads

The GitHub App's webhook delivers JSON with the event name in the
`X-GitHub-Event` request header (not the body) and a unique delivery id in
`X-GitHub-Delivery`. The suite's webhooks service (its D17) forwards both
headers inside the event payload for `github-hmac` hooks, lowercased keys
`x-github-event` / `x-github-delivery`.

v1 consumes exactly one event name: **`issues`** with `"action": "labeled"`.
The payload footprint used (all under the delivery body):

```json
{
  "action": "labeled",
  "label":  { "name": "execute" },
  "issue":  {
    "number": 41,
    "title":  "…",
    "body":   "…",
    "state":  "open",          // "open" | "closed"
    "labels": [ { "name": "…" } ]
  },
  "repository": {
    "name":           "acme",
    "full_name":      "ikigenba/acme",
    "clone_url":      "https://github.com/ikigenba/acme.git",
    "default_branch": "main"
  },
  "sender": { "login": "mgreenly" }
}
```

- `sender.login` for actions performed by a GitHub App is the App slug with a
  `[bot]` suffix — for our App, **`ikibot[bot]`**. This is the loop-suppression
  discriminator: label changes the bot itself makes come back as deliveries
  with that sender.
- Label events fire per label added; adding `execute` while other labels exist
  delivers one `labeled` action whose `label.name` is `execute`.
- Deliveries are at-least-once; `x-github-delivery` is the dedup key if ever
  needed (v1 relies on the one-active-session-per-issue guard instead).

## 2. GitHub REST endpoints the runner drives (via the github service)

The runner never calls GitHub directly; it drives the github service's MCP
verbs (and its loopback `GET /token`). The underlying REST contracts, for the
record:

- Create PR: `POST /repos/{org}/{repo}/pulls` `{title, head, base, body}`.
  A PR body containing `Fixes #<n>` links and auto-closes the issue on merge.
- Add labels: `POST /repos/{org}/{repo}/issues/{n}/labels` `{labels: [...]}` —
  atomic add, does not replace the set.
- Remove label: `DELETE /repos/{org}/{repo}/issues/{n}/labels/{name}` — atomic
  remove; 404 when the label isn't on the issue.
- List issue comments: `GET /repos/{org}/{repo}/issues/{n}/comments` —
  ascending by default; each item has `body` and `user.login`.
- Installation access tokens expire **one hour** after mint (`expires_at` in
  the mint response); they are bearer credentials valid for every repo in the
  installation.

## 3. git — the plumbing the service invokes

The service shells out to the real `git` binary (present on the box; missing
tooling fails loudly per the v1 stance). Commands relied on:

- `git clone <url> <dir>` — canonical clone; `git -C <dir> fetch origin` +
  `git -C <dir> reset --hard origin/<default>` (or `pull --ff-only`) to
  freshen.
- `git -C <canonical> worktree add -b <branch> <path> origin/<default>` —
  creates an isolated working tree on a new branch; a branch checked out in
  any worktree cannot be checked out in another (this is what makes
  worktree-per-session safe). `git worktree remove --force <path>` +
  `git worktree prune` reclaim it.
- Worktrees share the canonical repo's object store and its config; a
  worktree's `.git` is a file pointing back into the canonical
  `.git/worktrees/<name>/`.
- Credential injection without touching disk:
  `git -c http.extraHeader="Authorization: Basic <b64(x-access-token:TOKEN)>" push origin <branch>`
  passes the token for one invocation only; nothing lands in `.git/config`,
  the worktree, or the process's persistent environment. (GitHub accepts an
  installation token as the password half of basic auth with username
  `x-access-token`.)
- `git ls-remote --heads origin <branch>` / `git -C <dir> rev-parse --verify`
  — existence checks used for retry branch numbering.

## 4. agentkit — the engine contract (as consumed by prompts today)

Pinned module `github.com/ikigenba/agentkit` (currently v0.2.0). Consumed
surface: `Conversation{Provider, Model, System, Log, Gen, Retry, Tools,
MaxToolIterations}`, `Send(ctx, text) *Stream`, `Stream.Events()/Err()`,
`Close()`, `NewTool[In]`, `GenSettings`, `RetryPolicy`, provider constructors
`anthropic.New` / `openai.New` / `google.New` / `zai.New`, and
`provider.Pricing(model)` for model validation. The transcript written to
`Conversation.Log` is agentkit stream-json, one JSON object per line — the
same shape prompts stores as `output.jsonl`.

A separate, non-blocking suite task tracks an agentkit release adding
`gpt-5.6-sol` to the openai pricing table; v1 defaults to
`anthropic`/`claude-opus-4-8` and does not wait for it.

## 5. Suite peers — contracts consumed

- **webhooks** (after its D17): a `github-hmac` hook verifies
  `X-Hub-Signature-256` and publishes `webhooks:received/<hook name>` whose
  payload is `{name, owner, received_at, content_type, body, headers}` with
  `body` base64 of the raw delivery and `headers` the two-key allowlist above.
- **github** (after its D9/D10): MCP verbs `issue_get`, `issue_comments`,
  `issue_comment`, `label_add`, `label_remove`, `pr_create` (bare names, POST
  `/mcp`, identity asserted via `X-Owner-Email`/`X-Client-Id` on loopback);
  loopback `GET /token` returning `{token, expires_at}`.
- **registry**: `registry.MustPort("repos")` = 3007 (Core),
  `registry.BaseURL("github")` for the peer address. The registry row is a
  suite-level precondition appended outside this module.
