# Research — should `opsctl deploy` (re)install the nginx fragment?

> **Free-form research note** (slug `deploy-nginx-fragment`). Non-contractual:
> this informs the design author; nothing in the build loop reads it. opsctl has
> no product/research spine yet, so this is a working note under `research/`, not
> the mode-owned `research.md`.
>
> Date: 2026-06-30. Authored from a 6-way parallel investigation (opsctl
> mechanics, appkit feasibility, off-box tooling, dashboard apex, the
> path-routing architecture, external prior art).

## The question

Today a service's committed nginx location fragment (`<svc>/etc/nginx.conf`) is
installed **only** by `opsctl setup` (one-time provisioning). `opsctl deploy`
swaps the binary + regenerates `manifest.env` but **never touches nginx**. So any
change to a service's `etc/nginx.conf` ships **silently un-applied** until an
operator re-runs `setup` by hand. This bit us during the FOUT fix: crm's new
binary went live before its new `/srv/crm/static/` fragment, rendering the
landing page unstyled for logged-in users.

Should `opsctl deploy` (re)render + install the fragment and reload nginx, so a
fragment change ships atomically with the binary that needs it?

## What's actually broken (the precise gap)

A FOUT-style change has two coupled halves per service: (1) a **binary** change
and (2) an **nginx fragment** change. The `bump → ship → stage → deploy` pipeline
only ships half #1. Half #2 lives in `opsctl setup --fragment <file>`, documented
as one-time provisioning. The two halves can land out of order, and nothing warns.

## Key mechanics (verified this session)

- **`opsctl deploy <app> <ver>`** (`opsctl/internal/opsctl/deploy.go:116`): regenerate
  `etc/manifest.env` (run `<bin> manifest`), conditional pre-migration DB backup,
  `migrate`, `chown state/`, atomic swap `bin/run → libexec/<app>-<ver>`, restart
  unit, prune. **Zero nginx calls** (confirmed by grep — nginx reload lives only in
  `setup.go`/`initbox.go`/`teardown.go`).
- **`opsctl setup`** installs the fragment: `--fragment <path>` →
  `LoadFragmentFile` (`setup.go:279`) → `renderFragment(src, port)` substitutes
  **only `__PORT__`** (`setup.go:259`) → atomic write to `FragmentPath()` =
  `/etc/nginx/conf.d/locations/<app>.conf` (`layout.go:200`) → `nginx -t` +
  `systemctl reload nginx` (skippable via `--defer-nginx`). Reload, not restart.
- **The box keeps no fragment source.** `/opt/<app>/etc/` holds only `manifest.env`.
  `bin/ship` scps **only the single static binary** to `/tmp` (`bin/ship:241`); the
  committed `etc/nginx.conf` reaches the box only as a manual one-off scp to `/tmp`
  that the operator passes to `--fragment`, read once and discarded. **There is no
  on-box source for `deploy` to re-render from.** This is the crux.
- **The apply machinery is reusable.** `renderFragment` + `writeFileAtomic` +
  `System.NginxTest`/`NginxReload` (`seam.go:75`) are cleanly seam-isolated;
  calling them from `Deploy` is mechanically trivial. **The missing piece is the
  fragment source on the box, not the apply path.**
- **opsctl is not versioned/shipped** like the apps — it's a hand-built static
  binary `install`ed to `/usr/local/bin/opsctl`; the box never compiles it.

### The dashboard apex is a genuine special case

`dashboard/etc/nginx.conf` is **not** a location fragment — it's the full apex
vhost (two `server{}` blocks: `:80` ACME+redirect, `:443` with the cert refs, the
`/_authn` + `/_session-authn` internal hooks, and `include
.../locations/*.conf;`). It is installed **once** by `opsctl init-box` via
`--apex-block` with `__DOMAIN__`+`__PORT__` substitution (`templates.go:41`,
`initbox.go:69`) and a cert chicken-and-egg bootstrap. `dashboard setup` runs with
no `--port`/`--fragment`, so it drops no `/srv/` fragment. Any deploy-time design
**must not** funnel the dashboard through the `/srv/<svc>` path; it needs an
apex-aware branch (or stays in `init-box`).

