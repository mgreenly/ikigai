# Decisions — MCP rebrand + health/whoami rework

Captured from the design discussion. These are the agreed decisions; implementation
has not started.

## Context

Two related goals, suite-wide across all services (dashboard, crm, ledger, notify,
dropbox, ralph, wiki):

1. **Rebrand the MCP tool surface** to the new top-level project name `ikigenba`.
   (`ikigenba` will eventually also become the deployment domain — `<svc>.ikigenba.com`
   — but no domain change is being made now; only the tool names change.)
2. **Retire `whoami` in favor of a generic `health`** surface, on both the MCP tool
   and the shared HTTP endpoint.

## Decisions

### 1. MCP tool naming

- Every MCP tool is renamed `ikigenba_<service>_<toolname>` (was `<service>_<toolname>`).
  Applies to all tools in every service, not just the health/whoami one.
- The `ikigenba_<service>_` prefix is defined **once per service** (a `toolPrefix`
  const + small `tool()` helper) and used in both the descriptor list
  (`toolDescriptors`) and the dispatch `switch`, so the two sites can't drift.
- In-description **cross-references** to sibling tools (e.g. "use crm_log",
  "poll with wiki_job_status") are updated to the new full `ikigenba_<svc>_*` names —
  these are agent-facing and a stale reference is a defect.

### 2. `whoami` → `health` (MCP tool)

- Each service's `<svc>_whoami` MCP tool becomes `ikigenba_<svc>_health`.
- The branding prefix is an **MCP-tool-name** convention only. HTTP route *paths* are
  not branded (they stay `/srv/<svc>/...`).

### 3. The health envelope (the contract)

A fixed required-key envelope at the top level, plus per-service custom data namespaced
under a single `details` key. Custom keys are **not** splatted at the top level.

**Required top-level keys — filled by appkit, identical for every service:**

| key       | source                                  |
|-----------|-----------------------------------------|
| `status`  | appkit, defaults `"ok"`                 |
| `version` | appkit, from `versionString()`          |
| `service` | appkit, from `spec.App`                 |

These cost **zero per-service plumbing**: appkit already owns the serve wiring
(`server.New` in `appkit/verbs.go`) and the build-stamped version
(`appkit.version` / `versionString()`), and already has the app name in `spec.App`.

**`details` — per-service custom telemetry:**

- Supplied by an optional per-service reporter hook (a new `Spec.Health`), following the
  exact pattern of the existing `Spec.Handlers` / `Spec.Producer` / `Spec.Feed` hooks.
- `details` is **always present** — an empty `{}` when a service supplies no reporter —
  so consumers never branch on its presence.
- Required top-level keys are reserved; a reporter contributes only to `details`.
- Example (dropbox): `details: { mirror_bytes, disk_free_bytes, disk_total_bytes, failed_files }`
  (dropbox's current top-level telemetry moves under `details`).

### 4. Two transports, one builder

- appkit owns a single **envelope builder**; both the HTTP `/health` route and the MCP
  `ikigenba_<svc>_health` tool render through it, so they cannot diverge.
- A service author writes its telemetry **once** (its `Health()` reporter populating
  `details`); both surfaces stay in lockstep automatically.

### 5. HTTP `/srv/<svc>/health` — liveness, ungated

- The dashboard's liveness need is satisfied by a **200 OK**; it does not need identity.
- Renamed from `GET /whoami` → `GET /health` in appkit (shared by every path-routed
  service via the chassis).
- **Ungated** (joins PRM and `/feed`, which appkit already mounts unauthenticated) so it
  is a true liveness signal that survives an auth outage — letting the dashboard
  distinguish "service down" from "auth down".
- Body: `{ status, version, service, details }` (no identity).

### 6. MCP `ikigenba_<svc>_health` — diagnostics, gated, carries identity

- Behind the identity gate (MCP always is), so it additionally includes
  `owner_email` / `client_id`.
- Payload = the HTTP `/health` body **plus** the two identity fields:
  `{ status, version, service, owner_email, client_id, details }`.
- The connect-time **auth-chain proof** (token → header → echo) that `whoami` used to
  provide now lives here, on the MCP tool — which fits "the product surface is MCP".

### 7. Dropbox — fold the two tools into one

- Dropbox currently has both `dropbox_whoami` and a richer `dropbox_health`.
- Collapse to the single `ikigenba_dropbox_health`; drop `whoami`. Its telemetry moves
  under `details` per the envelope contract.

## Change inventory (for the implementation pass)

1. **appkit** (`appkit/server`, `appkit/verbs.go`): rename `GET /whoami` → ungated
   `GET /health`; add the envelope builder (`status`/`version`/`service` + `details`);
   add the `Spec.Health` reporter hook and thread it Spec → `server.Options`; update
   `handlers.go` / `server.go` and `server_test.go`.
2. **All 6 services** (`*/internal/mcp/tools.go` + tests): `tool()`/`toolPrefix` helper;
   rename every tool to `ikigenba_<svc>_*`; `whoami` → `health`; update in-description
   cross-references; update `*_test.go`.
3. **dropbox**: fold `whoami` into the single `ikigenba_dropbox_health`; telemetry under
   `details`.
4. **ralph** `describe.go` prose; **docs** (runbooks, event-plane, extraction-map)
   references to `<svc>_whoami`.
