# eventplane

The suite's shared **event-plane** library — the producer side of the internal
SSE event plane described in `../docs/event-protocol.md` (the normative wire
contract; on any conflict that doc wins).

This is a **library, not a service**: no port, no nginx fragment, no `bin/run`.
It is a sixth git repo under `ikigai/`, wired for local dev by `ikigai/go.work` and for
deterministic builds by a committed `replace eventplane => ../eventplane` in each
consumer's `go.mod`.

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
  used for the connect-time `stale-epoch` rejection. The consumer's `bin/restore`
  deletes the sidecar so the next boot mints a fresh epoch.
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
- **Best-effort semantics** (decision 1, 8; §11.2): the engine commits the cursor
  for **every** event regardless of what the handler returned — a handler error
  is logged and ignored, never retried, never blocking the advance. Type
  filtering is the service's job (§7.3); the engine calls the handler for every
  event type.
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

`go test ./...` (workspace mode via `ikigai/go.work`). The three §5.3 producer
correctness tests live in `outbox`: the concurrency stress test, the startup
behavioural probe (two `BEGIN IMMEDIATE`; the second must be refused), and the
slow-reader backpressure test. The consumer's highest-value tests live in
`consumer`: they wire the **real** `outbox.FeedHandler()` to an `httptest`
server and run `consumer.Run` against it — asserting in-order backlog drain +
cursor persistence, resume strictly after the committed cursor, tail-bootstrap
skipping history and surviving a restart-before-commit, each of the four resync
reasons triggering discard-and-reconnect, and control frames not corrupting the
cursor.