## Authoritative constraints (the path-routing architecture — wins on conflict)

From the suite's path-routing architecture (root `AGENTS.md` + `docs/`):

- **nginx is the sole trust boundary.** The fragment *is* the auth enforcement
  point: `auth_request /_authn`, the authoritative `X-Owner-Email`/`X-Client-Id`
  injection **after clearing inbound copies**, and the trailing-slash prefix
  strip all live in it. **A malformed re-render is a security defect, not a
  cosmetic one.** Any automated render must preserve these exactly.
- The model **assigns fragment install to `setup`** (idempotent, root, ssh-from-
  workstation), with the fragment as a templated **separate file** at
  `/etc/nginx/conf.d/locations/<svc>.conf`, reloaded via `systemctl reload nginx`.
  Moving (re)install into `deploy` is a **deliberate departure** from the
  documented setup/deploy split — not forbidden, but a conscious change.
- Only **root** may write `/etc/nginx/...` and reload. The service runs as a
  `nologin` user owning only `/opt/<app>/`. → A service binary **self-installing**
  its fragment is out. (Note: approach (a) below does *not* self-install — the
  binary only *emits*; **opsctl, as root, installs** — so the privilege model is
  preserved.)
- Services own **only** their `<svc>.conf` location file — never the apex block,
  cert, or `/_authn`. At most one `DEFAULT=true` app (the dashboard) per box.
- Minor drift to flag: the architecture describes fragment placeholders as
  `__APP__/__MOUNT__/__PORT__`, but opsctl's `renderFragment` substitutes only
  `__PORT__` (apex adds `__DOMAIN__`). Not load-bearing here, but worth a cleanup
  note.

## External prior art

The dominant industry pattern is **config-as-artifact: the proxy config travels
with the app and is applied in the same transaction as the binary.**

- **Dokku** (closest analog): the nginx vhost template (`nginx.conf.sigil`) lives
  *in the app's repo*; every deploy re-renders + `nginx -t`-validates it and
  **aborts the deploy on invalid config**. No separate "remember nginx" step.
- **Kamal / kamal-proxy**: routing is part of the app's deploy spec; traffic cuts
  to the new version only after a health check.
- **Binary-emits-its-own-config**: a recognized pattern; for a single static Go
  binary the natural form is `go:embed` the fragment + a subcommand
  (`svc emit-nginx-config`). Deploy: install binary → emit → `nginx -t` →
  atomic move → reload. Makes "config older/newer than the binary" **structurally
  impossible**; drift check is a trivial `diff <(svc emit) live`.
- Universal mechanics to keep regardless of approach: **`nginx -t` before reload,
  abort the deploy on failure** (nginx reload is already graceful — a bad config
  leaves the running config untouched), and an explicit **emitted-vs-live diff**
  as a deploy-time drift gate.

Sources: dokku.com/docs/networking/proxies/nginx, kamal-deploy.org/docs/configuration/proxy,
nginx.org/en/docs/beginners_guide, spacelift.io/blog/ansible-configuration-drift-management.

## A pivotal consequence

**Real drift detection is impossible without first getting the fragment source
onto the box.** opsctl can only detect "the live fragment differs from what this
binary expects" if it *has* what the binary expects. The box has no repo. So the
source-delivery question is **primary**: a deploy-time fragment step (and any
honest drift gate) presupposes that `deploy` can obtain the version-correct
fragment locally. Approach (d) "just add a drift check" is therefore **not**
standalone — it depends on (a) or (b).

## Options

### (a) Binary embeds + emits the fragment; opsctl reads it at deploy
The service binary carries its own `etc/nginx.conf` (`go:embed`) and exposes a
new appkit verb (`fragment`/`nginx-fragment`) that prints it, mirroring how
`manifest` is emitted and consumed (`preflight.go:38` already exec-captures
`<bin> manifest`). `opsctl deploy` execs `<staged-bin> fragment` → existing
`renderFragment` → `nginx -t` → atomic write → reload.

