# Design — A Fixed Service Registry (name → port)

This document proposes a single authoritative `name → port` table in code as the
one place the suite assigns and resolves service addresses. Today every service
binds a fixed loopback port, but the knowledge of *which port* is scattered
across roughly five places per service and is hardcoded a second time inside the
peer-to-peer wiring of the event-plane consumers. The registry collapses that to
one table: a service learns its own port and every peer's port **by name**, and
the per-service `manifest.env` becomes a generated artifact rather than a
hand-maintained one.

The short version: ports stop being a value you write down in five files and
become a value you look up by name in one. Code references names; the table
turns a name into an address.

## Context — what we are replacing

The suite is built on a deliberate constraint: **all services always run on the
same host, distinguished only by loopback port.** That makes addressing a pure
function of name — there is no host to discover, no DNS, no service mesh. The
only thing a caller needs is the port, and the set of ports is small, fixed, and
known at build time.

Despite that, "which port" is currently duplicated. For a single service the
port appears in:

1. the `Port:` literal in its `cmd/<svc>/main.go` (the runtime default),
2. its `<svc>/.envrc` (the dev override),
3. its `<svc>/etc/manifest.env` (`PORT=30xx`, the deploy registry that
   `bin/registry` reads),
4. the **hardcoded peer maps** inside other services that talk to it, and
5. transitively, `bin/registry`, the local nginx front door, and `bin/start`.

The fourth is the sharp one. The event-plane consumers and the dropbox-content
fetchers each carry a literal map of peer addresses:

- `prompts` — a feed-URL map for `cron, crm, ledger, dropbox, scripts,
  prompts(self)`, plus `DROPBOX_BASE_URL`;
- `scripts` — the same shape for `cron, crm, ledger, dropbox, prompts`, plus
  `DROPBOX_BASE_URL`;
- `notify` — `CRM_FEED_URL`, `PROMPTS_FEED_URL`;
- `sites` — `DROPBOX_BASE_URL`.

Every entry is a literal `name → http://127.0.0.1:30xx`. Renumber one service and
those maps, in other services, must all be found and edited in lockstep — a
class of error the compiler cannot catch.

## The core principle

> **A service is named by identity and reached by address. The two are different
> facts, and only the address belongs in a central table.**

This distinction is the whole point, because the codebase currently fuses the two
inside single hardcoded URL strings. There are genuinely two kinds of "knowing
about another service":

- **Identity / contract** — *which* service and *which* events. `notify` consumes
  `crm` and `prompts`; `prompts` subscribes to `cron, crm, ledger, dropbox,
  scripts`; trigger tables key on `(source-name, event-type)`. This is a real
  domain subscription. It **stays in app code** and the registry neither removes
  nor should remove it.
- **Address** — *at what loopback port* that named service answers. This is pure
  plumbing, today duplicated, and is exactly what the registry centralizes.

After this change a consumer still declares `consumes "crm"` in its own code, and
then **asks the registry where `crm` is.** The name stays local; the port
resolves centrally.

## The registry

A literal `name → port` map in **`appkit`** — the shared chassis every service
already imports — is the single source of truth. It is data, not arithmetic:
each name is pinned to an exact number. The map is the registry; everything else
derives from it.

The number space is organised into blocks by service type, so a port carries a
hint about what kind of thing answers on it:

| Block | Type | Direction | Purpose |
|---|---|---|---|
| `3000`+ | core | counts up | the platform services |
| `3100`+ | applications | counts up | the domain apps |
| `3200`+ | connectors | counts up | bridges to external systems |
| `3099`↓ | custom | counts **down** | unofficial / experimental, outside the namespace |

Custom services share the `3000` block with core but grow toward it from the top.
With core occupying only the low single digits and custom descending from 3099,
the two would have to grow ~45 each to collide; in practice they never approach.
No separate custom block is warranted.

### Seed assignments

`dashboard` is fixed at `3000`: it is the apex `DEFAULT` app and `opsctl --port`
already defaults to 3000, so it must not move. The remaining seed order within
each block is frozen as listed here.

