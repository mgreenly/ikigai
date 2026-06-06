# ledger

The **ledger** service for the metaspot single-tenant suite. A pure MCP API with
**no UI** and **no token logic**, deployed at `<account>.metaspot.org/srv/ledger/`
(e.g. `ai.metaspot.org/srv/ledger/`). First demo account: **ai**.

A real **double-entry bookkeeping** service for personal and small-business use,
modeled conceptually on [ledger-cli](https://ledger-cli.org/): an **immutable
journal** of balanced transactions, with every report a query over postings. The
surface is a **fixed set of eight verbs** (it does not grow as features are
added — see `PLAN.md` §1–2) over a single write entity, the transaction. It is an
event-plane **producer** (emits `transaction.recorded` to an outbox at `GET
/feed`, mirroring `../crm`). The chassis (auth, nginx, deploy, transport) is the
same production-grade crm chassis, renamed.

**Read the decisions first — do not re-derive them:**

- `../metaspot/AGENTS.md` — platform spec (Service layer = path routing).
- `../metaspot/docs/path-routing-architecture.md` — server-side topology + the
  auth contract you live under.
- `../metaspot/docs/connector-and-install.md` — the suite plugin + install layer.
  Note: a service's connector **skills live in the `dashboard` repo's `plugin/`**,
  not here.
- `PLAN.md` — the ledger design (the 8-verb rationale, the immutable-journal /
  emergent-typed-account model, the transaction contract, the events).
- `../crm` — the sibling service that shares this chassis and is the reference
  event-plane **producer** (`internal/contacts` → `/feed` outbox).

If anything here conflicts with those docs, the docs win — and flag the conflict.

## What this app is

A loopback-only domain service. nginx (owned by the dashboard) terminates TLS,
introspects every request via `auth_request` against the dashboard, and injects
`X-Owner-Email` / `X-Client-Id`. This service **trusts those headers** and does
no token validation of its own. nginx strips the `/srv/ledger/` prefix, so
internally routes stay bare (`/mcp`, `/health`, `/feed`, `/.well-known/...`).
Small business, ≤100 users: SQLite, single instance, is correct and deliberate.

**Books are global to the box** — one set of books per instance; no owner/tenant
column on transactions/postings. `Identity` (the injected headers) is consulted
only by `ikigenba_ledger_health`, matching crm and the single-tenant model.

## The MCP surface (8 fixed verbs)

There is **one write entity — the transaction** (a set of balanced postings); the
surface area is reads. The journal is **immutable** (transactions are never
mutated, only reversed). The chart of accounts is **emergent and typed**: accounts
spring into existence on first posting to a colon-path (`Assets:Bank:Checking`),
the only guardrail being that the top-level root must be one of five known types
(`Assets`, `Liabilities`, `Equity`, `Income` [alias `Revenue`], `Expenses`), with
alias + case-fold canonicalization so the tree can't fork. Money is integer cents,
single-currency USD. See `PLAN.md` §2–4 for the full contract.

- **`ikigenba_ledger_record`** — record one immutable double-entry transaction (≥2 postings
  that must balance to zero; at most one posting may elide its amount and receive
  the balancing residual; optional per-posting/txn status and array ordering).
- **`ikigenba_ledger_reverse`** — the correction primitive: post the sign-flipped mirror of
  an existing transaction, linked both ways (`reverses_id`/`reversed_by_id`); the
  mirror's legs reset to `pending`. Double-reversal is guarded.
- **`ikigenba_ledger_reconcile`** — the *only* mutation of existing rows: free status
  transitions among `pending`/`cleared`/`reconciled`, idempotent, all-or-nothing.
- **`ikigenba_ledger_balance`** — account balances with depth roll-up across the typed
  account tree; no args = the whole live chart. Raw signed sums (ledger-cli style).
- **`ikigenba_ledger_register`** — the running-total register for matched accounts over a
  period; also the list verb.
- **`ikigenba_ledger_get`** — fetch one transaction in full (postings, per-posting status,
  ord, reversal links).
- **`ikigenba_ledger_describe`** — static introspection (the five typed roots + normal
  balances, statuses, recipes) merged with the live account tree. The first call
  an agent should make.
- **`ikigenba_ledger_health`** — the shared health envelope (status/version/
  service/details) plus the authenticated caller's identity (owner email +
  client id); the end-to-end auth proof.

