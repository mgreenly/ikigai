# Design — Provider-Agnostic Secret Storage with TPM + systemd-creds

This document proposes replacing our cloud secret store (AWS Secrets Manager)
with on-box secrets sealed to the machine's **TPM** via **`systemd-creds`**,
backed by an off-box, provider-neutral recovery key. The goal is to remove a
cloud-provider dependency from the runtime so that an ikigai box depends on
nothing more than **a Linux server with a static IP** — cloud vTPM or bare-metal
silicon, the mechanism is identical.

The short version: each secret exists in **two independent encryptions**. A
runtime copy is sealed to the box's TPM (fast, box-bound, disposable). A
durable copy is encrypted to an **age** recovery public key and can be stored
anywhere because it is inert without the private key. The recovery private key
lives off-box in the team's shared KeePassXC database. TPM gives us runtime
confidentiality; the recovery key gives us durability. Those are two separate
jobs and must not be conflated.

## Context — what we are replacing

Secrets today live in AWS Secrets Manager, reached at runtime via an instance
IAM role. That couples the runtime to a specific cloud provider and to the IAM
machinery that justifies the box talking to AWS at all. It runs against the
operating bet behind ikigai: a quiet, recoverable appliance on one box per
customer, with no hard dependency on cluster or cloud-managed services.

Two facts make a different design available:

- **We run pets, not cattle.** Boxes are durable and long-lived. We do not
  autoscale or routinely replace instances, so a secret bound to a specific
  machine is not a liability the way it would be for immutable/auto-scaled
  infrastructure.
- **TPM 2.0 is a commodity.** Every host we would target — AWS NitroTPM, GCP /
  Azure vTPM, or a physical chip — exposes the same kernel interface
  (`/dev/tpmrm0`) and the same `systemd-creds` tooling. Nothing in our code sees
  the provider underneath.

## The core principle

> **TPM sealing is for runtime confidentiality. It is not the durability
> mechanism.**

A secret encrypted with a box-bound key (the TPM) *is* lost if the box dies —
and that is fine, because that copy is only there for runtime. Durability comes
from a **second** copy encrypted to a key that lives **off the box**. Once that
second copy is encrypted to an off-box key, it is safe to store literally
anywhere (git, object storage, another box, Dropbox), because its safety no
longer depends on where it is stored. That property is also what makes it
provider-agnostic.

The unavoidable trade-off, stated plainly:

> You cannot have all three of (a) box-bound via TPM, (b) recoverable after box
> death, and (c) no copy or key existing off the box. Recovery-after-death
> *requires* something off-box. The art is making that off-box thing a single,
> rarely-touched, well-guarded recovery key — which is far less infrastructure
> than a managed secret store.

## The two copies

| Copy | Encrypted to | Lives | Job | Survives box death? |
|---|---|---|---|---|
| **Runtime cred** | TPM (`host+tpm2`) | on the box, `/etc/credstore.encrypted/<name>.cred` | fast, box-bound runtime access | no — by design |
| **Recovery blob** | off-box **age** recovery public key | anywhere (repo, object store, second box) | durability / rebuild | yes |

At runtime, systemd decrypts the cred into a tmpfs at
`$CREDENTIALS_DIRECTORY/<name>`, readable only by the owning service. Units
declare `LoadCredentialEncrypted=<name>`; the app reads the file from
`$CREDENTIALS_DIRECTORY` instead of calling a cloud API. That is typically the
only app-side change.

### Why `host+tpm2` and not PCRs

We seal with `--with-key=host+tpm2` (TPM key + on-disk host key) and
**deliberately do not bind to PCRs**. PCR binding ties a sealed blob to the
exact boot chain (firmware/bootloader/kernel measurements), which:

- breaks on every legitimate kernel or firmware update unless we reseal, and
- couples blobs to provider-specific boot measurements, defeating portability.

`host+tpm2` without PCRs keeps the property we want — *bound to this box, on any
provider* — without the operational drag. PCR binding is only worth it if the
threat model includes physical or boot-chain tampering, which is not our case.

### Why age (not GPG)