| Port | Service | | Port | Service | | Port | Service |
|---|---|---|---|---|---|---|---|
| **3000** | dashboard | | **3100** | crm | | **3200** | dropbox |
| 3001 | wiki | | 3101 | ledger | | 3201 | notify |
| 3002 | prompts | | | | | 3202 | gmail |
| 3003 | scripts | | | | | 3203 | github *(reserved)* |
| 3004 | sites | | | | | | |
| 3005 | cron | | | | | | |
| 3006 | webhooks | | | | | | |

`github` is a connector that does not exist in the tree yet. It is included to
make a property explicit: **the table may reserve a name ahead of its code.** A
row asserts ownership of a number; the service can be built later.

### Assignment policy

Assignments are **fixed by default.** A name owns its number, and that number
does not move because of an unrelated edit. There is no positional/index-derived
scheme — those let an insertion or an alphabetisation silently shift every
service after it, which is precisely the collision-at-deploy failure the
timestamped-migrations rule exists to prevent. The table is explicit so that
adding a service can only ever *append* a row.

Reuse is **permitted but deliberate.** Retiring a service returns its port to the
pool, and a later change *may* reassign it — but doing so is an explicit,
reviewed code edit, never incidental. In normal operation a retired number simply
stays dead and new services take fresh numbers. "Fixed, with reuse as a
legal-but-rare exception" — not "immutable forever."

### Source of truth and `manifest.env`

The registry lives in Go, but `bin/registry` is bash and the deploy boundary is
the language-neutral `manifest.env` file. We reconcile by making the Go table
authoritative and the manifest a **generated artifact**:

- Each service gets its own port, and every peer's port, from the table — killing
  duplications (1) and (4) above.
- `appkit`'s `manifest` verb already *emits* `manifest.env`; it now emits `PORT`
  from the table, so `<svc>/etc/manifest.env` stops being hand-edited.
- `bin/registry` and `opsctl` keep reading the generated `manifest.env`
  **unchanged.** There is no parallel bash copy of the table to drift against —
  Go is authoritative, the manifest is its output, both languages are served by
  one source.

## Blast radius

The address coupling is **concentrated, not pervasive.** Only the consumers and
fetchers carry peer maps: `notify`, `scripts`, `prompts`, `sites`. The pure
producers — `crm`, `ledger` — and `dashboard` know about no one; they publish and
serve. So converting hardcoded ports to registry lookups touches roughly four
services, not all thirteen.

The renumber itself, however, moves **every service except `dashboard`** off its
current `3001`–`3011` contiguous slot into the new blocks. Per service that
means the `main.go` `Port:` literal and the `.envrc` default; `manifest.env`
ceases to be hand-edited once generated. The four peer maps, the nginx dev
config, and `bin/start` stop hardcoding numbers entirely and go through the table
or the generated manifests.

## Enforcement

A test asserts the two invariants the scheme depends on: **uniqueness** (no two
names share a port) and **block-membership** (each name's port falls in its
declared block's range). This is the mechanical guard that keeps the table honest
as rows are appended — consistent with the suite's preference for systems over
willpower.

## Decisions resolved

- **Authority** — a literal `name → port` map in `appkit`; the single source of
  truth.
- **Representation** — explicit fixed assignment, not positional; append-only in
  normal operation.
- **Reuse** — permitted only via a deliberate code change; otherwise retired
  numbers stay dead.
- **Blocks** — core 3000+, apps 3100+, connectors 3200+, custom 3099↓.
- **Categories** — core: dashboard, wiki, prompts, scripts, sites, cron,
  webhooks; apps: crm, ledger; connectors: dropbox, notify, gmail, github.
- **`manifest.env`** — becomes generated from the table; `bin/registry` and
  `opsctl` unchanged.
- **Custom/core collision** — moot given the headroom; no separate custom block.

## Non-goals

- **Runtime discovery.** Nothing is discovered at runtime. The registry is a
  compile-time table; the loopback host is a constant. This is addressing, not
  service discovery.
- **Removing identity coupling.** Consumers still know their producers by name and
  event type. The registry resolves addresses; it does not dissolve domain
  subscriptions.
- **Multi-host addressing.** The single-host invariant is assumed throughout. If
  that ever changes the registry would need a host dimension, but that is out of
  scope here.