## Domain layout

- **`internal/ledger/`** — the domain package, one file per concern within a single
  package (not a second dispatch layer): `types.go` (structs, account-type table,
  error sentinels), `store.go` (SQL-only, `*sql.Tx` methods), `service.go` (the
  `Service` type — owns transactions, the balance invariant, and event emission),
  `transaction.go` (record + get), `reverse.go`, `reconcile.go`, `balance.go`,
  `register.go`, `describe.go`, `events.go` (event payloads/builders).
- **`internal/mcp`** — JSON-RPC 2.0 MCP transport. `tools.go` holds the 8
  descriptors and is the **sole** dispatcher + arg-validation/normalization site
  (account canonicalization, date parsing, elision well-formedness), translating
  typed sentinels (`unbalanced`, `bad_root`, `validation`, `not_found`,
  `already_reversed`) to MCP tool-error text. `mcp.go` is the transport, unchanged.
- **`internal/server`** — routing, the unauthenticated RFC 9728 protected-resource
  metadata document, the `requireIdentityHeaders` gate, the ungated `/health` route, the
  unauthenticated `GET /feed` event handler, security headers, graceful shutdown.
- **`internal/db`** — SQLite open (WAL, FK, single-writer) + embedded migration
  runner. Migrations: `001_schema_migrations` (chassis), `002_ledger.sql`
  (`transactions` + `postings`; no accounts table — accounts are `SELECT DISTINCT
  account FROM postings`), `003_outbox.sql` (byte-identical to `outbox.SchemaSQL`,
  with a test asserting that equality).
- **`internal/logging`, `internal/ids`** — structured slog + request-id middleware,
  ULID generation. Carried from the chassis unchanged.

## Events (event-plane producer)

Producer-only. The outbox shares ledger's single SQLite writer; the event is
appended on the **same transaction** as the journal write, and `Ring()` fires
after commit. **Every committed transaction emits exactly one
`transaction.recorded`** through one shared insert helper — including a
`ikigenba_ledger_reverse` mirror (its payload carries `reverses_id` so a consumer can tell
it's a correction). `cmd/ledger/main.go` wires the `outbox` and injects it into
`ledger.Service`; the SSE handler is mounted at `GET /feed` (unauthenticated,
loopback-only — the perimeter is nginx). Second-wave payloads (`transaction.reversed`,
`posting.reconciled`) are designed but unwired in v1 (`PLAN.md` §6).

## nginx fragment (not a vhost)

`opsctl setup ledger` writes only `/etc/nginx/conf.d/locations/ledger.conf` (its
`location /srv/ledger/` + the PRM well-known location, per
`path-routing-architecture.md`) and reloads nginx. It does **not** install a
server block and does **not** issue a TLS cert — the dashboard owns both (the
box-global apex/cert pieces are `opsctl init-box`). A dev mirror of this fragment
lives at `../nginx/locations/ledger.conf`.

## Manifest / deploy

ledger is one static appkit binary (the `appkit.Main(appkit.Spec{…})` contract):
`<app>` serve + the fixed `version`/`manifest`/`migrate`/`schema`/`backup`/
`restore` verbs, no `run` wrapper. `etc/manifest.env` (`APP=ledger`,
`MOUNT=/srv/ledger/`, `DEFAULT=false`, `PORT=3002`, `MCP=true` so the dashboard
inventory lists it) is emitted by `ledger manifest` — the binary owns its own
identity, and `opsctl deploy` regenerates the on-box copy on every swap. Shipping
is the shared repo-root `bin/ship ledger` (no version arg; version is the
committed `ledger/VERSION`, advanced by `bin/bump ledger <field>`) → `opsctl
stage` + `opsctl deploy` (versioned release dir + atomic swap + rollback);
provisioning is `opsctl setup ledger`. The
only `bin/*` scripts ledger still carries are `start`/`stop` (systemd control). No
`plugin/` in this repo.
