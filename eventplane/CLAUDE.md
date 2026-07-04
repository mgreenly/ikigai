# eventplane

The suite's shared **event-plane** library — the producer side of the internal
SSE event plane described in `../docs/event-protocol.md` (the normative wire
contract; on any conflict that doc wins).

This is a **library, not a service**: no port, no nginx fragment, no `bin/run`.
It is a library module in the suite mono-repo, wired for local dev by the root
`go.work` and for deterministic builds by a committed `replace eventplane =>
../eventplane` in each consumer's `go.mod`.

## What it provides

Two packages — the producer half (`outbox`) and the consumer half (`consumer`).

`package outbox` — the producer half of the event plane:

- **Atomic outbox** (§4.1): `Append(tx, ev)` writes one event into the `outbox`
  table on the *caller's* existing `*sql.Tx`, so the event commits with the
  domain write or not at all. `Ring()` is a separate call the caller makes
  **after** `tx.Commit()` to wake parked feed connections (§4.3 — never inside
  the tx).
- **The DDL** (§4.5): `outbox.SchemaSQL` is the canonical outbox-table DDL. The
  consumer's own migration runner applies it (single migration authority per DB
  file); a consumer test asserts its migration matches `SchemaSQL`.
- **SSE feed** (§7, §8, §10): `FeedHandler()` serves `GET /feed` — event frames,
  the `caught-up`/`status`/`keepalive` control frames, and the connect-time
  resync/epoch check (`stale-epoch`, `past-horizon`, `diverged`,
  `unintelligible-cursor`) from day one.
- **Generation/epoch token** (§9.3): minted into a sidecar file *outside* the DB
  (so a file-level restore does not roll it back), embedded in every cursor, and
  used for the connect-time `stale-epoch` rejection. A box-level restore or
  rollback (`opsctl restore` / `opsctl rollback`, which recovers an S3 snapshot
  of `state/`) removes the sidecar, so the next boot mints a fresh epoch;
  restore/rollback are opsctl-owned box operations, not binary verbs.
- **Retention** (§11.3): `StartRetention(ctx)` trims the outbox on a background
  timer (off the hot path) and reclaims space with `wal_checkpoint(TRUNCATE)` +
  `VACUUM`.

`package consumer` — the consumer half of the event plane (the mirror of
`outbox`):

- **The engine** (§10): `Run(ctx, cfg, h)` connects to a producer's feed,
  streams events past a durable per-upstream cursor, and invokes the supplied
  `Handler` for every event. It owns the hand-rolled SSE client, the
  reconnect/backoff loop (exponential + jitter, 30s cap, retry indefinitely —
  §10.1), and all four connect-time resync reasons (`stale-epoch`,
  `past-horizon`, `diverged`, `unintelligible-cursor`) — none of which a
  consuming service re-implements.
- **The DDL** (§10.3): `consumer.SchemaSQL` is the canonical `feed_offset`-table
  DDL (the §10.3 schema **minus** the dedup table — this engine drives the
  best-effort external-hop model of §11.2, which tolerates loss and duplicates,
  so it keeps only the cursor + first-subscription marker, no dedup row). A
  consumer applies it through its own migration runner and asserts the migration
  matches the constant.
- **Handler return gates the cursor** (event-triggering decisions §1 — this
  supersedes the prior "commit regardless" best-effort-engine model). The engine
  calls the handler for **every** event (type filtering is the service's job,
  §7.3); its return value decides the cursor:
  - `nil` → advance (commit the cursor).
  - `consumer.ErrSkip` (matched with `errors.Is`, so a wrapped `ErrSkip` counts)
    → log loud + advance. The deliberate opt-in to loss for semantic poison a
    handler can never process — keeps "skip" distinct from a transient "error".
  - any other error → **stall**: the engine does NOT advance; it tears down the
    connection and reconnects from the last committed cursor, so the same event
    re-delivers before any later one (the §10 in-order, at-least-once stall). The
    default-on-unknown-error is therefore stall — the safe direction; a handler
    opts into loss, never the other way.
  **Best-effort is a handler choice, not an engine policy:** a best-effort
  handler (e.g. notify's ntfy push) swallows its external failure and returns
  `nil`. The engine's own unparseable-*envelope* skip (before the handler runs)
  is unchanged; `ErrSkip` is for semantic poison discovered *after* parsing.
- **Reconnect backoff** resets on **progress** (a connection that committed at
  least one event) and engages only on a **no-progress** stall — a reconnect
  that re-fails the same event having committed nothing. A transient blip after a
  long healthy run retries fast; a genuinely stuck handler climbs to the 30s cap.
  (No extra state beyond a `committedAny` flag on the attempt result.)
- **Structural vs transport** (decision 11): a `feed_offset` read/write failure
  (a missing table, a closed DB) escapes `Run` so the process crashes and
  systemd restart-loops visibly; a connect failure / non-200 / dropped
  connection is a transport fault retried internally and never escapes `Run`. A
  context cancellation is a clean shutdown (`Run` returns nil).

## Scope

**Both halves are built.** The producer (`outbox`) and the consumer (`consumer`)
together cover the wire contract end to end. The first real consumer is the
`notify` service, which wires `consumer.Run` to an ntfy push handler.

## Tests

`go test ./...` (workspace mode via the root `go.work`). The three §5.3 producer
correctness tests live in `outbox`: the concurrency stress test, the startup
behavioural probe (two `BEGIN IMMEDIATE`; the second must be refused), and the
slow-reader backpressure test. The consumer's highest-value tests live in
`consumer`: they wire the **real** `outbox.FeedHandler()` to an `httptest`
server and run `consumer.Run` against it — asserting in-order backlog drain +
cursor persistence, resume strictly after the committed cursor, tail-bootstrap
skipping history and surviving a restart-before-commit, each of the four resync
reasons triggering discard-and-reconnect, and control frames not corrupting the
cursor.
