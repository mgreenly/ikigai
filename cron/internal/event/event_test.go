package event

import (
	"context"
	"encoding/json"
	"path/filepath"
	"testing"
	"time"

	appkitdb "appkit/db"

	"cron/internal/crontab"
	"cron/internal/db"
)

func newStore(t *testing.T) (*crontab.Store, context.Context) {
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
	return crontab.NewStore(conn), ctx
}

// TestPublishes_ReflectsLiveCrontab: the live provider returns one cron.<name>
// type per crontab row, in name order, and tracks creates/deletes.
func TestPublishes_ReflectsLiveCrontab(t *testing.T) {
	store, ctx := newStore(t)
	provider := Publishes(store)

	if got := provider(); len(got) != 0 {
		t.Fatalf("empty crontab should publish nothing, got %d", len(got))
	}

	now := time.Now()
	if _, err := store.Create(ctx, "bar", "0 3 * * *", now); err != nil {
		t.Fatalf("create bar: %v", err)
	}
	if _, err := store.Create(ctx, "foo", "* * * * *", now); err != nil {
		t.Fatalf("create foo: %v", err)
	}

	reg := provider()
	if len(reg) != 2 {
		t.Fatalf("want 2 published types, got %d", len(reg))
	}
	// Store.List orders by name: bar, foo.
	if reg[0].Type != "cron.bar" || reg[1].Type != "cron.foo" {
		t.Fatalf("wrong live types: %s, %s", reg[0].Type, reg[1].Type)
	}

	// The reflection Detail must render the shared payload shape for a live type.
	detail, err := reg.Detail("cron.foo")
	if err != nil {
		t.Fatalf("detail cron.foo: %v", err)
	}
	if detail["type"] != "cron.foo" {
		t.Fatalf("detail type wrong: %v", detail["type"])
	}

	// Delete drops the type from the live set.
	if err := store.Delete(ctx, "bar"); err != nil {
		t.Fatalf("delete bar: %v", err)
	}
	if reg := provider(); len(reg) != 1 || reg[0].Type != "cron.foo" {
		t.Fatalf("delete not reflected: %+v", reg)
	}
}

// TestBuild_Payload: the event type and payload JSON match the contract
// (cron.<name>; {name, scheduled_for, fired_at} in UTC RFC3339).
func TestBuild_Payload(t *testing.T) {
	slot := time.Date(2026, 6, 6, 3, 0, 0, 0, time.UTC)
	firedAt := slot.Add(30 * time.Second)
	ev, err := Build("nightly", slot, firedAt)
	if err != nil {
		t.Fatalf("build: %v", err)
	}
	if ev.Type != "cron.nightly" {
		t.Fatalf("type wrong: %s", ev.Type)
	}
	var got map[string]any
	if err := json.Unmarshal(ev.Payload, &got); err != nil {
		t.Fatalf("unmarshal: %v", err)
	}
	want := map[string]any{
		"name":          "nightly",
		"scheduled_for": "2026-06-06T03:00:00Z",
		"fired_at":      "2026-06-06T03:00:30Z",
	}
	for k, v := range want {
		if got[k] != v {
			t.Fatalf("payload[%q] = %v, want %v (full: %v)", k, got[k], v, got)
		}
	}
	if len(got) != 3 {
		t.Fatalf("payload has extra fields: %v", got)
	}
}
