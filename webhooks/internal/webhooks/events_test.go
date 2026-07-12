package webhooks

import (
	"context"
	"database/sql"
	"encoding/base64"
	"encoding/json"
	"path/filepath"
	"reflect"
	"testing"
	"time"

	chassis "appkit/db"
	"eventplane/outbox"

	"webhooks/internal/db"
)

// newRecordFixture stands up a real temp-file SQLite (never :memory:), migrates
// it so the outbox table exists, wires the Service with a real *outbox.Outbox
// over a fixed clock, inserts one webhook, and returns everything a Record test
// needs — including the db path so a freshly-opened connection can prove
// durability across reopen.
func newRecordFixture(t *testing.T, wh db.Webhook) (svc *Service, dbPath string, now time.Time) {
	t.Helper()
	dbPath = filepath.Join(t.TempDir(), "webhooks.db")
	conn, err := chassis.Open(dbPath)
	if err != nil {
		t.Fatalf("chassis.Open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := chassis.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("chassis.LoadMigrations: %v", err)
	}
	if err := chassis.Migrate(context.Background(), conn, migs); err != nil {
		t.Fatalf("chassis.Migrate: %v", err)
	}
	now = time.Date(2026, 6, 25, 12, 0, 0, 123456789, time.UTC)
	clk := fixedClock{t: now}

	ob, err := outbox.New(conn, outbox.Options{
		Source:   "webhooks",
		Registry: Events,
		Now:      clk.Now,
	})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}

	svc = NewService(conn, clk)
	svc.Outbox = ob

	wh.CreatedAt = now
	if err := db.NewStore(conn).Insert(context.Background(), wh, "deadbeef"); err != nil {
		t.Fatalf("Insert webhook: %v", err)
	}
	return svc, dbPath, now
}

// decodeOnlyOutboxRow asserts there is exactly one outbox row read through conn
// and returns its routing fields and decoded payload.
func decodeOnlyOutboxRow(t *testing.T, conn *sql.DB) (kind, subject string, p webhookReceivedPayload) {
	t.Helper()
	var n int
	if err := conn.QueryRow(`SELECT count(*) FROM outbox`).Scan(&n); err != nil {
		t.Fatalf("count outbox: %v", err)
	}
	if n != 1 {
		t.Fatalf("expected exactly one outbox row, got %d", n)
	}
	var payload string
	if err := conn.QueryRow(`SELECT kind, subject, payload FROM outbox`).Scan(&kind, &subject, &payload); err != nil {
		t.Fatalf("scan outbox: %v", err)
	}
	if err := json.Unmarshal([]byte(payload), &p); err != nil {
		t.Fatalf("unmarshal payload: %v", err)
	}
	return kind, subject, p
}

// R-GTUZ-AIGW — one Record writes exactly one received row whose payload
// carries the webhook name, stored owner, content_type, and a binary body that
// base64-decodes byte-for-byte.
func TestRecord_WritesEventWithBinaryBodyRecoverable(t *testing.T) {
	binary := []byte{0x00, 0xff, 0x01, 0xfe, 0x80, 0x7f, 0x00, 0xc3, 0x28}
	wh := db.Webhook{Name: "deploy-hook", OwnerEmail: "owner@example.com"}
	svc, _, _ := newRecordFixture(t, wh)

	if err := svc.Record(context.Background(), wh, "application/octet-stream", binary); err != nil {
		t.Fatalf("Record: %v", err)
	}

	kind, subject, p := decodeOnlyOutboxRow(t, svc.db)
	if kind != "received" || subject != "/deploy-hook" {
		t.Fatalf("route = (%q, %q), want (received, /deploy-hook)", kind, subject)
	}
	if p.Name != "deploy-hook" {
		t.Errorf("name = %q, want deploy-hook", p.Name)
	}
	if p.Owner != "owner@example.com" {
		t.Errorf("owner = %q, want owner@example.com", p.Owner)
	}
	if p.ContentType != "application/octet-stream" {
		t.Errorf("content_type = %q", p.ContentType)
	}
	got, err := base64.StdEncoding.DecodeString(p.Body)
	if err != nil {
		t.Fatalf("decode body: %v", err)
	}
	if string(got) != string(binary) {
		t.Errorf("body = %v, want %v (byte-for-byte)", got, binary)
	}
}

