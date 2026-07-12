package event

import (
	"context"
	"database/sql"
	"encoding/json"
	"path/filepath"
	"testing"
	"time"

	appkitdb "appkit/db"

	"cron/internal/crontab"
	"cron/internal/db"

	"eventplane/outbox"
	"eventplane/routing"
)

func newStore(t *testing.T) (*crontab.Store, *sql.DB, context.Context) {
	t.Helper()
	ctx := context.Background()
	conn, err := appkitdb.Open(filepath.Join(t.TempDir(), "test.db"))
	if err != nil {
		t.Fatalf("open: %v", err)
	}
	t.Cleanup(func() { conn.Close() })
	migs, err := appkitdb.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := appkitdb.Migrate(ctx, conn, migs); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	return crontab.NewStore(conn), conn, ctx
}

// R-PVCR-LUXA
func TestSubjectIsSafeAndLiteralFiltersAreExact(t *testing.T) {
	store, conn, ctx := newStore(t)
	_ = store
	ob, err := outbox.New(conn, outbox.Options{Source: "cron"})
	if err != nil {
		t.Fatalf("new outbox: %v", err)
	}
	for _, name := range []string{"a", "a-1", "bill-sweep"} {
		subject := Subject(name)
		if !routing.ValidSubject(subject) {
			t.Fatalf("Subject(%q) = %q is not valid", name, subject)
		}
		ev, err := Build(name, time.Now(), time.Now())
		if err != nil {
			t.Fatalf("Build(%q): %v", name, err)
		}
		tx, err := conn.BeginTx(ctx, nil)
		if err != nil {
			t.Fatalf("begin: %v", err)
		}
		if err := ob.Append(tx, ev); err != nil {
			tx.Rollback()
			t.Fatalf("Append(%q): %v", name, err)
		}
		if err := tx.Commit(); err != nil {
			t.Fatalf("commit: %v", err)
		}
	}
	pattern := "cron:tick/bill-sweep"
	for key, want := range map[string]bool{
		"cron:tick/bill-sweep":  true,
		"cron:tick/bill-sweep2": false,
		"cron:tick/bill":        false,
	} {
		got, err := routing.Match(pattern, key)
		if err != nil || got != want {
			t.Fatalf("Match(%q, %q) = %v, %v; want %v, nil", pattern, key, got, err, want)
		}
	}
}

func TestBuildPreservesPayloadAndUsesRoutingFields(t *testing.T) {
	slot := time.Date(2026, 6, 6, 3, 0, 0, 0, time.UTC)
	ev, err := Build("nightly", slot, slot.Add(30*time.Second))
	if err != nil {
		t.Fatalf("build: %v", err)
	}
	if ev.Kind != Kind || ev.Subject != "/nightly" {
		t.Fatalf("routing fields = (%q, %q)", ev.Kind, ev.Subject)
	}
	var got map[string]any
	if err := json.Unmarshal(ev.Payload, &got); err != nil {
		t.Fatalf("unmarshal: %v", err)
	}
	if got["name"] != "nightly" || got["scheduled_for"] != "2026-06-06T03:00:00Z" || got["fired_at"] != "2026-06-06T03:00:30Z" || len(got) != 3 {
		t.Fatalf("payload = %v", got)
	}
}
