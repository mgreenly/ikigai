package appkit

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"reflect"
	"sync/atomic"
	"testing"
	"time"

	"appkit/db"
	"appkit/server"

	"eventplane/consumer"
)

func TestConsumers_RunTwoFeedsWithIndependentOffsets(t *testing.T) {
	// R-49SJ-YF14
	conn, err := db.Open(filepath.Join(t.TempDir(), "widget.db"))
	if err != nil {
		t.Fatalf("open db: %v", err)
	}
	t.Cleanup(func() { _ = conn.Close() })
	if _, err := conn.Exec(consumer.SchemaSQL); err != nil {
		t.Fatalf("create feed_offset: %v", err)
	}

	crmFeed := newSingleEventFeed(t, "crm", "contact.created", "crm-1")
	defer crmFeed.Close()
	ledgerFeed := newSingleEventFeed(t, "ledger", "invoice.paid", "ledger-1")
	defer ledgerFeed.Close()

	events := make(chan consumer.Event, 4)
	factoryErrors := make(chan string, 4)
	var handlersDone atomic.Bool
	var captured *Router
	spec := Spec{
		App: "widget",
		Consumers: []Consumer{
			{
				Source: "crm",
				Handler: func(rt *Router) consumer.Handler {
					if !handlersDone.Load() || rt != captured || rt.DB() != conn {
						factoryErrors <- "crm factory did not receive the post-Handlers Router"
					}
					return func(ctx context.Context, ev consumer.Event) error {
						events <- ev
						return nil
					}
				},
			},
			{
				Source: "ledger",
				Handler: func(rt *Router) consumer.Handler {
					if !handlersDone.Load() || rt != captured || rt.DB() != conn {
						factoryErrors <- "ledger factory did not receive the post-Handlers Router"
					}
					return func(ctx context.Context, ev consumer.Event) error {
						events <- ev
						return nil
					}
				},
			},
		},
	}

	_, err = server.New(server.Options{
		Addr:       "127.0.0.1:0",
		Logger:     discardLogger(),
		ResourceID: "http://localhost:8080/srv/widget/mcp",
		AuthServer: "http://localhost:8080",
		Version:    versionString(),
		Service:    spec.App,
		DB:         conn,
		Register: func(rt *server.Router) error {
			captured = rt
			handlersDone.Store(true)
			return nil
		},
	})
	if err != nil {
		t.Fatalf("server.New: %v", err)
	}

	workers, err := buildConsumerWorkers(spec, captured, func(k string) string {
		switch k {
		case "WIDGET_CRM_FEED_URL":
			return crmFeed.URL
		case "WIDGET_LEDGER_FEED_URL":
			return ledgerFeed.URL
		case "WIDGET_CRM_FROM", "WIDGET_LEDGER_FROM":
			return "earliest"
		default:
			return ""
		}
	})
	if err != nil {
		t.Fatalf("buildConsumerWorkers: %v", err)
	}
	if len(workers) != 2 {
		t.Fatalf("workers = %d, want 2", len(workers))
	}

	ctx, cancel := context.WithCancel(context.Background())
	srv := newServeTestServer(t)
	done := make(chan error, 1)
	go func() { done <- runServerAndWorkers(ctx, cancel, srv, workers, discardLogger()) }()

	gotEvents := map[string]consumer.Event{}
	deadline := time.After(5 * time.Second)
	for len(gotEvents) < 2 {
		select {
		case msg := <-factoryErrors:
			t.Fatal(msg)
		case ev := <-events:
			gotEvents[ev.Source] = ev
		case err := <-done:
			t.Fatalf("serve returned before both consumers delivered events: %v", err)
		case <-deadline:
			t.Fatalf("timed out waiting for both consumers; got %#v", gotEvents)
		}
	}
	wantOffsets := map[string]string{"crm": "crm-1", "ledger": "ledger-1"}
	waitFor(t, func() bool {
		got, complete := tryReadOffsets(t, conn)
		return complete && reflect.DeepEqual(got, wantOffsets)
	}, "feed_offset rows did not advance for both consumers")
	cancel()
	select {
	case err := <-done:
		if err != nil {
			t.Fatalf("serve shutdown = %v, want nil", err)
		}
	case <-time.After(5 * time.Second):
		t.Fatal("serve did not shut down after cancel")
	}

	if gotEvents["crm"].ID != "crm-1" || gotEvents["ledger"].ID != "ledger-1" {
		t.Fatalf("delivered events = %#v, want crm-1 and ledger-1", gotEvents)
	}
	wantHeaders := []string{"widget", "widget"}
	gotHeaders := []string{crmFeed.consumerID(), ledgerFeed.consumerID()}
	if !reflect.DeepEqual(gotHeaders, wantHeaders) {
		t.Fatalf("consumer ids = %#v, want %#v", gotHeaders, wantHeaders)
	}
	if got := readOffsets(t, conn); !reflect.DeepEqual(got, wantOffsets) {
		t.Fatalf("feed_offset rows = %#v, want independent crm/ledger cursors", got)
	}
}

type singleEventFeed struct {
	*httptest.Server
	consumerIDs chan string
}

func newSingleEventFeed(t *testing.T, source, eventType, id string) *singleEventFeed {
	t.Helper()
	ids := make(chan string, 4)
	h := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		ids <- r.Header.Get("X-Consumer-Id")
		w.Header().Set("Content-Type", "text/event-stream")
		w.WriteHeader(http.StatusOK)
		body, err := json.Marshal(map[string]any{
			"id":      id,
			"type":    eventType,
			"source":  source,
			"time":    "2026-07-06T00:00:00Z",
			"payload": map[string]any{"source": source},
		})
		if err != nil {
			t.Errorf("marshal event: %v", err)
			return
		}
		if _, err := fmt.Fprintf(w, "id: %s\nevent: %s\ndata: %s\n\n", id, eventType, body); err != nil {
			return
		}
		if f, ok := w.(http.Flusher); ok {
			f.Flush()
		}
		<-r.Context().Done()
	})
	return &singleEventFeed{Server: httptest.NewServer(h), consumerIDs: ids}
}

func (f *singleEventFeed) consumerID() string {
	select {
	case id := <-f.consumerIDs:
		return id
	default:
		return ""
	}
}

func readOffsets(t *testing.T, conn *sql.DB) map[string]string {
	t.Helper()
	got, complete := tryReadOffsets(t, conn)
	if !complete {
		t.Fatalf("feed_offset rows are incomplete: %#v", got)
	}
	return got
}

func tryReadOffsets(t *testing.T, conn *sql.DB) (map[string]string, bool) {
	t.Helper()
	rows, err := conn.Query(`SELECT source, cursor, subscribed FROM feed_offset ORDER BY source`)
	if err != nil {
		t.Fatalf("query feed_offset: %v", err)
	}
	defer rows.Close()

	got := map[string]string{}
	for rows.Next() {
		var (
			source     string
			cursor     sql.NullString
			subscribed int
		)
		if err := rows.Scan(&source, &cursor, &subscribed); err != nil {
			t.Fatalf("scan feed_offset: %v", err)
		}
		if subscribed != 1 {
			return got, false
		}
		if !cursor.Valid {
			return got, false
		}
		got[source] = cursor.String
	}
	if err := rows.Err(); err != nil {
		t.Fatalf("feed_offset rows: %v", err)
	}
	return got, len(got) == 2
}