- **Pros:** fragment is structurally bound to the binary version (it comes *out*
  of it) → drift is impossible, deploy is atomic, single artifact, no second
  transfer. Matches the suite's existing "binary is the source of truth for its
  own identity" principle (manifest) and the dominant external pattern. opsctl
  keeps `renderFragment`/port-resolution unchanged (port already in the parsed
  manifest). Privilege model preserved (binary emits, root opsctl installs).
- **Cons / real cost (the manifest analogy oversells it):** `manifest`
  *synthesizes* text from `Spec` fields with **no embed** — it is **not** a
  precedent for carrying a free-form file. The honest precedent is
  `Spec.Migrations embed.FS`, which only works because migrations sit *inside*
  their embedding package. `<svc>/etc/nginx.conf` sits two levels **above** the
  `main` package (`<svc>/cmd/<svc>/`), and **`go:embed` cannot escape the package
  directory.** So (a) needs, across all ~12 services, **either**:
  - **(a-i)** relocate `etc/nginx.conf` into an embedding package — breaks the
    committed `<svc>/etc/nginx.conf` convention that `opsctl setup --fragment`,
    `bin/registry`, the deploy docs, and several opsctl tests depend on
    (`teardown_test.go`, `nginx_test.go`); highest blast radius. **Or**
  - **(a-ii)** add a small **root-level embed package** per service (a `.go` file
    in a new package whose dir *contains* `etc/`) that does `//go:embed
    etc/nginx.conf` and feeds an `embed.FS` into a new `Spec.Fragment` field —
    keeps the committed path; cost is ~12 boilerplate files + the appkit change.
  - Plus a **byte-oracle discipline**: keep the committed `etc/nginx.conf` byte-
    equal to what `<bin> fragment` emits (same discipline as `manifest.env`), or
    tests/guarantees diverge.
  - Plus a **no-fragment branch** for worker/consumer services that have none.
  - Plus the **apex special case**: dashboard would re-emit its full vhost via an
    apex-aware branch, or stay in `init-box`.
  - Appkit dispatcher side itself is trivial: one `case` added to the closed
    switch (`appkit.go:244`) + one `Spec` field.

### (b) Ship the fragment as a sidecar file beside the binary
`bin/ship` does a second `scp` of `<svc>/etc/nginx.conf` to the box (a few lines;
the file exists uniformly for all services and ship already resolves
HOST/SSH_KEY). `opsctl stage`/`deploy` pick it up and feed it to the existing
render/validate/reload path.

- **Pros:** low ship-side cost; no Go restructure; fragment stays a plain file at
  its committed path; matches the architecture's "fragment is a separate file"
  framing most literally.
- **Cons:** the fragment becomes a **second artifact that can desync** from the
  binary (version-binding is by convention, not construction — weaker than (a)).
  `stage`'s contract is strictly one `--artifact` executable and it **deletes
  `/tmp` on success**, so the fragment needs its own path/flag and its own
  staging/versioning under `/opt/<app>/etc/`. `deploy` still needs a new fragment
  step (the apply machinery exists, but must be wired in) and the `__PORT__` value
  re-supplied/persisted. More moving parts on the box than (a)'s "emit from the
  one artifact."

### (c) Embed all fragments in opsctl itself — **rejected-leaning**
opsctl would carry every service's fragment and write it at deploy.

- **Cons:** opsctl isn't versioned/shipped; a fragment change would force an
  opsctl rebuild + reinstall on the box, and would couple opsctl releases to
  service-routing changes. Inverts ownership (service should own its routing).
  Only upside is "no per-service Go change," which (a-ii) largely neutralizes.

