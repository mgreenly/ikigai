# github — Product

**Authority: intent.** This document owns *why* the `github` service exists, *for
whom*, what is in and out of scope, and what we **promise** its callers — in
outcome terms only. Mechanism (the GitHub App JWT exchange, the REST client, the
JSON-RPC transport, the tool schemas, the loopback route, the nginx fragment) and
its checkable proof live in `project/design/`. Where the two touch observable
behavior, product states the *promise* and design states the *exact, checkable
form*; that boundary keeps product, design, and plan from overlapping.

## Problem

The suite reacts to the outside world through the event plane: a GitHub webhook
(a pull request opened, a push landed) enters through the `webhooks` service and
becomes a suite event. An agent in `prompts`, or a deterministic job in
`scripts`, can watch for that event, but once it wants to *act back on GitHub* —
read the PR's diff, leave a review, comment on an issue, commit a file — it has
nowhere to go. Nothing in the suite speaks to GitHub. Every service would
otherwise have to carry its own GitHub App credentials, mint its own installation
tokens, and hand-roll its own REST calls, duplicating credential handling across
the box and scattering a single trust relationship (the `@ikigenba` GitHub App)
into many.

## Purpose

`github` is the suite's **single connector to the `@ikigenba` GitHub
organization**. It holds the one GitHub App installation, mints and refreshes the
installation token itself, and exposes the org's repositories, pull requests,
issues, and file contents as a set of MCP tools other services drive on the
owner's behalf. It is the one place in the suite that talks to GitHub, so no other
service handles GitHub credentials or GitHub's API.

## Users

- **A `prompts` run (an agent).** The primary consumer. At run spawn, `prompts`
  discovers every peer's MCP surface and hands those tools to the in-run agent, so
  a prompt that watches for a GitHub webhook event can, with no github-specific
  wiring, read the PR, review it, comment, or commit back — all through the
  `github` tools.
- **A `scripts` job (deterministic).** A Python script wired to a suite event that
  needs GitHub data. Scripts do not speak MCP/JSON-RPC; they call a plain
  loopback HTTP route. v1 gives scripts one proven path: fetch a pull request.
- **The operator, confirming the connector is live.** Opens the service's landing
  page in a browser to see it is deployed and which version is running, and reads
  `health` to confirm the GitHub App can actually authenticate to the org.

All callers act on the **same** org through the **same** installation. The
authenticated caller identity (`X-Owner-Email`) is provenance only: it never
switches which GitHub account or org is reached.

## Scope

`github` does this and only this:

- **Authenticate as the `@ikigenba` GitHub App** — sign an app JWT with the app's
  private key, resolve the org's installation, and mint a short-lived installation
  token, refreshing it before it expires. Every GitHub call uses that token. No
  per-user OAuth, no human consent flow.
- **Expose org GitHub as MCP tools** — read plus light write, org-scoped:
  list/read repositories; list/read pull requests, comment on and review and merge
  them; list/read/create/comment-on/update issues; read and write single files via
  the Contents API. Plus the chassis tools every service has (`health`,
  `reflection`).
- **Give `scripts` one proven loopback path** — a loopback-only HTTP route that
  returns a pull request, the deterministic twin of the PR-read tool.
- **Serve the canonical suite landing page** — the same non-interactive landing
  layout every other service ships (a service eyebrow, a one-line description, and
  a service / version / API panel), on the suite design system, gated by the
  dashboard browser session. No GitHub data, nothing interactive.

It deliberately does **nothing else** in v1. In particular it does not: receive
GitHub webhooks (the `webhooks` service owns inbound ingress); publish or consume
any event-plane event (it has no `/feed`); create or delete branches, releases, or
low-level git objects; dispatch or read Actions/workflows; create commit statuses
or checks; search; expose any GitHub account or org other than `@ikigenba`; let
different owners on the box reach different GitHub backends; or attribute any
GitHub-visible action to a specific owner (see the promise below).

## Contractual constants

Promised values the design must honor verbatim and never re-declare:

- **One org, one installation.** The connector reaches exactly the `@ikigenba`
  organization through its single GitHub App installation. The org name is
  configuration; there is no runtime selection of a different org.
- **Loopback port 3203.** `github` answers on loopback port `3203` (frozen in the
  suite `registry`), mounted at `/srv/github/`.
- **Bot-only attribution, no owner PII on GitHub.** Every GitHub-visible artifact
  the service writes (a comment, a review, an issue, a commit) is attributed to
  the GitHub App identity alone and carries **no** owner email, owner name, or
  any owner-identifying marker. The org's repositories are mostly public; owner
  identity must never be published into their history or timelines. Owner
  provenance for a write is recorded only in the service's own logs.
- **The event plane is not github's.** GitHub-origin facts enter the suite through
  the `webhooks` service. `github` neither produces nor consumes events and serves
  no `/feed`.

## What we promise (user-facing behavior)

- **An agent can act on the org's GitHub through tools alone** — a `prompts` run
  discovers the `github` tools automatically and can read a pull request, leave a
  review or comment, update an issue, or commit a file, without any
  github-specific code in `prompts`.
- **A script can fetch a pull request over a plain loopback call** — a `scripts`
  job gets the PR it asks for from a loopback-only HTTP route, with no MCP client
  and no token handling of its own.
- **The connector authenticates itself** — callers never present a GitHub token;
  the service mints and refreshes the `@ikigenba` installation token on its own,
  and a caller's first request does not fail because a token expired.
- **Suite-authored GitHub activity looks like the bot** — a comment, review,
  issue, or commit the suite makes appears as the `@ikigenba` GitHub App and
  reveals nothing about which owner triggered it.
- **`health` proves the connector really works** — a successful `health` means the
  app private key, app id, and org installation are all correct and GitHub
  actually authenticated the app, not merely that the process started.
- **A logged-in human sees the canonical landing page** — opening `/srv/github/`
  in a browser shows the same suite landing layout the other services render (the
  service name, running version, and API on the suite design system); a browser
  with no dashboard session is refused.

## Success criteria (outcomes)

Each is a result a caller or operator can confirm against the running service:

- With the suite up, a `prompts` agent lists its available tools and finds the
  `github` verbs, then reads a real pull request from the `@ikigenba` org through
  them.
- Through the tools, an agent leaves a comment on an issue and it appears on
  GitHub authored by the `@ikigenba` App, with no owner email or owner marker in
  the comment.
- A `scripts` job fetches a specific pull request over the loopback route and gets
  that PR's data back; the same route refuses a request that arrives through the
  public front door.
- `health` succeeds only when the app can actually authenticate to the
  `@ikigenba` installation, and fails loudly (not a hang or a silent empty result)
  when the private key or app id is wrong.
- The connector keeps working across the installation token's expiry — a caller
  making requests an hour apart does not see a token-expiry failure.
- A logged-in dashboard user opens `/srv/github/` and sees the service name
  `github` and the running version; a browser with no session gets `401`.