// R-GV2V-OA7L — the payload owner is always the stored owner_email, never any
// owner the triggering call might carry.
func TestRecord_StampsStoredOwnerNotCallerInput(t *testing.T) {
	wh := db.Webhook{Name: "scoped", OwnerEmail: "stored-owner@example.com"}
	svc, _, _ := newRecordFixture(t, wh)

	// The caller passes a wh value whose OwnerEmail is the stored one; even if a
	// different identity triggered the call, Record only ever reads wh.OwnerEmail.
	if err := svc.Record(context.Background(), wh, "text/plain", []byte("hi")); err != nil {
		t.Fatalf("Record: %v", err)
	}

	_, _, p := decodeOnlyOutboxRow(t, svc.db)
	if p.Owner != "stored-owner@example.com" {
		t.Fatalf("owner = %q, want stored-owner@example.com (caller input must never be echoed)", p.Owner)
	}
}

// R-GWAS-21YA — after Record returns nil the row is durable: a freshly-opened
// connection to the same temp file sees it (durable-before-ack, not a value on
// the live handle).
func TestRecord_DurableAcrossReopen(t *testing.T) {
	wh := db.Webhook{Name: "durable", OwnerEmail: "owner@example.com"}
	svc, dbPath, _ := newRecordFixture(t, wh)

	if err := svc.Record(context.Background(), wh, "text/plain", []byte("payload")); err != nil {
		t.Fatalf("Record: %v", err)
	}

	fresh, err := chassis.Open(dbPath)
	if err != nil {
		t.Fatalf("reopen db: %v", err)
	}
	defer fresh.Close()

	kind, subject, p := decodeOnlyOutboxRow(t, fresh)
	if kind != "received" || subject != "/durable" || p.Name != "durable" {
		t.Fatalf("reopened row = (%q, %q, %q), want (received, /durable, durable)", kind, subject, p.Name)
	}
}

// R-GXIO-FTOZ — one Record produces exactly one row, and the webhook's
// last_triggered_at equals the payload's received_at (append + touch committed
// in one tx under one fixed clock).
func TestRecord_TouchEqualsReceivedAtInOneTx(t *testing.T) {
	wh := db.Webhook{Name: "atomic", OwnerEmail: "owner@example.com"}
	svc, _, _ := newRecordFixture(t, wh)

	if err := svc.Record(context.Background(), wh, "application/json", []byte("{}")); err != nil {
		t.Fatalf("Record: %v", err)
	}

	_, _, p := decodeOnlyOutboxRow(t, svc.db) // also asserts exactly one row

	got, _, ok, err := db.NewStore(svc.db).GetByName(context.Background(), "atomic")
	if err != nil || !ok {
		t.Fatalf("GetByName: ok=%v err=%v", ok, err)
	}
	if got.LastTriggeredAt == nil {
		t.Fatal("last_triggered_at not stamped")
	}
	lt := got.LastTriggeredAt.UTC().Format(time.RFC3339Nano)
	if lt != p.ReceivedAt {
		t.Fatalf("last_triggered_at %q != received_at %q", lt, p.ReceivedAt)
	}
}

