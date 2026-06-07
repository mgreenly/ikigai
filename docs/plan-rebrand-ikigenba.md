# Plan — Rebrand to ikigenba

Status: **Phases A–E complete — the suite is the sole live deployment on
`int.ikigenba.com`** (200 over valid TLS; all 7 units active; all 6 MCP services
advertised — see the "Phase A results" through "Phase E results" blocks). Phase 0
decisions locked (§6). **Phase E — `ai` teardown — COMPLETE** (account closed via
`aws organizations close-account` → SUSPENDED; mgmt Terraform plans clean; local
`ai` ssh/aws config removed; only residual is AWS's ~90-day deletion window, which
needs no action — see the "Phase E results" block). This is the execution
plan for renaming the project's public
identity from **metaspot/ikigai** to **ikigenba**, deployed to
**int.ikigenba.com**. It spans **two repos**: this one (the per-account suite)
and the sibling infra repo `../metaspot` (Terraform/AWS/DNS), whose specs are
authoritative — on any conflict, that repo wins.

The original draft of this plan deferred the high-risk "codename" rename
(`METASPOT_*` plumbing) to a separate, later phase, on the theory that renaming
it would break running boxes and migrate persistent data. **The Phase 0
decisions removed every premise of that theory** (see §6 / Q4), so the codename
rename is now folded into the greenfield stand-up of `int`. There is no separate
codename phase, no parallel-run window, and no data migration.

---

## 1. The decided model (glossary)

Two brand names collapse to one, plus one descriptive noun. **Both the brand hat
and the codename hat come off together**, because `int` is a brand-new box with
no data — it is simply *born* on the new names.

| Concept | Old | New |
|---|---|---|
| Business / platform / org / brand | `metaspot` | **ikigenba** |
| Per-account software (this repo) | `ikigai` | **the suite** (a common noun, not a brand) |
| Per-account domain | `<account>.metaspot.org` | `<account>.ikigenba.com` |
| The one live account | `ai` (`ai.metaspot.org`) | `int` (`int.ikigenba.com`) |
| Internal codename plumbing | `METASPOT_*`, `/etc/metaspot/env`, `metaspot-launch`, SSM `/metaspot/<acct>`, buckets `*-metaspot-org-*` | `IKIGENBA_*`, `/etc/ikigenba/env`, `ikigenba-launch`, SSM `/ikigenba/<acct>`, buckets `*-ikigenba-com-*` |
| Google Workspace owner identity | `logic-refinery.com` | **`logic-refinery.com` (unchanged — brand-independent)** |
| GitHub repo / `origin` / local checkout | `mgreenly/ikigai`, `~/projects/ikigai` | **unchanged (deferred — see §2)** |
| The unrelated sibling projects | `ikigai-cli`, `ikigai-tui` | **unchanged — different projects, keep their name** |
| The Dropbox app folder | `ikigai-onebox` | **unchanged (opaque, owner-only — see §2)** |

Key principles applied:

- **`ikigenba` is the one brand.** "suite" is a descriptive common noun for the
  per-account software, not a third brand to maintain.
- **`int` is dogfooding.** ikigenba runs its own business from the `int` account
  on the same suite chassis every customer runs.
- **`int` is greenfield, so there are no "two hats."** The original plan
  separated the customer-visible *brand* (`metaspot.org`) from the invisible
  *codename* (`METASPOT_*`, launcher, buckets, SSM) and removed them in separate
  phases to protect a running box. With `ai` abandoned (no data) and `int` built
  fresh, there is no running box to protect — `int` is provisioned on
  `IKIGENBA_*` from its first boot. Doing the codename rename **now**, while the
  cost is zero, avoids a future live-box migration whose cost only grows as `int`
  accumulates backups and live config (see §6/Q4 for the asymmetry argument).

---

## 2. Explicitly out of scope (deferred follow-ups)

These are intentionally **not** done in this effort. Each is independent and
costs nothing to defer.

- **GitHub org/repo move and local folder rename** — `mgreenly/ikigai` →
  `ikigenba/suite`, `~/projects/ikigai` → `~/projects/suite`. Deferred to a
  later, separately-scheduled process. Safe to defer indefinitely: Go modules
  are bare names (`module crm`, `module appkit`, …) and every `replace` is a
  relative path, so nothing in-repo depends on the remote URL or checkout
  location — the move is pure repository administration with zero code impact,
  equally cheap whenever it happens.
- **The Dropbox app folder `ikigai-onebox`** — renaming forces re-registering
  the Dropbox app and re-syncing the mirror, for an opaque name only the account
  owner ever sees in their own Dropbox. Left as-is (permanently acceptable).
- **The `ikigai-cli` / `ikigai-tui` sibling projects** — genuinely unrelated
  software (`github.com/ai4mgreenly/ikigai-cli`) that `agentkit` was ported from
  and references ~34× in code comments. **Never touched** by this rebrand.
- **`/sites/<name>/` path namespace and the `www` landing page** — a future
  chassis + nginx feature (a second path namespace parallel to `/srv/<svc>/` for
  hosted static sites). `ikigenba.com` serves no content today and will continue
  to until that feature ships on its own.
- **The tier-2 control plane** (create/provision/bill accounts) — future
  services on the same chassis, deployed to `int`.
- **`ai` account teardown** — handled entirely in the infra repo by an agent in
  `~/projects/metaspot` that `terraform destroy`s the `ai` account's resources.
  Not a step in this repo's plan; just a hand-off once `int` is live.