[age](https://age-encryption.org/) does exactly the one thing we need —
encrypt a file to a recipient public key, decrypt later with the private key —
with none of GPG's keyring/agent/web-of-trust machinery.

- Keys are **single-line strings** (`age1…` public, `AGE-SECRET-KEY-1…`
  private), so they drop straight into a KeePassXC entry with no binary export.
- **No keyring or agent state** to manage on the box.
- Fixed modern crypto (X25519 + ChaCha20-Poly1305) — nothing to misconfigure.
- The only thing age does not do is *signing*, which we do not need here.

## The recovery key — root of trust

The age **private** key is the root of trust. It lives **off-box** in the
team's shared **KeePassXC** (`.kdbx`) database. This stays provider-agnostic
(the `.kdbx` is itself an encrypted blob — sync it anywhere) and gives us team
continuity: recovery is not gated on one specific person.

This moves the true root of trust up one level to the **KeePassXC master
password/keyfile**, so the redundancy discipline applies there:

- Back up the `.kdbx` in at least two places (safe to — it is encrypted).
- The master credential is the one thing the whole team must never lose
  simultaneously. KeePassXC's password + keyfile + YubiKey options can harden it.
- **Test recovery on a throwaway box periodically.** A backup we have never
  restored is not a backup.

The team is two people and turnover is considered very unlikely, so the
operational weight here is intentionally light. The rotation path below exists
for completeness, not as a routine.

### Rotation / offboarding

Changing the `.kdbx` master password is necessary but not sufficient if someone
leaves — they may have copied the age key. True rotation means generating a new
age keypair and **re-encrypting every recovery blob** against the new public
key. In this design that is cheap: re-run the backup export on each box. A
`opsctl secrets rekey` path covers that day.

## Flows

### Normal submission (on the box, frequent, no human key)

Secrets are submitted through a web interface on the box. On submit the box
performs two encryptions and discards the plaintext:

```
web UI submit
   ├─ systemd-creds encrypt --name=<name> → /etc/credstore.encrypted/<name>.cred   (runtime, TPM)
   └─ age -r <recovery-pubkey>            → recovery/<name>.age                     (durable backup)
plaintext discarded from memory
```

The box only ever holds the **public** recovery key. Even a fully compromised
box can *write* recovery blobs but can never *read* them.

### Recovery / rebuild (operator-driven, rare, uses the private key)

Recovery happens **from the operator's workstation over SSH**, never by putting
the private key on the box. Decryption happens on the laptop; only the resulting
**plaintext** crosses to the box, transiently, just long enough to seal it into
the new box's TPM (sealing must happen locally on the target). The private key
is pulled from KeePassXC without touching disk:

```
operator laptop                                   new box
──────────────                                   ────────
recovery priv key (from kdbx) ──┐
recovery/<name>.age ────────────┴─ age -d ─► plaintext ──ssh──► systemd-creds encrypt ─► .cred
                                                                (seal into this box's TPM)
                              plaintext discarded both ends
```

One-liner, key never written to a file:

```bash
age -d -i <(keepassxc-cli show -s -a Password vault.kdbx "ikigai/age-recovery") \
    recovery/db_password.age \
  | ssh ai 'sudo systemd-creds encrypt --name=db_password - /etc/credstore.encrypted/db_password.cred'
```

The two paths must not blur: **normal submission uses only the public key + TPM;
recovery is the only path that ever touches the private key.** The web interface
never sees the private key.

## Tooling — fitting into `opsctl` / `bin`

Consistent with the existing split (`opsctl` is the on-box CLI; `bin/` holds the
off-box operator scripts):

| Command | Where it runs | What it does |
|---|---|---|
| `opsctl secrets seal <name>` | on the box | accept plaintext on stdin → write TPM `.cred` **and** age recovery blob |
| `opsctl secrets status` | on the box | list creds present and health-probe that each decrypts |
| `opsctl secrets rekey` | on the box | re-encrypt recovery blobs against a new age public key |
| `bin/secrets-restore <name>` | operator laptop | laptop-side age-decrypt (key from KeePassXC) piped to box-side seal over SSH |

The web submission UI lives on the box and calls the same seal path as
`opsctl secrets seal` under the hood. Health probing a cred is
`systemd-creds decrypt --name=<name> - /dev/null`.

## Cutover

1. Confirm TPM on the box: `systemd-creds has-tpm2` (want `+firmware +driver
   +system`), `ls /dev/tpmrm0`. On AWS this requires NitroTPM enabled on the
   instance / launch template.
2. Generate the age recovery keypair; store the private key in the shared
   KeePassXC, distribute the public key to the boxes.
3. For each secret, pull from AWS one last time and seal on the box:
   ```bash
   aws secretsmanager get-secret-value --secret-id db/password \
     --query SecretString --output text \
   | ssh ai 'sudo opsctl secrets seal db_password'
   ```
4. Add `LoadCredentialEncrypted=<name>` to the units; change apps to read
   `$CREDENTIALS_DIRECTORY/<name>`.
5. Restart, verify each service reads its credential, **test a recovery** on a
   throwaway box.
6. Remove the AWS Secrets Manager calls and the instance IAM permissions that
   granted them — which also deletes a chunk of the box's reason to talk to AWS
   at all.

## Consequences

- Runtime depends only on a Linux box with a TPM — no cloud SDK, no IAM role,
  no metadata service for secret access. The secrets layer becomes *more*
  portable, not less.
- We take on disciplines AWS handled implicitly: redundancy of the recovery
  root (the `.kdbx`), rotation as an N-box fan-out, and a written DR runbook.
  With a two-person team and pet boxes these are light.
- Killing AWS Secrets Manager is a good first domino toward fully
  cloud-agnostic runtime: it is self-contained and removing it deletes the IAM
  role that justified AWS access in the first place. Remaining provider tendrils
  (object storage, managed DB, DNS, networking) are separate efforts.

## Status

Proposal. Not yet implemented. Infrastructure specifics (enabling NitroTPM on
the instance) belong to `../metaspot`, which is authoritative on the box itself.
