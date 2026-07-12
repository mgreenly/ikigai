package e2e

import (
	"bufio"
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"
	"time"

	chassis "appkit/db"
	"eventplane/outbox"

	"webhooks/internal/db"
	"webhooks/internal/webhooks"
)

type routingClock struct{ now time.Time }

func (c routingClock) Now() time.Time { return c.now }

// R-A730-OF7N — a real producer record served through the real FeedHandler
// uses the canonical routed SSE key and a kind/subject (never type) envelope.
func TestRecordFeedFramesCanonicalRoutedKey(t *testing.T) {
	conn, err := chassis.Open(filepath.Join(t.TempDir(), "webhooks.db"))
	if err != nil {
		t.Fatalf("open temp sqlite: %v", err)
	}
	defer conn.Close()
	migs, err := chassis.LoadMigrations(db.FS, "migrations")
	if err != nil {
		t.Fatalf("load migrations: %v", err)
	}
	if err := chassis.Migrate(context.Background(), conn, migs); err != nil {
		t.Fatalf("migrate: %v", err)
	}
	clock := routingClock{now: time.Date(2026, 6, 25, 12, 0, 0, 0, time.UTC)}
	ob, err := outbox.New(conn, outbox.Options{Source: "webhooks", Registry: webhooks.Events, Now: clock.Now})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	svc := webhooks.NewService(conn, clock)
	svc.Outbox = ob
	hook := db.Webhook{Name: "deploy-hook", OwnerEmail: "owner@example.com", CreatedAt: clock.Now()}
	if err := db.NewStore(conn).Insert(context.Background(), hook, "secret"); err != nil {
		t.Fatalf("insert webhook: %v", err)
	}
	if err := svc.Record(context.Background(), hook, "application/json", []byte(`{"ok":true}`)); err != nil {
		t.Fatalf("Record: %v", err)
	}

	server := httptest.NewServer(ob.FeedHandler())
	defer server.Close()
	defer server.CloseClientConnections()
	resp, err := http.Get(server.URL)
	if err != nil {
		t.Fatalf("GET feed: %v", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		t.Fatalf("feed status = %d, want 200", resp.StatusCode)
	}
	scanner := bufio.NewScanner(resp.Body)
	var event, data string
	for scanner.Scan() {
		line := scanner.Text()
		if strings.HasPrefix(line, "event: ") {
			event = strings.TrimPrefix(line, "event: ")
		}
		if strings.HasPrefix(line, "data: ") && event == "webhooks:received/deploy-hook" {
			data = strings.TrimPrefix(line, "data: ")
			break
		}
	}
	if err := scanner.Err(); err != nil {
		t.Fatalf("read feed: %v", err)
	}
	if event != "webhooks:received/deploy-hook" || data == "" {
		t.Fatalf("SSE event/data = (%q, %q), want routed received frame", event, data)
	}
	var envelope map[string]any
	if err := json.Unmarshal([]byte(data), &envelope); err != nil {
		t.Fatalf("decode SSE data: %v (%s)", err, data)
	}
	if envelope["kind"] != "received" || envelope["subject"] != "/deploy-hook" {
		t.Fatalf("SSE envelope route = %v", envelope)
	}
	if _, found := envelope["type"]; found {
		t.Fatalf("SSE envelope unexpectedly retains type: %v", envelope)
	}
}