---

## 3. Surface inventory (what the rebrand touches)

Repo-wide counts (tracked files, case-insensitive): `metaspot` ≈ **391×**,
`ikigai` ≈ **130×**. The `ikigai` count is dominated by the **unrelated**
`ikigai-cli` (~34×, in `agentkit/*` comments) plus the repo path and the
sibling-project references — **most `ikigai` occurrences are NOT this suite** and
must be left alone (see §3c).

### 3a. Customer-facing brand/domain (`metaspot.org` → `ikigenba.com`)

- **Deploy config (the functional core):** `bin/ship` derives
  `HOST="${HOST:-${ACCOUNT}.metaspot.org}"` (line 130; comment at 21–22).
  Per-service `etc/deploy.env` (×7: dashboard, crm, ledger, notify, dropbox,
  wiki, ralph) set `ACCOUNT=ai` and `SSH_KEY=~/.ssh/id_ed25519_ai4mgreenly`.
- **opsctl:** `--domain` flag help (`opsctl/cmd/opsctl/main.go:361`) and
  `initbox.go` / `provision_test.go` fixtures reference `ai.metaspot.org` / the
  `__DOMAIN__` template var.
- **Go test constants (many):** hardcoded `https://ai.metaspot.org…` resource
  IDs and auth-server URLs in `appkit/config/config_test.go`,
  `appkit/server/server_test.go`, across `dashboard/internal/server/*_test.go`,
  `dashboard/cmd/dashboard/main_test.go`, and `bin/registry.test.sh`. These are
  expectations, not production strings — they move when the derived value moves.
- **One stray user-facing string:** `wiki/internal/ingest/url.go` sets
  `User-Agent: metaspot-wiki/1 (+https://metaspot.org)`.

### 3b. Codename plumbing (`METASPOT_*` → `IKIGENBA_*`) — now in scope

The original plan deferred this; it is now part of the greenfield build. Actual
blast radius in code is small — only **two real env-read sites**:

- `appkit/config/config.go:68` — `getenv("METASPOT_DOMAIN")` (composes
  `RESOURCE_ID` / `AUTH_SERVER`; also the source of the dashboard's
  `publicBaseURL`, hence the OAuth redirect URI).
- `dashboard/internal/backup/backup.go:277–285` — `METASPOT_BACKUP_BUCKET`,
  `METASPOT_AWS_REGION`, `METASPOT_DOMAIN`.

A third **real** site (missed by the original inventory) is the per-app secret
seeder — it composes the SSM path from the codename:

- `*/bin/secrets` (×5: dashboard, dropbox, notify, ralph, wiki) —
  `PARAM="/metaspot/${ACCOUNT}/app-config"` → `/ikigenba/${ACCOUNT}/app-config`;
  also the `aws sso login --profile ai` header comments in `dropbox/bin/secrets`
  and `ralph/bin/secrets` → `int`. (The `ACCOUNT`/profile value itself already
  flows from `etc/deploy.env`, handled in §3d.) **If this string isn't changed
  the launcher reads `/ikigenba/int/app-config` while `bin/secrets` writes
  `/metaspot/int/app-config` — the box boots with no secrets.**

Everything else is **template strings and comments**:

- `opsctl/internal/opsctl/templates.go` — `EnvironmentFile=/etc/metaspot/env`,
  `ExecStart=/usr/local/bin/metaspot-launch %s`, the `metaspot-certbot-renew.{timer,service}`
  units and their descriptions; plus references in `layout.go`, `setup.go`,
  `deploy.go`, `initbox.go`, `opsctl.go`.
- Doc comments naming `METASPOT_DOMAIN` in `appkit/appkit.go`,
  `appkit/config/config.go`, and each `cmd/<svc>/main.go`.

> The env-var **name** (`METASPOT_DOMAIN` → `IKIGENBA_DOMAIN`) and the env-var
> **value** (`int.ikigenba.com`) are now both `ikigenba`. In the original phased
> plan they diverged (codename name stayed `metaspot`, value became `ikigenba`);
> that divergence no longer exists.

### 3c. Retire `ikigai` (this suite only — NOT the sibling projects)

**Go module paths need NO change** — every module is a bare name (`module crm`,
`module appkit`, …), no `ikigai/` or domain prefix; imports are untouched. The
single biggest risk-reducer.

The rename target is **only** bare `ikigai` referring to *this suite*:
`README.md` / `AGENTS.md` titles, the "ikigai app chassis" package doc comments
in `appkit/*`, the `opsctl — ikigai on-box platform CLI` banner, "the ikigai
suite" in `docs/*`.

**Hard disambiguation rule — this is an allowlist edit, never a `sed`:** never
touch any token matching `ikigai-cli`, `ikigai-tui`, `ai4mgreenly/ikigai-cli`,
or `ikigai-onebox`. A blind `s/ikigai/ikigenba/` would corrupt ~34 unrelated
`ikigai-cli` provenance comments across `agentkit/*` and the
`github.com/ai4mgreenly/ikigai-cli` references. (Verified: those references are
**comments only — zero live imports**, so the risk is silent provenance
corruption, not a build break — still unacceptable.) `agentkit/*` ends this work
byte-for-byte unchanged.

### 3d. Account `ai` → `int`

