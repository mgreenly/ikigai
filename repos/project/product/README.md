# repos — Product

**Authority: intent.** This doc owns *why* the repos service exists, *for
whom*, what is in and out of scope, and the user-facing promises — stated once,
in outcome terms. It does **not** state mechanism, exact paths, formats, label
strings' plumbing, schemas, or test assertions; those belong to
`project/design/`. Where the two could overlap (observable behavior), product
states the *promise* and design states the *exact, checkable proof* of that
promise.

## Problem

The suite can talk *about* code but cannot *work on* code. A GitHub issue in
the org describes a bug or a small feature; today a human has to open a laptop,
clone, branch, fix, test, push, and open a PR — even when the work is exactly
the kind of bounded task an agent finishes unattended. The suite has an agent
runtime (prompts), a GitHub connector (github), and inbound GitHub facts
(webhooks), but nothing that owns **git trees on disk and the agent sessions
that modify them**. There is no development plane.

## Purpose

repos is the suite's **development plane**: it keeps local clones of the org's
GitHub repositories and runs **agent sessions** against them. A session checks
out work in an isolated working copy, does the work, and checks it in; the
durable product is commits, a pushed branch, and a pull request. In v1 it does
exactly one job end to end: **detect work in a repo, fetch it, do it, push,
and open a PR** — triggered by a human labeling a GitHub issue.

## Users

- **The owner** (an authenticated suite user) — labels a GitHub issue to hand
  it to the bot, and reviews the resulting pull request on GitHub. Through MCP
  they can also onboard a repository ahead of time, start a session directly,
  watch sessions, read a session's transcript, and cancel one.
- **GitHub collaborators** — anyone who can apply a label in the org's repos
  can dispatch work; anyone who can review a PR consumes the result. They never
  touch the suite directly; the issue and the PR are the whole interface.
- **Other suite services** — notify (or any consumer) can observe session
  outcomes on the event plane. They are downstream, not users.

## Scope

repos **does**, in v1:

- Watch the org's GitHub activity (arriving via the suite's inbound webhook
  ingress) for **an issue being labeled as ready to execute**, and start an
  agent session for it — cloning the repository on first contact, with no
  per-repo onboarding required.
- Report progress **on the issue itself**: acknowledge the dispatch, mark it
  in progress, link the PR on success, and mark it failed with a reason on
  failure. The bot's GitHub identity is **@ikibot**; everything it does on
  GitHub is attributed to that identity.
- Run each session in an **isolated working copy** on its **own branch**,
  never on the repository's default branch, and never with GitHub credentials
  in the agent's hands.
- Honor an **in-repo definition of done**: when the repository declares an
  executable check, a PR is opened only if that check passes; when it declares
  none, the PR says so.
- Let an owner, through MCP, **clone/list/inspect/remove** tracked
  repositories and **start/list/inspect/cancel** sessions, including reading a
  session's full transcript.
- Publish each session's terminal outcome as a **suite event**.
- Keep every session's transcript **durably**, surviving restarts, repo
  removal, and workspace cleanup.
- Serve the canonical suite **landing page** at its mount, session-gated like
  every other service.

repos does **nothing else** in v1. Deliberately excluded (they belong to the
already-designed later direction, `docs/repos-design.md`):

- **No release machinery.** No release events, no content endpoint for
  execution-plane services, no `source` bindings; sites, scripts, and prompts
  are untouched by this service.
- **No local-only repositories.** Every v1 repository is a clone of a GitHub
  remote; creating a fresh repo with no remote arrives with the release work.
- **No conversational mode.** The bot does not hold a discussion on the issue
  thread; a `discuss` label is reserved for that future mode, and v1 sessions
  read the full issue thread so contracts written that way carry over.
- **No delegated gating.** Only a human (or an explicit MCP call) dispatches
  work; no classifier applies the gate.
- **No review or merge.** The PR is reviewed and merged by humans on GitHub;
  repos never merges and never touches the default branch.

## Contractual constants

- **Bot identity `@ikibot`** — the GitHub App identity all bot activity is
  attributed to, and the branch namespace prefix (`ikibot/…`) session branches
  live under.
- **Dispatch label `execute`** — the human gate. The in-flight, failure, and
  reserved-conversation labels are `executing`, `failed`, and `discuss`.
- **Loopback port `3007`, mount `/srv/repos/`** — frozen in the suite
  registry.
- **Starting version `v0.1.0`.**

## What we promise (user-facing behavior)

- **Label an issue, get a PR.** A collaborator applies the `execute` label to
  an open issue; with no other setup, the bot acknowledges on the issue,
  works, and — when it finishes and the repo's own check (if any) passes — a
  pull request linked to the issue appears, closing the issue when merged.
- **The issue tells the whole story.** Dispatch is acknowledged on the issue;
  in-progress is visible as a label; failure leaves a labeled issue with a
  reason comment and the work-in-progress branch still pushed for inspection;
  success leaves a PR link. Re-applying the dispatch label after a failure
  retries with a fresh session.
- **First contact is enough.** Any repository in the org is eligible; the
  first dispatched issue clones it automatically. An owner can also onboard a
  repository explicitly ahead of time.
- **The agent can't leak what it never had.** The session agent works only
  inside its working copy with local tools; it holds no GitHub credentials and
  no suite tools. All GitHub activity is performed on its behalf and appears
  as @ikibot, with no owner-identifying marker.
- **The default branch is never touched.** All bot work lands on bot-namespace
  branches and reaches the default branch only through a human-merged PR.
- **The repo defines "done".** If the repository ships an executable check,
  the bot opens a PR only when that check passes; a failing check is reported
  as a failure with the check's output. If there is no check, the PR plainly
  says none was declared.
- **Work is bounded.** A session that exceeds its time budget fails cleanly
  and reports; a busy service queues new dispatches and acknowledges the queue
  position on the issue rather than silently stalling.
- **Transcripts survive.** Every session's transcript remains readable through
  MCP after success, failure, cancellation, restart, or repo removal.
- **A logged-in human who opens the mount sees a real page** — the canonical
  suite landing (service name, running version) gated by the dashboard
  session; agents and the event feed are unaffected.

## Success criteria (outcomes)

- Applying `execute` to an open issue in an org repo the suite has never seen
  results — with no other setup — in an @ikibot acknowledgment on that issue
  and, on completion, a pull request from an `ikibot/…` branch that references
  the issue.
- The PR's repository default branch has no bot commits; all bot work is on
  the bot's branch.
- On a repo whose declared check fails for the produced work, no PR is opened;
  the issue carries the failure label, a comment with the check's output, and
  the branch is pushed and inspectable.
- On a repo with no declared check, the opened PR states that no check was
  declared.
- Re-applying `execute` to a failed issue produces a fresh session and a new
  attempt on a fresh branch.
- An owner can, through MCP: onboard a repo before any issue exists; see it
  listed; start a session with written instructions; watch its state; read its
  transcript; cancel it; and remove the repo — after which that repo's past
  session transcripts are still readable.
- A second dispatch against a repo with a session already running waits its
  turn (and says so on the issue) rather than corrupting or interleaving work.
- A session that runs past its time budget ends as a clean, reported failure,
  and the service remains healthy for subsequent sessions.
- Each finished session is observable as one suite event carrying the repo,
  session, and outcome.
- A logged-in dashboard user opening `/srv/repos/` sees the service name and
  running version; a browser with no session is refused.
