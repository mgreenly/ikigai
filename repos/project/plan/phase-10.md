# Phase 10 — Consumer offset store (feed_offset migration + live-engine proof)

*Realizes design Decision 2 (data model & migrations), slice R-TY2R-GFRU +
R-TZAN-U7IJ. Depends on Phase 01.*

The service can actually boot its declared webhooks consumer: the eventplane
consumer engine's `feed_offset` table exists in this service's schema. A new
migration (created via `bin/create-migration repos feed_offset`) carries a
body byte-identical to the current eventplane `consumer.SchemaSQL`; a DDL
drift guard in `internal/db` asserts that identity (mirroring the existing
outbox guard and notify's `migrations_feed_offset_test.go`). A live-engine
test drives the real eventplane consumer — source `webhooks`, a fully-migrated
real temp SQLite, an `httptest` SSE feed — through its subscription handshake
and asserts the persisted `feed_offset` row, so a schema the engine cannot run
against fails the suite instead of failing at boot.

Frozen migrations stay untouched; the fix is purely additive (a new
timestamped migration file, its guard, and the engine test).

**Done when:** R-TY2R-GFRU (feed_offset migration present, byte-identical to
`consumer.SchemaSQL`, applied set yields the table) and R-TZAN-U7IJ (real
consumer engine completes subscription against the migrated DB and persists
the `webhooks` offset row) are each covered by a clearly-named test, and the
suite is green per design Conventions.
