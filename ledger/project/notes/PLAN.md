# Ledger Design Plan — agent-first double-entry bookkeeping over a small fixed MCP surface

Status: **built.** Phases 0–6 are implemented; the tree builds and all tests
pass. A grill-the-plan pass (2026-06-03) closed every §11 open question and
reconciled §8 with the crm chassis; those resolutions are folded into the
sections below and consolidated in §12, and the implementation follows them.
Scope is confined to the `ledger/` folder.

## 1. Goal & guiding decision

Turn the current skeleton (a whoami-only clone of the crm chassis) into a real
**double-entry bookkeeping service** for personal and small-business use, exposed
through a **small, fixed set of verbs** — modeled conceptually on
[ledger-cli](https://ledger-cli.org/): a journal of balanced transactions, with
every report a query over postings.

The governing decision, inherited from the crm redesign and adapted to accounting:

> **Tool count is a function of *verbs*, not *entities*.** Bookkeeping has
> essentially **one write entity — the transaction** (a set of balanced
> postings) — and its real surface area is **reads** (balances, registers,
> statements). The verb surface is fixed at eight and does not grow as features
> are added.

The test for any future change: *did this add a tool?* If yes, justify why a new
verb (not a new report scope, account convention, or field) was unavoidable.

Ruling decisions from the design discussion (all confirmed):

- **Accounting integrity is preserved.** The journal is **immutable**. There is
  no upsert and no delete for transactions. Corrections are made by posting a
  **reversal** (`ledger_reverse`) and re-recording. This is the deliberate
  divergence from crm's `save`-upsert + soft-delete model — accounting does not
  edit history, it compensates it.
- **Managed-but-emergent chart of accounts.** Accounts are **emergent like
  ledger-cli** — they spring into existence on first posting to a colon-path
  (`Assets:Bank:Checking`). No pre-registration, no chart to maintain. The only
  managed guardrail: the **top-level root must be one of five known types**.
- **Single currency, integer cents, USD.** No foreign currency, no commodities,
  no inventory in v1 (same money decision as crm).
- **Greenfield.** Nothing consumes the skeleton; replace the domain and tool
  layers outright. No transition shims, no back-compat.

## 2. The fixed tool surface (8 tools)

There is one write entity (the transaction) and an emergent, typed account tree.
No `save`/`delete` for journal facts — integrity forbids it.

| Tool | Shape | Role |
|---|---|---|
| `ledger_record` | `(date, description, postings[], status?)` | Post one balanced transaction. `postings: [{account, amount_cents, status?}]`. One posting may **elide its amount** → it receives the balancing residual (ledger-cli's killer ergonomic). Rejects unless postings sum to zero. The heart and the most frequent write. |
| `ledger_reverse` | `(id, date?, memo?)` | The correction primitive. Posts the mirror of an existing transaction (links via `reverses_id`/`reversed_by_id`). No edit, no delete — a new compensating fact, fully auditable. |
| `ledger_reconcile` | `(posting_ids[], status)` | The **only** permitted mutation of existing journal rows: a narrow status transition (`pending → cleared → reconciled`). Can never touch an amount, account, or date. |
| `ledger_balance` | `(query?, period?, depth?, status?)` | The `bal` report **and** the live chart. No args = the whole emergent account tree with balances. Serves trial balance, balance sheet, net worth, and per-customer A/R. `status` filter gives the cleared-vs-ledger-balance reconciliation view. |
| `ledger_register` | `(query, period?, status?)` | The `reg` report — chronological postings + running total for matched accounts. Customer statements, account history, search. Also the list/paginate verb. |
| `ledger_get` | `(id)` | One transaction in full: all postings, per-posting status, and reversal links. |
| `ledger_describe` | `()` | **Discovery.** The first call any agent should make. Returns the five typed roots (+ normal balance, which statement they feed), the live account tree (or a depth-summary), the unit (USD cents), the reconciliation states and their meaning, and **recipes** — how to produce a balance sheet / P&L / customer statement from `balance`+`register`. |
| `ledger_whoami` | `()` | Unchanged. The end-to-end auth proof. |

Deliberately **not** in the initial surface (documented escape hatches, add only
when justified):

- **A `save`/`create`-account tool** — accounts are pure emergent; there is no
  creation step. Add account *annotation* (explicit type override, human label,
  "closed" marker) only if a real need appears.
- **A separate named-statement tool** (`ledger_report`) — a balance sheet is just
  `balance Assets Liabilities Equity` at an instant; an income statement is
  `balance Income Expenses` over a period. The recipes live in `describe`, not in
  extra tools. Introduce a dedicated statement verb only if the period/format
  semantics outgrow what `balance` can express.
- **Periodic/recurring transactions** (ledger-cli's `~ monthly`) — the agent
  drives the monthly billing batch itself in v1. Revisit if it becomes toil.

## 3. Account model (emergent, typed at the root)

There is no account entity to CRUD. An account is a **colon-delimited path**
(`Assets:Receivable:Acme`) that exists exactly when a posting references it.

- **Five typed roots**, accepted with common aliases:
  `Assets`, `Liabilities`, `Equity`, `Income` (alias `Revenue`), `Expenses`.
  An unknown root is a typed error pointing the agent at `ledger_describe`.
- **Normal balances / statement membership** are derived from the root:
  Assets & Expenses are debit-normal; Liabilities, Equity & Income are
  credit-normal. Assets/Liabilities/Equity feed the **balance sheet**;
  Income/Expenses feed the **income statement (P&L)**.
- **Everything below the root is free-form** — the connecting agent's
  convention, treated as a *skill the agent brings*, not something the ledger
  dictates. `Assets:Receivable:<customer>` gives a per-customer A/R sub-ledger
  for free; `Income:Hosting` aggregates revenue. The ledger validates the type
  system; the agent owns the taxonomy.
- **Discovery closes the loop.** Because the agent doesn't define the chart up
  front, it must be able to *find* the existing one so it posts consistently
  instead of inventing a parallel naming scheme. That is `ledger_describe`'s job
  (conventions + recipes) plus `ledger_balance` with no args (the live tree).

**Decided defaults (easy to change, called out so they're not silent):** money as
integer cents, single currency USD; five roots above with the listed aliases;
reconciliation status on the **posting**, default `pending`.

## 4. The transaction & posting contract

```jsonc
// ledger_record
{
  "type": "object",
  "required": ["date", "description", "postings"],
  "properties": {
    "date":        { "type": "string", "description": "calendar date YYYY-MM-DD (a business day, not a timestamp)" },
    "description": { "type": "string", "description": "payee / memo" },
    "status":      { "type": "string", "enum": ["pending","cleared","reconciled"],
                     "description": "default for postings that omit their own" },
    "postings": {
      "type": "array",
      "items": {
        "type": "object",
        "required": ["account"],
        "properties": {
          "account":     { "type": "string", "description": "colon-path; root must be a known type" },
          "amount_cents":{ "type": "integer", "description": "signed minor units; omit on at most one posting to elide the residual" },
          "status":      { "type": "string", "enum": ["pending","cleared","reconciled"] }
        }
      }
    }
  }
}
```

- **Balancing.** After elision resolves (at most one posting may omit
  `amount_cents`; it receives the negation of the rest), the postings must sum to
  **zero**. Otherwise a typed `unbalanced` error reporting the residual — the
  agent self-corrects.
- **Well-formedness rules (validated at the `mcp` boundary, re-asserted in the
  service):**
  - **≥ 2 postings** — a single leg cannot represent a transfer.
  - **≤ 1 elision** — two or more omitted amounts leave the residual
    undetermined → typed `validation`.
  - **Residual → elided leg.** The elided posting gets `−Σ(explicit)`; if that
    is `0` the leg is still created with `amount_cents: 0` (not special-cased).
  - **Explicit-only** transactions must sum to exactly zero, else `unbalanced`.
  - **Zero-amount legs** are allowed (e.g. a memo leg) — they contribute nothing.
  - **`amount_cents` is signed minor units**: debit `+`, credit `−`. There is no
    sign normalization anywhere; the stored sums are raw and signed.
  - **Account validation/normalization:** non-empty colon-path whose root resolves
    to one of the five types after **canonicalization** — the root alias is folded
    (`Revenue`→`Income`) **and** its case is folded (`assets`→`Assets`) so the
    emergent chart can never fork; **sub-path case is preserved** as the agent
    wrote it. Empty segments (`A::B`), leading/trailing colons, and control chars
    are rejected (crm label hygiene). Unknown root → typed `bad_root` pointing at
    `ledger_describe`.
  - **Per-posting `status`** defaults to the transaction-level `status`, which
    defaults to `pending`.
  - **`ord`** is assigned `0,1,2…` in array order so `get`/`register` render
    postings in the order supplied.
- **Immutability.** Once recorded, amounts/accounts/date are write-once. No tool
  updates them. The service layer never issues `UPDATE`/`DELETE` against those
  columns.
- **Correction** is `ledger_reverse(id)`: insert a new transaction whose postings
  are the sign-flipped mirror of the original (whole-transaction only, no partial
  legs), linked both ways (`reverses_id` on the mirror, `reversed_by_id` on the
  original). The mirror's legs reset to `status: pending` (a reversal hasn't
  cleared anything). Double-reversal is **blocked** via `reversed_by_id` (typed
  `already_reversed`); reversing a *reversal* is allowed (it re-creates the
  original effect). The original stays in the journal; the books show the mistake
  and its compensation.
- **Reconciliation** is the lone exception: `ledger_reconcile(posting_ids[],
  status)` transitions a posting's `status` only. **Free transitions** among the
  three states (incl. backward un-reconcile); setting to the current status is an
  idempotent no-op; an unknown `posting_id` fails the whole batch loudly
  (`not_found`, one tx). Orthogonal to reversal — status lives on the posting,
  links on the transaction. Filterable everywhere (`balance`/`register`'s `status`
  arg) so "cleared balance vs. ledger balance" is a first-class view.
- **Reads return raw signed sums** (ledger-cli convention): credit-normal roots
  (Liabilities/Equity/Income) report negative, debit-normal (Assets/Expenses)
  positive; a balance over *everything* sums to zero. `ledger_describe` publishes
  each root's `normal_balance` + recipes so the agent interprets/presents.
- **`query` matching** (`balance`/`register`) is case-insensitive **substring** on
  the full account path (`LIKE '%'||?||'%' COLLATE NOCASE`), single term, no regex,
  no separate prefix mode. **`period`** accepts a bucket string (`"2026"`,
  `"2026-06"`, `"2026-06-01"`) **or** an inclusive `{from,to}` range — a bucket
  expands to a from/to pair (one impl), filtered on the `YYYY-MM-DD` `date` column.
- **`record`/`reverse`/`reconcile` return the full transaction** (same rich shape
  as `get`) so the agent sees the resolved residual, assigned ids, and reversal
  links without a follow-up call.

`ledger_get` returns the rich transaction; `ledger_balance`/`ledger_register`
return aggregated/trimmed rows (enough to act on: account, amounts, running
total, status).

## 5. Data model / migrations

Greenfield the domain schema. The dev DB (`tmp/ledger.db`) is deleted and
recreated; no production data exists.

- **Keep** `001_schema_migrations.sql` (chassis migration tracking) byte-identical.
- **Add** `002_ledger.sql`:
  - `transactions(id ULID, date TEXT, description TEXT, created_at TEXT,
    reverses_id TEXT NULL, reversed_by_id TEXT NULL)`. `date` is a bare
    `YYYY-MM-DD` calendar day (lexically sortable, prefix-filterable for
    `period`); `created_at` is the RFC3339Nano insertion instant (ULID-aligned).
  - `postings(id ULID, txn_id TEXT FK, account TEXT, amount_cents INTEGER,
    status TEXT NOT NULL DEFAULT 'pending', ord INTEGER)`.
  - **No accounts table** — accounts are `SELECT DISTINCT account FROM postings`,
    type inferred from the root at query time. Pure emergent. (If discovery perf
    ever bites, materialize a small derived table then — not v1.)
  - Indexes: `postings(account)`, `postings(txn_id)`, `transactions(date)`,
    `postings(status)`.
- **Add** `003_outbox.sql` (library-owned; must stay byte-identical to
  `outbox.SchemaSQL`) when the producer is wired — see §6. The crm tree has a
  test asserting this byte-equality; carry the same discipline.

**Service-layer invariant, asserted on every `record`:** `Σ amount_cents per
txn = 0`. The DB stores the postings; the service guarantees they balance. Fail
loudly on violation rather than persisting an unbalanced transaction.

**No per-service audit store in v1.** The suite-wide "services keep their own
audit store" convention is unbuilt in the crm reference, and a *ledger's* audit
trail is intrinsic: the journal is immutable/append-only, corrections are linked
reversals, and `created_at` records when each fact landed. A generic audit/auth
log is a deliberate follow-up with its own migration, not v1.

**Books are global to the box** — single set of books per instance. No owner/
tenant column on `transactions`/`postings`; `Identity` (X-Owner-Email/X-Client-Id)
is consulted only by `ledger_whoami`, matching crm and the single-tenant model.

## 6. Events (event-plane producer)

Producer-only in v1. Keep the atomic-outbox pattern (event appended on the same
tx as the journal write; `Ring()` after commit).

- **First wave:** `transaction.recorded` — snapshot of the transaction and its
  postings, so a downstream consumer can react (reporting, notifications).
  **Every committed transaction emits exactly one `transaction.recorded`, no
  special case** — including a `ledger_reverse` mirror (one shared insert helper
  rings it). A consumer rebuilding balances off the feed would otherwise silently
  miss corrections; the reversal's payload carries `reverses_id` so a curious
  consumer can already tell it's a correction before the second-wave event exists.
- **Second wave (define payloads, wire when needed):** `transaction.reversed`
  (additive linkage context, not a replacement for the mirror's recorded event),
  `posting.reconciled` (payload defined now, emission unwired in v1).

**Deferred — ledger as consumer.** The obvious future hook (consume a
metered-usage / billing event to auto-draft the monthly hosting charge) is real
but explicitly out of v1 scope. The single-tenant box and the agent-driven
monthly batch cover the billing use case without it.

## 7. Keep / replace / delete (exact)

**Keep untouched (platform scaffolding):**
- `internal/db/db.go` (migration runner), `internal/ids`, `internal/logging`.
- `internal/server/*` — routing, PRM well-known, `requireIdentityHeaders`,
  whoami, security headers, graceful shutdown. (Already mounts `/mcp`, `/whoami`,
  PRM; `/feed` is added with the producer.)
- `internal/mcp/mcp.go` — the JSON-RPC transport. Only the injected service type
  changes.
- `bin/*`, `etc/*`, `cmd/ledger/main.go` (rewire service construction +
  eventplane wiring only), `go.mod`/deps (`ulid`, sqlite).
- Migration `001`.

**Replace:**
- `internal/mcp/tools.go` → the 8-tool surface + dispatch (§2, §4).

**Add:**
- `internal/db/migrations/002_ledger.sql` (§5); `003_outbox.sql` with the producer.
- `internal/ledger/` domain package (§8).

**Update docs last (done):** `ledger/CLAUDE.md` rewritten from the "skeleton"
framing to the real 8-verb surface, and this plan's status set to "built."

## 8. Proposed package architecture (built for parallel subagents)

**The crm chassis layering is preserved** (resolved in the grill pass — §8 is
*not* a second dispatch layer): `store.go` plays crm's `repo.go` role (SQL-only,
every method takes `*sql.Tx`), `service.go` plays crm's `service.go` role
(exposes typed domain methods, owns transactions + the balance invariant + event
emission — **it is not a string-dispatcher**), and `internal/mcp/tools.go`
remains the **sole** dispatcher (switch on `ledger_*` tool name → `Service`
method) **and** the arg-validation/normalization site (account canonicalization,
date parsing, elision well-formedness — exactly where crm does its `R-CTNS-NRMZ`
normalization). The one-file-per-concern split below is purely files inside the
one package so parallel agents don't collide; it is not an architectural layer.

```
internal/ledger/
  types.go         shared structs (Transaction, Posting), account-type table +
                   normal-balance/statement membership, field-decode helpers,
                   error sentinels (unbalanced, bad_root, validation, not_found,
                   already_reversed)
  store.go         SQL-only data layer (crm repo.go role): *sql.Tx methods,
                   shared scanning
  transaction.go   Service.Record + Service.Get: insert balanced postings,
                   elision resolution, get-by-id
  reverse.go       Service.Reverse: build & insert the mirror (legs → pending),
                   link both ways, already_reversed guard
  reconcile.go     Service.Reconcile: free status transitions (the one mutation)
  balance.go       Service.Balance: raw-signed report + live-chart (emergent tree)
  register.go      Service.Register: raw-signed postings + running totals
  describe.go      Service.Describe: roots + normal balances, conventions, live
                   tree, recipes
  service.go       the Service type: holds the store + outbox, owns tx, asserts
                   the balance invariant, emits events. The integration seam —
                   NOT a verb dispatcher.
  events.go        event payloads + builders
```

`internal/mcp/tools.go` holds the 8 descriptors, validates/normalizes args, and
dispatches into `ledger.Service` (`Record/Reverse/Reconcile/Balance/Register/
Get/Describe`), translating typed sentinels to MCP tool-error text — the same
sentinel→wire pattern crm uses.

## 9. Build phases & subagent fan-out

Dependency-ordered. **Bold** phases are integration points (sequential); the rest
fan out.

1. **Phase 0 — Foundation (sequential, one agent).** Write `002_ledger.sql`;
   scaffold `internal/ledger/` with `types.go`, `store.go`, `service.go`
   (dispatcher stubs); rewire `cmd/ledger/main.go` + `internal/mcp/mcp.go` to the
   new service so the tree **compiles** (empty behavior is fine). Gate:
   `go build ./...` green.

2. **Phase 1 — Journal core (sequential, one agent).** `transaction.go`
   (record with elision + balance assertion, get) and `reverse.go`. These are the
   spine every read depends on, and they establish the store conventions. Gate:
   record/get/reverse round-trip; unbalanced and bad-root errors fire.

3. **Phase 2 — Reads (fan out: 3 parallel agents).** One agent each for
   `balance.go`, `register.go`, `describe.go` against the schema and the journal
   core. They share `types.go`/`store.go` conventions from Phases 0–1. Gate:
   package compiles; per-report unit tests (balance roll-up by depth; register
   running totals; describe returns roots + live tree).

4. **Phase 3 — Reconciliation (sequential, one agent).** `reconcile.go` +
   `status` filtering threaded through `balance`/`register`. Gate: cleared vs.
   ledger balance differ correctly after a partial reconcile.

5. **Phase 4 — Tool surface (sequential, one agent).** `internal/mcp/tools.go`:
   8 descriptors with per-field docs in descriptions, dispatch into
   `ledger.Service`, typed error translation. Gate: MCP-level tests (tools/list
   shows 8; tools/call exercises each verb).

6. **Phase 5 — Events (can overlap Phase 4).** Add `003_outbox.sql`, wire the
   producer, emit `transaction.recorded` through the atomic outbox; confirm feed
   emission. Gate: a recorded transaction emits an event on `/feed`.

7. **Phase 6 — Docs (sequential, one agent).** Update `ledger/CLAUDE.md` to the
   new surface; refresh this plan's status to "built."

Verification throughout: `go build ./...`, `go test ./...`, then drive `/mcp`
directly over loopback (services trust injected `X-Owner-Email`/`X-Client-Id`),
and the billing walkthrough below as an end-to-end check.

## 10. Worked example — the hosting-billing use case

Confirms the surface covers the intended use (customers incurring hosting costs,
billed monthly) with **zero billing-specific entities** — the per-customer A/R
sub-ledger falls out of emergent accounts:

```
# June charge to Acme
ledger_record("2026-06-01", "Acme — June hosting", [
  {account: "Assets:Receivable:Acme", amount_cents: 5000},
  {account: "Income:Hosting"}                              # elided → -5000
])

# Acme pays, money lands cleared in the bank
ledger_record("2026-06-09", "Acme payment", [
  {account: "Assets:Bank:Checking", amount_cents: 5000, status: "cleared"},
  {account: "Assets:Receivable:Acme"}                      # elided → -5000
])
```

- **Who owes me?** → `ledger_balance(query:"Assets:Receivable")` — per customer.
- **Acme's statement** → `ledger_register(query:"Assets:Receivable:Acme")`.
- **June revenue** → `ledger_balance(query:"Income", period:"2026-06")`.
- **Reconcile the bank** → `ledger_balance(query:"Assets:Bank", status:"cleared")`
  vs. unfiltered → the difference is the uncleared items.

## 11. Open questions — all resolved (grill pass, 2026-06-03)

The earlier design open-questions (integrity model, emergent-but-typed chart,
single-currency cents, per-posting reconciliation, producer-only events, no
named-statement tool) and the three orchestration-time items below are **all
resolved**; the resolutions are folded into the sections above and consolidated
in §12.

- `period` syntax — **resolved: support both** a bucket string (`"2026"`,
  `"2026-06"`, `"2026-06-01"`) and an inclusive `{from, to}` range, one
  bucket→range impl over the `YYYY-MM-DD` `date` column.
- `query` matching — **resolved: case-insensitive substring** on the full account
  path, single term, no regex, **no separate prefix mode** (substring subsumes it).
- Reversal of an already-reversed transaction — **resolved: block** via
  `reversed_by_id` (typed `already_reversed`); reversing a *reversal* is allowed.

## 12. Consolidated resolutions (grill pass, 2026-06-03)

| # | Decision |
|---|---|
| Architecture | crm layering kept: `store.go` (SQL-only, `*sql.Tx`) + `service.go` (typed domain methods, owns tx/invariant/events) + `mcp/tools.go` as the **sole** dispatcher & arg-validation site. Per-file split is just files in the package, not a layer. |
| Sign convention | **Raw signed** sums in all reads; `describe` publishes `normal_balance` + recipes. Balance over everything = 0 (free correctness check). |
| Dates / period | `date` = bare `YYYY-MM-DD`; `created_at` = RFC3339Nano. `register` orders `date ASC, id ASC`. `period` = bucket string **or** `{from,to}`. |
| Tenancy | One global set of books per box; no owner column; `Identity` only in `whoami`. |
| Reconcile | Free transitions among the three states; idempotent no-op; loud all-or-nothing (`not_found`) on bad ids; reversal-orthogonal; `posting.reconciled` payload defined, unwired. |
| Record rules | ≥2 postings; ≤1 elision; residual→elided leg (zero allowed); root canonicalized (alias + case fold), sub-path case preserved; empty/control segments rejected; `ord` by array order; status inherits txn→`pending`; invariant re-asserted in service. |
| Query | Case-insensitive substring (`LIKE '%'||?||'%' COLLATE NOCASE`), no regex/prefix mode. |
| Reverse | Blocks double-reversal (`already_reversed`); reversing-a-reversal allowed; whole-txn only; mirror legs reset to `pending`; links both ways. |
| Audit | No per-service audit store in v1; immutable journal + reversal links + `created_at` are the audit trail. |
| Events | `reverse` emits `transaction.recorded` for the mirror (every committed txn emits, no special case); `transaction.reversed` additive second-wave. |
| Return shape | `record`/`reverse`/`reconcile` return the full transaction (same as `get`). |
| Misc | ULIDs for txn + posting ids; `003_outbox.sql` byte-identical to `outbox.SchemaSQL` with the carried-over byte-equality test; `describe` is hand-authored static content merged with a live `SELECT DISTINCT account` tree; typed error sentinels (`unbalanced`, `bad_root`, `validation`, `not_found`, `already_reversed`) translated to wire in `mcp/tools.go`. |
