# webhooks — Product

**Authority: intent.** This doc owns *why* the webhooks service exists, *for
whom*, what is in and out of scope, and the user-facing promises — stated once,
in outcome terms. It does **not** state mechanism, exact paths, formats, exit
codes, schemas, or test assertions; those belong to `project/design/README.md`.
Where the two could overlap (observable behavior), product states the *promise*
and design states the *exact, checkable proof* of that promise. This boundary is
load-bearing and keeps product, design, and plan from restating each other.

## Problem

The ikigenba suite has a rich internal event plane — services publish facts to
an append-only outbox and consume each other's feeds — but there is **no way for
the outside world to inject a fact into it**. Every service entry point is gated
by dashboard OAuth introspection, which an external system (a SaaS tool, a
script, another company's backend) does not and cannot hold. So today an owner
who wants "when *that* happens out there, do something in here" has no path: the
event plane is sealed to inbound traffic.

## Purpose

webhooks is the suite's **inbound ingress** for the event plane. It lets an
owner mint a named, secret-protected URL that an outside system can call; a valid
call is turned into a single fact published on the event plane for the rest of
the suite to consume. It does one job: **accept an authenticated outside call and
durably publish it as an event, attributed to the owner who created the webhook.**

## Users

- **The owner** (an authenticated suite user) connects an agent and, through MCP,
  creates and manages their webhooks — minting a URL + secret, listing what they
  have, rotating a leaked secret, and deleting ones they no longer want.
- **External systems** (a SaaS provider, a cron job on another box, a teammate's
  script) hold a webhook's URL and secret and call it when something happens on
  their side. They are not suite users and have no suite identity; they only ever
  trigger, never manage.
- **Other suite services** (e.g. prompts, notify) consume the resulting events
  off the event plane and decide what to do — they are downstream of this service,
  not users of it.

## Scope

webhooks **does**:

- Let an owner **create** a webhook (a name plus a secret), receiving back the
  URL an outside system will call.
- Let an owner **list** their own webhooks.
- Let an owner **delete** one of their webhooks, after which its URL is dead.
- Let an owner **rotate** a webhook's secret without changing its name or URL, so
  a leaked secret is recoverable.
- Accept an authenticated inbound call to a webhook's URL, **acknowledge it
  immediately**, and **durably publish a single event** on the event plane
  carrying the webhook's name, the owning user, and the submitted payload.
- **Require a secret on every trigger** — an unguessable name is defense in
  depth, never the security boundary.
- Enforce an **upper bound on accepted payload size**, rejecting anything larger.
- Serve a minimal human **landing page** at the mount root — showing the service
  name and the running version on the suite's design system, gated by the
  dashboard browser session — the same uniform page every suite app now serves.

webhooks does **nothing else**. In particular, for this version it deliberately
excludes:

- **Provider-signature verification (HMAC).** No GitHub/Stripe-style per-provider
  signing schemes; authentication is a required suite-minted secret only. This is
  a named **future feature**, and the design must not preclude adding it.
- **Rate limiting.** No per-webhook or per-secret throttling; the payload cap is
  the only inbound resource guard in this version. Named fast-follow.
- **Exactly-once / de-duplication.** A retrying caller may produce duplicate
  events; the service does not detect or collapse them.
- **Enable/disable (pausing).** A webhook is either created or deleted; there is
  no paused state. Delete-and-recreate (which changes the name) is the workaround.
- **Account-shared webhooks.** A webhook belongs to the single user who created
  it; there is no notion of a webhook shared across an account's users.
- **Any management UI or token logic of its own.** Owner management is MCP-only
  and suite identity is established upstream; the service runs no token logic of
  its own. It does serve the one minimal landing page above (service name +
  version, gated by the dashboard browser session), like every other suite app,
  but that page performs **no** management action, exposes **no** webhook data,
  and carries no interactive control — creating, listing, rotating, and deleting
  webhooks stays MCP-only.

## Contractual constants

- **Starting version `0.1.0`** — the service's first committed bare-SemVer
  `VERSION`, matching the suite convention for a brand-new deployable service.
- **External mount `/srv/webhooks/`** — the path prefix under the account apex at
  which the service is reachable, by service-name convention.

The exact inbound trigger sub-path, the event-type string, and the payload-size
limit are **not** product constants — design declares them.

## What we promise (user-facing behavior)

- **Minting is self-service and complete.** An owner asks their agent to create a
  webhook with a chosen name; they get back the full URL an outside system will
  call **and** the secret to send with it. The secret is shown **exactly once, at
  creation** — it is never retrievable afterward. Lose it, and the remedy is to
  rotate.
- **A valid call always lands.** When an outside system calls a webhook's URL with
  the correct secret and a payload within the size limit, the call is
  **acknowledged immediately with success**, and the corresponding event is
  **durably recorded before that acknowledgment** — once accepted, it will appear
  on the event plane even across a service restart.
- **Each event says which webhook fired and for whom.** Every published event
  identifies the webhook by name, carries the owning user, and includes the
  submitted payload, so any consumer can tell webhooks apart and act on the
  owner's behalf.
- **Rejections leak nothing.** A call with a wrong secret and a call to a name
  that does not exist are rejected **the same way**, so an outsider cannot probe
  which webhooks exist. An over-size payload is rejected rather than accepted.
- **Rotation preserves the URL.** Rotating a secret issues a new secret (shown
  once) and immediately invalidates the old one, while the webhook's name and URL
  stay the same — no need to reconfigure the external system's address.
- **Deletion is final and immediate.** After an owner deletes a webhook, calls to
  its URL are rejected as if it never existed.
- **Owners see only their own.** Listing returns the calling owner's webhooks and
  no one else's.
- **A logged-in human who opens the mount sees a real page.** A logged-in
  dashboard user who opens `/srv/webhooks/` in a browser sees the service name
  and the running version on the suite's design system, not a raw proxy error or
  a blank page; a browser with no dashboard session is refused. Agents are
  unaffected — the bearer-gated `/mcp`, the PRM well-known, `/health`, the public
  `/in/` ingress, and the loopback `/feed` behave exactly as before.

## Success criteria (outcomes)

- An owner can create a webhook through MCP and receives back both a callable URL
  and a one-time secret.
- An outside call to that URL carrying the correct secret returns a success
  acknowledgment, and a matching event — naming the webhook, the owner, and the
  payload — is observable on the event plane afterward.
- An acknowledged call's event is still present after the service is restarted
  (it was durably recorded before the acknowledgment).
- An outside call carrying a wrong secret is rejected and produces no event.
- An outside call to a non-existent webhook name is rejected **in a way
  indistinguishable** from the wrong-secret rejection.
- An outside call whose payload exceeds the size limit is rejected and produces no
  event.
- After an owner rotates a webhook's secret, a call with the new secret succeeds
  and a call with the old secret is rejected — using the unchanged URL.
- After an owner deletes a webhook, a call to its former URL is rejected and
  produces no event.
- Listing webhooks returns exactly the calling owner's webhooks and none created
  by another user.
- A logged-in dashboard user opening `<account>.ikigenba.com/srv/webhooks/` sees
  a page showing the service name `webhooks` and the running version; a browser
  with no dashboard session is refused rather than shown the page; and the
  public `/in/` ingress, the bearer-gated `/mcp`, and the shielded `/feed`
  behave exactly as before the page existed.