### (d) Keep setup separate; add a deploy-time drift gate + `reload-nginx` convenience
Don't move install into deploy; instead fail the deploy **loudly** when the live
fragment ≠ what the new binary expects, and add an `opsctl reload-nginx` / `deploy
--with-setup` convenience.

- **Pros:** smallest behavioral change; preserves the documented setup/deploy
  split; addresses the *actual* failure mode (silent un-applied change) by making
  it loud.
- **Cons:** **not standalone** — a meaningful drift check needs the version-
  correct fragment on the box, i.e. it presupposes (a) or (b) for the source. On
  its own it can only detect manual tampering vs a stored hash, **not** a repo
  fragment change (the new expected value isn't on the box). So (d) is a
  *complement* to (a)/(b), not an alternative.

## Recommendation (for the design author to ratify)

**Target architecture: (a-ii) + a deploy-time apply & drift gate; (d)'s
fail-loud discipline folded in; (c) rejected.**

Rationale: the source-delivery question is primary, and (a) is the only option
that makes drift *structurally impossible* rather than merely *detectable*. It
matches both the suite's own "binary owns its identity" precedent (`manifest`)
and the dominant external pattern (Dokku/Kamal). The binary emits, **opsctl (root)
renders + `nginx -t` + atomically installs + reloads** inside `deploy`, reusing
the existing seam — so the trust-boundary and privilege constraints are honored.
The cost is real and worth stating plainly: a root-level embed package + a
`Spec.Fragment` field + a `fragment` verb across ~12 services, a byte-oracle
regen discipline, a no-fragment branch, and an apex-aware branch for the
dashboard.

If that cost is judged too high for now, **(b) is the pragmatic fallback** — it
delivers the source onto the box cheaply and unblocks the same deploy-time apply +
drift gate, accepting weaker (by-convention) version-binding.

Either way, the deploy step should: validate with `nginx -t` **before** reload,
**abort the deploy on invalid config** (Dokku discipline), reload (not restart),
and emit a clear log line when it (re)installs vs no-ops. And **`deploy.md` must
be updated** to document nginx-fragment handling in the deploy flow regardless of
which approach is chosen (today it mentions nginx only as a post-deploy health
check).

## Open questions for design-mode

1. **(a) vs (b)** — is the ~12-service embed restructure + byte-oracle discipline
   worth structural drift-immunity, or is the cheaper sidecar acceptable?
2. **Dashboard/apex** — fold the apex vhost into the same deploy-time mechanism
   (apex-aware branch, re-render from the dashboard binary), or leave it in
   `init-box` and exclude the dashboard from the deploy-fragment step? Note the
   cert chicken-and-egg that `init-box` deliberately handles once.
3. **Backward-compat / cutover** — services already on the box were set up the old
   way. Does the first deploy-with-fragment just overwrite the live
   `locations/<svc>.conf` (idempotent — fine), and do we keep `setup`'s fragment
   step too, or retire it?
4. **Worker/consumer services** with no fragment — explicit no-op vs a guarded
   skip.
5. **Placeholder cleanup** — reconcile the architecture's `__APP__/__MOUNT__/__PORT__`
   description with opsctl's `__PORT__`-only `renderFragment` while we're here.
6. **Rollback** — a `bin/run` rollback to an older binary should also restore that
   binary's fragment. Under (a) this is automatic (emit from the rolled-back
   binary); under (b) the older fragment must still be on the box.

## Source map (for whoever picks this up)

- opsctl: `internal/opsctl/{deploy,setup,templates,initbox,layout,seam,preflight}.go`,
  `cmd/opsctl/main.go`.
- appkit: `appkit.go:225` (dispatch switch), `verbs.go`, `manifest/manifest.go`;
  embed precedent `Spec.Migrations` (`appkit.go:127`).
- off-box: `bin/ship`, `bin/bump`, repo-root `deploy.md`.
- fragments: `<svc>/etc/nginx.conf` (all services); apex `dashboard/etc/nginx.conf`.
- path-routing architecture: root `AGENTS.md` + `docs/`.
