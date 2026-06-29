package push_test

import (
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"
	"time"

	"eventplane/consumer"
	"eventplane/outbox"

	"notify/internal/push"

	_ "modernc.org/sqlite"
)

// TestHandlerMalformedPayloadSkips asserts the P2 classification: a
// contact.created event with an undecodable payload is permanently unprocessable
// poison, so the handler returns an error satisfying errors.Is(err,
// consumer.ErrSkip) — the engine logs it loud and advances the cursor rather than
// stalling the feed forever. No push is fired.
func TestHandlerMalformedPayloadSkips(t *testing.T) {
	ntfy := newNtfyMock(t)
	discard := slog.New(slog.NewJSONHandler(io.Discard, nil))
	client := push.NewClient(ntfy.srv.URL, "topic", "tok", discard)
	h := push.Handler(client, discard)

	ev := consumer.Event{
		Type:    "contact.created",
		ID:      "01JBADPAYLOAD",
		Source:  "crm",
		Payload: json.RawMessage(`{"display_name": `), // truncated JSON
	}
	err := h(context.Background(), ev)
	if err == nil {
		t.Fatal("malformed payload returned nil, want an ErrSkip-wrapped error")
	}
	if !errors.Is(err, consumer.ErrSkip) {
		t.Fatalf("malformed payload error does not satisfy errors.Is(err, ErrSkip): %v", err)
	}
	// Give any (incorrect) async push a moment; there must be none.
	time.Sleep(20 * time.Millisecond)
	if got := ntfy.snapshot(); len(got) != 0 {
		t.Fatalf("malformed payload fired %d pushes, want 0", len(got))
	}
}

// TestHandlerNonMatchingTypeAdvances asserts a non-contact.created event returns
// nil (the engine advances; it is not ours), with no push.
func TestHandlerNonMatchingTypeAdvances(t *testing.T) {
	ntfy := newNtfyMock(t)
	discard := slog.New(slog.NewJSONHandler(io.Discard, nil))
	client := push.NewClient(ntfy.srv.URL, "topic", "tok", discard)
	h := push.Handler(client, discard)

	ev := consumer.Event{Type: "contact.updated", ID: "01JOTHER", Source: "crm", Payload: json.RawMessage(`{}`)}
	if err := h(context.Background(), ev); err != nil {
		t.Fatalf("non-matching type returned %v, want nil", err)
	}
	time.Sleep(20 * time.Millisecond)
	if got := ntfy.snapshot(); len(got) != 0 {
		t.Fatalf("non-matching type fired %d pushes, want 0", len(got))
	}
}

// capturedPush is one request the mock ntfy server received.
type capturedPush struct {
	method string
	path   string
	title  string
	auth   string
	body   string
}

type ntfyMock struct {
	mu     sync.Mutex
	pushes []capturedPush
	srv    *httptest.Server
}

func newNtfyMock(t *testing.T) *ntfyMock {
	t.Helper()
	m := &ntfyMock{}
	m.srv = httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body, _ := io.ReadAll(r.Body)
		m.mu.Lock()
		m.pushes = append(m.pushes, capturedPush{
			method: r.Method,
			path:   r.URL.Path,
			title:  r.Header.Get("Title"),
			auth:   r.Header.Get("Authorization"),
			body:   string(body),
		})
		m.mu.Unlock()
		w.WriteHeader(http.StatusOK)
	}))
	t.Cleanup(m.srv.Close)
	return m
}

func (m *ntfyMock) snapshot() []capturedPush {
	m.mu.Lock()
	defer m.mu.Unlock()
	out := make([]capturedPush, len(m.pushes))
	copy(out, m.pushes)
	return out
}

func openDB(t *testing.T, path, ddl string) *sql.DB {
	t.Helper()
	dsn := fmt.Sprintf("file:%s?_pragma=journal_mode(WAL)&_pragma=foreign_keys(ON)&_pragma=busy_timeout(5000)", path)
	db, err := sql.Open("sqlite", dsn)
	if err != nil {
		t.Fatalf("open %s: %v", path, err)
	}
	db.SetMaxOpenConns(1)
	if _, err := db.Exec(ddl); err != nil {
		t.Fatalf("apply schema: %v", err)
	}
	t.Cleanup(func() { db.Close() })
	return db
}