- All 7 `etc/deploy.env`: `ACCOUNT=ai` → `int`, and `SSH_KEY` →
  `~/.ssh/id_ed25519_int_ikigenba_com` (new key — §6/7, confirmed). The
  `ACCOUNT=int` value also re-points each `bin/secrets`' AWS profile and SSM
  path (§3b).
- **New SSH key (machine-local):** `ssh-keygen -t ed25519 -N "" -C
  int@ikigenba.com -f ~/.ssh/id_ed25519_int_ikigenba_com` — **no passphrase**
  (matches the existing key; required so the autonomous phase can `ssh`/deploy
  without an agent prompt). Its `.pub` is baked into `int/`'s
  `aws_key_pair.public_key` (§3e). Generated in the **interactive phase**
  (§4 Phase A).
- `~/.ssh/config` (machine-local, not in repo): add
  ```
  Host int.ikigenba.com int
    HostName int.ikigenba.com
    User ec2-user
    IdentityFile ~/.ssh/id_ed25519_int_ikigenba_com
    IdentitiesOnly yes
  ```
  mirroring the existing `ai` block, so `ssh int` works simply. The old `ai`
  entry is dropped once `ai` is destroyed (§4 Phase E).
- Docs/runbooks that show `ssh ai …` and `ai.metaspot.org` examples → `int`.

### 3e. Infra repo `../metaspot` — stand up `int.ikigenba.com` (greenfield, ikigenba codename)

The infra repo already anticipates this: `mgmt/ikigenba.tf` creates the
`ikigenba.com` / `ikigenba.dev` Route 53 zones (imported, empty), and `AGENTS.md`
documents `<account>.ikigenba.com` delegation to member accounts. Standing up
`int` mirrors the existing `ai` account, **but parameterized with the `ikigenba`
codename names from the start** (not `metaspot`):

- `mgmt/accounts.tf` — add an `int` entry to `local.member_accounts`
  (`name = "int"`, `email = mgreenly+int@gmail.com`, `admin = true`). AWS issues
  the account ID. (Verified shape: `prod/test/sandbox/ai/...` entries already
  present; `aws_organizations_account.this` + the admin SSO assignment iterate
  `for_each` over the map, so the `int` box, EIP, etc. follow automatically.)
- `mgmt/ikigenba.tf` — add a `delegation_int_ikigenba` NS record against
  `aws_route53_zone.ikigenba_com`, from the new `int/` root's nameserver output.
  **Corrected during Phase A:** the original draft said this goes in
  `delegations.tf` like `delegation_ai`; AGENTS.md §16 is explicit that
  `ikigenba.com` delegations live in `ikigenba.tf` (`delegations.tf` is
  `metaspot.org`-only). The records were hardcoded literals from `int/`'s
  `hosted_zone_name_servers` output — no `mgmt/outputs.tf` wiring needed.
- `bootstrap/int/` — new tfstate backend bucket (copy `bootstrap/ai/`; these
  roots use **local** `terraform.tfstate`, so no chicken-and-egg).
- **Templates: add, don't rename.** Add `templates/ikigenba-launch` and
  `templates/ikigenba-env.sh.tftpl` (copies of the `metaspot-*` originals).
  **Rename their *contents*, not just the filenames** — a filename-only copy
  leaves `METASPOT_*` inside and the box boots on the wrong names. Specifically:
  - `ikigenba-env.sh.tftpl` (cloud-init user_data): `/etc/metaspot/env` →
    `/etc/ikigenba/env`; every `METASPOT_*` var it writes (`ENV/NODE/FQDN/DOMAIN/
    DNS_ZONE/AWS_ACCOUNT_ID/AWS_REGION/BACKUP_BUCKET`) → `IKIGENBA_*` (must match
    what `appkit/config/config.go` + `dashboard/.../backup.go` read after §3b);
    it `cat`s the launcher to `/usr/local/bin/metaspot-launch` → `ikigenba-launch`.
  - `ikigenba-launch` (the launcher script): the `/usr/local/bin/ikigenba-launch`
    self-reference, `source /etc/ikigenba/env`, and the SSM fetch path
    `/ikigenba/<env>/app-config` — `<env>` resolves to `int`, so it reads
    `/ikigenba/int/app-config`, the exact path `bin/secrets` writes (§3b).
  **Leave `templates/metaspot-*` in place** so the still-existing `ai/` root
  doesn't break `terraform plan` before it's destroyed (§4 Phase E removes them
  with `ai`).
- `int/` — new per-account root copied from `ai/`, reparameterized to: zone
  `int.ikigenba.com`; `providers.tf` `profile = "int"` and backend
  `bucket`/`key` for the new bootstrap bucket; `templatefile` vars
  `env/node/fqdn/domain` → `int` / `int.ikigenba.com`; new account ID;
  `aws_key_pair.public_key` = the new `id_ed25519_int_ikigenba_com.pub` (§3d);
  **backups bucket `int-ikigenba-com-<id>`; SSM path `/ikigenba/int/app-config`;
  `IKIGENBA_DOMAIN=int.ikigenba.com`; `user_data` → `templates/ikigenba-env.sh.tftpl`;
  the launcher `ikigenba-launch`.** The env-var/bucket/SSM/launcher names must
  **agree with this repo's renamed `opsctl` templates and `bin/secrets`** (§3b)
  — agreement is automatic since both sides use the locked §1 names.
- **A new `int` AWS profile** in `~/.aws/config` (machine-local): copy the `ai`
  block, set `sso_account_id = <new id>` (same `sso_session = metaspot`,
  `sso_role_name = AdministratorAccess`, `region = us-east-2`). The live SSO
  session assumes it with no new browser login.

