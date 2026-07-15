package db

import (
	"context"
	"path/filepath"
	"reflect"
	"testing"
	"time"

	chassis "appkit/db"
)

// newTestStore stands up a real temp-file SQLite database (never :memory:),
// migrates it forward-only through the embedded set, and returns a Store over it.
func newTestStore(t *testing.T) *Store {
	t.Helper()
	conn, err := chassis.Open(filepath.Join(t.TempDir(), "webhooks.db"))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := chassis.LoadMigrations(FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := chassis.Migrate(context.Background(), conn, migs); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	return NewStore(conn)
}

// R-G7RX-751P — the full real migration set adds the scheme/default and nullable
// retained secret, and Store round-trips both schemes without putting secrets on Webhook.
func TestVerificationSchemaAndStoreRoundTrip(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()
	columns := map[string]struct {
		notNull int
		def     any
	}{}
	rows, err := s.db.Query(`PRAGMA table_info(webhooks)`)
	if err != nil {
		t.Fatal(err)
	}
	defer rows.Close()
	for rows.Next() {
		var cid, notNull, pk int
		var name, typ string
		var def any
		if err := rows.Scan(&cid, &name, &typ, &notNull, &def, &pk); err != nil {
			t.Fatal(err)
		}
		columns[name] = struct {
			notNull int
			def     any
		}{notNull, def}
	}
	if got := columns["verification"]; got.notNull != 1 || got.def != "'bearer'" {
		t.Fatalf("verification schema = %+v, want NOT NULL DEFAULT 'bearer'", got)
	}
	if got := columns["secret"]; got.notNull != 0 {
		t.Fatalf("secret schema = %+v, want nullable", got)
	}

	at := fixedTime(t)
	hmacHook := Webhook{Name: "github", OwnerEmail: "a@example.com", Verification: "github-hmac", CreatedAt: at}
	if err := s.Insert(ctx, hmacHook, "fingerprint", "retained-key"); err != nil {
		t.Fatal(err)
	}
	got, hash, secret, ok, err := s.GetByName(ctx, "github")
	if err != nil || !ok || got.Verification != "github-hmac" || hash != "fingerprint" || secret != "retained-key" {
		t.Fatalf("github round trip = (%+v,%q,%q,%v,%v)", got, hash, secret, ok, err)
	}
	if err := s.Insert(ctx, Webhook{Name: "bearer", OwnerEmail: "a@example.com", CreatedAt: at}, "bearer-hash"); err != nil {
		t.Fatal(err)
	}
	got, _, secret, ok, err = s.GetByName(ctx, "bearer")
	if err != nil || !ok || got.Verification != "bearer" || secret != "" {
		t.Fatalf("bearer round trip = (%+v,%q,%v,%v)", got, secret, ok, err)
	}
	typ := reflect.TypeFor[Webhook]()
	if _, found := typ.FieldByName("Secret"); found {
		t.Fatal("Webhook must not carry secret material")
	}
}

// fixedTime is a deterministic RFC3339Nano UTC stamp; this phase has no Clock, so
// timestamps are written directly.
func fixedTime(t *testing.T) time.Time {
	t.Helper()
	ts, err := time.Parse(time.RFC3339Nano, "2026-06-25T12:00:00Z")
	if err != nil {
		t.Fatalf("parse fixed time: %v", err)
	}
	return ts
}

// R-SZ8I-R4EY — a duplicate-name Insert fails on the real PK constraint and the
// original row is left byte-for-byte unchanged (not a Go-side pre-check).
func TestInsertDuplicateNameRejectedByConstraint(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()
	w := Webhook{Name: "deploy", OwnerEmail: "alice@example.com", CreatedAt: fixedTime(t)}
	if err := s.Insert(ctx, w, "hash-alice"); err != nil {
		t.Fatalf("first insert: %v", err)
	}

	dup := Webhook{Name: "deploy", OwnerEmail: "bob@example.com", CreatedAt: fixedTime(t)}
	if err := s.Insert(ctx, dup, "hash-bob"); err == nil {
		t.Fatal("second insert with duplicate name: want non-nil error, got nil")
	}

	got, secretHash, _, ok, err := s.GetByName(ctx, "deploy")
	if err != nil || !ok {
		t.Fatalf("GetByName after dup: ok=%v err=%v", ok, err)
	}
	if got.OwnerEmail != "alice@example.com" {
		t.Errorf("owner_email mutated by failed insert: got %q want alice@example.com", got.OwnerEmail)
	}
	if secretHash != "hash-alice" {
		t.Errorf("secret_hash mutated by failed insert: got %q want hash-alice", secretHash)
	}
}

// R-T0GF-4W5N — ListByOwner returns exactly the calling owner's set and never
// another owner's rows.
func TestListByOwnerIsOwnerScoped(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()
	at := fixedTime(t)
	seed := []Webhook{
		{Name: "a-deploy", OwnerEmail: "alice@example.com", CreatedAt: at},
		{Name: "a-ci", OwnerEmail: "alice@example.com", CreatedAt: at},
		{Name: "b-deploy", OwnerEmail: "bob@example.com", CreatedAt: at},
	}
	for _, w := range seed {
		if err := s.Insert(ctx, w, "h-"+w.Name); err != nil {
			t.Fatalf("seed %s: %v", w.Name, err)
		}
	}

	got, err := s.ListByOwner(ctx, "alice@example.com")
	if err != nil {
		t.Fatalf("ListByOwner: %v", err)
	}
	gotNames := map[string]bool{}
	for _, w := range got {
		if w.OwnerEmail != "alice@example.com" {
			t.Errorf("ListByOwner(alice) leaked %s owned by %s", w.Name, w.OwnerEmail)
		}
		gotNames[w.Name] = true
	}
	if len(gotNames) != 2 || !gotNames["a-deploy"] || !gotNames["a-ci"] {
		t.Errorf("ListByOwner(alice) = %v, want exactly {a-deploy, a-ci}", gotNames)
	}
	if gotNames["b-deploy"] {
		t.Error("ListByOwner(alice) returned bob's webhook")
	}
}

// R-T1OB-INWC — Delete is owner-scoped: A cannot delete B's webhook (deleted
// false, row survives), but B can (deleted true, row gone).
func TestDeleteIsOwnerScoped(t *testing.T) {
	s := newTestStore(t)
	ctx := context.Background()
	w := Webhook{Name: "deploy", OwnerEmail: "bob@example.com", CreatedAt: fixedTime(t)}
	if err := s.Insert(ctx, w, "hash-bob"); err != nil {
		t.Fatalf("seed: %v", err)
	}

	deleted, err := s.Delete(ctx, "alice@example.com", "deploy")
	if err != nil {
		t.Fatalf("Delete(alice): %v", err)
	}
	if deleted {
		t.Error("Delete(alice, deploy) reported deleted=true for bob's row")
	}
	if _, _, _, ok, err := s.GetByName(ctx, "deploy"); err != nil || !ok {
		t.Fatalf("bob's row should survive A's delete: ok=%v err=%v", ok, err)
	}

	deleted, err = s.Delete(ctx, "bob@example.com", "deploy")
	if err != nil {
		t.Fatalf("Delete(bob): %v", err)
	}
	if !deleted {
		t.Error("Delete(bob, deploy) reported deleted=false for his own row")
	}
	if _, _, _, ok, err := s.GetByName(ctx, "deploy"); err != nil || ok {
		t.Fatalf("row should be gone after B's delete: ok=%v err=%v", ok, err)
	}
}