// emit writes one event with the given type + payload into the producer outbox
// inside a transaction, commits, and rings — the crm producer wiring (§4.1, §4.3).
func emit(t *testing.T, ob *outbox.Outbox, db *sql.DB, typ, payload string) {
	t.Helper()
	tx, err := db.Begin()
	if err != nil {
		t.Fatalf("begin: %v", err)
	}
	if err := ob.Append(tx, outbox.Event{Type: typ, Payload: json.RawMessage(payload)}); err != nil {
		t.Fatalf("append: %v", err)
	}
	if err := tx.Commit(); err != nil {
		t.Fatalf("commit: %v", err)
	}
	ob.Ring()
}

func feedCursor(t *testing.T, db *sql.DB) sql.NullString {
	t.Helper()
	var cur sql.NullString
	err := db.QueryRow(`SELECT cursor FROM feed_offset WHERE source='crm'`).Scan(&cur)
	if err != nil && err != sql.ErrNoRows {
		t.Fatalf("read feed_offset: %v", err)
	}
	return cur
}

func waitFor(t *testing.T, what string, cond func() bool) {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	for time.Now().Before(deadline) {
		if cond() {
			return
		}
		time.Sleep(2 * time.Millisecond)
	}
	t.Fatalf("timed out waiting for %s", what)
}

// TestEndToEndContactCreatedPush is the §13c e2e: consumer.Run against the REAL
// outbox.FeedHandler, with notify's handler pointed at a mock ntfy. A
// contact.created event produces exactly one POST with the right Title, body, and
// bearer auth; a non-contact.created event produces NO push but the cursor still
// advances (§7.3). Real ntfy.sh is never contacted.
func TestEndToEndContactCreatedPush(t *testing.T) {
	dir := t.TempDir()
	discard := slog.New(slog.NewJSONHandler(io.Discard, nil))

	// Producer: real outbox + feed server.
	pdb := openDB(t, filepath.Join(dir, "producer.db"), outbox.SchemaSQL)
	ob, err := outbox.New(pdb, outbox.Options{
		Source:         "crm",
		DBPath:         filepath.Join(dir, "producer.db"),
		GenerationPath: filepath.Join(dir, "gen"),
		Logger:         discard,
	})
	if err != nil {
		t.Fatalf("outbox.New: %v", err)
	}
	feed := httptest.NewServer(ob.FeedHandler())
	t.Cleanup(feed.Close)

	// Mock ntfy + notify's push handler.
	ntfy := newNtfyMock(t)
	client := push.NewClient(ntfy.srv.URL, "mytopic", "secret-token", discard)
	handler := push.Handler(client, discard)

	// Consumer DB.
	cdb := openDB(t, filepath.Join(dir, "consumer.db"), consumer.SchemaSQL)

	// A contact.created event already in the backlog (earliest so we deterministically drain it).
	emit(t, ob, pdb, "contact.created", `{"id":"c1","display_name":"Alice Example"}`)

	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan error, 1)
	go func() {
		done <- consumer.Run(ctx, consumer.Config{
			FeedURL:    feed.URL + "/feed",
			From:       "earliest",
			DB:         cdb,
			Source:     "crm",
			ConsumerID: "notify",
			Logger:     discard,
		}, handler)
	}()
	t.Cleanup(func() {
		cancel()
		if err := <-done; err != nil {
			t.Errorf("consumer.Run returned non-nil: %v", err)
		}
	})

	// Exactly one push, with the contract fields (decision 6/7).
	waitFor(t, "one push", func() bool { return len(ntfy.snapshot()) == 1 })
	p := ntfy.snapshot()[0]
	if p.method != http.MethodPost {
		t.Errorf("method = %q, want POST", p.method)
	}
	if p.path != "/mytopic" {
		t.Errorf("path = %q, want /mytopic", p.path)
	}
	if p.title != "New contact" {
		t.Errorf("Title = %q, want %q", p.title, "New contact")
	}
	if p.body != "Alice Example" {
		t.Errorf("body = %q, want %q (display_name only)", p.body, "Alice Example")
	}
	if p.auth != "Bearer secret-token" {
		t.Errorf("Authorization = %q, want %q", p.auth, "Bearer secret-token")
	}

	// Record the cursor after event 1, then emit a NON-matching event.
	waitFor(t, "cursor after event 1", func() bool { return feedCursor(t, cdb).Valid })
	cur1 := feedCursor(t, cdb).String
	emit(t, ob, pdb, "contact.updated", `{"id":"c1","display_name":"Alice Example"}`)

	// The cursor MUST advance past the filtered event (§7.3) …
	waitFor(t, "cursor advances past filtered event", func() bool {
		c := feedCursor(t, cdb)
		return c.Valid && c.String != cur1
	})
	// … but NO new push is fired for it.
	if got := ntfy.snapshot(); len(got) != 1 {
		t.Fatalf("non-contact.created produced a push: got %d pushes, want 1", len(got))
	}
}