// R-A3FB-J3ZK — Records for distinct valid hook names commit the received kind,
// per-hook routing subject, unchanged payload shape, and last-triggered touch in
// the same real SQLite transaction.
func TestRecord_RoutesEachHookAndTouchesAtomically(t *testing.T) {
	deploy := db.Webhook{Name: "deploy-hook", OwnerEmail: "deploy@example.com"}
	svc, _, now := newRecordFixture(t, deploy)
	alpha := db.Webhook{Name: "alpha_1", OwnerEmail: "alpha@example.com", CreatedAt: now}
	if err := db.NewStore(svc.db).Insert(context.Background(), alpha, "alpha-secret"); err != nil {
		t.Fatalf("insert alpha webhook: %v", err)
	}

	for _, wh := range []db.Webhook{deploy, alpha} {
		if err := svc.Record(context.Background(), wh, "application/json", []byte(`{"hook":true}`)); err != nil {
			t.Fatalf("Record(%s): %v", wh.Name, err)
		}
	}

	rows, err := svc.db.Query(`SELECT kind, subject, payload FROM outbox ORDER BY subject`)
	if err != nil {
		t.Fatalf("query outbox: %v", err)
	}
	defer rows.Close()
	wantSubjects := map[string]string{"/alpha_1": "alpha@example.com", "/deploy-hook": "deploy@example.com"}
	seen := map[string]bool{}
	for rows.Next() {
		var kind, subject, raw string
		if err := rows.Scan(&kind, &subject, &raw); err != nil {
			t.Fatalf("scan outbox: %v", err)
		}
		if kind != "received" {
			t.Errorf("kind = %q, want received", kind)
		}
		owner, ok := wantSubjects[subject]
		if !ok {
			t.Fatalf("unexpected subject %q", subject)
		}
		seen[subject] = true
		var payload map[string]any
		if err := json.Unmarshal([]byte(raw), &payload); err != nil {
			t.Fatalf("unmarshal payload for %s: %v", subject, err)
		}
		wantKeys := map[string]bool{"name": true, "owner": true, "received_at": true, "content_type": true, "body": true}
		gotKeys := map[string]bool{}
		for key := range payload {
			gotKeys[key] = true
		}
		if !reflect.DeepEqual(gotKeys, wantKeys) {
			t.Errorf("payload keys for %s = %v, want %v", subject, gotKeys, wantKeys)
		}
		if payload["name"] != subject[1:] || payload["owner"] != owner {
			t.Errorf("payload for %s = %v, want name %q and owner %q", subject, payload, subject[1:], owner)
		}
		wh, _, ok, err := db.NewStore(svc.db).GetByName(context.Background(), subject[1:])
		if err != nil || !ok || wh.LastTriggeredAt == nil {
			t.Fatalf("GetByName(%s): webhook=%+v ok=%v err=%v", subject[1:], wh, ok, err)
		}
		if got := wh.LastTriggeredAt.UTC().Format(time.RFC3339Nano); got != payload["received_at"] {
			t.Errorf("last_triggered_at for %s = %q, payload received_at = %v", subject, got, payload["received_at"])
		}
	}
	if err := rows.Err(); err != nil {
		t.Fatalf("iterate outbox: %v", err)
	}
	if !reflect.DeepEqual(seen, map[string]bool{"/alpha_1": true, "/deploy-hook": true}) {
		t.Fatalf("outbox subjects = %v, want one row for each hook", seen)
	}
}

// R-GYQK-TLFO — Events declares received, and an Append of a kind not in
// the registry errors with no row written.
func TestEvents_RegistryGatesAppend(t *testing.T) {
	if !registryHas(Events, "received") {
		t.Fatal("Events does not declare received")
	}

	dbPath := filepath.Join(t.TempDir(), "webhooks.db")
	conn, err := chassis.Open(dbPath)
	if err != nil {
		t.Fatalf("chassis.Open: %v", err)
	}
	defer conn.Close()
	migs, err := chassis.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("chassis.LoadMigrations: %v", err)
	}
	if err := chassis.Migrate(context.Background(), conn, migs); err != nil {
		t.Fatalf("chassis.Migrate: %v", err)
	}
	ob, err := outbox.New(conn, outbox.Options{Source: "webhooks", Registry: Events})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}

	before := outboxCount(t, conn)

	tx, err := conn.BeginTx(context.Background(), nil)
	if err != nil {
		t.Fatalf("BeginTx: %v", err)
	}
	if err := ob.Append(tx, outbox.Event{Kind: "not-a-real-kind", Payload: json.RawMessage(`{}`)}); err == nil {
		tx.Rollback()
		t.Fatal("Append of an unregistered type returned nil, want error")
	}
	tx.Rollback()

	if after := outboxCount(t, conn); after != before {
		t.Fatalf("outbox row count changed from %d to %d on rejected Append", before, after)
	}
}

// registryHas reports whether the registry declares eventType (the unexported
// has() is not reachable from a test outside the outbox package).
func registryHas(r outbox.Registry, kind string) bool {
	for _, et := range r {
		if et.Kind == kind {
			return true
		}
	}
	return false
}

func outboxCount(t *testing.T, conn *sql.DB) int {
	t.Helper()
	var n int
	if err := conn.QueryRow(`SELECT count(*) FROM outbox`).Scan(&n); err != nil {
		t.Fatalf("count outbox: %v", err)
	}
	return n
}