> **Add, don't rename** (§6/Q5): add a fresh `int` account rather than rename
> `ai`. AWS Organizations accounts don't rename cleanly. `ai` is then destroyed
> wholesale by the infra-repo agent (§2) — no cutover, no soak, because `ai`
> holds no data.

---

## 4. Phases

**Organizing axis (decided): "needs the human present" vs "fully autonomous,"
not "infra vs suite."** Every step that requires a prompt — terraform `apply`
confirmations, `ssh-keygen`, machine-local config edits — is **front-loaded into
Phase A**, run by the **main agent** (subagents cannot prompt the user). Once
Phase A is signed off, Phases B–D run as **strictly-sequential subagents with no
prompts**. The terraform stand-up therefore comes **first** (most of the prompts
live there), then the suite rename and deploy run unattended.

Each code phase ends green (`go build ./...` && `go test ./...` at repo root;
`terraform plan` clean for infra). The codename and brand rename land together —
`int` is greenfield, nothing to stage around.

**Execution preconditions (verified this session):**
- **SSO is live** (`aws sts get-caller-identity --profile mgmt|ai` ✓). Agents run
  terraform against the existing session; **no agent ever runs `aws sso login`**
  (it's a browser flow). If a session expires mid-run, the agent stops and asks.
- **Apply gates:** agents run `terraform plan`, the main agent shows the plan and
  **waits for explicit go before each `apply`** — at minimum the `mgmt`
  account-creation apply (the one irreversible step: the email is burned ~90
  days). Manual verification gates in the TF process are wanted (§6/Q7).
- **OAuth callback whitelist is already done** (owner-confirmed) — not a step.
- **Git: all work directly on `main`, both repos, pushed to `origin`** (no
  feature branch — owner-authorized). Commits carry the `Co-Authored-By` trailer.

### Phase 0 — Decisions & naming lock (no code) — **DONE**

All open decisions resolved; recorded in §6. This document is the output.

### Phase A — Interactive bring-up (main agent; **all prompts live here**) — **DONE**

Stand up `int.ikigenba.com` infra to a reachable, empty, healthy box. Repo
`../metaspot` (authoritative) plus three machine-local files. Ends with the box
up — **no suite code involved**, so this is independent of the rename and goes
first.

> **Phase A results (2026-06-06) — captured facts for Phase B–D.**
> All seven steps executed and verified; committed + pushed to `../metaspot`
> `main` (commit `e3219ae`, "int: stand up int.ikigenba.com account, box, and
> NS delegation"). Both `mgmt` and `int` terraform roots are no-op clean.
>
> | Fact | Value |
> |---|---|
> | AWS account `int` | **`704229156466`** (ACTIVE, admin SSO, email `mgreenly+int@gmail.com`) |
> | Box public IP (EIP) | **`16.59.0.148`** |
> | `int.ikigenba.com` hosted zone | `Z09695053HDYS8VPS6CD8` |
> | `ikigenba.com` apex zone (mgmt) | `Z03790722PQEX6XC1Q4WR` |
> | AL2023 AMI (us-east-2, pinned) | `ami-078f95be0757084a3` (resolved 2026-06-06) |
> | tfstate bucket | `metaspot-int-tfstate-704229156466` |
> | backups bucket | `int-ikigenba-com-704229156466` |
> | SSM app-config param | `/ikigenba/int/app-config` (`{}` placeholder) |
> | SSH | `~/.ssh/id_ed25519_int_ikigenba_com` (no passphrase); `ssh int` works |
> | AWS profile | `int` (in `~/.aws/config`, sso_account_id `704229156466`) |
>
> **Verified on-box:** AL2023, cloud-init `done`, `/etc/ikigenba/env` written
> with all `IKIGENBA_*` identity vars (ENV/NODE/FQDN/DOMAIN=int·int.ikigenba.com,
> ACCOUNT_ID/REGION/BACKUP_BUCKET), `/usr/local/bin/ikigenba-launch` installed
> (0755 root:root), launcher deps `aws`+`jq` present. DNS resolves on public +
> authoritative resolvers.
>
> **Deviation from §3e (infra repo wins):** the NS delegation was added to
> `mgmt/ikigenba.tf` (record `delegation_int_ikigenba`, against
> `aws_route53_zone.ikigenba_com`), **not** `mgmt/delegations.tf` — AGENTS.md §16
> is explicit that `ikigenba.com` delegations live in `ikigenba.tf`.
> `delegations.tf` only holds `metaspot.org` records.
>
> **TLS deferred:** the Phase A gate listed "valid TLS," but nothing serves 443
> until the **dashboard** issues the apex cert during deploy. The box is
> correctly empty; TLS is verified at the end of **Phase D**, not here.
>
> **Codename-name agreement to honor in Phase B:** the box reads
> `/etc/ikigenba/env` (`IKIGENBA_*`), launches via `/usr/local/bin/ikigenba-launch`,
> and the launcher fetches SSM `/ikigenba/int/app-config`. Phase B's renamed
> `appkit/config/config.go`, `dashboard/.../backup.go`, `opsctl` templates, and
> `*/bin/secrets` must match these exact names.

1. **New SSH key** — `ssh-keygen -t ed25519 -N "" -C int@ikigenba.com -f
   ~/.ssh/id_ed25519_int_ikigenba_com` (no passphrase — §3d).
2. **Infra templates** — add `templates/ikigenba-launch` +
   `templates/ikigenba-env.sh.tftpl` (copies, reparameterized); leave
   `metaspot-*` (§3e).
3. **`mgmt` account** — add `int` to `local.member_accounts`; `plan`; **confirm
   gate**; `apply`; capture the account ID.
4. **Machine-local config** — add the `int` profile to `~/.aws/config` (copy
   `ai`, swap `sso_account_id`); add the `Host int.ikigenba.com int` block to
   `~/.ssh/config` (§3d).
5. **`bootstrap/int/`** — copy `bootstrap/ai/`; `plan`/`apply` (tfstate bucket).
6. **`int/` root** — copy `ai/`, reparameterize (§3e): zone, `profile = "int"`,
   backend, `aws_key_pair.public_key` = the new pubkey, bucket
   `int-ikigenba-com-<id>`, SSM `/ikigenba/int`, `IKIGENBA_DOMAIN`,
   `ikigenba-launch`/`ikigenba-env.sh.tftpl`; `plan`; **confirm gate**; `apply` —
   creates zone, box, EIP, A/wildcard records, backups bucket, SSM placeholder.
7. **NS delegation** — add `delegation_int_ikigenba` to `mgmt/delegations.tf`
   from `int/`'s nameserver output; `apply` in `mgmt`. Verify `dig NS
   int.ikigenba.com`.

Gate: `int.ikigenba.com` resolves; valid TLS; box reachable over `ssh int`;
`terraform plan` clean in `mgmt`/`int`. **Sign-off here is the hand-off from
interactive to autonomous.**

### Phase B — Suite: the full rename (autonomous; this repo) — *was Phase 1* — **DONE**

The functional rename, committed to `main` and pushed when green.

> **Phase B results (2026-06-06) — captured facts for Phase C–D.**
> The full rename landed in one commit on `main` (commit `9ddd2ba`, **97 files**,
> pushed to `origin`). Gate met: all **11 modules** `go build` + `go test` green.
>
> - **Codename `METASPOT_*` → `IKIGENBA_*`** across appkit config, dashboard
>   backup, opsctl templates/units, and the 5 `*/bin/secrets` SSM paths
>   (`/metaspot/` → `/ikigenba/`). Extended beyond the §3b inventory to
>   `crm|ralph/bin/{backup,restore}` and the host-deriving
>   `bin/{start,stop,teardown}` scripts that read the same box env.
> - **Brand/domain `metaspot.org` → `ikigenba.com`:** promoted the apex suffix to
>   a new **`APEX_SUFFIX`** `deploy.env` var (default `ikigenba.com`) so no domain
>   literal remains in `bin/ship` (§6/Q6); updated ship/opsctl help + fixtures/
>   tests, `wiki/internal/ingest/url.go` User-Agent
>   (`ikigenba-wiki/1 (+https://ikigenba.com)`), and README/AGENTS/docs prose.
> - **Account `ai` → `int`:** all 7 `etc/deploy.env` now `ACCOUNT=int`,
>   `SSH_KEY=~/.ssh/id_ed25519_int_ikigenba_com`, `APEX_SUFFIX=ikigenba.com`;
>   `--profile ai` → `int` in secrets headers/docs.
> - **Retired bare-suite `ikigai`** via the hand allowlist (README/AGENTS titles →
>   "suite", appkit package docs, opsctl banner, docs prose).
> - **Intentionally preserved** (per §2/§3c/§6): the sibling tokens `ikigai-cli` /
>   `ikigai-tui` / `ai4mgreenly/ikigai-cli` / `ikigai-onebox`; the googleidp
>   self-consistent stub `StubIdentity.HostedDomain="metaspot.org"` (§6/Q2); the
>   infra-repo `../metaspot` references and the metaspot-prefixed tfstate / AWS-org
>   identity; and the deferred GitHub-repo / `go.work` / local-checkout rename
>   (§2/Q4).

1. **Codename → `IKIGENBA_*` (§3b):** `appkit/config/config.go`
   (`METASPOT_DOMAIN` → `IKIGENBA_DOMAIN`); `dashboard/internal/backup/backup.go`
   (the three `METASPOT_*` reads); `opsctl/internal/opsctl/templates.go` +
   `layout.go`/`setup.go`/`deploy.go`/`initbox.go`/`opsctl.go`
   (`/etc/metaspot/env` → `/etc/ikigenba/env`, `metaspot-launch` →
   `ikigenba-launch`, `metaspot-certbot-renew` → `ikigenba-certbot-renew`);
   **`*/bin/secrets` `PARAM` path `/metaspot/` → `/ikigenba/` (×5) + the
   `--profile ai` header comments → `int` (§3b)**; all doc comments naming
   `METASPOT_*`.
2. **Brand/domain → `ikigenba.com` (§3a):** `bin/ship` — promote the apex suffix
   to a `deploy.env` variable so no domain literal remains in the script (§6/Q6);
   the default becomes `${ACCOUNT}.ikigenba.com`. `opsctl` `--domain` help +
   `provision_test.go`/`initbox.go` fixtures → `int.ikigenba.com`. Test constants
   `ai.metaspot.org` → `int.ikigenba.com` across `appkit`/`dashboard`/`bin/registry.test.sh`.
   `wiki/internal/ingest/url.go` User-Agent → `ikigenba-wiki/1 (+https://ikigenba.com)`.
3. **Account `ai` → `int` (§3d):** all 7 `etc/deploy.env` (`ACCOUNT=int`,
   `SSH_KEY=~/.ssh/id_ed25519_int_ikigenba_com`).
4. **Retire suite `ikigai` (§3c):** allowlist edit of bare-suite `ikigai` only —
   `README.md`/`AGENTS.md` titles, `appkit` package doc comments, the `opsctl`
   banner, `docs/*` prose, `dashboard/docs/phases.md`, `docs/runbook-*.md`,
   `nginx/*`. **Never touch `ikigai-cli`/`ikigai-tui`/`ai4mgreenly`/`ikigai-onebox`.**

Gate: `go build ./...` && `go test ./...` green; no `metaspot` string and no
bare-suite `ikigai` string remains (the sibling-project `ikigai-*` tokens and the
deferred GitHub references are expected to remain). Commit to `main`; push.

### Phase C — Bridge: provision the box from the renamed suite (autonomous) — **DONE**

Needs Phase A (box exists) **and** Phase B (renamed `opsctl`/`bin/secrets`).

> **Phase C results (2026-06-06) — captured facts for Phase D.**
> The box was greenfield (no `opsctl` present). Built a fresh ikigenba-named
> `opsctl` (static `linux/amd64` from `opsctl/cmd/opsctl`) and installed it to
> `/usr/local/bin/opsctl`.
>
> - **`opsctl init-box --skip-cert`** (TLS deferred), then **`opsctl setup`** for
>   all 7 services — units enabled-not-started, nginx fragments staged. Port map:
>   dashboard **3000** (apex), crm **3001**, ledger **3002**, notify **3003**,
>   ralph **3004**, dropbox **3005**, wiki **3006**.
> - **Seeded all 5 secret-bearing apps** into SSM `/ikigenba/int/app-config`
>   (final keys: dashboard, dropbox, notify, ralph, wiki).
>   `WIKI_OWNER = michaelgreenly@logic-refinery.com` (authoritative per §6
>   Decision #2; passed as an env override — no `~/.secrets/WIKI_OWNER` file
>   needed). Values originated locally; no migration from `ai`.
> - **opsctl fix `ae6ef00`:** defer nginx validate/reload (and add
>   `setup --defer-nginx`) on a greenfield box whose apex `:443` block references
>   a not-yet-existent cert.

1. `opsctl init-box` on the new box (Phase-B `opsctl`), then `opsctl setup <app>`
   per service as the deploy model requires.
2. **Seed secrets** — run each `<app>/bin/secrets` (after SSO). It read-modify-
   writes only that app's key in `/ikigenba/int/app-config`, sourcing values from
   `~/.secrets/` (the names each app's `.envrc` identifies), **masks every secret
   in its summary, never prints a value**, and `--value file://`. The agent feeds
   `yes` and only ever sees masked output — compliant with the secrets rule. Not
   migrated from `ai`; values originate locally. Covers `GOOGLE_*`
   (`GOOGLE_WORKSPACE_DOMAIN=logic-refinery.com` unchanged — §6/Q2), `DROPBOX_*`,
   `ANTHROPIC_API_KEY`, etc. — the per-app key set is whatever that app's
   `bin/secrets`/`getenv` requires.

Gate: `ikigenba-launch` finds `.["<app>"]` for every service that needs secrets
(launcher hard-fails otherwise); box healthy.

### Phase D — Suite: deploy to `int` (autonomous) — **DONE — suite LIVE**

> **Phase D results (2026-06-06) — the suite is live on `int.ikigenba.com`.**
> All 7 apps deployed via bump → ship → stage → deploy; final HEAD `226a888`,
> pushed to `origin/main` (confirmed via `git ls-remote`).
>
> - **Deployed (services first, dashboard last):** crm **v0.2.2**, ledger
>   **v0.2.2**, notify **v0.2.2**, dropbox **v0.2.3**, ralph **v0.2.1**, wiki
>   **v0.2.2** — each `active`, `manifest.env` present — then dashboard
>   **v0.1.1** (commit `226a888`) **last**, so it read every
>   `/opt/*/etc/manifest.env` at startup.
> - **Two first-boot bugs found & fixed:** dropbox `8006af8` (derive
>   `DROPBOX_MIRROR_PATH` from the data dir, not the unwritable cwd `./tmp`) and
>   wiki `efb2013` (same for `WIKI_DATA_ROOT`). opsctl only stamps DB/generation
>   path vars into `manifest.env`, so these per-service writable paths fell back
>   to dev defaults under root-owned `/opt/<svc>` — now derived from the DB path's
>   directory.
> - **TLS issued:** brought nginx up and issued the apex Let's Encrypt cert
>   (`CN=int.ikigenba.com`). **opsctl fix `530ab17`:** bootstrap the FIRST apex
>   cert via `certbot certonly --standalone` (binds :80 itself, before any
>   `nginx -t`) then reconcile onto webroot for renewals — resolves the greenfield
>   first-cert chicken-and-egg; idempotent via a `CertExists` check.
> - **Verified live:** all 7 units `active`; `https://int.ikigenba.com/` → 200
>   over valid TLS; OAuth AS metadata valid JSON; `/services` advertises all 6 MCP
>   services at `https://int.ikigenba.com/srv/<svc>/mcp`; each `/srv/<svc>/`
>   returns a controlled 401 (no 502s).

Per service: `bin/bump <app> <level>` (commits `VERSION` to `main`, pushes) →
`bin/ship <app>` → `ssh int sudo opsctl stage <app> v<ver> --artifact …` →
`ssh int sudo opsctl deploy <app> v<ver>`. Restart the dashboard **last** (it
re-reads service manifests).

Gate: full suite live and healthy on `int.ikigenba.com`; tests green.

### Phase E — Abandon `ai` — **COMPLETE**

Hand off to the infra-repo agent in `~/projects/metaspot`: `terraform destroy`
the `ai/` root, remove `ai` from `mgmt/accounts.tf`, drop `delegation_ai`, remove
the now-unused `templates/metaspot-*`. Delete the `ai` `~/.ssh/config` entry
locally. No data to preserve, no soak required.

> **Phase E results (2026-06-06) — `ai` infra destroyed; account org-departure
> blocked on one manual step.** Owner confirmed the `ai` data was disposable
> development data. All teardown commits landed in `~/projects/metaspot`:
> `4b51197`, `0e41138`, `0b6e31f`, `58897bf`, `825d2ae` (pushed to `origin/main`,
> confirmed via `git ls-remote`).
>
> - **NS delegation removed:** `ai.metaspot.org` NS delegation dropped from
>   `mgmt/delegations.tf` and applied (`4b51197`).
> - **Per-account infra destroyed:** `ai` backups bucket
>   (`ai-metaspot-org-417780655767`, 9 disposable dev objects) emptied; the `ai/`
>   Terraform root destroyed (**17 resources**: box/EIP/SG/IAM/zone
>   `ai.metaspot.org`/buckets/SSM); `ai/` dir removed from the repo (`0e41138`).
> - **Dead templates removed:** `templates/metaspot-*` deleted; `AGENTS.md`
>   template refs repointed to `ikigenba-*` (`0b6e31f`).
> - **Bootstrap tfstate destroyed:** the `bootstrap/ai` tfstate bucket
>   (`metaspot-ai-tfstate-417780655767`) emptied (28 versions/markers) and
>   destroyed; `bootstrap/ai` dir removed (`58897bf`).
> - **Org membership — removed via account closure (`825d2ae`):** `ai` removed
>   from `mgmt/accounts.tf` and its SSO admin assignment destroyed (`825d2ae`).
>   The earlier standalone-account leave-org route hit AWS
>   `ConstraintViolationException` (missing standalone prereqs); see the
>   finalization block below for how it was resolved.
>
> **Phase E finalization (2026-06-06) — `ai` account CLOSED; Phase E complete.**
> Rather than complete the manual standalone-account prereqs (Route 1), the owner
> confirmed closing the account outright (Route 2):
>
> - **Account closed:** `aws organizations close-account --account-id
>   417780655767` was run from the `mgmt` management account. Account
>   `417780655767` is now **Status=SUSPENDED** (AWS permanently deletes it after
>   the ~90-day window).
> - **Terraform state reconciled:** `terraform -chdir=metaspot/mgmt state rm
>   'aws_organizations_account.this["ai"]'` dropped the closed account from state;
>   `mgmt` now **plans clean ("No changes")**.
> - **Local config cleaned:** removed `Host ai.metaspot.org ai` from
>   `~/.ssh/config` and `[profile ai]` from `~/.aws/config`. The shared key
>   `~/.ssh/id_ed25519_ai4mgreenly` is **preserved** (used elsewhere).
> - **Residual:** the only thing outstanding is AWS's ~90-day deletion window for
>   the SUSPENDED account — **nothing for anyone to do**; it completes on its own.

Gate: **MET.** `ai` infra destroyed and the account closed (SUSPENDED); `int` is
the sole live deployment; mgmt Terraform plans clean; local `ai` ssh/aws config
removed. Phase E is **complete** — the only residual is AWS's ~90-day deletion
window, which requires no action.

> **Unrelated follow-up (doc-drift, not blocking):** `../metaspot/AGENTS.md`
> prose still references the old `/etc/metaspot/env`, `METASPOT_*`, and
> `/metaspot/<env>/app-config` names — a minor doc-only cleanup, independent of
> this rebrand.

> **Deferred, not in this plan (§2):** GitHub org/repo move + folder rename;
> `ikigai-onebox`. The Google Workspace domain stays `logic-refinery.com`
> permanently.

---

## 5. Execution model & cross-repo coordination

- **Interactive vs autonomous boundary.** Phase A is run by the **main agent**
  (it needs prompts; subagents can't ask). Phases B→C→D are **strictly-sequential
  subagents**, each gated by the main agent verifying the prior phase's green
  before launching the next. One subagent per phase (B is one large rename
  subagent; C and D each one). No phase starts until the previous gate passes.
- **Ordering rationale.** Phase A (infra) is self-contained — it bakes the
  **locked §1 names** and depends on no suite code, so it goes first and clears
  the prompts. Only Phase C (`init-box`/seeding) and D (deploy) depend on Phase
  B's renamed `opsctl`/`bin/secrets`, and they run after B. The original §5
  claim "rename must land before infra" was over-stated: only the box-provision
  steps need it, not the terraform apply.
- **Name agreement is automatic** — infra templates (Phase A) and
  `opsctl`/`bin/secrets` (Phase B) both use the §1 table, so
  `/etc/ikigenba/env`, `ikigenba-launch`, `/ikigenba/int/app-config`, and
  `int-ikigenba-com-<id>` line up without a manual cross-check.
- **The `ikigenba.com` zone already exists** (`mgmt/ikigenba.tf`); Phase A only
  adds the `int` account + its NS delegation — no apex zone work.
- **`ai` teardown is deferred** (Phase E), the infra repo's job, independent of
  this run.

---

## 6. Decisions / risks — RESOLVED (Phase 0)

1. **`ai` data migration.** *Resolved:* none. `ai` holds no data worth keeping;
   it is abandoned (destroyed wholesale by the infra-repo agent), not migrated.
   This is what lets the codename rename fold into greenfield (Q4) and removes
   the parallel-run window. (Secrets are not "data" in this sense either — they
   are seeded onto `int` from local sources, never migrated off `ai`; see
   Phase C.)
2. **Google Workspace federation domain.** *Resolved:* stays `logic-refinery.com`
   (owner = `michaelgreenly@logic-refinery.com`), unchanged and brand-independent.
   The runtime gate (`callback.go:91`) reads only `GOOGLE_WORKSPACE_DOMAIN`; the
   `metaspot.org` in tests is a self-consistent stub token
   (`callback_test.go:67–72` asserts it equals `StubIdentity.HostedDomain`), not
   a production value — it can stay as an arbitrary token. **No identity-domain
   move exists** (the original plan's framing of this as "a real decision, not a
   string swap" was incorrect).
3. **`ikigai-onebox` Dropbox app folder.** *Resolved:* leave as-is permanently —
   opaque, owner-only, not customer-facing.
4. **GitHub org/repo move timing.** *Resolved:* deferred to a later process;
   repo, `origin` (`git@github.com:mgreenly/ikigai.git`), and the local checkout
   all stay. Zero code impact (bare module names, relative `replace` paths).
5. **Add-vs-rename the `ai` account** (infra). *Resolved:* add fresh `int`,
   destroy `ai` afterward — AWS accounts don't rename cleanly and there's no data
   to carry.
6. **Apex-domain literal in `bin/ship`.** *Resolved:* promote the `.ikigenba.com`
   suffix to a `deploy.env` variable so no domain literal remains in the script.
7. **SSH key for the `int` box.** *Resolved (this session):* generate a dedicated
   `~/.ssh/id_ed25519_int_ikigenba_com`, **no passphrase**, with a `Host
   int.ikigenba.com int` ssh-config entry so `ssh int` works simply. Owner's
   default key is `id_ecdsa`, but a dedicated per-account key is preferred for
   blast-radius isolation; bake its `.pub` into `int/`'s `aws_key_pair` (§3d/§3e).

**Execution-model decisions (this session) — how the next run is driven.**
- **AWS auth:** SSO session is live; agents run terraform/`aws` against it. **No
  agent runs `aws sso login`.** Expiry mid-run → stop and ask.
- **Apply gates:** agents `plan`; the main agent pauses for explicit go before
  each `apply` (mandatory at `mgmt` account creation — the one irreversible
  step). Manual verification in the TF process is wanted.
- **Interactive vs autonomous:** subagents can't prompt, so all human-touch
  steps are front-loaded into **Phase A** (main agent); Phases B→C→D are
  strictly-sequential subagents with no prompts (§4/§5).
- **Secret seeding:** existing per-app `bin/secrets` (masked, never prints a
  value, `--value file://`); the agent feeds `yes` and sees only masked output.
  No agent-authored seeding script.
- **Git:** all work directly on `main` in **both** repos, pushed to `origin`; no
  feature branch (owner-authorized); `Co-Authored-By` trailer on commits.
- **OAuth callback whitelist:** already done (owner-confirmed) — not a step.
- **Scope of the run:** ends at **`int` fully live (Phase D)**. `ai` teardown
  (Phase E) is a separate, later hand-off — **not** part of this run.

**Q4 — fold the codename rename into greenfield (the structural change).**
*Resolved: yes — delete the separate codename phase.* The original plan deferred
`METASPOT_* → IKIGENBA_*` because it "breaks every running box and migrates
persistent data." With `ai` abandoned (no data, Q1) and `int` built fresh, there
is no running box to break and nothing to migrate — `int` is *born* on
`IKIGENBA_*`. The asymmetry that decides it: the GitHub move (Q4-adjacent) is
equally cheap whenever done, so deferring costs nothing; the codename rename is
**cheapest now and strictly more expensive every day `int` runs** (a later rename
would mean swapping a live `/etc/ikigenba/env`, re-pointing accumulated S3
backups, recreating SSM params). Deferring it would manufacture exactly the
live-box migration the deferral was meant to avoid. Per the principles skill:
pay the complexity once, now, while it is free.

---

## 7. Done-definition

- The suite runs on `int.ikigenba.com` with valid TLS; all tests green.
- **Zero `metaspot` strings anywhere** in this repo — brand *and* codename
  (`IKIGENBA_*`, `/etc/ikigenba/env`, `ikigenba-launch`, `*-ikigenba-com-*`
  buckets, `/ikigenba/int` SSM).
- **No bare-suite `ikigai` string remains.** The unrelated `ikigai-cli` /
  `ikigai-tui` references and the `ikigai-onebox` app-folder name are
  intentionally retained (§2).
- `ai` account/box destroyed by the infra-repo agent; `int` is the sole live
  deployment.
- **Deferred (not part of "done" here):** GitHub org/repo + local folder move.
  Google Workspace owner identity remains `logic-refinery.com`.