// R-4LKF-FB23
func TestNotifyConsumerReconstructsCursorAfterProducerCacheRemint(t *testing.T) {
	dir := t.TempDir()
	discard := slog.New(slog.NewJSONHandler(io.Discard, nil))

	producerDB := filepath.Join(dir, "producer.db")
	generationPath := filepath.Join(dir, "cache", "crm.db.generation")
	if err := os.MkdirAll(filepath.Dir(generationPath), 0o755); err != nil {
		t.Fatalf("create producer cache dir: %v", err)
	}
	pdb := openDB(t, producerDB, outbox.SchemaSQL)
	ob1, err := outbox.New(pdb, outbox.Options{
		Source:         "crm",
		DBPath:         producerDB,
		GenerationPath: generationPath,
		Logger:         discard,
	})
	if err != nil {
		t.Fatalf("outbox.New first boot: %v", err)
	}

	var (
		mu      sync.Mutex
		current = ob1
	)
	feed := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		mu.Lock()
		ob := current
		mu.Unlock()
		ob.FeedHandler().ServeHTTP(w, r)
	}))
	t.Cleanup(feed.Close)

	ntfy := newNtfyMock(t)
	client := push.NewClient(ntfy.srv.URL, "topic", "tok", discard)
	notifyDB := filepath.Join(dir, "notify", "state", "notify.db")
	if err := os.MkdirAll(filepath.Dir(notifyDB), 0o755); err != nil {
		t.Fatalf("create notify state dir: %v", err)
	}
	cdb := openDB(t, notifyDB, consumer.SchemaSQL)

	emit(t, ob1, pdb, "contact.created", `{"id":"c1","display_name":"Alice"}`)
	runNotifyConsumerUntil(t, feed.URL+"/feed", cdb, push.Handler(client, discard), func() bool {
		cur := cursorFor(t, cdb, "crm")
		return cur.Valid && strings.HasPrefix(cur.String, ob1.Generation()+".")
	})
	waitFor(t, "first push", func() bool { return len(ntfy.snapshot()) >= 1 })
	cur1 := cursorFor(t, cdb, "crm").String

	if err := os.Remove(generationPath); err != nil {
		t.Fatalf("remove producer generation sidecar: %v", err)
	}
	ob2, err := outbox.New(pdb, outbox.Options{
		Source:         "crm",
		DBPath:         producerDB,
		GenerationPath: generationPath,
		Logger:         discard,
	})
	if err != nil {
		t.Fatalf("outbox.New after cache remint: %v", err)
	}
	if ob2.Generation() == ob1.Generation() {
		t.Fatalf("producer generation did not change after cache sidecar removal")
	}
	mu.Lock()
	current = ob2
	mu.Unlock()

	emit(t, ob2, pdb, "contact.created", `{"id":"c2","display_name":"Bob"}`)
	runNotifyConsumerUntil(t, feed.URL+"/feed", cdb, push.Handler(client, discard), func() bool {
		cur := cursorFor(t, cdb, "crm")
		return cur.Valid && strings.HasPrefix(cur.String, ob2.Generation()+".")
	})
	cur2 := cursorFor(t, cdb, "crm").String
	if cur2 == cur1 {
		t.Fatalf("notify cursor stayed at stale producer generation %q", cur1)
	}
	if !strings.HasPrefix(cur2, ob2.Generation()+".") {
		t.Fatalf("notify cursor = %q, want new producer generation %q", cur2, ob2.Generation())
	}
	waitFor(t, "push after cache remint", func() bool {
		for _, p := range ntfy.snapshot() {
			if p.body == "Bob" {
				return true
			}
		}
		return false
	})
}

func runNotifyConsumerUntil(t *testing.T, feedURL string, db *sql.DB, h consumer.Handler, done func() bool) {
	t.Helper()
	ctx, cancel := context.WithCancel(context.Background())
	errCh := make(chan error, 1)
	go func() {
		errCh <- consumer.Run(ctx, consumer.Config{
			FeedURL:    feedURL,
			From:       "earliest",
			DB:         db,
			Source:     "crm",
			ConsumerID: "notify",
			Logger:     slog.New(slog.NewJSONHandler(io.Discard, nil)),
		}, h)
	}()
	waitFor(t, "notify consumer condition", done)
	cancel()
	if err := <-errCh; err != nil {
		t.Fatalf("consumer.Run returned non-nil: %v", err)
	}
}
